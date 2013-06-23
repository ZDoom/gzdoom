#ifndef __GL_BASIC
#define __GL_BASIC

#include <new>
#include "stats.h"


// Disabled because it doesn't work and only accumulates large portions of blocked heap
// without providing any relevant performance boost.
template <class T> struct FreeList
{
	//T * freelist;

	T * GetNew()
	{
		/*
		if (freelist)
		{
			T * n=freelist;
			freelist=*((T**)n);
			return new ((void*)n) T;
		}
		*/
		return new T;
	}

	void Release(T * node)
	{
		/*
		node->~T();
		*((T**)node) = freelist;
		freelist=node;
		*/
		delete node;
	}

	~FreeList()
	{
		/*
		while (freelist!=NULL)
		{
			T * n = freelist;
			freelist=*((T**)n);
			delete n;
		}
		*/
	}
};

template<class T> class UniqueList
{
	TArray<T*> Array;
	FreeList<T>	TheFreeList;

public:

	T * Get(T * t)
	{
		for(unsigned i=0;i<Array.Size();i++)
		{
			if (!memcmp(t, Array[i], sizeof(T))) return Array[i];
		}
		T * newo=TheFreeList.GetNew();

		*newo=*t;
		Array.Push(newo);
		return newo;
	}

	void Clear()
	{
		for(unsigned i=0;i<Array.Size();i++) TheFreeList.Release(Array[i]);
		Array.Clear();
	}

	~UniqueList()
	{
		Clear();
	}
};

#endif
