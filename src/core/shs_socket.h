#ifndef SHS_SOCKET_H
#define SHS_SOCKET_H

int tcp_listen(const char *host, const char *serv);
int shs_nonblocking(int s);
int shs_blocking(int s);

#define shs_nonblocking_n   "ioctl(FIONBIO)"
#define shs_blocking_n      "ioctl(!FIONBIO)"

int shs_tcp_nopush(int s);
int shs_tcp_push(int s);

#if (__linux__)

#define shs_tcp_nopush_n   "setsockopt(TCP_CORK)"
#define shs_tcp_push_n     "setsockopt(!TCP_CORK)"

#else

#define shs_tcp_nopush_n   "setsockopt(TCP_NOPUSH)"
#define shs_tcp_push_n     "setsockopt(!TCP_NOPUSH)"

#endif

#endif
