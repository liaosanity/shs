#include "thread_key.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <map>

using namespace std;

#define MAGIC 0x20100915

static pthread_key_t tsd_key;
static int magic;

typedef struct thd_key_data_tag
{
    void* data;
    data_destroy destroy;
} thd_key_data_t;

void thread_key_destroy(void* data)
{
    map<string, void*> *user_data_map = (map<string, void*>*)data;
    if (NULL == user_data_map)
    {
        return;
    }

    map<string, void*>::iterator iter;
    for (iter = user_data_map->begin(); iter != user_data_map->end(); iter++)
    {
        if (iter->second != NULL)
        {
            thd_key_data_t *thd_data = (thd_key_data_t*)iter->second;
            if (thd_data->destroy != NULL)
            {
                thd_data->destroy(thd_data->data);
            }

            free(thd_data);
            user_data_map->erase(iter);
        }
    }

    delete user_data_map;
}

int thread_key_create()
{
    int ret = pthread_key_create(&tsd_key, thread_key_destroy);
    if (ret != 0)
    {
        return ret;
    }

    magic = MAGIC;

    return 0;
}

int thread_key_delete()
{
    if (magic != MAGIC)
    {
        return 0;
    }

    return pthread_key_delete(tsd_key);
}

void thread_key_set(string key, void* data, data_destroy destroy)
{
    map<string, void*> *user_data_map;
    thd_key_data_t* key_data = NULL;

    user_data_map = (map<string, void*>*)pthread_getspecific(tsd_key);
    if (user_data_map == NULL)
    {
        user_data_map = new map<string, void*>;
        pthread_setspecific(tsd_key, (void*)user_data_map);
    }

    key_data = (thd_key_data_t*)calloc(1, sizeof(thd_key_data_t));
    key_data->data = data;
    key_data->destroy = destroy;
    user_data_map->insert(pair<string, void*>(key, (void*)key_data));
}

void* thread_key_get(string key)
{
    map<string, void*> *user_data_map = NULL;

    user_data_map = (map<string, void*>*)pthread_getspecific(tsd_key);
    if (user_data_map == NULL)
    {
        return NULL;
    }

    map<string, void*>::iterator iter;
    iter = user_data_map->find(key);
    if (iter != user_data_map->end())
    {
        thd_key_data_t* thd_data = (thd_key_data_t*)iter->second;

        return thd_data->data;
    }

    return NULL;
}
