#ifndef CONCURRENT_ALLOC_H
#define CONCURRENT_ALLOC_H

#include "my_central_cache.h"
#include "my_common.h"
#include "my_page_cache.h"
#include "my_thread_cache.h"

void* ConcurrentAlloc(size_t size) {
    if (size > MAX_BYTES) {
        Span* new_span = PageCache::GetInstance()->AllocBigPageObj(size);
        return (void*)(new_span->page_id << PAGE_SHIFT);
    } else {
        if (thread_local_cache == nullptr) {
            thread_local_cache = new ThreadCache();
        }
        // std::cout << "Now is entering " << "Allocate" << std::endl;
        return thread_local_cache->Allocate(size);
    }
}

void ConcurrentDealloc(void* ptr) {
    Span* mapped_span = PageCache::GetInstance()->MapObjectToSpan(ptr);
    if (mapped_span->obj_size > MAX_BYTES) {
        PageCache::GetInstance()->FreeBigPageObj(ptr, mapped_span);
    } else {
        thread_local_cache->Deallocate(mapped_span->obj_size, ptr);
    }
}

#endif