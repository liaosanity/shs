#include "process_cycle.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <map>
#include <string>
#include <gflags/gflags.h>

#include "log/logging.h"
#include "core/shs_types.h"
#include "core/shs_thread.h"
#include "core/shs_conn.h"
#include "core/shs_memory.h"
#include "core/shs_channel.h"

DEFINE_bool(use_graceful_quit, false, "use graceful quit");
DEFINE_int32(graceful_quit_interval, 10, 
    "graceful quit wait time, default 10, in second");

namespace shs 
{
DECLARE_string(status_file);
}

static void start_worker_processes(cycle_t* cycle, int type);
static void start_monitor_processes(cycle_t* cycle, int type);
static void pass_open_channel(shs_channel_t *ch);
static void signal_worker_processes(int signo);
static uint32_t reap_children(cycle_t* cycle);
static void master_process_exit(cycle_t* cycle);
static void worker_process_cycle(void *data);
static void monitor_process_cycle(void *data);
static void worker_process_init(event_base_t *base, cycle_t* cycle);
static void worker_process_exit();
static int channel_add_event(event_base_t *base, int fd, int event, 
    event_handler_pt handler, void *data);
static void channel_handler(event_t *ev);

uint32_t     g_process;
sig_atomic_t g_reap;
sig_atomic_t g_sigio;
sig_atomic_t g_sigalrm;
sig_atomic_t g_terminate;
sig_atomic_t g_quit;
sig_atomic_t g_debug_quit;
sig_atomic_t g_reconfigure;
sig_atomic_t g_reopen;
sig_atomic_t g_change_binary;
pid_t        g_new_binary;
uint32_t     g_inherited;
uint32_t     g_daemonized;
sig_atomic_t g_noaccept;
uint32_t     g_noaccepting;
uint32_t     g_restart;
cycle_t      g_cycle;
pid_t        g_monitor_pid;
uint32_t     g_process_generation;
bool         g_running = true;

using namespace std;
using namespace shs;

void set_callback(const string& name, const MasterEvent& event)
{
    g_cycle.hooks[name] = event;
}

void set_inherited_sockets(std::map<std::string, int> sockets)
{
    g_cycle.listening = sockets;
}

void set_inherited_sockets(const std::string& type, int sockfd)
{
    g_cycle.listening.insert(std::make_pair(type, sockfd));
}

bool init_cycle(int argc, char** argv)
{
    if (0 == argc || NULL == argv)
    {
        return false;
    }

    g_os_argv = (char **)argv;
    g_os_environ = environ;
    g_argc = argc;
    g_argv = (char **)argv;

    return true;
}

void master_process_cycle(int32_t connection_n, int num_processes, 
    const string& pid_file, worker_main_fn worker_main, 
    worker_exit_fn worker_exit, monitor_main_fn monitor_main, 
    monitor_exit_fn monitor_exit, void* data)
{
    uint32_t         delay_shutdown_process;
    uint32_t         sigio;
    sigset_t         set;
    struct itimerval itv;
    uint32_t         live;
    uint32_t         delay;
    uint32_t         delay_times;
    struct timeval   last_reconfig_tm = {0, 0};

    g_process = SHS_PROCESS_MASTER;

    if (init_signals() != SHS_OK) 
    {
        exit(-1);
    }

    if (num_processes <= 0)
    {
        num_processes = 1;
    }

    g_cycle.connection_n = connection_n;
    g_cycle.num_processes = num_processes;
    g_cycle.worker_main_fn = worker_main;
    g_cycle.worker_exit_fn = worker_exit;
    g_cycle.monitor_main_fn = monitor_main;
    g_cycle.monitor_exit_fn = monitor_exit;
    g_cycle.data = data;
    g_cycle.pid_file = pid_file;
    g_cycle.oldpid_file = pid_file + ".oldbin";

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, shs_signal_value(SHS_RECONFIGURE_SIGNAL));
    sigaddset(&set, shs_signal_value(SHS_REOPEN_SIGNAL));
    sigaddset(&set, shs_signal_value(SHS_NOACCEPT_SIGNAL));
    sigaddset(&set, shs_signal_value(SHS_TERMINATE_SIGNAL));
    sigaddset(&set, shs_signal_value(SHS_SHUTDOWN_SIGNAL));
    sigaddset(&set, shs_signal_value(SHS_CHANGEBIN_SIGNAL));
    sigprocmask(SIG_BLOCK, &set, NULL);
    sigemptyset(&set);

    map<string, MasterEvent>::iterator iter = 
        g_cycle.hooks.find("on_start_worker");
    if (iter != g_cycle.hooks.end())
    {
        (*iter).second();
    }

    start_worker_processes(&g_cycle, PROCESS_RESPAWN);
    start_monitor_processes(&g_cycle, PROCESS_RESPAWN);

    delay_shutdown_process = 0;
    g_new_binary = 0;
    delay = 0;
    delay_times = 0;
    sigio = 0;
    live = 1;

    bool graceful_quit = false;

    for ( ;; ) 
    {
        if (delay > 0) 
        {
            itv.it_interval.tv_sec = 0;
            itv.it_interval.tv_usec = 0;
            itv.it_value.tv_sec = delay / 1000;
            itv.it_value.tv_usec = (delay % 1000 ) * 1000;

            if (graceful_quit)
            {
                sleep(FLAGS_graceful_quit_interval);
                graceful_quit = false;
            }

            setitimer(ITIMER_REAL, &itv, NULL);

            delay = 0;
        }

        if (g_sigalrm) 
        {
            sigio = 0;
            g_sigalrm = 0;
            delay_times++;
        }

        sigsuspend(&set);

        map<string, MasterEvent>::iterator iter = 
            g_cycle.hooks.find("on_signal");
        if (iter != g_cycle.hooks.end())
        {
            (*iter).second();
        }

        if (g_reap) 
        {
            g_reap = 0;
            live = reap_children(&g_cycle);

            map<string, MasterEvent>::iterator iter = 
                g_cycle.hooks.find("on_child");
            if (iter != g_cycle.hooks.end())
            {
                (*iter).second();
            }
        }

        if (!live && (g_terminate || g_quit)) 
        {
            master_process_exit(&g_cycle);
        }

        if (g_terminate) 
        {
            if (FLAGS_use_graceful_quit)
            {
                if (access(FLAGS_status_file.c_str(), F_OK) == 0)
                {
                    graceful_quit = true;
                    unlink(FLAGS_status_file.c_str());
                }
            }

            delay = 50 * (2 << delay_times);

            if (sigio) 
            {
                sigio--;

                continue;
            }

            sigio = g_cycle.num_processes + 1;

            if (!graceful_quit)
            {
                if (delay > 1000)
                {
                    signal_worker_processes(SIGKILL);
                } 
                else 
                {
                    signal_worker_processes(shs_signal_value(
                        SHS_TERMINATE_SIGNAL));
                }
            }

            continue;
        }

        if (g_quit) 
        {
            signal_worker_processes(shs_signal_value(
                SHS_SHUTDOWN_SIGNAL));

            continue;
        }

        if (g_reconfigure) 
        {
            g_reconfigure = 0;

            struct timeval now;
            gettimeofday(&now, NULL);

            if (delay_shutdown_process && (last_reconfig_tm.tv_sec 
                && (now.tv_sec - last_reconfig_tm.tv_sec) < 150))
            {
                continue;
            }
            last_reconfig_tm = now;

            if (g_new_binary) 
            {
                start_worker_processes(&g_cycle, PROCESS_RESPAWN);
                start_monitor_processes(&g_cycle, PROCESS_RESPAWN);
                g_noaccepting = 0;

                continue;
            }

            map<string, MasterEvent>::iterator iter = 
                g_cycle.hooks.find("on_reload");
            if (iter != g_cycle.hooks.end())
            {
                if (!(*iter).second())
                {
                    continue;
                }
            }

            start_worker_processes(&g_cycle, PROCESS_JUST_RESPAWN);
            start_monitor_processes(&g_cycle, PROCESS_JUST_RESPAWN);

            if (delay == 0) 
            {
                delay_shutdown_process = 1;
                delay = 60000;
            }

            continue;
        }

        if (delay_shutdown_process && g_sigalrm) 
        {
            delay = 0;
            delay_times = 0;
            delay_shutdown_process = 0;
            live = 1;
            signal_worker_processes(shs_signal_value(SHS_SHUTDOWN_SIGNAL));
        }

        if (g_restart) 
        {
            g_restart = 0;
            start_worker_processes(&g_cycle, PROCESS_RESPAWN);
            start_monitor_processes(&g_cycle, PROCESS_RESPAWN);
            live = 1;
        }

        if (g_change_binary) 
        {
            g_change_binary = 0;
            g_new_binary = exec_new_binary(&g_cycle, g_argv);
        }

        if (g_noaccept) 
        {
            g_noaccept = 0;
            g_noaccepting = 1;
            signal_worker_processes(shs_signal_value(SHS_SHUTDOWN_SIGNAL));
        }
    }
}

static void start_worker_processes(cycle_t* cycle, int type)
{
    shs_channel_t ch;
    ch.command = SHS_CMD_OPEN_CHANNEL;

    for (int i = 0; i < cycle->num_processes; i++) 
    {
        spawn_process(worker_process_cycle, cycle, 
            "worker process", type, 1);

        ch.pid = g_processes[g_process_slot].pid;
        ch.slot = g_process_slot;
        ch.fd = g_processes[g_process_slot].channel[0];

        pass_open_channel(&ch);
    }
}

static void start_monitor_processes(cycle_t* cycle, int type)
{
    spawn_process(monitor_process_cycle, cycle, 
        "monitor process", type, 2);

    shs_channel_t ch;
    ch.command = SHS_CMD_OPEN_CHANNEL;
    ch.pid = g_processes[g_process_slot].pid;
    ch.slot = g_process_slot;
    ch.fd = g_processes[g_process_slot].channel[0];

    pass_open_channel(&ch);

    g_monitor_pid = g_processes[g_process_slot].pid;
}

static void pass_open_channel(shs_channel_t *ch)
{
    for (int i = 0; i < g_last_process; i++) 
    {
        if (i == g_process_slot || -1 == g_processes[i].pid
            || -1 == g_processes[i].channel[0])
        {
            continue;
        }

        shs_write_channel(g_processes[i].channel[0], ch, 
            sizeof(shs_channel_t));
    }
}

static void signal_worker_processes(int signo)
{
    int i;
    int err;
    shs_channel_t ch;

    switch (signo) 
    {
    case shs_signal_value(SHS_SHUTDOWN_SIGNAL):
        ch.command = SHS_CMD_QUIT;
        break;

    case shs_signal_value(SHS_TERMINATE_SIGNAL):
        ch.command = SHS_CMD_TERMINATE;
        break;

    case shs_signal_value(SHS_REOPEN_SIGNAL):
        ch.command = SHS_CMD_REOPEN;
        break;

    default:
        ch.command = 0;
    }

    ch.fd = -1;

    for (i = 0; i < g_last_process; i++) 
    {
        if (g_processes[i].detached || -1 == g_processes[i].pid)
        {
            continue;
        }

        if (g_processes[i].just_spawn) 
        {
            g_processes[i].just_spawn = 0;

            continue;
        }

        if (g_processes[i].exiting
            && signo == shs_signal_value(SHS_SHUTDOWN_SIGNAL))
        {
            continue;
        }

        if (ch.command) 
        {
            if (shs_write_channel(g_processes[i].channel[0], &ch, 
                sizeof(shs_channel_t)) == SHS_OK)
            {
                if (signo != shs_signal_value(SHS_REOPEN_SIGNAL)) 
                {
                    g_processes[i].exiting = 1;
                }

                continue;
            }
        }

        if (-1 == kill(g_processes[i].pid, signo)) 
        {
            err = errno;
            if (err == ESRCH) 
            {
                g_processes[i].exited = 1;
                g_processes[i].exiting = 0;
                g_reap = 1;
            }

            continue;
        }

        if (signo != shs_signal_value(SHS_REOPEN_SIGNAL)) 
        {
            g_processes[i].exiting = 1;
        }
    }
}

static uint32_t reap_children(cycle_t* cycle)
{
    int             i, n;
    uint32_t        live;
    shs_channel_t    ch;

    ch.command = SHS_CMD_CLOSE_CHANNEL;
    ch.fd = -1;

    live = 0;
    for (i = 0; i < g_last_process; i++) 
    {
        if (g_processes[i].pid == -1) 
        {
            continue;
        }

        if (g_processes[i].exited) 
        {
            if (!g_processes[i].detached) 
            {
                shs_close_channel(g_processes[i].channel);

                g_processes[i].channel[0] = -1;
                g_processes[i].channel[1] = -1;

                ch.pid = g_processes[i].pid;
                ch.slot = i;

                for (n = 0; n < g_last_process; n++) 
                {
                    if (g_processes[n].exited || g_processes[n].pid == -1
                        || g_processes[n].channel[0] == -1)
                    {
                        continue;
                    }

                    shs_write_channel(g_processes[n].channel[0], &ch, 
                        sizeof(shs_channel_t));
                }
            }

            if (g_processes[i].respawn && !g_processes[i].exiting
                && !g_terminate && !g_quit)
            {
                if (-1 == spawn_process(
                    g_processes[i].proc, g_processes[i].data, 
                    g_processes[i].name, i, g_processes[i].type))
                {
                    continue;
                }

                ch.command = SHS_CMD_OPEN_CHANNEL;
                ch.pid = g_processes[g_process_slot].pid;
                ch.slot = g_process_slot;
                ch.fd = g_processes[g_process_slot].channel[0];

                pass_open_channel(&ch);

                live = 1;

                continue;
            }

            if (g_processes[i].pid == g_new_binary) 
            {
                if (rename(cycle->oldpid_file.c_str(), 
                    cycle->pid_file.c_str()) != 0)
                {
                    // log err
                }

                g_new_binary = 0;
                if (g_noaccepting) 
                {
                    g_restart = 1;
                    g_noaccepting = 0;
                }
            }

            if (i == g_last_process - 1) 
            {
                g_last_process--;
            } 
            else 
            {
                g_processes[i].pid = -1;
            }
        } 
        else if (g_processes[i].exiting || !g_processes[i].detached) 
        {
            live = 1;
        }
    }

    return live;
}

static void master_process_exit(cycle_t* cycle)
{
    string& name = g_new_binary ? cycle->oldpid_file : cycle->pid_file;
    unlink(name.c_str());

    exit(0);
}

static void worker_process_cycle(void *data)
{ 
    cycle_t* cycle = (cycle_t*)data;

    g_process = SHS_PROCESS_WORKER;

    shs_thread_t* main_thread = 
        (shs_thread_t *)memory_calloc(sizeof(shs_thread_t));
    if (!main_thread)
    {
        SLOG(ERROR) << "memory_calloc() failed";

        exit(2);
    }
    main_thread->type = THREAD_MASTER;
    main_thread->event_base.nevents = cycle->connection_n;

    if (thread_event_init(main_thread) != SHS_OK)
    {
        SLOG(ERROR) << "thread_event_init() failed";

        exit(2);
    }

    worker_process_init(&main_thread->event_base, cycle);
    cycle->data = main_thread;

    if (cycle->worker_main_fn)
    {
        if (!cycle->worker_main_fn(cycle->data))
        {
            exit(2);
        }
    }

    while (g_running)
    {
        thread_event_process(main_thread);
    }

    worker_process_exit();
}

static void monitor_process_cycle(void* data)
{
    cycle_t* cycle = (cycle_t*)data;

    g_process = SHS_PROCESS_MONITOR;

    shs_thread_t* main_thread = 
        (shs_thread_t *)memory_calloc(sizeof(shs_thread_t));
    if (!main_thread)
    {
        SLOG(ERROR) << "memory_calloc() failed";

        exit(2);
    }
    main_thread->type = THREAD_MASTER;
    main_thread->event_base.nevents = 1024;

    if (thread_event_init(main_thread) != SHS_OK)
    {
        SLOG(ERROR) << "thread_event_init() failed";

        exit(2);
    }

    worker_process_init(&main_thread->event_base, cycle);
    cycle->data = main_thread;

    if (cycle->monitor_main_fn)
    {
        if (!cycle->monitor_main_fn(cycle->data))
        {
            exit(2);
        }
    }

    while (g_running)
    {
        thread_event_process(main_thread);
    }

    worker_process_exit();
}

static void worker_process_init(event_base_t *base, cycle_t* cycle)
{
    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);

    for (int n = 0; n < g_last_process; n++) 
    {
        if (-1 == g_processes[n].pid) 
        {
            continue;
        }

        if (n == g_process_slot) 
        {
            continue;
        }

        if (-1 == g_processes[n].channel[1]) 
        {
            continue;
        }

        close(g_processes[n].channel[1]);
    }

    close(g_processes[g_process_slot].channel[0]);

    process_t* process = &g_processes[g_process_slot];

    if (channel_add_event(base, process->channel[1], 
        EVENT_READ_EVENT, channel_handler, cycle) != SHS_OK)
    {
        SLOG(ERROR) << "channel_add_event() failed";

        exit(2);
    }
}

static void worker_process_exit()
{
    exit(0);
}

static int channel_add_event(event_base_t *base, int fd, int event, 
    event_handler_pt handler, void *data)
{
    event_t *ev = NULL;
    event_t *rev = NULL;
    event_t *wev = NULL;
    conn_t  *c = NULL;

    c = conn_get_from_mem(fd);
    if (!c)
    {
        return SHS_ERROR;
    }

    c->pool = NULL;
    c->ev_base = base;
    c->conn_data = data;

    rev = c->read;
    wev = c->write;

    ev = (event == EVENT_READ_EVENT) ? rev : wev;
    ev->handler = handler;

    return event_add(base, ev, event, 0);
}

static void channel_handler(event_t *ev)
{
    int n;
    shs_channel_t ch;
    conn_t *c = (conn_t *)ev->data;
    event_base_t *ev_base = c->ev_base;
    cycle_t* cycle = (cycle_t *)c->conn_data;

    for ( ;; ) 
    {
        n = shs_read_channel(c->fd, &ch, sizeof(shs_channel_t));
        if (n == SHS_ERROR) 
        {
            if (ev_base->event_flags & EVENT_USE_EPOLL_EVENT)
            {
                event_del_conn(ev_base, c, 0);
            }

            close(c->fd);
            conn_free_mem(c);

            return;
        }

        if (ev_base->event_flags & EVENT_USE_EVENTPORT_EVENT)
        {
            if (event_add(ev_base, ev, EVENT_READ_EVENT, 0) < 0)
            {
                SLOG(ERROR) << "g_process = " << g_process 
                    << " event_add() failed";

                return;
            }
        }

        if (n == SHS_AGAIN) 
        {
            return;
        }

        switch (ch.command) 
        {
        case SHS_CMD_QUIT:
            g_quit = 1;
            break;

        case SHS_CMD_TERMINATE:
            g_terminate = 1;
            break;

        case SHS_CMD_REOPEN:
            g_reopen = 1;
            break;

        case SHS_CMD_OPEN_CHANNEL:
            g_processes[ch.slot].pid = ch.pid;
            g_processes[ch.slot].channel[0] = ch.fd;
            break;

        case SHS_CMD_CLOSE_CHANNEL:
            close(g_processes[ch.slot].channel[0]);
            g_processes[ch.slot].channel[0] = -1;
            g_processes[ch.slot].pid = -1;
            break;
        }

        if (g_quit || g_terminate)
        {
            if (cycle->monitor_exit_fn && getpid() == g_monitor_pid)
            {
                cycle->monitor_exit_fn(NULL);
            }
            else if (cycle->worker_exit_fn)
            {
                cycle->worker_exit_fn(cycle->data);
            }
        }
    }
}

