#pragma once

#include <stdint.h>
#include <string>
#include <gflags/gflags.h>

namespace shs 
{

DECLARE_bool(accesslog);
DECLARE_string(dlopen_flag);
DECLARE_bool(t);

class Config 
{
public:
    Config();
    ~Config();

    bool Init(int argc, char **argv);

    int32_t monitor_port() const { return monitor_port_; }
    int32_t http_port() const { return http_port_; }
    bool daemonize() const { return daemonize_; }
    bool kill_running() const { return kill_running_; }
    bool t() const { return t_; }

    const std::string& log_conf() const { return log_conf_; }
    const std::string& pid_file() const { return pid_file_; }
    const std::string& mod_path() const { return mod_path_; }
    const std::string& mod_conf() const { return mod_conf_; }
    const std::string& status_file() const { return status_file_; }
    const std::string& working_dir() const { return working_dir_; }
    const std::string& default_module() const { return default_module_; }
    const std::string& default_method() const { return default_method_; }

    int32_t num_processes() const { return num_processes_; }
    int32_t timeout() const { return timeout_; }
    int32_t queue_lwm() const { return queue_lwm_; }
    int32_t queue_hwm() const { return queue_hwm_; }
    int32_t connection_n() const { return connection_n_; }

    int argc() const { return argc_; }
    char** argv() const { return argv_; }
    char** os_argv() const { return os_argv_; }

private:
    int32_t timeout_;
    int32_t http_port_;
    int32_t monitor_port_;
    bool daemonize_;
    bool kill_running_;
    std::string log_conf_;
    std::string pid_file_;
    std::string mod_path_;
    std::string mod_conf_;
    std::string default_module_;
    std::string default_method_;
    int32_t num_processes_;
    std::string status_file_;
    int32_t queue_lwm_;
    int32_t queue_hwm_;
    std::string working_dir_;
    int argc_;
    char** argv_;
    char** os_argv_;
    bool t_;
    int32_t connection_n_;
};

} // namespace shs
