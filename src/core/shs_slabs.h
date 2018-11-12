#ifndef SHS_SLABS_H
#define SHS_SLABS_H

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "shs_shmem_allocator.h"

//#define SHS_SLAB_DEFAULT_MAX_SIZE ((size_t)10 << 20)     // 10MB
#define SHS_SLAB_DEFAULT_MAX_SIZE  ((size_t)10485760) // 10MB
#define SHS_SLAB_DEFAULT_MIN_SIZE  ((size_t)1024)     // KB
#define SHS_SLAB_MAX_SIZE_PADDING  ((size_t)1 << 20)    // MB

enum 
{
    SHS_SLAB_ERR_NONE = 0,
    SHS_SLAB_ERR_START = 100,
    SHS_SLAB_ERR_CREATE_PARAM,
    SHS_SLAB_ERR_CREATE_POWER_FACTOR,
    SHS_SLAB_ERR_CREATE_LINER_FACTOR,
    SHS_SLAB_ERR_CREATE_UPTYPE,
    SHS_SLAB_ERR_RELEASE_PARAM,
    SHS_SLAB_ERR_ALLOC_PARAM,
    SHS_SLAB_ERR_ALLOC_INVALID_ID,
    SHS_SLAB_ERR_ALLOC_FAILED,
    SHS_SLAB_ERR_SPLIT_ALLOC_PARAM,
    SHS_SLAB_ERR_SPLIT_ALLOC_NOT_SUPPORTED,
    SHS_SLAB_ERR_SPLIT_ALLOC_CHUNK_SIZE_TOO_LARGE,
    SHS_SLAB_ERR_FREE_PARAM,
    SHS_SLAB_ERR_FREE_NOT_SUPPORTED,
    SHS_SLAB_ERR_FREE_CHUNK_ID,
    SHS_SLAB_ERR_ALLOCATOR_ERROR,
    SHS_SLAB_ERR_END
};

#define SHS_SLAB_NO_ERROR                          0
#define SHS_SLAB_ERROR_INVALID_ID                 -1
#define SHS_SLAB_ERROR_NOSPACE                    -2
#define SHS_SLAB_ERROR_UNKNOWN                    -3
#define SHS_SLAB_ERROR_ALLOC_FAILED               -4
#define SHS_SLAB_SPLIT_ID                         -5

#define	SHS_SLAB_FALSE	            0
#define	SHS_SLAB_TRUE	            1
#define SHS_SLAB_NO_FREE            2

#define SHS_SLAB_ERROR             -1
#define SHS_SLAB_OK                 0

#define SHS_SLAB_RECOVER_FACTOR     2

#define SHS_SLAB_ALLOC_TYPE_REQ     0
#define SHS_SLAB_ALLOC_TYPE_ACT     1

#define SHS_SLAB_CHUNK_SIZE    sizeof(chunk_link_t)

enum 
{
    SHS_SLAB_UPTYPE_POWER = 1,
    SHS_SLAB_UPTYPE_LINEAR
};

enum 
{
    SHS_SLAB_POWER_FACTOR = 2,
    SHS_SLAB_LINEAR_FACTOR = 1024,
};

typedef struct chunk_link chunk_link_t;

struct chunk_link 
{
    size_t        size;
    size_t        req_size;
    ssize_t       id;
    chunk_link_t *next;
};

typedef struct slabclass_s 
{
    size_t        size;
    chunk_link_t *free_list;
} slabclass_t;

typedef struct shs_slab_errno_s 
{
    unsigned int slab_errno;
    unsigned int allocator_errno;
} shs_slab_errno_t;

typedef struct shs_slab_stat_s
{
    size_t used_size;
    size_t reqs_size;
    size_t free_size;
    size_t chunk_count;
    size_t chunk_size;
    size_t system_size;
    size_t failed;
    size_t recover;
    size_t recover_failed;
    size_t split_failed;
} shs_slab_stat_t;

typedef struct shs_slab_manager_s 
{
    int                  uptype;    // increament type
    ssize_t              free_len;  // length of array slabclass
    size_t               factor;    // increament step
    size_t               min_size;  // the size of slabclass[0]
    shs_slab_stat_t      slab_stat; // stat for slab
    shs_mem_allocator_t *allocator; // the pointer to memory
    slabclass_t         *slabclass; // slab class array 0..free_len - 1
} shs_slab_manager_t;

shs_slab_manager_t * shs_slabs_create(shs_mem_allocator_t *allocator, 
    int uptype, size_t factor, const size_t item_size_min, 
    const size_t item_size_max, shs_slab_errno_t *err_no);
int shs_slabs_release(shs_slab_manager_t **slab_mgr, shs_slab_errno_t *err_no);
void * shs_slabs_alloc(shs_slab_manager_t *slab_mgr, int alloc_type, 
    size_t req_size, size_t *slab_size, shs_slab_errno_t *err_no);
void * shs_slabs_split_alloc(shs_slab_manager_t *slab_mgr, size_t req_size,
    size_t *slab_size, size_t req_minsize, shs_slab_errno_t *err_no);
int shs_slabs_free(shs_slab_manager_t *slab_mgr, void *ptr, 
    shs_slab_errno_t *err_no);
const char * shs_slabs_strerror(shs_slab_manager_t *slab_mgr, 
    const shs_slab_errno_t *err_no);
int shs_slabs_get_stat(shs_slab_manager_t *slab_mgr, shs_slab_stat_t *stat);
size_t shs_slabs_get_chunk_size(shs_slab_manager_t *slab_mgr);

#endif

