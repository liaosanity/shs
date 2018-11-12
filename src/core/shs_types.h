#ifndef SHS_TYPES_H
#define SHS_TYPES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define SHS_DEBUG               1

/* compile option */
#define _xcdecl
#define _xinline                inline
#define _xvolatile              volatile
#define _xlibc_cdecl

#define SHS_MAX_SIZE_T_VALUE    2147483647L

/* sys return value */
#define SHS_OK                  0
#define SHS_ERROR              -1
#define SHS_AGAIN              -2
#define SHS_BUSY               -3
#define SHS_PUSH               -4
#define SHS_DECLINED           -5
#define SHS_ABORT              -6
#define SHS_GO_ON              -7
#define SHS_BADMEM             -8
#define SHS_BUFFER_FULL        -9
#define SHS_CONN_CLOSED        -10
#define SHS_TIME_OUT           -11

#define SHS_TRUE                1
#define SHS_FALSE               0

#define SHS_LINEFEED_SIZE       1
#define SHS_INVALID_FILE       -1
#define SHS_INVALID_PID        -1
#define SHS_FILE_ERROR         -1  

#define SHS_TID_T_FMT           "%ud"    
#define SHS_INT32_LEN           sizeof("-2147483648") - 1          
#define SHS_INT64_LEN           sizeof("-9223372036854775808") - 1 
#define SHS_PTR_SIZE            4

#define MB_SIZE                 1024 * 1024
#define GB_SIZE                 1024 * 1024 * 1024

#define shs_signal_helper(n)    SIG##n
#define shs_signal_value(n)     shs_signal_helper(n)

#define SHS_SHUTDOWN_SIGNAL      QUIT
#define SHS_TERMINATE_SIGNAL     TERM
#define SHS_NOACCEPT_SIGNAL      WINCH
#define SHS_RECONFIGURE_SIGNAL   HUP

#define SHS_REOPEN_SIGNAL        USR1
#define SHS_CHANGEBIN_SIGNAL     USR2

#define SHS_CMD_OPEN_CHANNEL   1
#define SHS_CMD_CLOSE_CHANNEL  2
#define SHS_CMD_QUIT           3
#define SHS_CMD_TERMINATE      4
#define SHS_CMD_REOPEN         5

#define SHS_PROCESS_SINGLE     0
#define SHS_PROCESS_MASTER     1
#define SHS_PROCESS_SIGNALLER  2
#define SHS_PROCESS_WORKER     3
#define SHS_PROCESS_HELPER     4
#define SHS_PROCESS_MONITOR    5

#if (SHS_PTR_SIZE == 4)
#define SHS_INT_T_LEN SHS_INT32_LEN
#else
#define SHS_INT_T_LEN SHS_INT64_LEN
#endif

#if ((__GNU__ == 2) && (__GNUC_MINOR__ < 8))
#define SHS_MAX_UINT32_VALUE    (uint32_t)0xffffffffLL
#else
#define SHS_MAX_UINT32_VALUE    (uint32_t)0xffffffff
#endif 

#include <assert.h> 
#include <sys/types.h>
#include <sys/time.h>

#if HAVE_INTTYPES_H
#include <inttypes.h> 
#endif

#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <execinfo.h>

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include <glob.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>
#include <ctype.h>
#include <sys/socket.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <netinet/tcp.h>

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <sys/un.h>

#include <time.h>

#if HAVE_MALLOC
#include <malloc.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <sys/sysctl.h>
#include <sys/prctl.h>
#include <crypt.h>
#include <sys/utsname.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include <aio.h>
#include <openssl/md5.h>
#include <pthread.h> 

#ifndef SHS_OFF_T_LEN
#define SHS_OFF_T_LEN          sizeof("-9223372036854775808") - 1
#endif

#define SHS_MD5_INIT            MD5_Init
#define SHS_MD5_UPDATE          MD5_Update
#define SHS_MD5_FINAL           MD5_Final
#define SHS_MD5_KEY_N           16
#define SHS_MD5_KEY_TXT_N       32
#define SHS_MAX_ERROR_STR       2048

typedef u_char                  uchar_t;
typedef int                     atomic_int_t;
typedef uint32_t                atomic_uint_t;
typedef int64_t                 rbtree_key;
typedef int64_t                 rbtree_key_int;
typedef int64_t                 rb_msec_t;
typedef int64_t                 rb_msec_int_t;
typedef struct pool_s           pool_t;
typedef struct array_s          array_t;
typedef struct string_s         string_t;
typedef struct buffer_s         buffer_t;
typedef struct hashtable_link_s hashtable_link_t;
typedef struct event_base_s     event_base_t;
typedef struct event_timer_s    event_timer_t;
typedef struct event_s          event_t;
typedef struct listening_s      listening_t;
typedef struct conn_s           conn_t;
typedef struct chain_s          chain_t;

#define MMAP_PROT          PROT_READ | PROT_WRITE
#define MMAP_FLAG          MAP_PRIVATE | MAP_ANONYMOUS

#define ALIGN_POINTER       sizeof(void *)

#define ALIGNMENT(x, align) \
    (((x) % (align)) ? ((x) + ((align) - (x) % (align))) : (x))

#define DEFAULT_PAGESIZE       4096
#define DEFAULT_CACHELINE_SIZE 64

#define SHS_MAX_ALLOC_FROM_POOL ((size_t)(DEFAULT_PAGESIZE - 1))
#define SHS_ALIGNMENT sizeof(void *)

#define shs_align_ptr(p, a)       \
    (uchar_t *) (((uintptr_t)(p) + ((uintptr_t)(a) - 1)) & ~((uintptr_t)(a) - 1))

#define shs_sys_open(a,b,c)     open((char *)a, b, c)
#define shs_open_fd(a,b)        open((char *)a, b)
#define shs_write_fd            write
#define shs_read_fd             read
#define shs_delete_file(s)      unlink((char *)s)
#define shs_lseek_file		lseek

typedef ssize_t  (*sysio_recv_pt)(conn_t *c, uchar_t *buf, size_t size);
typedef ssize_t  (*sysio_recv_chain_pt)(conn_t *c, chain_t *in);
typedef ssize_t  (*sysio_send_pt)(conn_t *c, uchar_t *buf, size_t size);
typedef chain_t *(*sysio_send_chain_pt)(conn_t *c, chain_t *in, size_t limit);
typedef chain_t *(*sysio_sendfile_pt)(conn_t *c, chain_t *in, int fd, size_t limit);

#ifndef SHS_PAGE_SIZE
    #if defined(_SC_PAGESIZE)
        #define SHS_PAGE_SIZE sysconf(_SC_PAGESIZE)
    #elif defined(_SC_PAGE_SIZE)
        #define SHS_PAGE_SIZE sysconf(_SC_PAGE_SIZE)
    #else
        #define SHS_PAGE_SIZE (4096)
    #endif
#endif

#ifndef SHS_DEBUG
#define SHS_DEBUG 1
#endif

#endif

