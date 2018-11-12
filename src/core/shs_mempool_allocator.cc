#include "shs_mempool_allocator.h"
#include "shs_memory_pool.h"
#include "shs_memory.h"

static int shs_mempool_allocator_init(shs_mem_allocator_t *me, 
    void *param_data);
static int shs_mempool_allocator_release(shs_mem_allocator_t *me, 
    void *param_data);
static void * shs_mempool_allocator_alloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data);
static void *shs_mempool_allocator_calloc(shs_mem_allocator_t *me,
    size_t size, void *param_data);
static const char * shs_mempool_allocator_strerror(shs_mem_allocator_t *me, 
    void *err_data);

static shs_mem_allocator_t shs_mempool_allocator;  
//static shs_mem_allocator_t shs_mempool_allocator = 
//{
//    .private_data = NULL,
//    .type         = SHS_MEM_ALLOCATOR_TYPE_MEMPOOL,
//    .init         = shs_mempool_allocator_init,
//    .release      = shs_mempool_allocator_release,
//    .alloc        = shs_mempool_allocator_alloc,
//    .calloc       = shs_mempool_allocator_calloc,
//    .free         = NULL,
//    .strerror     = shs_mempool_allocator_strerror,
//    .stat         = NULL
//};

static int shs_mempool_allocator_init(shs_mem_allocator_t *me, 
    void *param_data)
{
    shs_mempool_allocator_param_t *param = NULL;

    if (!me || !param_data) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    if (me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_OK;
    }
	
    param = (shs_mempool_allocator_param_t *)param_data;
    me->private_data = pool_create(param->pool_size, param->max_size);
    if (!me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }
	
    return SHS_MEM_ALLOCATOR_OK;
}

static int shs_mempool_allocator_release(shs_mem_allocator_t *me, 
    void *param_data)
{
    pool_t *pool = NULL;

    if (!me || !me->private_data) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    pool = (pool_t *)me->private_data;
    pool_destroy(pool);
    me->private_data = NULL;

    return SHS_MEM_ALLOCATOR_OK;
}

static void * shs_mempool_allocator_alloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data)
{
    pool_t *pool = NULL;

    if (!me || !me->private_data) 
    {
        return NULL;
    }

    pool = (pool_t *)me->private_data;

    return pool_alloc(pool, size);
}

static void * shs_mempool_allocator_calloc(shs_mem_allocator_t *me, 
    size_t size, void *param_data)
{
    void *ptr = NULL;

    ptr = me->alloc(me, size, param_data); 
    if (ptr) 
    {
        memory_zero(ptr, size);
    }

    return ptr;
}

static const char * shs_mempool_allocator_strerror(shs_mem_allocator_t *me, 
    void *err_data)
{
    return "no more error info";
}

const shs_mem_allocator_t * shs_get_mempool_allocator(void)
{
    shs_mempool_allocator.private_data = NULL;
    shs_mempool_allocator.type         = SHS_MEM_ALLOCATOR_TYPE_MEMPOOL;
    shs_mempool_allocator.init         = shs_mempool_allocator_init;
    shs_mempool_allocator.release      = shs_mempool_allocator_release;
    shs_mempool_allocator.alloc        = shs_mempool_allocator_alloc;
    shs_mempool_allocator.calloc       = shs_mempool_allocator_calloc;
    shs_mempool_allocator.free         = NULL;
    shs_mempool_allocator.strerror     = shs_mempool_allocator_strerror;
    shs_mempool_allocator.stat         = NULL;

    return &shs_mempool_allocator;
}

