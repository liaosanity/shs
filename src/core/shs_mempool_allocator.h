#ifndef SHS_MEMPOOL_ALLOCATOR_H
#define SHS_MEMPOOL_ALLOCATOR_H

#include "shs_mem_allocator.h"

typedef struct shs_mempool_allocator_param_s 
{
    size_t pool_size;
    size_t max_size;
} shs_mempool_allocator_param_t;

#endif

