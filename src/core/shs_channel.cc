#include "shs_channel.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "core/shs_types.h"

int shs_write_channel(int s, shs_channel_t *ch, size_t size)
{
    ssize_t       n;
    int           err;
    struct iovec  iov[1];
    struct msghdr msg;

    union 
    {
        struct cmsghdr cm;
        char           space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    if (ch->fd == -1) 
    {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    } 
    else 
    {
        msg.msg_control = (caddr_t) &cmsg;
        msg.msg_controllen = sizeof(cmsg);

        cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
        cmsg.cm.cmsg_level = SOL_SOCKET;
        cmsg.cm.cmsg_type = SCM_RIGHTS;

        memcpy(CMSG_DATA(&cmsg.cm), &ch->fd, sizeof(int));
    }

    msg.msg_flags = 0;

    iov[0].iov_base = (char *)ch;
    iov[0].iov_len = size;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    n = sendmsg(s, &msg, 0);
    if (n == -1) 
    {
        err = errno;
        if (err == EAGAIN) 
        {
            return SHS_AGAIN;
        }

        return SHS_ERROR;
    }

    return SHS_OK;
}

int shs_read_channel(int s, shs_channel_t *ch, size_t size)
{
    ssize_t       n;
    int           err;
    struct iovec  iov[1];
    struct msghdr msg;

    union 
    {
        struct cmsghdr cm;
        char           space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    iov[0].iov_base = (char *) ch;
    iov[0].iov_len = size;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = (caddr_t) &cmsg;
    msg.msg_controllen = sizeof(cmsg);

    n = recvmsg(s, &msg, 0);
    if (n == -1) 
    {
        err = errno;
        if (err == EAGAIN) 
        {
            return SHS_AGAIN;
        }

        return SHS_ERROR;
    }

    if (n == 0) 
    {
        return SHS_ERROR;
    }

    if ((size_t) n < sizeof(shs_channel_t)) 
    {
        return SHS_ERROR;
    }

    if (ch->command == SHS_CMD_OPEN_CHANNEL) 
    {
        if (cmsg.cm.cmsg_len < (socklen_t) CMSG_LEN(sizeof(int))) 
        {
            return SHS_ERROR;
        }

        if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS)
        {
            return SHS_ERROR;
        }

        memcpy(&ch->fd, CMSG_DATA(&cmsg.cm), sizeof(int));
    }

    if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) 
    {
        // log err
    }

    return n;
}

void shs_close_channel(int *fd)
{
    if (close(fd[0]) == -1) 
    {
        // log err
    }

    if (close(fd[1]) == -1) 
    {
        // log err
    }
}
