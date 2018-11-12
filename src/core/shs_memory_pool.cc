#include "shs_memory_pool.h"
#include "shs_memory.h"

static void *pool_alloc_block(pool_t *pool, size_t size);
static void *pool_alloc_large(pool_t *pool, size_t size);

pool_t * pool_create(size_t size, size_t max_size)
{
    pool_t *p = NULL;

    if (0 == size) 
    {
        return NULL;
    }
	
    if (size < sizeof(pool_t)) 
    {
        size += sizeof(pool_t);
    }
	
    p = (pool_t *)memory_alloc(size);
    if (!p) 
    {
        return NULL;
    }
	
    p->data.last = (uchar_t *) p + sizeof(pool_t);
    p->data.end = (uchar_t *) p + size;
    p->data.next = NULL;
    
    size -= sizeof(pool_t);
    p->max = (size < max_size) ? size : max_size;
    p->current = p;
    p->large = NULL;   

    return p;
}

void pool_reset(pool_t *pool)
{
    pool_t       *p = NULL;
    pool_large_t *l = NULL;
    
    if (!pool) 
    {
        return;
    }

    for (l = pool->large; l; l = l->next) 
    {
        if (l->alloc) 
        {
            memory_free(l->alloc, l->size);
            l->alloc = NULL;
        }
    }
	
    p = pool;
    p->current = p;

    while (p) 
    {
        p->data.last = (uchar_t *)p + sizeof(pool_t);
        p = p->data.next;
    }
}

void pool_destroy(pool_t *pool)
{
    pool_t       *p = NULL;
    pool_t       *n = NULL;
    pool_large_t *l = NULL;

    if (!pool) 
    {
        return;
    }

    for (l = pool->large; l; l = l->next) 
    {
        if (l->alloc) 
        {
            memory_free(l->alloc, l->size);
        }
    }

    for (p = pool, n = pool->data.next; ; p = n, n = n->data.next) 
    {
        memory_free(p, ((char *)p->data.end) - ((char *)p));
        if (!n) 
        {
            break;
        }
    }
}

void * pool_alloc(pool_t *pool, size_t size)
{
    pool_t  *p = NULL;
    uchar_t *m = NULL;

    if (!pool || (0 == size)) 
    {
        return NULL;
    }
	
    if (size <= pool->max) 
    {
        p = pool->current;
		
        while (p) 
        {
            m = (uchar_t *)shs_align_ptr(p->data.last, SHS_ALIGNMENT);
            if ((m < p->data.end) && ((size_t) (p->data.end - m) >= size)) 
            {
                p->data.last = m + size;
				
                return m;
            }
			
            p = p->data.next;
        }
		
        return pool_alloc_block(pool, size);
    }
	
    return pool_alloc_large(pool, size);
}

void * pool_calloc(pool_t *pool, size_t size)
{
    void *p = NULL;

    p = pool_alloc(pool, size);
    if (p) 
    {
        memory_zero(p, size);
    }

    return p;
}

static void * pool_alloc_block(pool_t *pool, size_t size)
{
    uchar_t  *nm = NULL;
    size_t    psize = 0;
    pool_t   *p = NULL;
    pool_t   *np = NULL;
    pool_t   *current = NULL;

    if (!pool || 0 == size) 
    {
        return NULL;
    }

    psize = (size_t) (pool->data.end - (uchar_t *) pool);
    
    nm = (uchar_t *)memory_alloc(psize);
    if (!nm) 
    {
        return NULL;
    }
    np = (pool_t *) nm;
    np->data.end = nm + psize;
    np->data.next = NULL;
    nm += sizeof(pool_data_t);
    nm = shs_align_ptr(nm, SHS_ALIGNMENT);
    np->data.last = nm + size;
    
    for (p = pool->current; p->data.next; p = p->data.next) 
    {
        if ((size_t)(p->data.end - p->data.last) < SHS_ALIGNMENT) 
        {
            current = p->data.next;
        }
    }
    p->data.next = np;
    pool->current = current ? current : np;
    
    return nm;
}

static void * pool_alloc_large(pool_t *pool, size_t size)
{
    void         *p = NULL;
    pool_large_t *large = NULL;

    if (!pool || (0 == size)) 
    {
        return NULL;
    }
    
    p = memory_alloc(size);
    if (!p ) 
    {
        return NULL;
    }

    large = (pool_large_t *)pool_alloc(pool, sizeof(pool_large_t));
    if (!large) 
    {
        memory_free(p, size);

        return NULL;
    }

    large->alloc = p;
    large->size = size;
    large->next = pool->large;
    pool->large = large;
    
    return p;
}

void * pool_memalign(pool_t *pool, size_t size, size_t alignment)
{
    void         *p = NULL;
    pool_large_t *large = NULL;

    p = memory_memalign(alignment, size);
    if (!p) 
    {
        return NULL;
    }
	
    large = (pool_large_t *)pool_alloc(pool, sizeof(pool_large_t));
    if (!large) 
    {
        memory_free(p, size);

        return NULL;
    }
	
    large->alloc = p;
    large->size = size;
    large->next = pool->large;
    pool->large = large;
    
    return p;
}

size_t shs_align(size_t d, uint32_t a)
{
    if (d % a) 
    {
        d += a - (d % a);
    }

    return d;
}

