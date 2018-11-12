#include "stats.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <boost/format.hpp>

#include "types.h"

namespace shs 
{

using namespace std;

Stats* g_stats;
void*  g_shared_mem;

bool Stats::Init()
{
    g_shared_mem = mmap(0, sizeof(Stats), PROT_READ | PROT_WRITE, 
        MAP_ANON | MAP_SHARED, -1, 0);
    if (g_shared_mem == MAP_FAILED)
    {
        fprintf(stderr, "stats_init: mmap failed! err=%s\n", strerror(errno));

        return false;
    }

    g_stats = (Stats*)g_shared_mem;
    memset(g_stats, 0, sizeof(Stats));

    pthread_mutexattr_t mutex_shared_attr;
    int ret = pthread_mutexattr_init(&mutex_shared_attr);
    if (ret != 0)
    {
        fprintf(stderr, "pthread_mutexattr_init failed!\n");

        return false;
    }

    ret = pthread_mutexattr_setpshared(
        &mutex_shared_attr, PTHREAD_PROCESS_SHARED);
    if (ret != 0)
    {
        fprintf(stderr, "pthread_mutexattr_setpshared failed!\n");

        return false;
    }

    ret = pthread_mutex_init(&(g_stats->mutex), &mutex_shared_attr);
    if (ret != 0)
    {
        fprintf(stderr, "pthread_mutex_init failed!\n");

        return false;
    }

    g_stats->started = Timestamp::Now().SecondsSinceEpoch();

    return true;
}

ProcessStats* ProcessStats::current()
{
    return &(g_stats->process_stats[g_process_slot]);
}

void ProcessStats::Reset()
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->num_requests = 0;
    pstats->num_error_requests = 0;
    pstats->num_timeout_requests = 0;
    pstats->curr_queue_size = 0;
    pstats->max_queue_size = 0;
    pstats->queueing_time = 0.0; 
    pstats->invoke_elapsed_time = 0.0;

    pstats->curr_server_conns = 0;
    pstats->curr_server_reqs = 0;
}

void ProcessStats::AddRequest()
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->num_requests++;
    pstats->persistent_num_requests++;
}

void ProcessStats::AddErrorRequest()
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->num_error_requests++;
    pstats->persistent_num_error_requests++;
}

void ProcessStats::AddTimeoutRequest()
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->num_timeout_requests++;
    pstats->persistent_num_timeout_requests++;
}

void ProcessStats::AddInvokeElapsedTime(double elapsed_time)
{
    ProcessStats* pstats = ProcessStats::current();
    pthread_mutex_lock(&g_stats->mutex);
    pstats->invoke_elapsed_time += elapsed_time;
    pstats->persistent_invoke_elapsed_time += elapsed_time;
    pstats->persistent_last_calc_avg_cnt += 1;

    pstats->persistent_avg_invoke_elapsed_time += 
        (elapsed_time - pstats->persistent_avg_invoke_elapsed_time) / 
        (pstats->persistent_last_calc_avg_cnt);
    pthread_mutex_unlock(&g_stats->mutex);
}

void ProcessStats::AddElapsedTimeInQueue(double elapsed_time)
{
    ProcessStats* pstats = ProcessStats::current();
    pthread_mutex_lock(&g_stats->mutex);
    pstats->persistent_avg_elapsed_time_in_queue += 
        (elapsed_time - pstats->persistent_avg_elapsed_time_in_queue) / 
        pstats->persistent_last_calc_avg_cnt;
    pthread_mutex_unlock(&g_stats->mutex);
}

void ProcessStats::SetQueueSize(size_t len)
{
    ProcessStats* pstats = ProcessStats::current();
    if (len > pstats->max_queue_size)
    {
        pstats->max_queue_size = len;
    }
    pstats->curr_queue_size = len;
}

void ProcessStats::SetServerConns(size_t len)
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->curr_server_conns = len;
}

void ProcessStats::IncServerReqs()
{
    ProcessStats::current()->curr_server_reqs++;
}

void ProcessStats::DecServerReqs()
{
    ProcessStats::current()->curr_server_reqs--;
}

void ProcessStats::SetPid(pid_t pid)
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->pid_ = pid;
}

void ProcessStats::SetType(int type)
{
    ProcessStats* pstats = ProcessStats::current();
    pstats->type_ = type;
}

double Stats::GetAvgTimePerRequest()
{
    int cnt = 0;
    double total = 0.000000f;

    pthread_mutex_lock(&g_stats->mutex);
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }

        if (g_stats->process_stats[i].persistent_last_calc_avg_cnt > 0)
        {
            cnt += g_stats->process_stats[i].persistent_last_calc_avg_cnt;
            total += (
                g_stats->process_stats[i].persistent_avg_invoke_elapsed_time * 
                g_stats->process_stats[i].persistent_last_calc_avg_cnt);
        }
    }
    pthread_mutex_unlock(&g_stats->mutex);

    if (cnt > 0)
    {
        return total / cnt;
    }

    return 0.000000f;
}

double Stats::GetAvgTimeInQueue()
{
    int cnt = 0;
    double total = 0.000000f;

    pthread_mutex_lock(&g_stats->mutex);
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }

        if (g_stats->process_stats[i].persistent_last_calc_avg_cnt > 0)
        {
            cnt += g_stats->process_stats[i].persistent_last_calc_avg_cnt;
            total += (
                g_stats->process_stats[i].persistent_avg_elapsed_time_in_queue * 
                g_stats->process_stats[i].persistent_last_calc_avg_cnt);
        }
    }
    pthread_mutex_unlock(&g_stats->mutex);

    if (cnt > 0)
    {
        return total / cnt;
    }

    return 0.000000f;
}

uint32_t Stats::GetQueueSize()
{
    uint32_t ret = 0;

    pthread_mutex_lock(&g_stats->mutex);
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }
        ret += g_stats->process_stats[i].curr_queue_size;
    }
    pthread_mutex_unlock(&g_stats->mutex);

    return ret;
}

uint32_t Stats::GetServerConns()
{
    uint32_t ret = 0;

    pthread_mutex_lock(&g_stats->mutex);
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }
        ret += g_stats->process_stats[i].curr_server_conns;
    }
    pthread_mutex_unlock(&g_stats->mutex);

    return ret;
}

uint32_t Stats::GetServerReqs()
{
    uint32_t ret = 0;

    pthread_mutex_lock(&g_stats->mutex);
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }
        ret += g_stats->process_stats[i].curr_server_reqs;
    }
    pthread_mutex_unlock(&g_stats->mutex);

    return ret;
}

} // namespace shs
