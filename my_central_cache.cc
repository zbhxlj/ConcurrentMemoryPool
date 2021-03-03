#include "my_central_cache.h"

#include "my_page_cache.h"

std::mutex CentralCache::_mutex;
CentralCache* CentralCache::_instance = nullptr;

Span *CentralCache::GetOneSpan(SpanList &spanlist, size_t byte_size) {
    size_t index = SizeClass::Index(byte_size);
    SpanList &_span_index_list = _span_list[index];
    if (_span_index_list.Empty() == false) {
        auto cur_span = _span_index_list.Begin();
        while (cur_span != _span_index_list.End()) {
            if (cur_span->list_ptr != nullptr)
                return cur_span;
            else
                cur_span = cur_span->next;
        }
    }

    // std::cout << "Now is entering Newspan" << std::endl;
    // 山穷水尽 再要一个Span
    Span *new_span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePages(byte_size));
    new_span->obj_size = byte_size;
    char *begin = (char *)(new_span->page_id << PAGE_SHIFT);
    new_span->list_ptr = begin;
    char *end = begin + new_span->obj_size;

    while (end != (void *)((new_span->page_id + new_span->page_num) << PAGE_SHIFT)) {
        NextObj(begin) = end;
        begin = end;
        end = end + new_span->obj_size;
    }
    NextObj(begin) = nullptr;

    _span_list->PushFront(new_span);

    // std::cout << "Now is quitting Nes_span" << std::endl;
    return new_span;

}

size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t n, size_t byte_size) {
    size_t index = SizeClass::Index(byte_size);
    SpanList &_span_index_list = _span_list[index];

    std::lock_guard<std::mutex> lock(_span_index_list.mutex);

    // std::cout << "Now is entering GetOneSpan " << std::endl;
    Span *new_span = GetOneSpan(_span_index_list, byte_size);
    start = new_span->list_ptr;

    size_t batch_size = 0;
    void *prev = nullptr;
    void *cur = new_span->list_ptr;
    for (int i = 0; i < n; i++) {
        ++batch_size;
        prev = cur;
        cur = NextObj(cur);
        if (cur == nullptr) break;
    }
    end = prev;

    new_span->list_ptr = cur;
    new_span->use_count += batch_size;

    if (new_span->list_ptr == nullptr) {
        _span_index_list.Erase(new_span);
        _span_index_list.PushBack(new_span);
    }

    // std::cout << "Now is quitting FetchRangeObj !" << std::endl;
    return batch_size;
}

void CentralCache::ReleaseListToSpans(void *start, size_t byte_size) {
    size_t index = SizeClass::Index(byte_size);
    SpanList &_cur_span_list = _span_list[index];

    std::lock_guard<std::mutex> lock(_cur_span_list.mutex);

    void *cur = start;
    while (cur != nullptr) {
        void *next = NextObj(cur);

        Span *_mapped_span = PageCache::GetInstance()->MapObjectToSpan(cur);
        void *_original_list_ptr = _mapped_span->list_ptr;
        NextObj(cur) = _original_list_ptr;
        _mapped_span->list_ptr = cur;

        if (--_mapped_span->use_count == 0) {
            _cur_span_list.Erase(_mapped_span);
            PageCache::GetInstance()->ReleaseSpanToPageCache(_mapped_span);
        }

        cur = next;
    }
}