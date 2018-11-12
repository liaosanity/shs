#include "shs_event.h"
#include "shs_conn.h"
#include "shs_epoll.h"

void event_process_posted(volatile queue_t *posted)
{
    conn_t  *c;
    event_t *ev = NULL;
    queue_t *eq = NULL;
	
    while (!queue_empty(posted)) 
    {
        eq = queue_head(posted);

        queue_remove(eq);

        ev = queue_data(eq, event_t, post_queue);
        if (!ev) 
        {
            return;
        }

        c = (conn_t *)ev->data;

        if (c->fd != SHS_INVALID_FILE) 
        {
            ev->handler(ev);
        } 
    }
}

int event_handle_read(event_base_t *base, event_t *rev, uint32_t flags)
{
    if (!rev->active && !rev->ready) 
    {
        if (SHS_ERROR == event_add(base, rev, EVENT_READ_EVENT, EVENT_CLEAR_EVENT))
        {
            return SHS_ERROR;
        }
    }

    return SHS_OK;
}

int event_del_read(event_base_t *base, event_t *rev)
{
    rev->ready = SHS_FALSE;
	
    return event_delete(base, rev, EVENT_READ_EVENT, EVENT_CLEAR_EVENT);
}

int event_handle_write(event_base_t *base, event_t *wev, size_t lowat)
{
    if (!wev->active && !wev->ready) 
    {
        if (SHS_ERROR == event_add(base, wev, EVENT_WRITE_EVENT, EVENT_CLEAR_EVENT)) 
        {
            return SHS_ERROR;
        }
    } 

    return SHS_OK;
}

int event_del_write(event_base_t *base, event_t *wev)
{
    wev->ready = SHS_FALSE;
	
    return event_delete(base, wev, EVENT_WRITE_EVENT, EVENT_CLEAR_EVENT);
}

