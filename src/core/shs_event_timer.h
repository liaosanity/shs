#ifndef SHS_EVENT_TIMER_H
#define SHS_EVENT_TIMER_H

#include "shs_types.h"
#include "shs_rbtree.h"
#include "shs_event.h"

typedef rb_msec_t (*curtime_ptr)(void);

struct event_timer_s 
{
    rbtree_t          timer_rbtree;
    rbtree_node_t     timer_sentinel;
    rbtree_insert_pt  timer_insert_ptr;
    curtime_ptr       time_handler;
};

int event_timer_init(event_timer_t *timer, curtime_ptr handler);
void event_timers_expire(event_timer_t *timer);
rb_msec_t event_find_timer(event_timer_t *timer);
void event_timer_del(event_timer_t *ev_timer, event_t *ev);
void event_timer_add(event_timer_t *ev_timer, event_t *ev, rb_msec_t timer);

#endif

