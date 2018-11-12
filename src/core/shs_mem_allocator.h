#ifndef SHS_MEM_ALLOCATOR_H
#define SHS_MEM_ALLOCATOR_H

#include <stdlib.h>

typedef struct shs_mem_allocator_s shs_mem_allocator_t;
typedef int   (*shs_init_ptr_t)     (shs_mem_allocator_t *me, void *param_data);
typedef int   (*shs_release_ptr_t)  (shs_mem_allocator_t *me, void *param_data);
typedef void* (*shs_alloc_ptr_t)    (shs_mem_allocator_t *me, size_t size,
    void *param_data);
typedef void* (*shs_calloc_ptr_t)   (shs_mem_allocator_t *me, size_t size,
    void *param_data);
typedef void* (*shs_split_alloc_ptr_t) (shs_mem_allocator_t *me, size_t *act_size,
    size_t req_minsize, void *param_data);
typedef int  (*shs_free_ptr_t)     (shs_mem_allocator_t *me, void *ptr,
    void *param_data);
typedef const char* (*shs_strerror_ptr_t) (shs_mem_allocator_t *me,
    void *err_data);
typedef int (*shs_stat_ptr_t) (shs_mem_allocator_t *me, void *stat_data);

struct shs_mem_allocator_s 
{
    void                 *private_data;
    int                   type;
    shs_init_ptr_t        init;
    shs_release_ptr_t     release;
    shs_alloc_ptr_t       alloc;
    shs_calloc_ptr_t      calloc;
    shs_split_alloc_ptr_t split_alloc;
    shs_free_ptr_t        free;
    shs_strerror_ptr_t    strerror;
    shs_stat_ptr_t        stat;
};

enum
{
    SHS_MEM_ALLOCATOR_TYPE_SHMEM = 1,
    SHS_MEM_ALLOCATOR_TYPE_MEMPOOL,
    SHS_MEM_ALLOCATOR_TYPE_COMMPOOL,
};

#define SHS_MEM_ALLOCATOR_OK    (0)
#define SHS_MEM_ALLOCATOR_ERROR (-1)

extern const shs_mem_allocator_t *shs_get_mempool_allocator(void);
extern const shs_mem_allocator_t *shs_get_shmem_allocator(void);
extern const shs_mem_allocator_t *shs_get_commpool_allocator(void);
shs_mem_allocator_t *shs_mem_allocator_new(int allocator_type);
shs_mem_allocator_t *shs_mem_allocator_new_init(int allocator_type, 
    void *init_param);
void shs_mem_allocator_delete(shs_mem_allocator_t *allocator);

#endif

