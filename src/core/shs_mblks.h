#ifndef SHS_MBLKS_H 
#define SHS_MBLKS_H

#include <stdlib.h>

#include "shs_lock.h"

#define MEM_BLOCK_HEAD          sizeof(struct mem_data)
#define SIZEOF_PER_MEM_BLOCK(X) ((X) + MEM_BLOCK_HEAD)

#define LOCK_INIT(x) do  { \
    shs_atomic_lock_init(x); \
} while(0)

#define TRY_LOCK(x) do  { \
    shs_lock_errno_t error; \
    shs_atomic_lock_try_on((x), &error); \
} while(0)

#define LOCK(x) do {\
    shs_lock_errno_t error; \
    shs_atomic_lock_on((x), &error); \
} while(0)


#define UNLOCK(x) do { \
    shs_lock_errno_t error; \
    shs_atomic_lock_off((x), &error); \
} while(0)

#define LOCK_DESTROY(x) {}

struct mem_data 
{
    void *next;
    char  data[0];
};

typedef struct mem_mblks_param_s 
{
    void *(*mem_alloc) (void *priv, size_t size);
    void (*mem_free) (void *priv,void *mem_addr);
    void *priv;
} mem_mblks_param_t;

struct mem_mblks 
{
    int                hot_count;
    int                cold_count;
    size_t             padded_sizeof_type;
    size_t             real_sizeof_type;
    void              *start_mblks;
    void              *end_mblks;
    mem_mblks_param_t  param;
    shs_atomic_lock_t  lock;
    struct mem_data   *free_blks;
};

struct mem_mblks *mem_mblks_new_fn(size_t, int64_t, mem_mblks_param_t *);

#define mem_mblks_new(type, count, param) mem_mblks_new_fn(sizeof(type), count, param)

void *mem_get(struct mem_mblks*);
void *mem_get0(struct mem_mblks*);
void mem_put(void *);
void mem_mblks_destroy(struct mem_mblks *);

#endif

