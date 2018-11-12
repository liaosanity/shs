#ifndef PROCESS_CYCLE_H
#define PROCESS_CYCLE_H

#include <stdint.h>
#include <signal.h>
#include <string>
#include <vector>
#include <map>

#include "cycle.h"
#include "process.h"

extern uint32_t     g_process;
extern pid_t        g_new_binary;
extern uint32_t     g_inherited;
extern uint32_t     g_daemonized;
extern uint32_t     g_exiting;
extern sig_atomic_t g_reap;
extern sig_atomic_t g_sigio;
extern sig_atomic_t g_sigalrm;
extern sig_atomic_t g_quit;
extern sig_atomic_t g_debug_quit;
extern sig_atomic_t g_terminate;
extern sig_atomic_t g_noaccept;
extern sig_atomic_t g_reconfigure;
extern sig_atomic_t g_reopen;
extern sig_atomic_t g_change_binary;
extern bool         g_running;

bool init_cycle(int argc, char** argv);
void master_process_cycle(int32_t connection_n, int num_prcesses, 
    const std::string& pid_file, worker_main_fn worker_main, 
    worker_exit_fn worker_exit, monitor_main_fn, monitor_exit_fn, void* data);
void set_callback(const std::string& name, const MasterEvent& event);
void set_inherited_sockets(const std::string&, int sockfd);
void set_inherited_sockets(std::map<std::string, int> sockets);

#endif
