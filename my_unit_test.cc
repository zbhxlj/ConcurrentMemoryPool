#include "my_common.h"
#include "my_page_cache.h"
#include "my_concurrent_alloc.h"
#include<iostream>
#include<vector>

using std::endl;
using std::cout;
using std::vector;
#include<thread>

void TestSize()
{
	/*cout << SizeClass::Index(10) << endl;
	cout << SizeClass::Index(16) << endl;
	cout << SizeClass::Index(128) << endl;
	cout << SizeClass::Index(129) << endl;
	cout << SizeClass::Index(128 + 17) << endl;
	cout << SizeClass::Index(1025) << endl;
	cout << SizeClass::Index(1024 + 129) << endl;
	cout << SizeClass::Index(8*1024+1) << endl;
	cout << SizeClass::Index(8*1024 + 1024) << endl;
	*/

	cout << SizeClass::RoundUp(10) << endl;
	cout << SizeClass::RoundUp(1025) << endl;
	cout << SizeClass::RoundUp(1024 * 8 + 1) << endl;

	cout << SizeClass::NumMovePages(16) << endl;
	cout << SizeClass::NumMovePages(1024) << endl;
	cout << SizeClass::NumMovePages(1024 * 8) << endl;
	cout << SizeClass::NumMovePages(1024 * 64) << endl;

}

void Alloc(size_t n)
{
	size_t begin1 = clock();
	std::vector<void*> v;
	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		// std::cout<< i << " " << "Returned ConcurrentAlloc" << std::endl;
	}

	//v.push_back(ConcurrentAlloc(10));

	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentDealloc(v[i]);
		// cout << v[i] << endl;
	}
	v.clear();
	size_t end1 = clock();

	std::cout << "Loop 2" << std::endl;
	
	size_t begin2 = clock();
	cout << endl << endl;
	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
	}

	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentDealloc(v[i]);
		// cout << v[i] << endl;
	}
	v.clear();
	size_t end2 = clock();

	cout << end1 - begin1 << endl;
	cout << end2 - begin2 << endl;
}

void TestThreadCache()
{
	std::thread t1(Alloc, 100);
	std::thread t2(Alloc, 5);
	std::thread t3(Alloc, 5);
	std::thread t4(Alloc, 5);

	t1.join();
	t2.join();
	t3.join();
	t4.join();

}

void TestCentralCache()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 8; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
	}

	for (size_t i = 0; i < 8; ++i)
	{
		//ConcurrentFree(v[i], 10);
		cout << v[i] << endl;
	}
}

void TestPageCache()
{
	PageCache::GetInstance()->NewSpan(2);
}

void TestConcurrentAllocFree()
{
	size_t n = 2;
	std::vector<void*> v;
	for (size_t i = 0; i < n; ++i)
	{
		void* ptr = ConcurrentAlloc(99999);
		v.push_back(ptr);
		//printf("obj:%d->%p\n", i, ptr);
		//if (i == 2999999)
		//{
		//	printf("obj:%d->%p\n", i, ptr);
		//}
	}

	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentDealloc(v[i]);
	}
	cout << "hehe" << endl;
}

void AllocBig()
{
	void* ptr1 = ConcurrentAlloc(65 << PAGE_SHIFT);
	void* ptr2 = ConcurrentAlloc(129 << PAGE_SHIFT);

	ConcurrentDealloc(ptr1);
	ConcurrentDealloc(ptr2);
}


int main()
{
	// TestSize();
	TestThreadCache();
	// TestCentralCache();
	// TestPageCache();
	// TestConcurrentAllocFree();

	// AllocBig();
	// system("pause");
	return 0;
}