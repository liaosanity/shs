#ifndef SHS_THREAD_H
#define SHS_THREAD_H

#include "shs_types.h"
#include "shs_sys.h"
#include "shs_conn_pool.h"
#include "shs_queue.h"
#include "shs_epoll.h"
#include "shs_event_timer.h"

typedef void *(*TREAD_FUNC)(void *);

typedef struct shs_thread_s 
{
    //pthread_t      thread_id;
    int            type;
    event_base_t   event_base;
    event_timer_t  event_timer;
    conn_pool_t    conn_pool;
    //TREAD_FUNC     run_func;
    //uint32_t       state;
    //int            running;
} shs_thread_t;

enum 
{
    THREAD_MASTER,
    THREAD_WORKER
};

enum 
{
    THREAD_ST_OK,
    THREAD_ST_EXIT,
    THREAD_ST_UNSTART,
};

typedef struct shs_timer_s
{
    event_timer_t event_timer;
    event_t       ev;
} shs_timer_t;

void           thread_env_init();
shs_thread_t  *thread_new(pool_t *pool);
void           thread_bind_key(shs_thread_t *thread);
shs_thread_t  *get_local_thread();
event_base_t  *thread_get_event_base();
event_timer_t *thread_get_event_timer();
conn_pool_t   *thread_get_conn_pool();
void           thread_clean(shs_thread_t *thread);
int  thread_create(void *arg);
int  thread_event_init(shs_thread_t* thread);
void thread_event_process(shs_thread_t* thread);

#endif

