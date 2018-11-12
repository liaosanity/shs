#pragma once

#include <stdint.h>
#include <pthread.h>
#include <string>

#include "comm/timestamp.h"

#include "process.h"

namespace shs 
{

struct ProcessStats
{
    uint32_t curr_queue_size;
    uint32_t max_queue_size;
    double   queueing_time; 
    double   invoke_elapsed_time;

    uint32_t curr_server_conns;
    uint32_t curr_server_reqs;

    uint64_t num_requests;
    uint64_t num_error_requests;
    uint64_t num_timeout_requests;

    uint64_t persistent_num_requests;
    uint64_t persistent_num_error_requests;
    uint64_t persistent_num_timeout_requests;
    double   persistent_invoke_elapsed_time;

    uint64_t persistent_last_calc_avg_cnt;
    double   persistent_avg_invoke_elapsed_time;
    double   persistent_avg_elapsed_time_in_queue;

    pid_t    pid_;
    int      type_; // 1: worker, 2 : monitor

    static void Reset();
    static void AddRequest();
    static void AddErrorRequest();
    static void AddTimeoutRequest();
    static void AddInvokeElapsedTime(double elapsed_time);
    static void AddElapsedTimeInQueue(double elapsed_time);
    static void SetQueueSize(size_t len);
    static void SetServerConns(size_t len);
    static void IncServerReqs();
    static void DecServerReqs();
    static ProcessStats* current();

    static void SetPid(pid_t pid);
    static void SetType(int type);
};

struct Stats
{
    time_t   started;
    int32_t  num_processes;
    int32_t  num_coredump;
    time_t   last_coredump_time;
    int32_t  num_reload;
    time_t   last_reload_time;  
    int32_t  last_num_requests;
    double   start_stats_time;
    Timestamp last_stats_time;
    double   avg_qps;
    double   last_invoke_elapsed_time;
    double   qps;
    double   time_per_request;

    bool     last_status;
    time_t   last_status_time;
    int32_t  num_status;
    int32_t  num_status_errors;
    time_t   last_status_error_time;

    int64_t  total_persistent_num_requests;
    int64_t  total_persistent_num_error_requests;
    int64_t  total_persistent_num_timeout_requests;
    double   total_persistent_invoke_elapsed_time;
    ProcessStats process_stats[MAX_PROCESSES];

    pthread_mutex_t mutex;

    static bool Init();
    static double GetAvgTimePerRequest();
    static double GetAvgTimeInQueue();
    static uint32_t GetQueueSize();
    static uint32_t GetServerConns();
    static uint32_t GetServerReqs();
};

extern Stats* g_stats;

}
