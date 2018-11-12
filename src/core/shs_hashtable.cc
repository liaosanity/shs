#include <assert.h>
#include <math.h>

#include "shs_hashtable.h"
#include "shs_memory.h"
#include "shs_math.h"
#include "shs_string.h"
#include "shs_lock.h"

#define SHS_HASH4(x) ((x) = ((x) << 5) + (x) + *key++) 

size_t shs_hashtable_hash_hash4(const void *data, size_t data_size, 
    size_t hashtable_size)
{
    size_t      ret = 0;
    size_t      loop = data_size >> 3;
    const char *key = (const char *)data;

    if (data_size & (8 - 1)) 
    {
        SHS_HASH4(ret);
    }

    while (loop--) 
    {
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
        SHS_HASH4(ret);
    }

    return ret % hashtable_size;
}

size_t shs_hashtable_hash_key8(const void *data, size_t data_size, 
    size_t hashtable_size)
{
    int         bnum = 0;
    size_t      i = 0;
    size_t      ret = 0;
    size_t      one = 0;
    size_t      n = 0;
    size_t      loop = data_size >> 3;
    const char *s = (const char *)data;
    
    if ((bnum = (data_size % 8))) 
    {
        loop++;
    }
	
    for (i = 0; i < loop; i++) 
    {
        one = *((uint64_t *)s);
		
        if (bnum && i == loop - 1) 
        {
            one >>= (bnum * 8);
        }
		
        n ^= 271 * one; 
        s += 8;
    }

    ret = n ^ (loop * 271);
    
    return ret % hashtable_size;
}

size_t shs_hashtable_hash_low(const void *data, size_t data_size, 
    size_t hashtable_size)
{
    const char *s = (const char *)data;
    size_t      n = 0;
    size_t      j = 0;
	
    while (*s && j < data_size) 
    {
        j++;
        n = n * 31 + string_xxctolower(*s);
        s++;
    }
    
    return n % hashtable_size;
}

/*
 * hash_create - creates a new hash table, uses the cmp_func
 * to compare keys.  Returns the identification for the hash table;
 * otherwise returns NULL.
 */
shs_hashtable_t * shs_hashtable_create(SHS_HASHTABLE_CMP *cmp_func, 
    size_t hash_sz, SHS_HASHTABLE_HASH *hash_func, 
    shs_mem_allocator_t *allocator)
{
    shs_hashtable_t *ht = NULL;
    unsigned int     err_no = -1;

    if (allocator) 
    {
        ht = (shs_hashtable_t *)allocator->calloc(allocator, 
            sizeof(shs_hashtable_t), &err_no);
        ht->allocator = allocator;
    } 
    else 
    {
        ht = (shs_hashtable_t *)memory_calloc(sizeof(shs_hashtable_t));
    }
    
    if (!ht) 
    {
        return NULL;
    }
    
    if (shs_hashtable_init(ht, cmp_func, hash_sz, hash_func, 
        allocator) == SHS_HASHTABLE_OK) 
    {
        return ht;
    }
    
    if (allocator) 
    {
        allocator->free(allocator, ht, &err_no);
    } 
    else 
    {
        memory_free(ht, sizeof(shs_hashtable_t));
    }
    
    return NULL;
}

int shs_hashtable_init(shs_hashtable_t *ht, SHS_HASHTABLE_CMP *cmp_func, 
    size_t hash_sz, SHS_HASHTABLE_HASH *hash_func, shs_mem_allocator_t *allocator)
{    
    unsigned int err_no = -1;

    if (!ht) 
    {
        return SHS_HASHTABLE_ERROR;
    }

    ht->size = !hash_sz ? SHS_HASHTABLE_DEFAULT_SIZE :
        shs_math_find_prime(hash_sz);
        
    if (allocator) 
    {
        ht->allocator = allocator;
        ht->buckets = (shs_hashtable_link_t **)allocator->calloc(allocator,
            ht->size * sizeof(shs_hashtable_link_t *), &err_no);
    } 
    else 
    {
        ht->buckets = (shs_hashtable_link_t **)memory_calloc(ht->size *
            sizeof(shs_hashtable_link_t *));
        ht->allocator = NULL;
    }

    if (!ht->buckets) 
    {
        return SHS_HASHTABLE_ERROR;
    }
       
    ht->cmp = cmp_func;
    ht->hash = hash_func;
    ht->count = 0;

    return SHS_HASHTABLE_OK;
}

/*
 *  hash_join - joins a hash_link under its key lnk->key
 *  into the hash table 'ht'.  
 *
 *  It does not copy any data into the hash table,
 *  only add links pointers to the hash table.
 */
int shs_hashtable_join(shs_hashtable_t *ht, shs_hashtable_link_t *hl)
{
    size_t i = 0;
    
    if (!ht || !hl) 
    {
        return SHS_HASHTABLE_ERROR;
    }

    i = ht->hash(hl->key, hl->len, ht->size);
    hl->next = ht->buckets[i];
    ht->buckets[i] = hl;
    ht->count++;
    
    return SHS_HASHTABLE_OK;
}

/*
 *  hash_lookup - locates the item under the key 'k' in the hash table
 *  'ht'.  Returns a pointer to the hash bucket on success; otherwise
 *  returns NULL.
 */
void * shs_hashtable_lookup(shs_hashtable_t *ht, const void *key, 
    size_t len)
{
    size_t                i = 0;
    shs_hashtable_link_t *walker = NULL;
    
    if (!key || !ht) 
    {
        return NULL;
    }

    i = ht->hash(key, len, ht->size);

    for (walker = ht->buckets[i]; walker; walker = walker->next) 
    {
        if (!walker->key) 
        {
            return NULL;
        }
		
        if ((ht->cmp) (key, walker->key, len) == 0) 
        {
            return (walker);
        }
		
        assert(walker != walker->next);
    }
   
    return NULL;
}

/*
 *  hash_remove_link - deletes the given hash_link node from the 
 *  hash table 'ht'.  Does not free the item, only removes it
 *  from the list.
 *
 *  An assertion is triggered if the hash_link is not found in the
 *  list.
 */
int shs_hashtable_remove_link(shs_hashtable_t *ht, 
    shs_hashtable_link_t *hl)
{
    int                    i = 0;
    shs_hashtable_link_t **link;

    if (!ht || !hl) 
    {
        return SHS_HASHTABLE_ERROR;
    }
    
    i = ht->hash(hl->key, hl->len, ht->size);
	
    for (link = &ht->buckets[i]; *link; link = &(*link)->next) 
    {
        if (*link == hl) 
        {
            *link = hl->next;
            ht->count--;
			
            return SHS_HASHTABLE_OK;
        }
    }

    return SHS_HASHTABLE_ERROR;
}

/*
 *  hash_get_bucket - returns the head item of the bucket 
 *  in the hash table 'hid'. Otherwise, returns NULL on error.
 */
shs_hashtable_link_t * shs_hashtable_get_bucket(shs_hashtable_t *ht, 
    uint32_t bucket)
{
    if (bucket >= ht->size) 
    {
        return NULL;
    }

    return ht->buckets[bucket];
}

void shs_hashtable_free_memory(shs_hashtable_t *ht)
{
    unsigned int err_no = -1;

    if (!ht) 
    {
        return;
    }
    
    if (ht->allocator) 
    {
        if (ht->buckets) 
        {
            ht->allocator->free(ht->allocator, ht->buckets, &err_no);
        }
       
        ht->allocator->free(ht->allocator, ht, &err_no);
    } 
    else 
    {
        if (ht->buckets) 
        {
            memory_free(ht->buckets,
                ht->size * sizeof(shs_hashtable_link_t *));
        }
		
        memory_free(ht, sizeof(shs_hashtable_t));
    }
}

void shs_hashtable_free_items(shs_hashtable_t *ht, 
    void (*free_object_func)(void *), void *param)
{
    size_t                i = 0;
    shs_hashtable_link_t *walker = NULL;
    shs_hashtable_link_t *next = NULL;
    
    if (!ht || !free_object_func) 
    {
    	return;
    }

    for (i = 0; i < ht->size; i++) 
    {
        for (walker = ht->buckets[i]; walker;) 
        {
            next = walker->next;
            free_object_func(walker);
            walker = next;
            ht->count--;
        }
    }
}

int shs_hashtable_empty(shs_hashtable_t *ht)
{
    return ht && ht->count ? SHS_HASHTABLE_FALSE : SHS_HASHTABLE_TRUE;
}

