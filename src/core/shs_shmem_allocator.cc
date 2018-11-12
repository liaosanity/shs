#include "shs_shmem_allocator.h"
#include "shs_shmem.h"
#include "shs_memory.h"

static int shs_shmem_allocator_init(shs_mem_allocator_t *me, 
    void *param_data);
static int shs_shmem_allocator_release(shs_mem_allocator_t *me, 
    void *param_data);
static void * shs_shmem_allocator_alloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data);
static void * shs_shmem_allocator_calloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data);
static void * shs_shmem_allocator_split_alloc(shs_mem_allocator_t *me, 
    size_t *act_size, size_t req_minsize, void *param_data);
static int shs_shmem_allocator_free(shs_mem_allocator_t *me, 
    void *ptr, void *param_data);
static const char * shs_shmem_allocator_strerror(shs_mem_allocator_t *me, 
    void *err_data);
static int shs_shmem_allocator_stat(shs_mem_allocator_t *me, 
    void *stat_data);

static shs_mem_allocator_t shs_shmem_allocator; 
//static shs_mem_allocator_t shs_shmem_allocator = 
//{
//    .private_data   = NULL,
//    .type           = SHS_MEM_ALLOCATOR_TYPE_SHMEM,
//    .init           = shs_shmem_allocator_init,
//    .release        = shs_shmem_allocator_release,
//    .alloc          = shs_shmem_allocator_alloc,
//    .calloc         = shs_shmem_allocator_calloc,
//    .split_alloc    = shs_shmem_allocator_split_alloc,
//    .free           = shs_shmem_allocator_free,
//    .strerror       = shs_shmem_allocator_strerror,
//    .stat           = shs_shmem_allocator_stat
//};

static int shs_shmem_allocator_init(shs_mem_allocator_t *me, void *param_data)
{
    shs_shmem_allocator_param_t *param =
        (shs_shmem_allocator_param_t *)param_data;
         
    if (!me || !param) 
    {
        return SHS_MEM_ALLOCATOR_ERROR; 
    }

    if (me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_OK;
    }
    
    me->private_data = shs_shmem_create(param->size, param->min_size,
         param->max_size, param->level_type, param->factor, &param->err_no); 
    if (!me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }
	
    return SHS_MEM_ALLOCATOR_OK;
}

static int shs_shmem_allocator_release(shs_mem_allocator_t *me, 
	                                              void *param_data)
{
    shs_shmem_allocator_param_t  *param = NULL;
    shs_shmem_t                 **shmem = NULL;

    param = (shs_shmem_allocator_param_t *)param_data;

    if (!me || !me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }
    
    shmem = (shs_shmem_t **)&me->private_data;
    if (shs_shmem_release(shmem, &param->err_no) == SHS_SHMEM_ERROR) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    return SHS_MEM_ALLOCATOR_OK;
}

static void * shs_shmem_allocator_alloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data)
{
    unsigned int  shmem_errno = -1;
    shs_shmem_t  *shmem = NULL;
    void         *ptr = NULL;

    if (!me || !me->private_data) 
    {
        return NULL;
    }
	
    shmem = (shs_shmem_t *)me->private_data;
    ptr = shs_shmem_alloc(shmem, size, &shmem_errno);
    if (!ptr) 
    {
        if (param_data) 
        {
            *(unsigned int *)param_data = shmem_errno;
        }
    }

    return ptr;
}

static void * shs_shmem_allocator_calloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data)
{
    void *ptr = NULL;
	
    if (!me) 
    {
        return NULL;
    }
	
    ptr = me->alloc(me, size, param_data);
    if (ptr) 
    {
        memory_zero(ptr, size);
    }

    return ptr;
}

static void * shs_shmem_allocator_split_alloc(shs_mem_allocator_t *me, 
    size_t *act_size, size_t req_minsize, void *param_data)
{
    shs_shmem_t  *shmem = NULL;
    unsigned int *shmem_errno = NULL;

    if (!me || !me->private_data || !param_data) 
    {
        return NULL;
    }

    shmem = (shs_shmem_t *)me->private_data;
    shmem_errno = (unsigned int *)param_data;

    return shs_shmem_split_alloc(shmem, act_size, req_minsize, shmem_errno);
}

static int shs_shmem_allocator_free(shs_mem_allocator_t *me, 
    void *ptr, void *param_data)
{
    unsigned int  shmem_errno = -1;
    shs_shmem_t  *shmem = NULL;  

    if (!me || !me->private_data || !ptr) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    shmem = (shs_shmem_t *)me->private_data;
    shs_shmem_free(shmem, ptr, &shmem_errno);

    if (shmem_errno != SHS_SHMEM_ERR_NONE) 
    {
        if (param_data) 
        {
            *(unsigned int *)param_data = shmem_errno;
        }

        return SHS_MEM_ALLOCATOR_ERROR;
    }

    return SHS_MEM_ALLOCATOR_OK;
}

static const char * shs_shmem_allocator_strerror(shs_mem_allocator_t *me, 
    void *err_data)
{
    if (!me || !err_data) 
    {
        return NULL;
    }
	
    return shs_shmem_strerror(*(unsigned int *)err_data); 
}

static int shs_shmem_allocator_stat(shs_mem_allocator_t *me, void *stat_data)
{
    shs_shmem_stat_t *stat = (shs_shmem_stat_t *)stat_data;
    shs_shmem_t      *shmem = NULL;

    if (!me || !me->private_data || !stat) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }
   
    shmem = (shs_shmem_t *)me->private_data;
    if (shs_shmem_get_stat(shmem, stat) == SHS_SHMEM_ERROR) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    return SHS_MEM_ALLOCATOR_OK;
}

const shs_mem_allocator_t * shs_get_shmem_allocator(void)
{
    shs_shmem_allocator.private_data   = NULL;
    shs_shmem_allocator.type           = SHS_MEM_ALLOCATOR_TYPE_SHMEM;
    shs_shmem_allocator.init           = shs_shmem_allocator_init;
    shs_shmem_allocator.release        = shs_shmem_allocator_release;
    shs_shmem_allocator.alloc          = shs_shmem_allocator_alloc;
    shs_shmem_allocator.calloc         = shs_shmem_allocator_calloc;
    shs_shmem_allocator.split_alloc    = shs_shmem_allocator_split_alloc;
    shs_shmem_allocator.free           = shs_shmem_allocator_free;
    shs_shmem_allocator.strerror       = shs_shmem_allocator_strerror;
    shs_shmem_allocator.stat           = shs_shmem_allocator_stat;

    return &shs_shmem_allocator;
}

