#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <string>

#include "cycle.h"

typedef void (*spawn_proc_pt)(void *data);

typedef struct process_s
{
    pid_t         pid;
    int           status;
    int           channel[2];
    spawn_proc_pt proc;
    void*         data;
    const char*   name;
    unsigned      respawn:1;
    unsigned      just_spawn:1;
    unsigned      detached:1;
    unsigned      exiting:1;
    unsigned      exited:1;
    int           type; // 1: worker , 2 : monitor
    int           queue_length;
} process_t;

typedef struct exec_ctx_s
{
    char        *path;
    char        *name;
    char *const *argv;
    char        **env;
} exec_ctx_t;

#define shs_value_helper(n)  #n
#define shs_value(n)         shs_value_helper(n)

#define MAX_PROCESSES        1024

#define PROCESS_NORESPAWN    -1
#define PROCESS_JUST_SPAWN   -2
#define PROCESS_RESPAWN      -3
#define PROCESS_JUST_RESPAWN -4
#define PROCESS_DETACHED     -5

extern int         g_argc;
extern char      **g_argv;
extern char      **g_os_argv;
extern char      **g_os_environ;
extern pid_t       g_pid;
extern int         g_channel;
extern int         g_process_slot;
extern int         g_last_process;
extern int         g_num_coredump;
extern process_t   g_processes[MAX_PROCESSES];

int add_inherited_sockets(std::map<std::string, int>& listening);
pid_t spawn_process(spawn_proc_pt proc, void *data, 
    const char *name, int respawn, int type);
pid_t execute(exec_ctx_t *ctx);
pid_t exec_new_binary(cycle_t *cycle, char *const *argv);
int init_signals();

void update_queue_length(int n);
void get_queue_status(int* min, int* max, int* curr, int* zero);
void set_proc_tag(const char* prefix, char** argv);
std::string get_exe_path();

#endif
