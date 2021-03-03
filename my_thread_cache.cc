#include "my_thread_cache.h"
#include "my_central_cache.h"
#include<algorithm>


// Allocate 负责小对象的内存分配
void* ThreadCache::Allocate(size_t bytes){
    assert(bytes <= 64 * 1024);

    size_t index = SizeClass::Index(bytes);
    FreeList& free_list = _free_list[index];
    if(free_list.Empty() == false){
        return free_list.Pop();
    }else {
        return  FetchFromCentralCache(index, SizeClass::RoundUp(bytes));
    }
}

// Deallocate 负责回收小对象
void ThreadCache::Deallocate(size_t bytes, void* ptr){
    assert(bytes <= 64 * 1024);

    size_t index = SizeClass::Index(bytes);
    FreeList& free_list = _free_list[index];
    free_list.Push(ptr);

    if(free_list.Size() >= free_list.MaxSize()){
        ListTooLong(&free_list, SizeClass::RoundUp(bytes));
    }
}

// 当自由链表很长时, 让 Central Cache 回收
void ThreadCache::ListTooLong(FreeList* list, size_t obj_size) {
    void* start = list->PopRange();
    return CentralCache::GetInstance()->ReleaseListToSpans(start, obj_size);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t obj_size){
    FreeList& free_list = _free_list[index];

    size_t obj_num = std::min(SizeClass::NumMoveObjs(obj_size), 2 * free_list.MaxSize());

    void* begin = nullptr, *end = nullptr;
    size_t batch_size = CentralCache::GetInstance() -> FetchRangeObj(begin, end, obj_num, obj_size);

    if(batch_size > 1){
        free_list.PushRange(NextObj(begin), end, batch_size - 1);
    }

    if(batch_size > free_list.MaxSize()){
        free_list.SetMaxSize(batch_size);
    }

    return begin;
}