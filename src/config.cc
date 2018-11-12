#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <boost/format.hpp>
#include <gflags/gflags.h>

#include "process.h"

namespace shs 
{

using namespace std;

const int32_t   kDefaultNumProcesses = 4;
const int32_t   kDefaultTimeout = 1000;
const int32_t   kDefaultQueueLWM = 0;
const int32_t   kDefaultQueueHWM = 0;

DECLARE_int32(http_port);
DECLARE_int32(monitor_port);
DEFINE_int32(queue_lwm, kDefaultQueueLWM, "Queue low water mark");
DEFINE_int32(queue_hwm, kDefaultQueueHWM, "Queue high water mark");
DEFINE_int32(timeout, kDefaultTimeout, "Timeout");
DEFINE_int32(num_processes, kDefaultNumProcesses, "Number of processes");
DEFINE_bool(daemonize, false, "Run server as daemon");
DEFINE_bool(kill_running, false, "Kill the currently running process if exists");
DEFINE_string(log_conf, "", "Logging config file path");
DEFINE_string(pid_file, "", "Pid file path");
DEFINE_string(mod_path, "", "Module library path");
DEFINE_string(mod_conf, "", "Module config path");
DEFINE_string(default_module, "", "default module name");
DEFINE_string(default_method, "", "default method name");
DEFINE_string(status_file, "", "Check service status");
DEFINE_bool(rewrite_path_to_default, false, "rewrite http path to default module name");
DEFINE_bool(t, false, "Test Configure");
DEFINE_bool(accesslog, true, "Print accesslog or not");
DEFINE_string(dlopen_flag, "RTLD_LAZY", "default RTLD_LAZY");
DEFINE_int32(connection_n, 65535, "number of connections per thread, default 65535");

Config::Config()
{
}

Config::~Config() 
{
    if (NULL != argv_) 
    {
        for (int i = 0; NULL != argv_[i]; ++i)
        {
            free(argv_[i]);
        }

        free(argv_);
    }
}

bool Config::Init(int argc, char **argv) 
{
    string usage = string("\nUsage: ") + argv[0] 
        + " [options] ...\n\n   Version: MP+HTTP\nBuild time: " 
        + __DATE__ + " " + __TIME__;

    argc_ = argc;
    os_argv_ = argv;
    argv_ = (char **)calloc(argc + 1, sizeof(char *));
    for (int i = 0; NULL != argv[i]; ++i)
    {
        argv_[i] = strdup(argv[i]);
    }

    google::SetUsageMessage(usage);
    google::AllowCommandLineReparsing();
    google::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_http_port <= 0) 
    {
        fprintf(stderr, "Config: -http_port is not specified.\n");
        //return false;
    }

    if (FLAGS_daemonize 
        && (FLAGS_pid_file.empty() || FLAGS_pid_file[0] != '/')) 
    {
        fprintf(stderr, "Config: -pid_file=%s is invalid.\n", 
            FLAGS_pid_file.c_str());

        return false;
    }

    if (FLAGS_default_module.empty() && !FLAGS_default_method.empty()) 
    {
        fprintf(stderr, "Config: When specifying the -default_method, \
            -default_module must be specified.\n");

        return false;
    }

    if (FLAGS_num_processes <= 0) 
    {
        num_processes_ = kDefaultNumProcesses;
    }

    if (num_processes_ > MAX_PROCESSES) 
    {
        num_processes_ = MAX_PROCESSES;
    }

    timeout_ = FLAGS_timeout;
    num_processes_ = FLAGS_num_processes;
    http_port_ = FLAGS_http_port;
    monitor_port_ = FLAGS_monitor_port;
    daemonize_ = FLAGS_daemonize;
    kill_running_ = FLAGS_kill_running;
    log_conf_ = FLAGS_log_conf;
    pid_file_ = FLAGS_pid_file;
    mod_path_ = FLAGS_mod_path;
    mod_conf_ = FLAGS_mod_conf;
    default_module_ = FLAGS_default_module;
    default_method_ = FLAGS_default_method;
    status_file_ = FLAGS_status_file;
    queue_lwm_ = FLAGS_queue_lwm;
    queue_hwm_ = FLAGS_queue_hwm;
    t_ = FLAGS_t;
    connection_n_ = FLAGS_connection_n;

    if (queue_lwm_ < 0)
    {
        queue_lwm_ = 0;
    }

    if (queue_hwm_ < 0)
    {
        queue_hwm_ = 0;
    }

    char buf[1024] = {0};
    string path = boost::str(boost::format("/proc/%1%/exe") % getpid());
    if (readlink(path.c_str(), buf, sizeof(buf) - 1) == -1)
    {
        fprintf(stderr, "Config::Init readlink failed! '%s' %s\n", 
            path.c_str(), strerror(errno));

        return false;
    }

    char* p = strrchr(buf, '/');
    if (NULL == p)
    {
        return false;
    }

    *p = 0;
    working_dir_ = buf;

    string proc_path = working_dir_ + "/../proc";
    if (access(proc_path.c_str(), F_OK) == -1)
    {
        fprintf(stderr, "Config::Init access '%s' failed! %s\n", 
            proc_path.c_str(), strerror(errno));

        return false;
    }

    return true;
}

} // namespace shs
