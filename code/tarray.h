#ifndef __TARRAY_H__
#define __TARRAY_H__

#include <stdlib.h>
#include "m_alloc.h"

template <class T>
class TArray
{
public:
	TArray ()
	{
		Most = 0;
		Count = 0;
		Array = NULL;
	}
	TArray (int max)
	{
		Most = max;
		Count = 0;
		Array = (T *)Malloc (sizeof(T)*max);
	}
	~TArray ()
	{
		if (Array)
			free (Array);
	}
	T &operator[] (size_t index) const
	{
		return Array[index];
	}
	size_t Push (T item)
	{
		if (Count >= Most)
		{
			Most = Most ? Most * 2 : 16;
			Array = (T *)Realloc (Array, sizeof(T)*Most);
		}
		Array[Count] = item;
		return Count++;
	}
	bool Pop (T &item)
	{
		if (Count > 0)
		{
			item = Array[--Count];
			return true;
		}
		return false;
	}
	void Delete (int index)
	{
		if (index < Count-1)
			memmove (Array + index, Array + index + 1, (Count - index) * sizeof(T));
		if (index < Count)
			Count--;
	}
	size_t Size ()
	{
		return Count;
	}
	size_t Max ()
	{
		return Most;
	}
	void Clear ()
	{
		Count = 0;
	}
private:
	T *Array;
	size_t Most;
	size_t Count;
};

#endif //__TARRAY_H__
