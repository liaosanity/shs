#ifndef SHS_LOCK_H
#define SHS_LOCK_H

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include "shs_mem_allocator.h"
#include "shs_memory_pool.h"

#define	SHS_LOCK_ERROR -1
#define SHS_LOCK_OK     0
#define	SHS_LOCK_OFF    1
#define	SHS_LOCK_ON     2

#define SHS_LOCK_FALSE  0
#define SHS_LOCK_TRUE   1

#if (__GNUC__ >= 4 && __GNUC_MINOR__ >=2 )
#define CAS(val, old, set) \
    __sync_bool_compare_and_swap((val), (old), (set))
#else
#define CAS(val, old, set) \
	cmp_and_swap((val), (old), (set))
#endif

enum 
{
    SHS_LOCK_ERR_NONE = 0,
    SHS_LOCK_ERR_START = 100,
    SHS_LOCK_ERR_PARAM,
    SHS_LOCK_ERR_ALLOCATOR_ERROR,
    SHS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT,
    SHS_LOCK_ERR_SYSCALL_MUTEXATTR_SETPSHARED,
    SHS_LOCK_ERR_SYSCALL_MUTEX_INIT,
    SHS_LOCK_ERR_SYSCALL_MUTEXATTR_DESTROY,
    SHS_LOCK_ERR_SYSCALL_MUTEX_DESTROY,
    SHS_LOCK_ERR_SYSCALL_MUTEX_LOCK,
    SHS_LOCK_ERR_SYSCALL_MUTEX_TRYLOCK,
    SHS_LOCK_ERR_SYSCALL_MUTEX_UNLOCK,
    SHS_LOCK_ERR_SYSCALL_SIGPROCMASK,
    SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT,
    SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_INIT,
    SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_RDLOCK,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_UNLOCK,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_WRLOCK,
    SHS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK,
    SHS_LOCK_ERR_END
};

typedef struct 
{
    pthread_mutex_t      lock;
    pthread_mutexattr_t  mutex_attr;
    sigset_t             sig_block_mask;
    sigset_t             sig_prev_mask;
    shs_mem_allocator_t *allocator;
} shs_process_lock_t;

typedef struct 
{
    pthread_rwlock_t     rwlock;
    pthread_rwlockattr_t rwlock_attr;
    sigset_t             sig_block_mask;
    sigset_t             sig_prev_mask;
    shs_mem_allocator_t *allocator;
} shs_process_rwlock_t;

typedef struct 
{
    volatile uint64_t    lock;
    shs_mem_allocator_t *allocator;
} shs_atomic_lock_t;

typedef struct 
{
    int                  syscall_errno;
    unsigned int         lock_errno;
    unsigned int         allocator_errno;
    shs_mem_allocator_t *err_allocator;
} shs_lock_errno_t;

shs_process_lock_t * shs_process_lock_create(shs_mem_allocator_t *allocator,
    shs_lock_errno_t *error);
int shs_process_lock_release(shs_process_lock_t *process_lock,
    shs_lock_errno_t *error);
int shs_process_lock_reset(shs_process_lock_t *process_lock,
    shs_lock_errno_t *error);
int shs_process_lock_on(shs_process_lock_t *process_lock,
    shs_lock_errno_t *error);
int shs_process_lock_off(shs_process_lock_t *process_lock,
    shs_lock_errno_t *error);
int shs_process_lock_try_on(shs_process_lock_t *process_lock,
    shs_lock_errno_t *error);
shs_process_rwlock_t * shs_process_rwlock_create(shs_mem_allocator_t *allocator,
    shs_lock_errno_t *error);
int shs_process_rwlock_release(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_init(shs_process_rwlock_t * process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_destroy(shs_process_rwlock_t* process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_reset(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_read_on(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_write_on(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_write_try_on(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
int shs_process_rwlock_off(shs_process_rwlock_t *process_rwlock,
    shs_lock_errno_t *error);
shs_atomic_lock_t * shs_atomic_lock_create(shs_mem_allocator_t *allocator,
    shs_lock_errno_t *error);
int shs_atomic_lock_init(shs_atomic_lock_t *atomic_lock);
int shs_atomic_lock_release(shs_atomic_lock_t *atomic_lock,
    shs_lock_errno_t *error);
int shs_atomic_lock_reset(shs_atomic_lock_t *atomic_lock,
    shs_lock_errno_t *error);
int shs_atomic_lock_try_on(shs_atomic_lock_t *atomic_lock,
    shs_lock_errno_t *error);
int shs_atomic_lock_on(shs_atomic_lock_t *atomic_lock,
    shs_lock_errno_t *error);
int shs_atomic_lock_off(shs_atomic_lock_t *atomic_lock,
    shs_lock_errno_t *error);
const char * shs_lock_strerror(shs_lock_errno_t *error);
inline void shs_atomic_lock_off_force(shs_atomic_lock_t *atomic_lock);
shs_atomic_lock_t *shs_atomic_lock_pool_create(pool_t *pool);

#endif

