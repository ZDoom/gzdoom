/*
** tarray.h
** Templated, automatically resizing array
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

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
	TArray (const TArray<T> &other)
	{
		DoCopy (other);
	}
	TArray<T> &operator= (const TArray<T> &other)
	{
		if (&other != this)
		{
			if (Array != NULL)
			{
				free (Array);
			}
			DoCopy (other);
		}
		return *this;
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
	size_t Push (const T &item)
	{
		if (Count >= Most)
		{
			Most = (Most >= 16) ? Most + Most / 2 : 16;
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
		else if (index < Count)
			Count--;
	}
	void ShrinkToFit ()
	{
		if (Most > Count)
		{
			Most = Count;
			if (Most == 0)
			{
				if (Array != NULL)
				{
					free (Array);
					Array = NULL;
				}
			}
			else
			{
				Array = (T *)Realloc (Array, sizeof(T)*Most);
			}
		}
	}
	// Grow Array to be large enough to hold amount more entries without
	// further growing.
	void Grow (size_t amount)
	{
		if (Count + amount > Most)
		{
			const size_t choicea = Count + amount;
			const size_t choiceb = Most + Most/2;
			Most = (choicea > choiceb ? choicea : choiceb);
			Array = (T *)Realloc (Array, sizeof(T)*Most);
		}
	}
	// Resize Array so that it has exactly amount entries in use.
	void Resize (size_t amount)
	{
		if (Count < amount)
		{
			Grow (amount - Count);
		}
		Count = amount;
	}
	// Reserves amount entries at the end of the array, but does nothing
	// with them.
	size_t Reserve (size_t amount)
	{
		if (Count + amount > Most)
		{
			Grow (amount);
		}
		size_t place = Count;
		Count += amount;
		return place;
	}
	size_t Size () const
	{
		return Count;
	}
	size_t Max () const
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

	void DoCopy (const TArray<T> &other)
	{
		Most = Count = other.Count;
		if (Count != 0)
		{
			Array = (T *)Malloc (sizeof(T)*Most);
			for (size_t i = 0; i < Count; ++i)
			{
				Array[i] = other.Array[i];
			}
		}
		else
		{
			Array = NULL;
		}
	}
};

// An array with accessors that automatically grow the
// array as needed. But can still be used as a normal
// TArray if needed. Used by ACS world and global arrays.

template <class T>
class TAutoGrowArray : public TArray<T>
{
public:
	T GetVal (size_t index)
	{
		if (index >= Size())
		{
			return 0;
		}
		return (*this)[index];
	}
	void SetVal (size_t index, T val)
	{
		if (index >= Size())
		{
			Resize (index + 1);
		}
		(*this)[index] = val;
	}
};

#endif //__TARRAY_H__
