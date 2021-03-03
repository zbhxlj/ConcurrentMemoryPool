#ifndef THREAD_CACHE_H
#define THREAD_CACHE_H
#include "my_common.h"
#include <unistd.h>
#include<pthread.h>
// TLS
// __thread ThreadCache memory_allocator;
class ThreadCache{
public:
    FreeList _free_list[NLIST];

    void* Allocate(size_t bytes);
    void Deallocate(size_t bytes, void* ptr);
    void* FetchFromCentralCache(size_t index, size_t obj_size);
    void ListTooLong(FreeList* list, size_t obj_size);
};

static __thread  ThreadCache* thread_local_cache = nullptr;

#endif