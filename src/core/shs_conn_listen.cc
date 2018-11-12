#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "shs_conn_listen.h"
#include "shs_time.h"
#include "shs_memory.h"
#include "shs_event.h"
#include "shs_epoll.h"

#define SHS_INET_ADDRSTRLEN (sizeof("255.255.255.255") - 1)

int conn_listening_open(array_t *listening)
{
    int          s = SHS_INVALID_FILE;
    int          reuseaddr = 1;
    uint32_t     i = 0;
    uint32_t     tries = 0;
    uint32_t     failed = 0;
    listening_t *ls = NULL;

    if (!listening) 
    {
        return SHS_ERROR;
    }

    for (tries = 5; tries; tries--) 
    {
        failed = 0;
        ls = (listening_t *)listening->elts;
		
        for (i = 0; i < listening->nelts; i++) 
        {
            if (ls[i].ignore) 
            {
                continue;
            }

            if (ls[i].fd != SHS_INVALID_FILE) 
            {
                continue;
            }

            if (ls[i].inherited) 
            {
                continue;
            }

            s = socket(ls[i].family, ls[i].type, 0);
            if (s == SHS_INVALID_FILE) 
            {
                return SHS_ERROR;
            }

            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                (const void *) &reuseaddr, sizeof(int)) == SHS_ERROR) 
            {
                goto error;
            }

            if (ls[i].rcvbuf != -1) 
            {
                if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                    (const void *) &ls[i].rcvbuf, sizeof(int)) == SHS_ERROR) 
                {
                    // log err
                }

            }

            if (ls[i].sndbuf != -1) 
            {
                if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
                    (const void *) &ls[i].sndbuf, sizeof(int)) == SHS_ERROR) 
                {
                    // log err
                }

            }

            // we can't set linger onoff = 1 on listening socket
            if (conn_nonblocking(s) == SHS_ERROR) 
            {
                goto error;
            }

            if (bind(s, ls[i].sockaddr, ls[i].socklen) == SHS_ERROR) 
            {
                close(s);
				
                if (errno != SHS_EADDRINUSE) 
                {
                    return SHS_ERROR;
                }

                failed = 1;
				
                continue;
            }

            if (listen(s, ls[i].backlog) == SHS_ERROR) 
            {
                goto error;
            }

            ls[i].listen = 1;
            ls[i].open = 1;
            ls[i].fd = s;
        }

        if (!failed) 
        {
            break;
        }

        time_msleep(500);
    }

    if (failed) 
    {
        return SHS_ERROR;
    }

    return SHS_OK;
	
error:
    close(s);
	
    return SHS_ERROR;
}

listening_t * conn_listening_add(array_t *listening, pool_t *pool, 
    in_addr_t addr, in_port_t port, event_handler_pt handler, 
    int rbuff_len, int sbuff_len)
{
    uchar_t            *address = NULL;
    listening_t        *ls = NULL;
    struct sockaddr_in *sin = NULL;

    if (!listening || !pool || (port <= 0)) 
    {
        return NULL;
    }
    
    sin = (sockaddr_in *)pool_alloc(pool, sizeof(struct sockaddr_in));
    if (!sin) 
    {
        return NULL;
    }
	
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = addr;
    sin->sin_port = htons(port);
    address = (uchar_t *)inet_ntoa(sin->sin_addr);
   
    ls = (listening_t *)array_push(listening);
    if (!ls) 
    {
        return NULL;
    }

    memory_zero(ls, sizeof(listening_t));
    ls->addr_text.data = (uchar_t *)pool_calloc(pool,
        INET_ADDRSTRLEN - 1 + sizeof(":65535") - 1);

    if (!ls->addr_text.data) 
    {
        return NULL;
    }
	
    ls->addr_text.len = string_xxsprintf(ls->addr_text.data,
        "%s:%d", address, port) - ls->addr_text.data;
    ls->fd = SHS_INVALID_FILE;
    ls->family = AF_INET;
    ls->type = SOCK_STREAM;
    ls->sockaddr = (struct sockaddr *) sin;
    ls->socklen = sizeof(struct sockaddr_in);
    ls->backlog = CONN_DEFAULT_BACKLOG;
   	ls->rcvbuf = rbuff_len > CONN_DEFAULT_RCVBUF? rbuff_len: CONN_DEFAULT_RCVBUF;
    ls->sndbuf = sbuff_len > CONN_DEFAULT_SNDBUF? sbuff_len: CONN_DEFAULT_SNDBUF;
    ls->conn_psize = CONN_DEFAULT_POOL_SIZE;
    ls->handler = handler;
    ls->open = 0;
    ls->linger = 1;

    return ls;
}

int conn_listening_close(array_t *listening)
{
    size_t       i = 0;
    listening_t *ls = NULL;

    for (i = 0; i < listening->nelts; i++) 
    {
        if (ls[i].fd != SHS_INVALID_FILE) 
        {
            close(ls[i].fd);
        }
    }

    return SHS_OK;
}

int conn_listening_add_event(event_base_t *base, array_t *listening)
{
    conn_t      *c = NULL;
    event_t     *rev = NULL;
    uint32_t     i = 0;
    listening_t *ls = NULL;
      
    ls = (listening_t *)listening->elts;
	
    for (i = 0; i < listening->nelts; i++) 
    {
        c = ls[i].connection;
		
        if (!c) 
        {
            c = conn_get_from_mem(ls->fd);
            if (!c) 
            {
                return SHS_ERROR;
            }
			
            //c->listening = &ls[i];
            ls[i].connection = c;
            rev = c->read;
            rev->accepted = SHS_TRUE;
            rev->handler = ls->handler;
        }
        else 
        {
            rev = c->read;
        }
		
        // setup listenting event
        if (event_add(base, rev, EVENT_READ_EVENT, 0) == SHS_ERROR) 
        {
            return SHS_ERROR;
        }
    }

    return SHS_OK;
}

int conn_listening_del_event(event_base_t *base, array_t *listening)
{
    conn_t      *c = NULL;
    uint32_t     i = 0;
    listening_t *ls = NULL;

    ls = (listening_t *)listening->elts;
	
    for (i = 0; i < listening->nelts; i++) 
    {
        c = ls[i].connection;
		
        if (event_delete(base, c->read, EVENT_READ_EVENT, 0) == SHS_ERROR) 
        {
            return SHS_ERROR;
        }
    }

    return SHS_OK;
}

