#include "shs_sysio.h"
#include "shs_chain.h"

static ssize_t sysio_writev_iovs(conn_t *c, sysio_vec *iovs, int count);
static int sysio_pack_chain_to_iovs(sysio_vec *iovs, int iovs_count, 
	chain_t *in, size_t *last_size, size_t limit);

sysio_t linux_io = 
{
    // read
    sysio_unix_recv,
    sysio_readv_chain,
    sysio_udp_unix_recv,
    // write
    sysio_unix_send,
    sysio_writev_chain,
    sysio_sendfile_chain,
    0
};

ssize_t sysio_unix_recv(conn_t *c, uchar_t *buf, size_t size)
{
    ssize_t  n = 0;

    if (!c) 
    {
        return SHS_ERROR;
    }
	
    if (!buf) 
    {
        return SHS_ERROR;
    }
	
    for ( ;; ) 
    {
        errno = 0;
		
        n = recv(c->fd, buf, size, 0);
        if (n > 0) 
        {
            return n;
        }

        if (0 == n) 
        {
            return n;
        }

        if (errno == SHS_EINTR) 
        {
            continue;
        }
		
        if (errno == SHS_EAGAIN) 
        {
            return SHS_AGAIN;
        }
 
        return SHS_ERROR;
    }

    return SHS_OK;
}

ssize_t sysio_readv_chain(conn_t *c, chain_t *chain)
{
    int           i = 0;
    uchar_t      *prev = NULL;
    ssize_t       n = 0;
    ssize_t       size = 0;
    struct iovec  iovs[SHS_IOVS_REV];

    while (chain && i < SHS_IOVS_REV) 
    {
        if (prev == chain->buf->last) 
        {
            iovs[i - 1].iov_len += chain->buf->end - chain->buf->last;
        } 
        else 
        {
            iovs[i].iov_base = (void *) chain->buf->last;
            iovs[i].iov_len = chain->buf->end - chain->buf->last;

            i++;
        }
		
        size += chain->buf->end - chain->buf->last;
        prev = chain->buf->end;
        chain = chain->next;
    }

    for ( ;; ) 
    {
        errno = 0;
		
        n = readv(c->fd, iovs, i);
        if (n > 0) 
        {
            return n;
        }

        if (0 == n) 
        {
            return n;
        }

        if (errno == SHS_EAGAIN) 
        {
            return SHS_AGAIN;
        }

        if (errno == SHS_EINTR) 
        {
            continue;
        }

        return SHS_ERROR;
    } 

    return SHS_OK;
}

ssize_t sysio_udp_unix_recv(conn_t *c, uchar_t *buf, size_t size)
{
    ssize_t n = 0;

    for ( ;; ) 
    {
        errno = 0;
		
        n = recv(c->fd, buf, size, 0);
        if (n >= 0) 
        {
            return n;
        }

        if (errno == SHS_EINTR) 
        {
            continue;
        }
		
        if (errno == SHS_EAGAIN) 
        {
            return SHS_AGAIN;
        }

        return SHS_ERROR;
    }
    
    return SHS_OK;
}

ssize_t sysio_unix_send(conn_t *c, uchar_t *buf, size_t size)
{
    ssize_t  n = 0;
    event_t *wev = NULL;

    wev = c->write;

    for ( ;; ) 
    {
        errno = 0;
		
        n = send(c->fd, buf, size, 0);
        if (n > 0) 
        {
            if (n < (ssize_t) size) 
            {
                wev->ready = 0;
            }

            return n;
        }

        if (0 == n) 
        {
            wev->ready = 0;

            return n;
        }

        if (errno == SHS_EINTR) 
        {
            continue;
        }

        if (errno == SHS_EAGAIN) 
        {
            wev->ready = 0;

            return SHS_AGAIN;
        }

        return SHS_ERROR;
    }

    return SHS_OK;
}

chain_t * sysio_writev_chain(conn_t *c, chain_t *in, size_t limit)
{
    int        pack_count = 0;
    size_t     packall_size = 0;
    //size_t     last_size = 0;
    ssize_t    sent_size = 0;
    chain_t   *cl = NULL;
    event_t   *wev = NULL;
    sysio_vec  iovs[SHS_IOVS_MAX];

    if (!in) 
    {
        return NULL;
    }
	
    if (!c) 
    {
        return SHS_CHAIN_ERROR;
    }
	
    wev = c->write;
    if (!wev) 
    {
        return SHS_CHAIN_ERROR;
    }

    if (!wev->ready) 
    {
        return in;
    }

    // the maximum limit size is the 2G - page size
    if (0 == limit || limit > SHS_MAX_LIMIT) 
    {
        limit = SHS_MAX_LIMIT;
    }
 
    while (in && packall_size < limit) 
    {
        //last_size = packall_size;
        if (SHS_FALSE == in->buf->memory) 
	{
            break;
        }
		
        pack_count = sysio_pack_chain_to_iovs(iovs,
            SHS_IOVS_MAX, in, &packall_size, limit);
        if (0 == pack_count) 
        {
            return NULL;
        }

        sent_size = sysio_writev_iovs(c, iovs, pack_count);
        if (sent_size > 0) 
	{
            //c->sent += sent_size;
            cl = chain_write_update(in, sent_size);
			
            if (packall_size >= limit) 
            {
                return cl;
            }
			
            in = cl;

            continue;
        }
		
        if (SHS_AGAIN == sent_size) 
        {
            wev->ready = 0;
			
            return in;
        }
		
        if (SHS_ERROR == sent_size) 
        {
            return SHS_CHAIN_ERROR;
        }
    }
    
    return in;
}

/*
 * On Linux up to 2.4.21 sendfile() (syscall #187) works with 32-bit
 * offsets only, and the including <sys/sendfile.h> breaks the compiling,
 * if off_t is 64 bit wide.  So we use own sendfile() definition, where offset
 * parameter is int32_t, and use sendfile() for the file parts below 2G only,
 * see src/os/unix/dfs_linux_config.h
 *
 * Linux 2.4.21 has the new sendfile64() syscall #239.
 *
 * On Linux up to 2.6.16 sendfile() does not allow to pass the count parameter
 * more than 2G-1 bytes even on 64-bit platforms: it returns EINVAL,
 * so we limit it to 2G-1 bytes.
 */
chain_t * sysio_sendfile_chain(conn_t *c, chain_t *in, int fd, size_t limit)
{
    int      rc = 0;
    size_t   pack_size = 0;
    event_t *wev = NULL;	
    size_t   sent = 0;

    if (!in) 
    {
        return NULL;
    }
	
    if (!c) 
    {
        return SHS_CHAIN_ERROR;
    }

    wev = c->write;
    if (!wev) 
    {
        return SHS_CHAIN_ERROR;
    }

    if (!wev->ready) 
    {
        return in;
    }

    // the maximum limit size is the 2G - page size
    if (0 == limit || limit > SHS_MAX_LIMIT) 
    {
        limit = SHS_MAX_LIMIT;
    }
	
    while (in && sent < limit) 
    {
        if (SHS_TRUE == in->buf->memory) 
        {
            break;
        }

        pack_size = buffer_size(in->buf);
        if (0 == pack_size) 
        {
            in = in->next;

            continue;
        }

        if (sent + pack_size > limit) 
	{
            pack_size = limit - sent;
        }
		
        rc = sendfile(c->fd, fd, &in->buf->file_pos, pack_size);
        if (SHS_ERROR == rc) 
        {
            if (errno == SHS_EAGAIN) 
            {
                wev->ready = 0;

                return in;
            } 
            else if (errno == SHS_EINTR) 
            {
                continue;
            } 
			
            return SHS_CHAIN_ERROR;
        }

        if (!rc) 
        {
            return SHS_CHAIN_ERROR;
        }
        
        if (rc > 0) 
        {
            //c->sent += rc;
            sent += rc;
			
            if (buffer_size(in->buf) == 0) 
            {
                in = in->next;
            }
        }
    }
 
    return in;
}

static int sysio_pack_chain_to_iovs(sysio_vec *iovs, int iovs_count, 
    chain_t *in, size_t *last_size, size_t limit)
{
    int      i = 0;
    ssize_t  bsize = 0;
    uchar_t *last_pos = NULL;

    if (!iovs || !in || !last_size) 
    {
        return i;
    }
	
    while (in && i < iovs_count && *last_size < limit) 
    {
        if (SHS_FALSE == in->buf->memory) 
        {
            break;
        }

        bsize = buffer_size(in->buf);
        if (bsize <= 0) 
        {
            in = in->next;
			
            continue;
        }
		
        if (*last_size + bsize > limit) 
        {
            bsize = limit - *last_size;
        }
		
        if (last_pos != in->buf->pos) 
        {
            iovs[i].iov_base = in->buf->pos;
            iovs[i].iov_len = bsize;
            i++;
        }
        else 
        {
            iovs[i - 1].iov_len += bsize;
        }
		
        *last_size += bsize;
        last_pos = in->buf->pos;
        in = in->next;
    }

    return i;
}

static ssize_t sysio_writev_iovs(conn_t *c, sysio_vec *iovs, int count)
{
    ssize_t rc = SHS_ERROR;
    
    if (!c || !iovs || count <= 0) 
    {
        return rc;
    }
    
    for ( ;; ) 
    {
        errno = 0;
		
        rc = writev(c->fd, iovs, count);
        if (rc > 0) 
        {
            return rc;
        }
		
        if (SHS_ERROR == rc) 
        {
            if (errno == SHS_EINTR) 
            {
                continue;
            }

            if (errno == SHS_EAGAIN) 
            {
                return SHS_AGAIN;
            }
			
            return SHS_ERROR;
        }
		
        if (0 == rc) 
        {
            return SHS_ERROR;
        }
    }

    return SHS_ERROR;
}

