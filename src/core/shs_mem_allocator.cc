#include "shs_mem_allocator.h"
#include "shs_shmem_allocator.h"
#include "shs_mempool_allocator.h"

shs_mem_allocator_t *shs_mem_allocator_new(int allocator_type)
{
    shs_mem_allocator_t *allocator = NULL;
	
    allocator = (shs_mem_allocator_t *)malloc(sizeof(shs_mem_allocator_t));
    if (!allocator) 
    {
        return NULL;
    }
	
    switch (allocator_type) 
    {
    case SHS_MEM_ALLOCATOR_TYPE_SHMEM:
        *allocator = *shs_get_shmem_allocator();
        break;
    		
    case SHS_MEM_ALLOCATOR_TYPE_MEMPOOL:
        *allocator = *shs_get_mempool_allocator();
        break;
    		
    case SHS_MEM_ALLOCATOR_TYPE_COMMPOOL:
        *allocator = *shs_get_commpool_allocator();
 
    default:
        return NULL;
    }

    allocator->private_data = NULL;

    return allocator;
}

shs_mem_allocator_t *shs_mem_allocator_new_init(int allocator_type, void *init_param)
{
    shs_mem_allocator_t *allocator = NULL;

    allocator = (shs_mem_allocator_t *)malloc(sizeof(shs_mem_allocator_t));
    if (!allocator) 
    {
        return NULL;
    }

    switch (allocator_type) 
    {
    case SHS_MEM_ALLOCATOR_TYPE_SHMEM:
        *allocator = *shs_get_shmem_allocator();
        break;
    		
    case SHS_MEM_ALLOCATOR_TYPE_MEMPOOL:
        *allocator = *shs_get_mempool_allocator();
        break;
    		
    case SHS_MEM_ALLOCATOR_TYPE_COMMPOOL:
        *allocator = *shs_get_commpool_allocator();
        break;
    		
    default:
        return NULL;
    }
	
    allocator->private_data = NULL;
    if (init_param && allocator->init(allocator, init_param)
        == SHS_MEM_ALLOCATOR_ERROR) 
    {
        return NULL;
    }
	
    return allocator;
}

void shs_mem_allocator_delete(shs_mem_allocator_t *allocator)
{
    shs_shmem_allocator_param_t   shmem;
    shs_mempool_allocator_param_t mempool;

    if (!allocator) 
    {
        return;
    }

    if (allocator->private_data) 
    {
        switch (allocator->type) 
	{
        case SHS_MEM_ALLOCATOR_TYPE_SHMEM:
            allocator->release(allocator, &shmem);
            break;
	    		
        case SHS_MEM_ALLOCATOR_TYPE_MEMPOOL:
            allocator->release(allocator, &mempool);
            break;
	    		
        case SHS_MEM_ALLOCATOR_TYPE_COMMPOOL:
            allocator->release(allocator, NULL);
            break;
        }
    }

    free(allocator);
}

