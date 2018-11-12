#ifndef SHS_CONN_LISTEN_H
#define SHS_CONN_LISTEN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "shs_types.h"
#include "shs_string.h"
#include "shs_array.h"
#include "shs_conn.h"

struct listening_s 
{
    int               fd;
    struct sockaddr  *sockaddr;
    socklen_t         socklen;    // size of sockaddr
    string_t          addr_text;
    int               family;
    int               type;
    int               backlog;
    int               rcvbuf;
    int               sndbuf;
    event_handler_pt  handler; //handler of accepted connection
    size_t            conn_psize;
    listening_t      *previous;
    conn_t           *connection;
    uint32_t          open:1;
    uint32_t          ignore:1;
    uint32_t          linger:1;
    uint32_t          inherited:1;
    uint32_t          listen:1;
};

int conn_listening_open(array_t *listening);
listening_t * conn_listening_add(array_t *listening, pool_t *pool, 
    in_addr_t addr, in_port_t port, event_handler_pt handler,
    int rbuff_len, int sbuff_len);
int conn_listening_close(array_t *listening);
int conn_listening_add_event(event_base_t *base, array_t *listening);
int conn_listening_del_event(event_base_t *base, array_t *listening);

#endif

