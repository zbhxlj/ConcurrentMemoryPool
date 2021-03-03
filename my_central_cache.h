#pragma once
#include "my_common.h"

class CentralCache{

public:
    CentralCache(const CentralCache&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;

    static CentralCache* GetInstance(){
        std::lock_guard<std::mutex> lock(_mutex);
        if(_instance == nullptr){
            _instance = new CentralCache();
        }
        return _instance;
    }
    // 根据 byte_size  计算出
    Span* GetOneSpan(SpanList& spanlist, size_t byte_size);
    size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);
    void ReleaseListToSpans(void* start, size_t size);

private :
    CentralCache(){}

    SpanList _span_list[NLIST];

    static std::mutex _mutex;
    static CentralCache* _instance;
};

std::mutex CentralCache::_mutex;
CentralCache* CentralCache::_instance = nullptr;