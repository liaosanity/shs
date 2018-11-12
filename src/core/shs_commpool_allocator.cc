#include "shs_memory.h"
#include "shs_commpool.h"

static int mpool_mgmt_allocator_init(shs_mem_allocator_t *me, void *param_data)
{
    struct mpool_mgmt_param_s *param = (mpool_mgmt_param_s *)param_data;
    mpool_mgmt_t              *pool = NULL;
    
    if (!param) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    pool = mpool_mgmt_create(param);
    if (!pool) 
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    me->private_data = pool;

    return SHS_MEM_ALLOCATOR_OK;
}

static int mpool_mgmt_allocator_release(shs_mem_allocator_t *me, void *param_data)
{
    (void)param_data;

    mpool_mgmt_t *pool = (mpool_mgmt_t *)me->private_data;
    if (!pool)
    {
        return SHS_MEM_ALLOCATOR_ERROR;
    }

    destroy_mpool_mgmt(pool);
    
    return SHS_MEM_ALLOCATOR_OK;
}

static void * mpool_mgmt_allocator_alloc(shs_mem_allocator_t *me, size_t size, void *param_data)
{
    (void)param_data;

    mpool_mgmt_t *pool = (mpool_mgmt_t *)me->private_data;
    if (!pool) 
    {
        return NULL;
    }

    return mpool_alloc(pool, size);
}

static void * mpool_mgmt_allocator_calloc(shs_mem_allocator_t *me, size_t size, void *param_data)
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

static int mpool_mgmt_allocator_free(shs_mem_allocator_t *me, void *ptr, void *param_data) 
{
    (void)me;
    (void)ptr;
    (void)param_data;

    return SHS_MEM_ALLOCATOR_OK;
}

//static shs_mem_allocator_t shs_mpool_mgmt = 
//{
//    .private_data = NULL,
//    .type         = SHS_MEM_ALLOCATOR_TYPE_COMMPOOL,
//    .init         = mpool_mgmt_allocator_init,
//    .release      = mpool_mgmt_allocator_release,
//    .alloc        = mpool_mgmt_allocator_alloc,
//    .calloc       = mpool_mgmt_allocator_calloc,
//    .free         = mpool_mgmt_allocator_free,
//};

static shs_mem_allocator_t shs_mpool_mgmt; 

const shs_mem_allocator_t *shs_get_commpool_allocator(void)
{
    shs_mpool_mgmt.private_data = NULL;
    shs_mpool_mgmt.type         = SHS_MEM_ALLOCATOR_TYPE_COMMPOOL;
    shs_mpool_mgmt.init         = mpool_mgmt_allocator_init;
    shs_mpool_mgmt.release      = mpool_mgmt_allocator_release;
    shs_mpool_mgmt.alloc        = mpool_mgmt_allocator_alloc;
    shs_mpool_mgmt.calloc       = mpool_mgmt_allocator_calloc;
    shs_mpool_mgmt.free         = mpool_mgmt_allocator_free;

    return &shs_mpool_mgmt;
}

