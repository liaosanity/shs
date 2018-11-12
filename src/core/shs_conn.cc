#include <netinet/in.h>

#include "shs_conn.h"
#include "shs_epoll.h"
#include "shs_memory.h"
#include "shs_event_timer.h"
#include "shs_epoll.h"

int conn_connect_peer(conn_peer_t *pc, event_base_t *ep_base)
{
    int      rc = 0;
    conn_t  *c = NULL;
    event_t *wev = NULL;

    if (!pc) 
    {
        return SHS_ERROR;
    }

    c = pc->connection;
    if (!c) 
    {
        return SHS_BUSY;
    }
    
    if (c->fd != SHS_INVALID_FILE) 
    {
        goto connecting;
    }
    
    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (SHS_ERROR == c->fd) 
    {
        return SHS_ERROR;
    }
    
    conn_nonblocking(c->fd);
   
    c->recv = shs_recv;
    c->send = shs_send;
    //c->recv_chain = shs_recv_chain;
    //c->send_chain = shs_send_chain;
    //c->sendfile_chain = shs_sendfile_chain;
    //c->sendfile = SHS_TRUE;
	
    if (pc->sockaddr->sa_family != AF_INET) 
    {
        c->tcp_nopush = CONN_TCP_NOPUSH_DISABLED;
        c->tcp_nodelay = CONN_TCP_NODELAY_DISABLED;
    }

    c->tcp_nodelay = CONN_TCP_NODELAY_UNSET;
    c->tcp_nopush = CONN_TCP_NOPUSH_UNSET;

    wev = c->write;
    
connecting:
    if (SHS_ERROR == event_add_conn(ep_base, c)) 
    {
        return SHS_ERROR;
    }
    
    errno = 0;
    rc = connect(c->fd, pc->sockaddr, pc->socklen);
    if (SHS_ERROR == rc) 
    {
        if (errno == SHS_EINPROGRESS) 
        {
            return SHS_AGAIN;
        }

        return SHS_ERROR;
    }

    wev->ready = 1;

    return SHS_OK;
}

int conn_tcp_nopush(int s)
{
    int cork = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
        (const void *) &cork, sizeof(int));
}

int conn_tcp_push(int s)
{
    int cork = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
        (const void *) &cork, sizeof(int));
}

int conn_tcp_nodelay(int s)
{
    int nodelay = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
        (const void *) &nodelay, sizeof(int));
}

int conn_tcp_delay(int s)
{
    int nodelay = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
        (const void *) &nodelay, sizeof(int));
}

conn_t * conn_get_from_mem(int s)
{
    conn_t  *c   = NULL;
    event_t *rev = NULL;
    event_t *wev = NULL;

    c = (conn_t *)memory_calloc(sizeof(conn_t));
    rev = (event_t *)memory_calloc(sizeof(event_t));
    wev = (event_t *)memory_calloc(sizeof(event_t));

    if (!c || !rev || !wev) 
    {
        return NULL;
    }
	
    c->read = rev;
    c->write = wev;
	
    conn_set_default(c, s);

    return c;
}

void conn_set_default(conn_t *c, int s)
{
    event_t  *rev = NULL;
    event_t  *wev = NULL;
    uint32_t  instance;
    uint32_t  last_instance;

    c->fd = s;

    rev = c->read;
    wev = c->write;
    instance = rev->instance;
    last_instance = rev->last_instance;
    //c->sent = 0;

    c->conn_data = NULL;
    //c->error = 0;
    //c->listening = NULL;
    c->next = NULL;
    //c->sendfile = SHS_FALSE;
    //c->sndlowat = 0;
    //c->sockaddr = NULL;
    //memory_zero(&c->addr_text, sizeof(string_t));
    //c->socklen = 0;
    // set rev & wev->instance to !last->instance
    memory_zero(rev, sizeof(event_t));
    memory_zero(wev, sizeof(event_t));
    rev->instance = !instance;
    wev->instance = !instance;
    rev->last_instance = last_instance;
    
    rev->data = c;
    wev->data = c;
    wev->write = SHS_TRUE;
}

void conn_close(conn_t *c)
{
    if (!c) 
    {
        return;
    }
	
    if (c->fd > 0) 
    {
        close(c->fd);
        c->fd = -1;
    
        if (c->read->timer_set && c->ev_timer) 
        {
            event_timer_del(c->ev_timer, c->read);
        }

        if (c->write->timer_set && c->ev_timer) 
        {
            event_timer_del(c->ev_timer, c->write);
        }

        if (c->ev_base) 
        {
            event_del_conn(c->ev_base, c, EVENT_CLOSE_EVENT);
        }
    } 
}

void conn_release(conn_t *c)
{
    conn_close(c);

    if (c->pool) 
    {
        pool_destroy(c->pool);
    }

    c->pool = NULL;
}

void conn_free_mem(conn_t *c)
{
    event_t *rev = NULL;
    event_t *wev = NULL;

    rev = c->read;
    wev = c->write;
    memory_free(rev, sizeof(event_t));
    memory_free(wev, sizeof(event_t));
    memory_free(c, sizeof(conn_t));
}

