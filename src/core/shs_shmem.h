#ifndef SHS_SHMEM_H
#define SHS_SHMEM_H

#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "shs_queue.h"
#include "shs_string.h"

#define SHS_SHMEM_DEFAULT_MAX_SIZE  ((size_t)10 << 20) // 10MB
#define SHS_SHMEM_DEFAULT_MIN_SIZE  ((size_t)1024)

#define SHS_SHMEM_STORAGE_SIZE sizeof(struct storage)

#define SHS_SHMEM_INVALID_SPLIT_THRESHOLD       (0)
#define SHS_SHMEM_DEFAULT_SPLIT_THRESHOLD       (64)
#define SHS_SHMEM_MEM_LEVEL                     (32)
#define SHS_SHMEM_LEVEL_TYPE_LINEAR             (0)
#define SHS_SHMEM_LEVEL_TYPE_EXP                (1)
#define SHS_SHMEM_EXP_FACTOR                     2
#define SHS_SHMEM_LINEAR_FACTOR                  1024

#define DEFAULT_ALIGNMENT_MASK  (1)
#define DEFAULT_ALIGNMENT_SIZE  (1 << DEFAULT_ALIGNMENT_MASK)

#define SHS_SHMEM_ERROR                    (-1)
#define SHS_SHMEM_OK                       (0)

enum 
{
    SHS_SHMEM_ERR_NONE = 0,
    SHS_SHMEM_ERR_START = 100,
    SHS_SHMEM_ERR_CREATE_SIZE,
    SHS_SHMEM_ERR_CREATE_MINSIZE,
    SHS_SHMEM_ERR_CREATE_LEVELTYPE,
    SHS_SHMEM_ERR_CREATE_FREELEN,
    SHS_SHMEM_ERR_CREATE_TOTALSIZE,
    SHS_SHMEM_ERR_CREATE_TOTALSIZE_NOT_ENOUGH,
    SHS_SHMEM_ERR_CREATE_MMAP,
    SHS_SHMEM_ERR_CREATE_STORAGESIZE,
    SHS_SHMEM_ERR_RELEASE_NULL,
    SHS_SHMEM_ERR_RELEASE_MUNMAP,
    SHS_SHMEM_ERR_ALLOC_PARAM,
    SHS_SHMEM_ERR_ALLOC_EXHAUSTED,
    SHS_SHMEM_ERR_ALLOC_MAX_AVAILABLE_EMPTY,
    SHS_SHMEM_ERR_ALLOC_NO_AVAILABLE_FREE_LIST,
    SHS_SHMEM_ERR_ALLOC_NO_FIXED_FREE_SPACE,
    SHS_SHMEM_ERR_ALLOC_FOUND_NO_FIXED,
    SHS_SHMEM_ERR_ALLOC_REMOVE_FREE,
    SHS_SHMEM_ERR_GET_MAX_PARAM,
    SHS_SHMEM_ERR_GET_MAX_EXHAUSTED,
    SHS_SHMEM_ERR_GET_MAX_CRITICAL,
    SHS_SHMEM_ERR_SPLIT_ALLOC_PARAM,
    SHS_SHMEM_ERR_SPLIT_ALLOC_NO_FIXED_REQ_MINSIZE,
    SHS_SHMEM_ERR_SPLIT_ALLOC_REMOVE_FREE,
    SHS_SHMEM_ERR_FREE_PARAM,
    SHS_SHMEM_ERR_FREE_NONALLOCED,
    SHS_SHMEM_ERR_FREE_REMOVE_NEXT,
    SHS_SHMEM_ERR_FREE_REMOVE_PREV,
    SHS_SHMEM_ERR_END
};

struct storage
{
    queue_t       order_entry;
    queue_t       free_entry;
    unsigned int  alloc;
    size_t        size;
    size_t        act_size;
    queue_t      *free_list_head;
};

struct free_node
{
    queue_t      available_entry;
    queue_t      free_list_head; 
    unsigned int index;
};

typedef struct shs_shmem_stat_s 
{
    size_t used_size;
    size_t reqs_size;
    size_t st_count;
    size_t st_size;
    size_t total_size;
    size_t system_size;
    size_t failed;
    size_t split_failed;
    size_t split;
} shs_shmem_stat_t;

typedef struct shs_shmem_s 
{
    queue_t           order;  // all block list include free and alloced
    struct free_node *free;   // free array from 0 to free_len - 1
    size_t            free_len; // length of free array
    queue_t           available; // the list of non-empty free_list
    size_t            max_available_index; //the id of last element in available
    size_t            min_size; // the size of free[0]
    size_t            max_size; // the size of free[free_len - 1]
    size_t            factor;  // increament step
    size_t            split_threshold; // threshold size for split default to min_size
    int               level_type;// increament type                    
    shs_shmem_stat_t  shmem_stat;// stat for shmem
} shs_shmem_t;

shs_shmem_t* shs_shmem_create(size_t size, size_t min_size, 
    size_t max_size, int level_type, size_t factor, unsigned int *shmem_errno);
int shs_shmem_release(shs_shmem_t **shm, unsigned int *shmem_errno);   
inline size_t shs_shmem_set_alignment_size(size_t size);
inline size_t shs_shmem_get_alignment_size();
void * shs_shmem_alloc(shs_shmem_t *shm, size_t size, 
    unsigned int *shmem_errno);
void * shs_shmem_calloc(shs_shmem_t *shm, size_t size, 
    unsigned int *shmem_errno);
void shs_shmem_free(shs_shmem_t *shm, void *addr, unsigned int *shmem_errno);
void * shs_shmem_split_alloc(shs_shmem_t *shm, size_t *act_size,
    size_t minsize, unsigned int *shmem_errno);
void       shs_shmem_dump(shs_shmem_t *shm, FILE *out);
size_t     shs_shmem_get_used_size(shs_shmem_t *shm);
size_t     shs_shmem_get_total_size(shs_shmem_t *shm);
size_t     shs_shmem_get_system_size(shs_shmem_t *shm);
inline int shs_shmem_set_split_threshold(shs_shmem_t *shm, size_t size);
const char * shs_shmem_strerror(unsigned int shmem_errno);
int shs_shmem_get_stat(shs_shmem_t *shmem, shs_shmem_stat_t *stat);
uchar_t *shs_shm_strdup(shs_shmem_t *shm, string_t *str);

#endif

