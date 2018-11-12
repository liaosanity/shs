#ifndef THREAD_KEY_H
#define THREAD_KEY_H

#include <string>

typedef void (*data_destroy)(void* data);

int thread_key_create();
int thread_key_delete();
void thread_key_set(std::string key, void* data, data_destroy destroy);
void* thread_key_get(std::string key);

#endif
