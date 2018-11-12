#ifndef CYCLE_H
#define CYCLE_H

#include <map>
#include <string>
#include <tr1/functional>
#include <stdint.h>

typedef bool (*worker_main_fn)(void* data);
typedef void (*worker_exit_fn)(void* data);
typedef bool (*monitor_main_fn)(void* data);
typedef void (*monitor_exit_fn)(void* data);
typedef void (*master_on_child_fn)(void* data);

typedef std::tr1::function<bool()> MasterEvent;

typedef struct cycle_s 
{
    int32_t connection_n;
    int num_processes;
    bool (*worker_main_fn)(void* data);
    void (*worker_exit_fn)(void* data);
    void* data;

    bool (*monitor_main_fn)(void* data);
    void (*monitor_exit_fn)(void* data);

    std::map<std::string, MasterEvent> hooks;
    std::string pid_file;
    std::string oldpid_file;
    std::map<std::string, int> listening;
} cycle_t;

#endif
