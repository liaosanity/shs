#ifndef SHS_MEMORY_POOL_H
#define SHS_MEMORY_POOL_H

#include "shs_types.h"

typedef struct pool_large_s  pool_large_t;

struct pool_large_s 
{
    void         *alloc;
    size_t        size;
    pool_large_t *next;
};

typedef struct pool_data_s 
{
    uchar_t *last;
    uchar_t *end;
    pool_t  *next;
} pool_data_t;

struct pool_s 
{
    pool_data_t   data;    // pool's list used status(used for pool alloc)
    size_t        max;     // max size pool can alloc
    pool_t       *current; // current pool of pool's list
    pool_large_t *large;   // large memory of pool
};

pool_t *pool_create(size_t size, size_t max_size);
void    pool_destroy(pool_t *pool);
void   *pool_alloc(pool_t *pool, size_t size);
void   *pool_calloc(pool_t *pool, size_t size);
void   *pool_memalign(pool_t *pool, size_t size, size_t alignment);
void    pool_reset(pool_t *pool);
size_t  shs_align(size_t d, uint32_t a);

#endif

