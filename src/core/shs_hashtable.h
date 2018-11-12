#ifndef SHS_HASHTABLE_H
#define SHS_HASHTABLE_H

#include "shs_lock.h"
#include "shs_queue.h"
#include "shs_mem_allocator.h"

#define  SHS_HASHTABLE_DEFAULT_SIZE       7951
#define  SHS_HASHTABLE_STORE_DEFAULT_SIZE 16777217

typedef void   SHS_HASHTABLE_FREE(void *);
typedef int    SHS_HASHTABLE_CMP(const void *, const void *, size_t);
typedef size_t SHS_HASHTABLE_HASH(const void *, size_t, size_t);

enum 
{
    SHS_HASHTABLE_COLL_LIST,
    SHS_HASHTABLE_COLL_NEXT
};

#define SHS_HASHTABLE_ERROR    -1
#define SHS_HASHTABLE_OK        0

#define SHS_HASHTABLE_FALSE     0
#define SHS_HASHTABLE_TRUE      1

typedef struct shs_hashtable_link_s shs_hashtable_link_t;

struct shs_hashtable_link_s 
{
    void                 *key;
    size_t                len;
    shs_hashtable_link_t *next;
};

typedef struct shs_hashtable_errno_s 
{
    unsigned int     hash_errno;
    unsigned int     allocator_errno;
    shs_lock_errno_t lock_errno;
} shs_hashtable_errno_t;

typedef struct shs_hashtable_s 
{
    shs_hashtable_link_t **buckets;
    SHS_HASHTABLE_CMP     *cmp;         // compare function
    SHS_HASHTABLE_HASH    *hash;        // hash function
    shs_mem_allocator_t   *allocator;   // create on shmem
    size_t                 size;        // bucket number
    int                    coll;        // collection algrithm
    int                    count;       // total element that inserted to hashtable
} shs_hashtable_t;

#define shs_hashtable_link_make(str) {(void*)(str), sizeof((str)) - 1, NULL,NULL}
#define shs_hashtable_link_null      {NULL, 0, NULL,NULL}

SHS_HASHTABLE_HASH shs_hashtable_hash_hash4;
SHS_HASHTABLE_HASH shs_hashtable_hash_key8;
SHS_HASHTABLE_HASH shs_hashtable_hash_low;

shs_hashtable_t * shs_hashtable_create(SHS_HASHTABLE_CMP *cmp_func, 
    size_t hash_sz, SHS_HASHTABLE_HASH *hash_func, shs_mem_allocator_t *allocator);
int shs_hashtable_init(shs_hashtable_t *ht, SHS_HASHTABLE_CMP *cmp_func,
    size_t hash_sz, SHS_HASHTABLE_HASH *hash_func, shs_mem_allocator_t *);
int   shs_hashtable_empty(shs_hashtable_t *ht);
int   shs_hashtable_join(shs_hashtable_t *, shs_hashtable_link_t *);
int   shs_hashtable_remove_link(shs_hashtable_t *, shs_hashtable_link_t *);
void *shs_hashtable_lookup(shs_hashtable_t *, const void *, size_t len);
void  shs_hashtable_free_memory(shs_hashtable_t *);
void shs_hashtable_free_items(shs_hashtable_t *ht,
    void (*free_object_func)(void*), void*);
shs_hashtable_link_t *shs_hashtable_get_bucket(shs_hashtable_t *, uint32_t);

#endif

