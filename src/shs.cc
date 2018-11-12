#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <map>
#include <string>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <libdaemon/daemon.h>

#include "core/shs_socket.h"
#include "core/shs_thread.h"
#include "core/shs_time.h"
#include "service/http_service.h"
#include "service/monitor_service.h"

#include "output.h"
#include "stats.h"
#include "config.h"
#include "framework.h"
#include "process_cycle.h"

using namespace std;
using namespace boost;
using namespace shs;

const char *g_pid_file = NULL;
const char *get_pid_file()
{
    return g_pid_file;
}

extern pid_t g_monitor_pid;
volatile int g_worker_exiting = 0;
extern uint32_t g_process_generation;

Framework* g_framework = NULL;
std::map<std::string, int> g_inherited_listening;

void worker_exit(void* data)
{
    if (0 == g_worker_exiting)
    {
        g_worker_exiting = 1;
        g_framework->StopService();
        g_framework->Stop();
        set_proc_tag("[worker is shutting down]", 
            g_framework->config()->os_argv());
    }
}

void monitor_exit(void* data)
{
    if (0 == g_worker_exiting)
    {
        g_worker_exiting = 1;
        g_framework->StopService();
        g_framework->Stop();
        set_proc_tag("[monitor is shutting down]", 
            g_framework->config()->os_argv());
    }
}

bool master_init(Config* cfg)
{
    g_process_generation++;

    if (NULL == g_framework)
    {
        g_framework = new Framework(cfg);
    }

    if (!g_framework->InitInMaster())
    {
        return false;
    }

    set_proc_tag("[master]", cfg->os_argv());

    return true;
}

bool master_on_child()
{
    if (g_num_coredump != g_stats->num_coredump)
    {
        g_stats->last_coredump_time = time(NULL);
    }

    g_stats->num_coredump = g_num_coredump;

    return true;
}

bool master_on_reload()
{
    bool ret = true;
    ret = master_init(g_framework->config());
    if (ret)
    {
        g_stats->num_reload++;
        g_stats->last_reload_time = time(NULL);
    }

    return ret;
}

bool worker_main(void *data)
{
    ProcessStats::Reset();

    shs_thread_t* thread = (shs_thread_t *)data;

    if (!g_framework->InitInWorker(&thread->event_base, 
        &thread->event_timer, &thread->conn_pool))
    {
        return false;
    }

    g_framework->RemoveService("Monitor");
#ifdef USE_SO_REUSEPORT
    g_framework->BindService();
#endif
    g_framework->RegisterService();

    set_proc_tag("[worker]", g_framework->config()->os_argv());

    return true;
}

bool monitor_main(void *data)
{
    ProcessStats::Reset();

    g_monitor_pid = getpid();
    shs_thread_t* thread = (shs_thread_t *)data;

    g_framework->InitInMonitor(&thread->event_base, 
        &thread->event_timer, &thread->conn_pool);
    g_framework->RemoveServiceExcept("Monitor");
#ifdef USE_SO_REUSEPORT
    g_framework->BindService();
#endif
    g_framework->RegisterService();

    set_proc_tag("[monitor]", g_framework->config()->os_argv());

    return true;
}

int run_service(Config* cfg) 
{
    std::vector<ServicePtr> services;

    if (cfg->http_port())
    {
        ServicePtr http(new HTTPService());
        services.insert(services.begin(), http);
    }

    if (cfg->monitor_port())
    {
        ServicePtr monitor(new MonitorService());
        services.insert(services.begin(), monitor);
    }

    for (auto& s: services)
    {
        s->Init(g_framework);
        g_framework->AddService(s);

#ifndef USE_SO_REUSEPORT
        bool ret = false;
        auto it = g_inherited_listening.find(s->GetType());
        if (it != g_inherited_listening.end())
        {
            ret = s->Listen(it->second);
        }
        else
        {
            ret = s->ListenExpectAddress();
        }

        if (!ret)
        {
            return -1;
        }

        set_inherited_sockets(s->GetType(), s->ListenFD());
#endif
    }

    time_init();

    init_cycle(cfg->argc(), cfg->argv());
    set_callback("on_child", boost::bind(&master_on_child));
    set_callback("on_reload", boost::bind(&master_on_reload));
    master_process_cycle(cfg->connection_n(), cfg->num_processes(), 
        cfg->pid_file(), worker_main, worker_exit, monitor_main, 
        monitor_exit, NULL);

    return 0;
}

int run_daemon(Config* cfg) 
{
    pid_t pid;
    if (cfg->kill_running())
    {
        daemon_pid_file_kill_wait(SIGTERM, 5);
    }

    if ((pid = daemon_pid_file_is_running()) >= 0)
    {
        return -1;
    }

    daemon_retval_init();
    if ((pid = daemon_fork()) < 0)
    {
        daemon_retval_done();

        return -1;
    }
    else if (pid > 0)
    {
        int ret;
        if ((ret = daemon_retval_wait(5)) < 0)
        {
            return -1;
        }

        exit(0);
    } 
    else 
    {
        daemon_retval_send(0);
        umask(022);

        return 0;
    }

    return 0;
}

int main(int argc, char **argv) 
{
    Config cfg;
    if (!cfg.Init(argc, argv))
    {
        return -1;
    }

    if (!Stats::Init())
    {
        fprintf(stderr, "Create shared memory failed!\n");

        return -1;
    }

    add_inherited_sockets(g_inherited_listening);
    g_pid_file = cfg.pid_file().c_str();
    daemon_pid_file_proc = get_pid_file;

    if (0 == g_inherited_listening.size() && cfg.daemonize())
    {
        if (run_daemon(&cfg) != 0)
        {
            return -1;
        }

        g_daemonized = 1;
        if (!cfg.pid_file().empty())
        {
            string oldpid_file = cfg.pid_file() + ".oldbin";
            unlink(oldpid_file.c_str());
        }
    } 

    if (g_inherited_listening.size() > 0)
    {
        g_daemonized = 1;
    }

    if (!cfg.t() && !cfg.pid_file().empty() && daemon_pid_file_create() < 0)
    {
        return -1;
    }

    if (!master_init(&cfg))
    {
        fprintf(stderr, "Initialize master failed!\n");

        if (!cfg.pid_file().empty())
        {
            daemon_pid_file_remove();
        }

        return -1;
    }

    if (!cfg.t())
    {
        run_service(&cfg);
    }
    else
    {
        fprintf(stderr, "Check configure file: ok\n");
    }

    return 0;
}
