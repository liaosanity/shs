#ifndef SHS_CONN_H
#define SHS_CONN_H

#include "shs_types.h"
#include "shs_sysio.h"
#include "shs_string.h"
#include "shs_event.h"

#define CONN_DEFAULT_RCVBUF    (64 << 10)
#define CONN_DEFAULT_SNDBUF    (64 << 10)
#define CONN_DEFAULT_POOL_SIZE 4096
#define CONN_DEFAULT_BACKLOG   2048

typedef struct conn_peer_s conn_peer_t;

enum 
{
    CONN_TCP_NODELAY_UNSET = 0,
    CONN_TCP_NODELAY_SET,
    CONN_TCP_NODELAY_DISABLED
};

enum 
{
    CONN_TCP_NOPUSH_UNSET = 0,
    CONN_TCP_NOPUSH_SET,
    CONN_TCP_NOPUSH_DISABLED
};

enum 
{
    CONN_ERROR_NONE = 0,
    CONN_ERROR_REQUEST,
    CONN_ERROR_EOF
};

struct conn_s 
{
    int                    fd;
    void                  *next;
    void                  *conn_data;
    event_t               *read;
    event_t               *write;
    sysio_recv_pt          recv;
    sysio_send_pt          send;
    //sysio_recv_chain_pt    recv_chain;
    //sysio_send_chain_pt    send_chain;
    //sysio_sendfile_pt      sendfile_chain;
    //listening_t           *listening;
    //size_t                 sent;
    pool_t                *pool;
    //struct sockaddr       *sockaddr;
    //socklen_t              socklen;
    //string_t               addr_text;
    //struct timeval         accept_time;
    //uint32_t               error:1;
    //uint32_t               sendfile:1;
    //uint32_t               sndlowat:1;
    uint32_t               tcp_nodelay:2;
    uint32_t               tcp_nopush:2;
    event_timer_t         *ev_timer;
    event_base_t          *ev_base;  
};

struct conn_peer_s 
{
    conn_t                *connection;
    struct sockaddr       *sockaddr;
    socklen_t              socklen;
    string_t              *name;
    int                    rcvbuf;
};

#define SHS_EPERM         EPERM
#define SHS_ENOENT        ENOENT
#define SHS_ESRCH         ESRCH
#define SHS_EINTR         EINTR
#define SHS_ECHILD        ECHILD
#define SHS_ENOMEM        ENOMEM
#define SHS_EACCES        EACCES
#define SHS_EBUSY         EBUSY
#define SHS_EEXIST        EEXIST
#define SHS_ENOTDIR       ENOTDIR
#define SHS_EISDIR        EISDIR
#define SHS_EINVAL        EINVAL
#define SHS_ENOSPC        ENOSPC
#define SHS_EPIPE         EPIPE
#define SHS_EAGAIN        EAGAIN
#define SHS_EINPROGRESS   EINPROGRESS
#define SHS_EADDRINUSE    EADDRINUSE
#define SHS_ECONNABORTED  ECONNABORTED
#define SHS_ECONNRESET    ECONNRESET
#define SHS_ENOTCONN      ENOTCONN
#define SHS_ETIMEDOUT     ETIMEDOUT
#define SHS_ECONNREFUSED  ECONNREFUSED
#define SHS_ENAMETOOLONG  ENAMETOOLONG
#define SHS_ENETDOWN      ENETDOWN
#define SHS_ENETUNREACH   ENETUNREACH
#define SHS_EHOSTDOWN     EHOSTDOWN
#define SHS_EHOSTUNREACH  EHOSTUNREACH
#define SHS_ENOSYS        ENOSYS
#define SHS_ECANCELED     ECANCELED
#define SHS_ENOMOREFILES  0

#define SHS_SOCKLEN       512

#define conn_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
#define conn_blocking(s)     fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK)

int  conn_connect_peer(conn_peer_t *pc, event_base_t *ep_base);
conn_t *conn_get_from_mem(int s);
void conn_free_mem(conn_t *c);
void conn_set_default(conn_t *c, int s);
void conn_close(conn_t *c);
void conn_release(conn_t *c);
int  conn_tcp_nopush(int s);
int  conn_tcp_push(int s);
int  conn_tcp_nodelay(int s);
int  conn_tcp_delay(int s);

#endif

