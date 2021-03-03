#include "my_page_cache.h"
#include <new>

Span* PageCache::AllocBigPageObj(size_t size){
    assert(size > MAX_BYTES);

    size = SizeClass::_RoundUp(size, PAGE_SHIFT);
    size_t num_of_pages = size >> PAGE_SHIFT ;

    if(num_of_pages < NPAGES){
        Span* new_span = NewSpan(num_of_pages);
        new_span -> obj_size = size;
        return new_span;
    }else {
        void* ptr = malloc(size);

        if(ptr == (void*) -1) {
            throw std::bad_alloc();
        }
        
        Span* new_span = new Span();
        new_span -> obj_size = size;
        new_span -> page_id = (Page_ID)ptr >> PAGE_SHIFT;
        new_span -> page_num = num_of_pages;

        _map_id_to_span[new_span -> page_id] = new_span;
        return new_span;
    }
}

void PageCache::FreeBigPageObj(void* ptr, Span* span){
    if(span -> page_num < NPAGES){
        span -> obj_size = 0;
        ReleaseSpanToPageCache(span);
    }else {
        free(ptr);
        _map_id_to_span.erase(span -> page_id);
        delete span;
    }
}

Span* PageCache::MapObjectToSpan(void* obj){
    Page_ID page_id = (Page_ID)obj >> PAGE_SHIFT;
    auto span = _map_id_to_span.find(page_id);
    if(span != _map_id_to_span.end()){
        return span -> second;
    }else {
        assert(false);
        return nullptr;
    }
}

Span* PageCache::NewSpan(size_t pages_num){
    std::lock_guard<std::mutex> lock(_mutex);

    return _NewSpan(pages_num);
}

Span* PageCache::_NewSpan(size_t pages_num){
    assert(pages_num < NPAGES);

    if(_span_list[pages_num].Empty() == false) 
        return _span_list[pages_num].PopFront();

    for(int i = pages_num + 1; i < NPAGES; i++){
        if(_span_list[i].Empty() == false){
            // 将较大的 Span 拆成较小的 Span
            Span* left = _span_list[i].PopFront();
            Span* ret_span = new Span();
            
            ret_span ->page_id = left -> page_id;
            ret_span ->page_num = pages_num;

            left -> page_num = i - pages_num;
            left -> page_id += pages_num;

            for(int i = ret_span ->page_id; i != left ->page_id; i++)
                _map_id_to_span[i] = ret_span;

            // 将 left 插入合适的桶
            _span_list[left -> page_num].PushFront(left);

            return ret_span;
        }
    }

    // 山穷水尽  向系统要空间
    void* ptr = malloc((NPAGES - 1) << PAGE_SHIFT);
    if(ptr == nullptr)
        throw std::bad_alloc();
    Span* new_span = new Span();
    new_span ->page_num = NPAGES - 1;
    new_span ->page_id = (Page_ID)ptr >> PAGE_SHIFT;

    for(int i = new_span ->page_id; i != new_span ->page_id + new_span -> page_num; i++)
        _map_id_to_span[i] = new_span;
    
    _span_list[NPAGES - 1].PushFront(new_span);

    return _NewSpan(pages_num);
}


void PageCache::ReleaseSpanToPageCache(Span* cur){
    // 有可能多个线程同时归还span, 要加全局锁
    std::lock_guard<std::mutex> lock(_mutex);

    if(cur -> page_num >= NPAGES){
        free((void*)(cur ->page_id << PAGE_SHIFT));
        _map_id_to_span.erase(cur -> page_id);
        delete cur;
    }


    // 向前合并
    while(1){
        
        Page_ID prev_id = --cur ->page_id;

        // 该页面必须已经被我们所管理
        if(_map_id_to_span.find(prev_id) == _map_id_to_span.end())
            break;
        
        auto prev = _map_id_to_span[prev_id];

        // 必须是完整的空白Span, 不能有分出去的Object
        if(prev -> use_count != 0)
            break;

        // 页面数量不能超过 NPAGES - 1
        if(cur -> page_num + prev -> page_num > NPAGES - 1)
            break;

        for(int i = prev -> page_id; i != prev -> page_id + prev -> page_num; i++){
            _map_id_to_span[i] = cur;
        }

        cur -> page_num += prev -> page_num;
        cur ->page_id = prev ->page_id;

        _span_list[prev -> page_num].Erase(prev);
        
        delete(prev);
    }

    //向后合并
    while(1){
        Page_ID next_id = cur ->page_id + cur -> page_num;

        // 该页面必须已经被我们所管理
        if(_map_id_to_span.find(next_id) == _map_id_to_span.end())
            break;
        
        auto next = _map_id_to_span[next_id];

        // 必须是完整的空白Span, 不能有分出去的Object
        if(next -> use_count != 0)
            break;

        // 页面数量不能超过 NPAGES - 1
        if(cur -> page_num + next -> page_num > NPAGES - 1)
            break;

        for(int i = next -> page_id; i != next -> page_id + next -> page_num; i++){
            _map_id_to_span[i] = cur;
        }

        cur -> page_num += next -> page_num;

        _span_list[next -> page_num].Erase(next);

        delete(next);
    }

    _span_list[cur -> page_num].PushFront(cur);
}