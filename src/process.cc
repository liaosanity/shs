#include "process.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <strings.h>
#include <string>
#include <algorithm>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <gflags/gflags.h>

#include "log/logging.h"
#include "core/shs_socket.h"
#include "core/shs_types.h"
#include "core/shs_channel.h"

#include "process_cycle.h"
#include "stats.h"

DECLARE_bool(use_graceful_quit);

using namespace std;
using namespace shs;

typedef struct shs_signal_s 
{
    int signo;
    const char *signame;
    const char *name;
    void  (*handler)(int signo);
} shs_signal_t;

static void execute_proc(void *data);
static void signal_handler(int signo);
static void process_get_status(void);

int         g_argc;
char      **g_argv;
char      **g_os_argv;
char      **g_os_environ;
pid_t       g_pid;
int         g_channel;
int         g_process_slot;
int         g_last_process;
int         g_num_coredump;
process_t   g_processes[MAX_PROCESSES];

shs_signal_t  g_signals[] = 
{
    { shs_signal_value(SHS_RECONFIGURE_SIGNAL),
      "SIG" shs_value(MW_RECONFIGURE_SIGNAL),
      "reload",
      signal_handler },

    { shs_signal_value(SHS_REOPEN_SIGNAL),
      "SIG" shs_value(MW_REOPEN_SIGNAL),
      "reopen",
      signal_handler },

    { shs_signal_value(SHS_NOACCEPT_SIGNAL),
      "SIG" shs_value(MW_NOACCEPT_SIGNAL),
      "",
      signal_handler },

    { shs_signal_value(SHS_TERMINATE_SIGNAL),
      "SIG" shs_value(MW_TERMINATE_SIGNAL),
      "stop",
      signal_handler },

    { shs_signal_value(SHS_SHUTDOWN_SIGNAL),
      "SIG" shs_value(MW_SHUTDOWN_SIGNAL),
      "quit",
      signal_handler },

    { shs_signal_value(SHS_CHANGEBIN_SIGNAL),
      "SIG" shs_value(MW_CHANGEBIN_SIGNAL),
      "",
      signal_handler },

    { SIGALRM, "SIGALRM", "", signal_handler },

    { SIGINT, "SIGINT", "", signal_handler },

    { SIGIO, "SIGIO", "", signal_handler },

    { SIGCHLD, "SIGCHLD", "", signal_handler },

    { SIGSYS, "SIGSYS, SIG_IGN", "", SIG_IGN },

    { SIGPIPE, "SIGPIPE, SIG_IGN", "", SIG_IGN },

    { 0, NULL, "", NULL }
};

pid_t spawn_process(spawn_proc_pt proc, void *data, 
    const char *name, int respawn, int type)
{
    u_long on;
    pid_t  pid;
    int    s;

    if (respawn >= 0) 
    {
        s = respawn;
    } 
    else 
    {
        for (s = 0; s < g_last_process; s++) 
        {
            if (-1 == g_processes[s].pid) 
            {
                break;
            }
        }

        if (s == MAX_PROCESSES) 
        {
            return -1;
        }
    }

    if (respawn != PROCESS_DETACHED) 
    {
        if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, g_processes[s].channel))
        {
            return -1;
        }

        if (-1 == shs_nonblocking(g_processes[s].channel[0])) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        if (-1 == shs_nonblocking(g_processes[s].channel[1])) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        on = 1;
        if (-1 == ioctl(g_processes[s].channel[0], FIOASYNC, &on)) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        if (-1 == fcntl(g_processes[s].channel[0], F_SETOWN, g_pid)) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        if (-1 == fcntl(g_processes[s].channel[0], F_SETFD, FD_CLOEXEC)) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        if (-1 == fcntl(g_processes[s].channel[1], F_SETFD, FD_CLOEXEC)) 
        {
            shs_close_channel(g_processes[s].channel);

            return -1;
        }

        g_channel = g_processes[s].channel[1];
    } 
    else 
    {
        g_processes[s].channel[0] = -1;
        g_processes[s].channel[1] = -1;
    }

    g_process_slot = s;

    pid = fork();
    switch (pid) 
    {
    case -1:
        shs_close_channel(g_processes[s].channel);
        return -1;

    case 0:
        g_pid = getpid();
        g_processes[s].pid = g_pid;
        g_processes[s].type = type;
        ProcessStats::SetPid(g_pid);
        ProcessStats::SetType(type);
        proc(data);
        break;

    default:
        break;
    }

    g_processes[s].pid = pid;
    g_processes[s].exited = 0;
    g_processes[s].type = type;

    if (respawn >= 0) 
    {
        return pid;
    }

    g_processes[s].proc = proc;
    g_processes[s].data = data;
    g_processes[s].name = name;
    g_processes[s].exiting = 0;

    switch (respawn) 
    {
    case PROCESS_NORESPAWN:
        g_processes[s].respawn = 0;
        g_processes[s].just_spawn = 0;
        g_processes[s].detached = 0;
        break;

    case PROCESS_JUST_SPAWN:
        g_processes[s].respawn = 0;
        g_processes[s].just_spawn = 1;
        g_processes[s].detached = 0;
        break;

    case PROCESS_RESPAWN:
        g_processes[s].respawn = 1;
        g_processes[s].just_spawn = 0;
        g_processes[s].detached = 0;
        break;

    case PROCESS_JUST_RESPAWN:
        g_processes[s].respawn = 1;
        g_processes[s].just_spawn = 1;
        g_processes[s].detached = 0;
        break;

    case PROCESS_DETACHED:
        g_processes[s].respawn = 0;
        g_processes[s].just_spawn = 0;
        g_processes[s].detached = 1;
        break;
    }

    if (s == g_last_process) 
    {
        g_last_process++;
    }

    return pid;
}

pid_t execute(exec_ctx_t *ctx)
{
    return spawn_process(execute_proc, ctx, ctx->name, PROCESS_DETACHED, 0);
}

static void execute_proc(void *data)
{
    exec_ctx_t *ctx = (exec_ctx_t *)data;

    if (-1 == execve(ctx->path, ctx->argv, ctx->env)) 
    {
        SLOG(ERROR) << "execve() failed while executing " 
            << ctx->name << "'" << ctx->path << "'";
    }

    exit(1);
}

int add_inherited_sockets(std::map<std::string, int>& listening)
{
    char* p = getenv("MASTER_WORKERS");
    if (!p)
    {
        return 0;
    }

    std::string inherited(p);
    std::vector<std::string> v;
    boost::algorithm::split(v, inherited, boost::is_any_of(":;"));
    for (auto& vi : v)
    {
        std::vector<std::string> addr;
        boost::algorithm::split(addr, vi, boost::is_any_of(","));
        if (addr.size() != 2)
        {
            continue;
        }

        listening.insert(std::make_pair(addr[0], atoi(addr[1].c_str())));
    }

    g_inherited = 1; 
    g_daemonized = 1;

    return listening.size();
}

char** set_environment(cycle_t* cycle)
{
    char** p;
    char** env;
    int n = 0;

    if (NULL == g_os_environ)
    {
        return NULL;
    }

    const string ld_path = "LD_LIBRARY_PATH";
    for (p = g_os_environ; *p; p++) 
    {
        if (0 == strncmp(*p, ld_path.c_str(), ld_path.size())
            && '=' == (*p)[ld_path.size()]) 
        {
            n++;

            break;
        }
    }

    env = (char**)calloc(n + 1 + 1, sizeof(char*));
    if (NULL == env)
    {
        return NULL;
    }

    n = 0;
    for (p = g_os_environ; *p; p++) 
    {
        if (0 == strncmp(*p, ld_path.c_str(), ld_path.size())
            && '=' == (*p)[ld_path.size()]) 
        {
            env[n++] = strdup(*p);

            break;
        }
    }

    if (cycle->listening.size() > 0)
    {
        string str_env = string("MASTER_WORKERS") + "=";
        for (auto& l : cycle->listening)
        {
            char tmp[11];
            sprintf(tmp, "%s,%ud;",l.first.c_str(), l.second);
            str_env += string(tmp);
        }

        env[n++] = strdup(str_env.c_str());
    }

    env[n] = NULL;

    return env;
}

pid_t exec_new_binary(cycle_t *cycle, char *const *argv)
{
    char**     env;
    pid_t      pid;
    exec_ctx_t ctx;

    memset(&ctx, 0, sizeof(exec_ctx_t));

    ctx.path = argv[0];
    //ctx.name = "new binary process";
    ctx.argv = argv;

    env = set_environment(cycle);
    if (NULL == env)
    {
        return -1;
    }

    for (char** e = env; *e; e++) 
    {
        SLOG(DEBUG) << "ENV: " << *e;
    }

    if (rename(cycle->pid_file.c_str(), cycle->oldpid_file.c_str()) != SHS_OK) 
    {
        for (int i = 0; env[i] != NULL; ++i) 
        {
            free(env[i]);
        }

        free(env);

        return -1;
    }

    ctx.env = env;
    pid = execute(&ctx);
    if (-1 == pid) 
    {
        if (rename(cycle->oldpid_file.c_str(), 
            cycle->pid_file.c_str()) != SHS_OK) 
        {
            // log err
        }
    }

    for (int i = 0; env[i] != NULL; ++i) 
    {
        free(env[i]);
    }

    free(env);

    return pid;
}

int init_signals()
{
    shs_signal_t     *sig;
    struct sigaction sa;

    for (sig = g_signals; sig->signo != 0; sig++) 
    {
        bzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);

        if (sigaction(sig->signo, &sa, NULL) == -1) 
        {
            return SHS_ERROR;
        }
    }

    return SHS_OK;
}

void signal_handler(int signo)
{
    const char   *action;
    int          ignore;
    int          err;
    shs_signal_t *sig;

    ignore = 0;

    err = errno;

    for (sig = g_signals; sig->signo != 0; sig++) 
    {
        if (sig->signo == signo) 
        {
            break;
        }
    }

    action = "";

    switch (g_process) 
    {
    case SHS_PROCESS_MASTER:
    case SHS_PROCESS_SINGLE:
        switch (signo) 
        {
        case shs_signal_value(SHS_SHUTDOWN_SIGNAL):
            g_quit = 1;
            action = ", shutting down";
            break;

        case shs_signal_value(SHS_TERMINATE_SIGNAL):
        case SIGINT:
            g_terminate = 1;
            action = ", exiting";
            break;

        case shs_signal_value(SHS_NOACCEPT_SIGNAL):
            if (g_daemonized) 
            {
                g_noaccept = 1;
                action = ", stop accepting connections";
            }
            break;

        case shs_signal_value(SHS_RECONFIGURE_SIGNAL):
            g_reconfigure = 1;
            action = ", reconfiguring";
            break;

        case shs_signal_value(SHS_REOPEN_SIGNAL):
            g_reopen = 1;
            action = ", reopening logs";
            break;

        case shs_signal_value(SHS_CHANGEBIN_SIGNAL):
            if (getppid() > 1 || g_new_binary > 0) 
            {
                action = ", ignoring";
                ignore = 1;
                break;
            }

            g_change_binary = 1;
            action = ", changing binary";
            break;

        case SIGALRM:
            g_sigalrm = 1;
            break;

        case SIGIO:
            g_sigio = 1;
            break;

        case SIGCHLD:
            g_reap = 1;
            break;
        }

        break;

    case SHS_PROCESS_WORKER:
    case SHS_PROCESS_HELPER:
    case SHS_PROCESS_MONITOR:
        switch (signo) 
        {
        case shs_signal_value(SHS_NOACCEPT_SIGNAL):
            if (!g_daemonized) 
            {
                break;
            }
            g_debug_quit = 1;
        case shs_signal_value(SHS_SHUTDOWN_SIGNAL):
            g_quit = 1;
            action = ", shutting down";
            break;

        case shs_signal_value(SHS_TERMINATE_SIGNAL):
            if (FLAGS_use_graceful_quit)
            {
                break;
            }
        case SIGINT:
            g_terminate = 1;
            action = ", exiting";
            break;

        case shs_signal_value(SHS_REOPEN_SIGNAL):
            g_reopen = 1;
            action = ", reopening logs";
            break;

        case shs_signal_value(SHS_RECONFIGURE_SIGNAL):
        case shs_signal_value(SHS_CHANGEBIN_SIGNAL):
        case SIGIO:
            action = ", ignoring";
            break;
        }

        break;
    }

    (void) action;
    (void) ignore;

    if (signo == SIGCHLD) 
    {
        process_get_status();
    }

    errno = err;
}

static void process_get_status(void)
{
    int      status;
    pid_t    pid;
    int      err;
    int      i;
    uint32_t one;

    one = 0;

    for ( ;; ) 
    {
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) 
        {
            return;
        }

        if (pid == -1) 
        {
            err = errno;

            if (err == EINTR) 
            {
                continue;
            }

            if (err == ECHILD && one) 
            {
                return;
            }

#if (__FreeBSD__)
            if (err == ECHILD) 
            {
                return;
            }
#endif
            return;
        }

        one = 1;
        const char* process = "unknown process";

        for (i = 0; i < g_last_process; i++) 
        {
            if (g_processes[i].pid == pid) 
            {
                g_processes[i].status = status;
                g_processes[i].exited = 1;
                process = g_processes[i].name;

                break;
            }
        }

        (void) process;

        if (WTERMSIG(status)) 
        {
#ifdef WCOREDUMP
            if (WCOREDUMP(status))
            {
                g_num_coredump++;
            }
#else
            // log err
#endif
        } 
        else 
        {
            // log err
        }

        if (WEXITSTATUS(status) == 2 && g_processes[i].respawn) 
        {
            g_processes[i].respawn = 0;
        }
    }
}

int os_signal_process(char *name, int pid)
{
    shs_signal_t *sig;

    for (sig = g_signals; sig->signo != 0; sig++) 
    {
        if (strcmp(name, sig->name) == 0) 
        {
            if (kill(pid, sig->signo) != -1) 
            {
                return 0;
            }

            SLOG(ERROR) << "os_signal_process: kill(" << pid 
                << ", " << sig->signo << ") failed";
        }
    }

    return 1;
}

void update_queue_length(int n)
{
    if (g_processes[g_process_slot].pid != getpid())
    {
        return;
    }

    g_processes[g_process_slot].queue_length += n;
}

void get_queue_status(int* min, int* max, int* curr, int* zero)
{
    if (curr)
    {
        *curr = g_processes[g_process_slot].queue_length;
    }

    if (max)
    {
        *max = 0;
    }

    if (min)
    {
        *min = 0;
    }

    if (zero)
    {
        *zero = 0;
    }

    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        pid_t pid = g_stats->process_stats[i].pid_;
        int type = g_stats->process_stats[i].type_;
        if (pid <= 0 || type != 1)
        {
            continue;
        }

        int n = g_stats->process_stats[i].curr_queue_size;
        if (n == 0 && zero)
        {
            *zero += 1;
        }

        if (min)
        {
            *min = std::min(*min, n);
        }

        if (max)
        {
            *max = std::max(*max, n);
        }
    }
}

void set_proc_tag(const char* tag, char** argv)
{
    std::string name(tag);
    name.append(" ");
    name.append(google::GetArgv());

    argv[1] = NULL;
    memcpy(argv[0], name.c_str(), name.length());
    argv[0][name.length()] = '\0';
}

std::string get_exe_path()
{
    char buf[1024];
    auto path = "/proc/" + std::to_string(getpid()) + "/exe";
    auto n = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (-1 == n)
    {
        return "";
    }
    buf[n] = '\0';

    return buf;
}

