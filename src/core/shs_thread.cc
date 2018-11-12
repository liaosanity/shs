#include "shs_thread.h"
#include "shs_memory.h"
#include "shs_memory_pool.h"
#include "shs_time.h"

extern _xvolatile rb_msec_t shs_current_msec;
static pthread_key_t shs_thread_key;

void thread_env_init()
{
    pthread_key_create(&shs_thread_key, NULL);
}

shs_thread_t *thread_new(pool_t *pool)
{
    if (pool) 
    {
        return (shs_thread_t *)pool_calloc(pool, sizeof(shs_thread_t));
    }
	
    return (shs_thread_t *)memory_calloc(sizeof(shs_thread_t));
}

void thread_bind_key(shs_thread_t *thread)
{
    pthread_setspecific(shs_thread_key, thread);
}

shs_thread_t *get_local_thread()
{
    return (shs_thread_t *)pthread_getspecific(shs_thread_key);
}

event_base_t  *thread_get_event_base()
{
    shs_thread_t *thread = (shs_thread_t *)pthread_getspecific(shs_thread_key);

    return thread != NULL ? &thread->event_base : NULL;
}

event_timer_t *thread_get_event_timer()
{
    shs_thread_t *thread = (shs_thread_t *)pthread_getspecific(shs_thread_key);

    return thread != NULL ? &thread->event_timer : NULL;
}

conn_pool_t * thread_get_conn_pool()
{
    shs_thread_t *thread = (shs_thread_t *)pthread_getspecific(shs_thread_key);

    return &thread->conn_pool;
}

int thread_create(void *args)
{
    //pthread_attr_t  attr;
    //int             ret;
    //shs_thread_t   *thread = (shs_thread_t *)args;

    //pthread_attr_init(&attr);

    //if ((ret = pthread_create(&thread->thread_id, &attr, 
    //    thread->run_func, thread)) != 0) 
    //{
    //    return SHS_ERROR;
    //}

    return SHS_OK;
}

void thread_clean(shs_thread_t *thread)
{
}

int thread_event_init(shs_thread_t* thread)
{
    if (-1 == epoll_init(&thread->event_base)) 
    {
        return SHS_ERROR;
    }

    event_timer_init(&thread->event_timer, time_curtime);

    thread->event_base.time_update = time_update;

    if (conn_pool_init(&thread->conn_pool, thread->event_base.nevents) != SHS_OK)
    {
        return SHS_ERROR; 
    }

    return SHS_OK;
}

void thread_event_process(shs_thread_t* thread)
{
    uint32_t      flags = EVENT_UPDATE_TIME;
    rb_msec_t     timer = 0;
    rb_msec_t     delta = 0;
    event_base_t *ev_base;
    
    ev_base = &thread->event_base;

    if (THREAD_MASTER == thread->type)
    {
        flags |= EVENT_POST_EVENTS;
    }
    
    timer = event_find_timer(&thread->event_timer);

    if ((timer > 10) || (timer == EVENT_TIMER_INFINITE)) 
    {
        timer = 10;
    }
    
    delta = shs_current_msec;

    (void)event_process_events(ev_base, timer, flags);

    if ((THREAD_MASTER == thread->type) 
        && !queue_empty(&ev_base->posted_accept_events)) 
    {
        event_process_posted(&ev_base->posted_accept_events);
    }

    if ((THREAD_MASTER == thread->type) 
        && !queue_empty(&ev_base->posted_events)) 
    {
        event_process_posted(&ev_base->posted_events);
    }

    delta = shs_current_msec - delta;
    if (delta) 
    {
        event_timers_expire(&thread->event_timer);
    }
}

