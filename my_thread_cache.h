#pragma once
#include "my_common.h"

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