#pragma once
#include<cstdlib>
#include<assert.h>
#include<mutex>

// 该文件包含基本类 和 常量定义

const size_t MAX_BYTES = 64 * 1024; // ThreadCache 负责分配的 小对象的最大内存
const size_t PAGE_SHIFT = 12;  // 页面大小为4k
const size_t NLIST = 184;  // 自由链表数组元素个数
const size_t NPAGES = 129; // Span 的最大页面数

// obj 即为自由链表中的节点
// 该函数使得可以使用该空间的前八个字节储存下一未分配空间的地址
// 额外节省一个指针
// 类似于使用了union, 因为 自由链表节点的两个作用 
//                          1. 作为自由链表的指针指向下一节点
//                          2. 将自身分配出去
// 互斥出现 
// 注意仔细思考手法
inline static void*& NextObj(void* obj){
    return *((void**)obj);
}

// 自由链表类
class FreeList{
private:
    void* _list = nullptr;
    size_t _size;   // 自由链表中的节点个数
    size_t _max_size;   //自由链表中最大节点个数

public:
    // 插入头部
    void Push(void* obj){
        NextObj(obj) = _list;
        _list = obj;
        _size++;
    }

    void PushRange(void* begin, void* end, size_t num){
        NextObj(end) = _list;
        _list = begin;
        _size += num;
    }

    void* Pop(){
        auto front = _list;
        _list = NextObj(front);
        _size--;
        return front;
    }

    void* PopRange(){
        auto front = _list;
        _list = nullptr;
        _size = 0;
        return front;
    }

    bool Empty(){
        return _size == 0;
    }

    size_t Size(){
        return _size;
    }

    size_t MaxSize(){
        return _max_size;
    }

    void SetMaxSize(size_t max_size){
        _max_size = max_size;
    }
};

class SizeClass{
public:
    // 返回 大小为size 的内存块 对应的自由链表在数组中的index
    // size 最小为1 
    inline static size_t _Index(size_t size, size_t align){
        size_t alignnum = 1 << align;
        return (size + alignnum - 1) >> align - 1; 
    }

    // 将 size 上调为 align 的倍数
    // Example : 
    //  size = 1,  align = 8,  return value = 8
    inline static size_t _RoundUp(size_t size, size_t align){
        size_t alignnum = 1 << align;
        // ~(alignnum - 1) 将 align 以下低位全部置零
        return (size + alignnum - 1) &  ~(alignnum - 1);
    }

public:
    // 我是一名复制粘贴怪! 
    // 控制在12%左右的内碎片浪费
	// [1,128]				8byte对齐 freelist[0,16)
	// [129,1024]			16byte对齐 freelist[16,72)
	// [1025,8*1024]		128byte对齐 freelist[72,128)
	// [8*1024+1,64*1024]	1024byte对齐 freelist[128,184)

    // 可以根据具体情况进行调整
    inline static size_t Index(size_t bytes){
        assert(bytes <= 64 * 1024);
        
        static int group_array[4] = {16, 56, 56, 56};

        if(bytes <= 128){
            return _Index(bytes, 3);
        }else if(bytes <= 1024){
            return _Index(bytes - 128 , 4) + group_array[0];
        }else if(bytes <= 8 * 1024){
            return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
        }else {
            return _Index(bytes - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
        }
    }

    inline static size_t RoundUp(size_t bytes){
        assert(bytes <= 64 * 1024);

        if(bytes <= 128){
            return _RoundUp(bytes, 3);
        }else if(bytes <= 1024){
            return _RoundUp(bytes, 4);
        }else if(bytes <= 8 * 1024){
            return _RoundUp(bytes, 7);
        }else {
            return _RoundUp(bytes, 10);
        }
    }

    // 当 ThreadCache 中某size处没有可用对象, ThreadCache 调用该函数 向 CentralCache 
    // 请求 分配  NumMoveObjs(size) 个对象
    inline static size_t NumMoveObjs(size_t size){
        auto num = MAX_BYTES / size;
        if(num < 2) 
            num = 2;
        if(num > 512)
            num = 512;
        return num;
    }

    // 当 CentralCache 中 某size处可用对象不能满足 ThreadCache 的响应时, 
    // 向 PageCache 请求 大小为 NumMovePages 的span对象(单位为页)
    inline static size_t NumMovePages(size_t size){
        auto num = NumMoveObjs(size);
        size_t npages = (num * size) >> PAGE_SHIFT;
        if(npages == 0) 
            npages = 1;
        return npages;
    }
};

using Page_ID = unsigned long long;

// 既可以分配内存出去 (Central Free List 中切分成若干个object, 向 Thread Cache 分配)
// 也负责将内存回收到 PageCache 合并 (use_count 为 0)

struct Span{    
    Page_ID page_id = 0 ;  //页号
    size_t page_num = 0 ;  //页数

    Span* prev = nullptr ; 
    Span* next = nullptr ;

    void* list_ptr = nullptr;  // Span下面挂着的object
    size_t obj_size = 0;   // object的大小

    size_t use_count = 0;  //分配出去的obj的数目
};

// 双向循环列表, 插入删除效率高
class SpanList{
    
public:
    Span* head;
    std::mutex mutex;
    
public: 
    SpanList() : head(new Span()) {
        head -> next = head;
        head -> prev = head;
    }

    ~SpanList() {
        Span* cur = head -> next;
        while(cur != head){
            Span* next = cur -> next;
            // 因为整个内存池都销毁, 是不是这时候一定是进程销毁了?  是 
            // 获得的内存由我们再分配
            // 保存页面页号的数据结构被破坏, 而系统仍然认为这时候我们持有这块内存
            delete cur; 
            cur = next;
        }
        delete head;
        head = nullptr;
    }

    // 返回第一个节点
    Span* Begin(){
        return head -> next;
    }

    // 返回最后一个节点的下一节点
    Span* End(){
        return head;
    }

    bool Empty(){
        return head -> next == head;
    }

    // 在 pos前插入
    void Insert(Span* pos, Span* new_span){
        Span* prev = pos -> prev;
        prev -> next = new_span;
        new_span -> prev = prev;
        pos -> prev = new_span;
        new_span -> next = pos;
    }

    // 将 pos 移出 SpanList
    void Erase(Span* pos){
        Span* prev = pos -> prev;
        Span* next = pos -> next;
        prev -> next = next;
        next -> prev = prev;
    }

    void PushFront(Span* new_span){
        Insert(Begin(), new_span);
    }

    void PushBack(Span* new_span){
        Insert(End(), new_span);
    }

    Span* PopFront(){
        Span* front = Begin();
        Erase(front);
        return front;
    }

    Span* PopBack(){
        Span* back = End() -> prev;
        Erase(back);
        return back;
    }

    void Lock(){
        mutex.lock();
    }

    void UnLock(){
        mutex.unlock();
    }
};