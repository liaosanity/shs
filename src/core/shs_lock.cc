#include <errno.h>

#include "shs_lock.h"

#define SHS_LOCK_ERRNO_CLEAN(e)                \
    (e)->lock_errno = SHS_LOCK_ERR_NONE;       \
    (e)->syscall_errno = 0;                    \
    (e)->allocator_errno = SHS_LOCK_ERR_NONE;  \
    (e)->err_allocator = NULL

#define SHS_LOCK_ERRNO_ALLOCATOR(e, a)         \
    (e)->err_allocator = (a)

/*
 * Special Note:
 * When the instances of shs_process_lock are more than one
 * the shs_process_lock_held assignment must be confused!!!!!!
 */

static const char *shs_lock_err_string[] = 
{
    "shs_lock: unknown error number",
    "shs_lock: parameter error",
    "shs_lock: allocator error",
    "shs_lock: mutex attribute init error",
    "shs_lock: mutex attribute setpshared error",
    "shs_lock: mutex init error",
    "shs_lock: mutex attribute destroy error",
    "shs_lock: mutex destroy error",
    "shs_lock: mutex lock error", 
    "shs_lock: mutex try lock error",
    "shs_lock: mutex unlock error",
    "shs_lock: sigprocmask error",
    "shs_lock: rwlock attribute init error",
    "shs_lock: rwlock attribute setpshared error",
    "shs_lock: rwlock init error",
    "shs_lock: rwlock attribute destroy error",
    "shs_lock: rwlock destroy error",
    "shs_lock: rwlock reading lock error",
    "shs_lock: rwlock unlock error",
    "shs_lock: rwlock writing lock error",
    "shs_lock: rwlock writing try lock error",
    NULL
};

static inline unsigned char cmp_and_swap(volatile uint64_t *ptr, uint64_t old, uint64_t nw)
{
    unsigned char ret = 0;
	
    __asm__ __volatile__("lock; cmpxchgq %1,%2; setz %0"
        : "=a"(ret)
        : "q"(nw), "m"(*(volatile uint64_t *)(ptr)), "0"(old)
        : "memory");
	
    return ret;
}

shs_process_lock_t * shs_process_lock_create(shs_mem_allocator_t *allocator, shs_lock_errno_t *error)
{
    shs_process_lock_t *process_lock = NULL;

    if (!error) 
    {
        return NULL;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return NULL;
    }

    process_lock = (shs_process_lock_t *)allocator->alloc(allocator, sizeof(shs_process_lock_t),
        &error->allocator_errno);
    if (!process_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;

        return NULL;
    }

    process_lock->allocator = allocator;

    if ((error->syscall_errno = pthread_mutexattr_init(&process_lock->mutex_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT;

        return NULL;
    }

    if ((error->syscall_errno = pthread_mutexattr_setpshared(&process_lock->mutex_attr, PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEXATTR_SETPSHARED;
		
        return NULL;
    }

    if ((error->syscall_errno = pthread_mutex_init(&process_lock->lock, &process_lock->mutex_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_INIT;

        return NULL;
    }

    sigfillset(&process_lock->sig_block_mask);
    sigdelset(&process_lock->sig_block_mask, SIGALRM);
    sigdelset(&process_lock->sig_block_mask, SIGINT);
    sigdelset(&process_lock->sig_block_mask, SIGCHLD);
    sigdelset(&process_lock->sig_block_mask, SIGPIPE);
    sigdelset(&process_lock->sig_block_mask, SIGSEGV);
    sigdelset(&process_lock->sig_block_mask, SIGHUP);
    sigdelset(&process_lock->sig_block_mask, SIGQUIT);
    sigdelset(&process_lock->sig_block_mask, SIGTERM);
    sigdelset(&process_lock->sig_block_mask, SIGIO);
    sigdelset(&process_lock->sig_block_mask, SIGUSR1);

    return process_lock;    
}

int shs_process_lock_release(shs_process_lock_t *process_lock, shs_lock_errno_t *error)
{
    shs_mem_allocator_t *allocator = NULL; 

    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_lock || !process_lock->allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;
    	
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_SETMASK, &process_lock->sig_prev_mask, NULL)) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;

        return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_mutexattr_destroy(&process_lock->mutex_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEXATTR_DESTROY;
		
    	return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_mutex_destroy(&process_lock->lock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_DESTROY;

    	return SHS_LOCK_ERROR;
    }

    allocator = process_lock->allocator;

    if (allocator->free(allocator, process_lock, &error->allocator_errno) == SHS_MEM_ALLOCATOR_ERROR) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;
    }

    return SHS_LOCK_OK;
}

int shs_process_lock_reset(shs_process_lock_t *process_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

	return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);
	
    if ((error->syscall_errno = pthread_mutex_init(&process_lock->lock, &process_lock->mutex_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_INIT;
		
        return SHS_LOCK_ERROR;
    }

    return SHS_LOCK_OK;
}

int shs_process_lock_on(shs_process_lock_t *process_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
 
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_lock->sig_block_mask, &process_lock->sig_prev_mask)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;

    	return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_mutex_lock(&process_lock->lock))) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_LOCK;
		
    	return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_ON;
}

int shs_process_lock_try_on(shs_process_lock_t *process_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM; 
		
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_lock->sig_block_mask, &process_lock->sig_prev_mask)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;
		
        return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_mutex_trylock(&process_lock->lock))) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_TRYLOCK;

        return SHS_LOCK_OFF;
    }
    
    return SHS_LOCK_ON;
}

int shs_process_lock_off(shs_process_lock_t *process_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
    
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, process_lock->allocator);

    if ((error->syscall_errno = pthread_mutex_unlock(&process_lock->lock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_MUTEX_UNLOCK;

    	return SHS_LOCK_ERROR;
    }

    if (sigprocmask(SIG_SETMASK, &process_lock->sig_prev_mask, NULL)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;   
    	error->syscall_errno = errno;

    	return SHS_LOCK_ERROR;
    }
   
    return SHS_LOCK_OFF;
}

shs_atomic_lock_t * shs_atomic_lock_create(shs_mem_allocator_t *allocator, shs_lock_errno_t *error)
{
    shs_atomic_lock_t *atomic_lock = NULL; 	
    
    if (!error) 
    {
        return NULL;
    }

    SHS_LOCK_ERRNO_CLEAN(error);
    SHS_LOCK_ERRNO_ALLOCATOR(error, allocator);

    if (!allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return NULL;
    }
    
    atomic_lock = (shs_atomic_lock_t *)allocator->alloc(allocator, sizeof(shs_atomic_lock_t), &error->allocator_errno);
    if (!atomic_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;

        return NULL;
    }
	
    atomic_lock->allocator = allocator; 
    atomic_lock->lock = SHS_LOCK_OFF; 
    
    return atomic_lock;
}

int shs_atomic_lock_init(shs_atomic_lock_t *atomic_lock)
{
    atomic_lock->lock = SHS_LOCK_OFF; 
    
    return SHS_LOCK_OK;
}

int shs_atomic_lock_release(shs_atomic_lock_t *atomic_lock, shs_lock_errno_t *error)
{
    shs_mem_allocator_t *allocator = NULL;

    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!atomic_lock || !atomic_lock->allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);

    allocator = atomic_lock->allocator; 
    if (allocator->free(allocator, atomic_lock, &error->allocator_errno) == SHS_MEM_ALLOCATOR_ERROR) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;

        return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_OK;
}

int shs_atomic_lock_reset(shs_atomic_lock_t *atomic_lock, shs_lock_errno_t *error) 
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!atomic_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    atomic_lock->lock = SHS_LOCK_OFF;

    return SHS_LOCK_OK;
}

int shs_atomic_lock_try_on(shs_atomic_lock_t *atomic_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!atomic_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);

    return atomic_lock->lock == SHS_LOCK_OFF
        && CAS(&atomic_lock->lock, SHS_LOCK_OFF, SHS_LOCK_ON) ? SHS_LOCK_ON : SHS_LOCK_OFF;
}

int shs_atomic_lock_on(shs_atomic_lock_t *atomic_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);
	
    if (!atomic_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    
    while (!CAS(&atomic_lock->lock, SHS_LOCK_OFF, SHS_LOCK_ON));
    
    return SHS_LOCK_ON;
}

int shs_atomic_lock_off(shs_atomic_lock_t *atomic_lock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!atomic_lock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;
		
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, atomic_lock->allocator);
    
    if (atomic_lock->lock == SHS_LOCK_OFF) 
    {
    	return SHS_LOCK_OFF;
    }
    
    while (!CAS(&atomic_lock->lock, SHS_LOCK_ON, SHS_LOCK_OFF));
    
    return SHS_LOCK_OFF;
}

shs_process_rwlock_t* shs_process_rwlock_create(shs_mem_allocator_t *allocator, shs_lock_errno_t *error)
{
    shs_process_rwlock_t *process_rwlock = NULL;

    if (!error) 
    {
        return NULL;
    }

    SHS_LOCK_ERRNO_CLEAN(error);
    SHS_LOCK_ERRNO_ALLOCATOR(error, allocator);

    if (!allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return NULL;
    }

    process_rwlock = (shs_process_rwlock_t *)allocator->alloc(allocator, sizeof(shs_process_rwlock_t), &error->allocator_errno);
    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;

        return NULL;
    }

    if ((error->syscall_errno = pthread_rwlockattr_init(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT;
		
        return NULL;
    }

    if ((error->syscall_errno = pthread_rwlockattr_setpshared(&process_rwlock->rwlock_attr, PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED;
		
        return NULL;
    }

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock, &process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_INIT;

        return NULL;
    }
    
    process_rwlock->allocator = allocator;

    sigfillset(&process_rwlock->sig_block_mask);
    sigdelset(&process_rwlock->sig_block_mask, SIGALRM);
    sigdelset(&process_rwlock->sig_block_mask, SIGINT);
    sigdelset(&process_rwlock->sig_block_mask, SIGCHLD);
    sigdelset(&process_rwlock->sig_block_mask, SIGPIPE);
    sigdelset(&process_rwlock->sig_block_mask, SIGSEGV);
    sigdelset(&process_rwlock->sig_block_mask, SIGHUP);
    sigdelset(&process_rwlock->sig_block_mask, SIGQUIT);
    sigdelset(&process_rwlock->sig_block_mask, SIGTERM);
    sigdelset(&process_rwlock->sig_block_mask, SIGIO);
    sigdelset(&process_rwlock->sig_block_mask, SIGUSR1);

    return process_rwlock;    
}

int shs_process_rwlock_release(shs_process_rwlock_t* process_rwlock, shs_lock_errno_t *error)
{
    shs_mem_allocator_t *allocator = NULL;
	
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
   
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_rwlock || !process_rwlock->allocator) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;

        return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_rwlockattr_destroy(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY;
		
        return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_rwlock_destroy(&process_rwlock->rwlock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY;
		
    	return SHS_LOCK_ERROR;
    }

    allocator = process_rwlock->allocator; 
    if (allocator->free(allocator, process_rwlock, &error->allocator_errno) == SHS_MEM_ALLOCATOR_ERROR) 
    {
        error->lock_errno = SHS_LOCK_ERR_ALLOCATOR_ERROR;
		
        return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_OK;
}

int shs_process_rwlock_reset(shs_process_rwlock_t *process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_CLEAN(error);
	
    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock, &process_rwlock->rwlock_attr))) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_INIT;
		
        return SHS_LOCK_ERROR;
    }

    return SHS_LOCK_OK;
}

int shs_process_rwlock_read_on(shs_process_rwlock_t *process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask, &process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;

    	return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_rwlock_rdlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_RDLOCK;

    	return SHS_LOCK_ERROR;
    }

    return SHS_LOCK_ON;
}

int shs_process_rwlock_off(shs_process_rwlock_t *process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
    
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);
  
    if ((error->syscall_errno = pthread_rwlock_unlock(&process_rwlock->rwlock)))
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_UNLOCK;

    	return SHS_LOCK_ERROR;
    }

    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_OFF;
}

int shs_process_rwlock_write_on(shs_process_rwlock_t *process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
    
    SHS_LOCK_ERRNO_CLEAN(error);
	
    error->lock_errno = SHS_LOCK_ERR_NONE;

    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }
	
    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask, &process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;
		
    	return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_rwlock_wrlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_WRLOCK;

    	return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_ON;
}

int shs_process_rwlock_write_try_on(shs_process_rwlock_t *process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }
   
    SHS_LOCK_ERRNO_CLEAN(error);

    if (!process_rwlock) 
    {
        error->lock_errno = SHS_LOCK_ERR_PARAM;

        return SHS_LOCK_ERROR;
    }

    SHS_LOCK_ERRNO_ALLOCATOR(error, process_rwlock->allocator);

    if (sigprocmask(SIG_BLOCK, &process_rwlock->sig_block_mask, &process_rwlock->sig_prev_mask)) 
    {
    	error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
    	error->syscall_errno = errno;

    	return SHS_LOCK_ERROR;
    }
   
    if ((error->syscall_errno = pthread_rwlock_trywrlock(&process_rwlock->rwlock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK;
		
    	return SHS_LOCK_ERROR;
    }
    
    return SHS_LOCK_ON;
}

const char* shs_lock_strerror(shs_lock_errno_t *error)
{
    if (!error) 
    {
        return shs_lock_err_string[0];
    }

    if (error->lock_errno <= SHS_LOCK_ERR_START || error->lock_errno >= SHS_LOCK_ERR_END) 
    {
        return shs_lock_err_string[0];
    }

    if (error->lock_errno == SHS_LOCK_ERR_ALLOCATOR_ERROR
        && error->err_allocator && error->err_allocator->strerror) 
    {
        return error->err_allocator->strerror(error->err_allocator,
            &error->allocator_errno);
    }
	
    if (error->lock_errno >= SHS_LOCK_ERR_SYSCALL_MUTEXATTR_INIT
        && error->lock_errno <= SHS_LOCK_ERR_SYSCALL_RWLOCK_TRYWRLOCK) 
    {
        return strerror(error->syscall_errno);
    }

    return shs_lock_err_string[error->lock_errno - SHS_LOCK_ERR_START];
}

inline void shs_atomic_lock_off_force(shs_atomic_lock_t *atomic_lock)
{
    atomic_lock->lock = SHS_LOCK_OFF;
}

shs_atomic_lock_t * shs_atomic_lock_pool_create(pool_t *pool)
{
    shs_atomic_lock_t *atomic_lock = NULL;

    if (!pool) 
    {
        return NULL;
    }

    atomic_lock = (shs_atomic_lock_t *)pool_alloc(pool, sizeof(shs_atomic_lock_t));
    if (!atomic_lock) 
    {
        return NULL;
    }

    atomic_lock->lock = SHS_LOCK_OFF; 

    return atomic_lock;
}

int shs_process_rwlock_init(shs_process_rwlock_t * process_rwlock, shs_lock_errno_t *error)
{
    if (!error) 
    {
        return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_rwlockattr_init(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_INIT;

        return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_rwlockattr_setpshared(&process_rwlock->rwlock_attr, PTHREAD_PROCESS_SHARED))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_SETPSHARED;

        return SHS_LOCK_ERROR;
    }

    if ((error->syscall_errno = pthread_rwlock_init(&process_rwlock->rwlock, &process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_INIT;

        return SHS_LOCK_ERROR;
    }
    
    sigfillset(&process_rwlock->sig_block_mask);
    sigdelset(&process_rwlock->sig_block_mask, SIGALRM);
    sigdelset(&process_rwlock->sig_block_mask, SIGINT);
    sigdelset(&process_rwlock->sig_block_mask, SIGCHLD);
    sigdelset(&process_rwlock->sig_block_mask, SIGPIPE);
    sigdelset(&process_rwlock->sig_block_mask, SIGSEGV);
    sigdelset(&process_rwlock->sig_block_mask, SIGHUP);
    sigdelset(&process_rwlock->sig_block_mask, SIGQUIT);
    sigdelset(&process_rwlock->sig_block_mask, SIGTERM);
    sigdelset(&process_rwlock->sig_block_mask, SIGIO);
    sigdelset(&process_rwlock->sig_block_mask, SIGUSR1);

    return SHS_LOCK_OK;    
}

int shs_process_rwlock_destroy(shs_process_rwlock_t* process_rwlock, shs_lock_errno_t *error)
{
    if (sigprocmask(SIG_SETMASK, &process_rwlock->sig_prev_mask, NULL)) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_SIGPROCMASK;
        error->syscall_errno = errno;

        return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_rwlockattr_destroy(&process_rwlock->rwlock_attr))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCKATTR_DESTROY;

        return SHS_LOCK_ERROR;
    }
    
    if ((error->syscall_errno = pthread_rwlock_destroy(&process_rwlock->rwlock))) 
    {
        error->lock_errno = SHS_LOCK_ERR_SYSCALL_RWLOCK_DESTROY;

    	return SHS_LOCK_ERROR;
    }
      
    return SHS_LOCK_OK;
}

