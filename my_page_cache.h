#pragma once 
#include "my_common.h"
#include<unordered_map>

class PageCache{
public:
    PageCache(const PageCache&) = delete;
    PageCache& operator=(const PageCache&) = delete;

    static PageCache* GetInstance(){
        std::lock_guard<std::mutex> lock(_mutex);
        if(_instance == nullptr){
            _instance = new PageCache();
        }
        return _instance;
    }

    Span* AllocBigPageObj(size_t size);
	void FreeBigPageObj(void* ptr, Span* span);

	Span* _NewSpan(size_t n);
	Span* NewSpan(size_t n);//获取的是以页为单位

	//获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	//释放空间span回到PageCache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

private:
    // 将页面映射到相应的Span
    std::unordered_map<Page_ID, Span*> _map_id_to_span;
    SpanList                           _span_list[NPAGES];

private:
    PageCache(){}
    static PageCache* _instance;
    static std::mutex _mutex;
};

PageCache* PageCache::_instance = nullptr;
std::mutex PageCache::_mutex;