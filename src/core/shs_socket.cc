#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

int tcp_listen(const char *host, const char *serv)
{
    int fd = -1, on = 1;
    struct addrinfo ai, *pa;
    memset(&ai, 0, sizeof(struct addrinfo));
    ai.ai_family = AF_INET;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_flags = AI_PASSIVE;

    if (getaddrinfo(host, serv, &ai, &pa) != 0) 
    {
        return -1; 
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        return -1; 
    }

#ifdef USE_SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (void *)&on, sizeof(on));
#endif
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) 
    {
        freeaddrinfo(pa);
        close(fd);

        return -1; 
    }

    if (bind(fd, pa->ai_addr, pa->ai_addrlen) || listen(fd, 4096)) 
    {
        freeaddrinfo(pa);
        close(fd);

        return -1; 
    }   

    freeaddrinfo(pa);

    return fd; 
} 

int shs_nonblocking(int s)
{
    int nb = 1;

    return ioctl(s, FIONBIO, &nb);
}

int shs_blocking(int s)
{
    int nb = 0;

    return ioctl(s, FIONBIO, &nb);
}

#if (__FreeBSD__)

int shs_tcp_nopush(int s)
{
    int tcp_nopush = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH, 
        (const void *) &tcp_nopush, sizeof(int));
}

int shs_tcp_push(int s)
{
    int tcp_nopush = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
        (const void *) &tcp_nopush, sizeof(int));
}

#elif (__linux__)

int shs_tcp_nopush(int s)
{
    int cork = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
        (const void *) &cork, sizeof(int));
}

int shs_tcp_push(int s)
{
    int cork = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
        (const void *) &cork, sizeof(int));
}

#else

int shs_tcp_nopush(int s)
{
    return 0;
}

int shs_tcp_push(int s)
{
    return 0;
}

#endif
