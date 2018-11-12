#ifndef SHS_SHMEM_ALLOCATOR_H
#define SHS_SHMEM_ALLOCATOR_H

#include "shs_mem_allocator.h"

typedef struct shs_shmem_allocator_param_s 
{
    size_t       size;
    size_t       min_size;
    size_t       max_size;
    size_t       factor;
    int          level_type;
    unsigned int err_no;
} shs_shmem_allocator_param_t;

#endif

