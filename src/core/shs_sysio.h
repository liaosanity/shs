#ifndef SHS_SYSIO_H
#define SHS_SYSIO_H

#include "shs_types.h"
#include "shs_conn.h"

#define SHS_IOVS_REV       16
#define SHS_MAX_LIMIT      2147479552L
#define SHS_SENDFILE_LIMIT 2147479552L

#define SHS_IOV_MAX        32
#if (SHS_IOV_MAX > 64)
#define SHS_HEADERS_MAX    64
#define SHS_IOVS_MAX       64
#else
#define SHS_HEADERS_MAX    SHS_IOV_MAX
#define SHS_IOVS_MAX       SHS_IOV_MAX
#endif

#define shs_sys_open(a,b,c)    open((char *)a, b, c)
#define shs_open_fd(a,b)       open((char *)a, b)
#define shs_write_fd           write
#define shs_read_fd            read
#define shs_delete_file(s)     unlink((char *)s)
#define shs_lseek_file         lseek

typedef struct sysio_s sysio_t;

struct sysio_s 
{
    sysio_recv_pt       recv;
    sysio_recv_chain_pt recv_chain;
    sysio_recv_pt       udp_recv;
    sysio_send_pt       send;
    sysio_send_chain_pt send_chain;
    sysio_sendfile_pt   sendfile_chain;
    uint32_t            flags;
};

typedef struct iovec sysio_vec;

#define shs_recv           linux_io.recv
#define shs_recv_chain     linux_io.recv_chain
#define shs_udp_recv       linux_io.udp_recv
#define shs_send           linux_io.send
#define shs_send_chain     linux_io.send_chain
#define shs_sendfile_chain linux_io.sendfile_chain

extern sysio_t linux_io;

ssize_t  sysio_unix_recv(conn_t *c, uchar_t *buf, size_t size);
ssize_t  sysio_readv_chain(conn_t *c, chain_t *chain);
ssize_t  sysio_unix_send(conn_t *c, uchar_t *buf, size_t size);
chain_t *sysio_writev_chain(conn_t *c, chain_t *in, size_t limit);
ssize_t  sysio_udp_unix_recv(conn_t *c, uchar_t *buf, size_t size);
chain_t *sysio_sendfile_chain(conn_t *c, chain_t *in, int fd, size_t limit);

#endif

