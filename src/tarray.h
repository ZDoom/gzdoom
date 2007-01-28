/*
** tarray.h
** Templated, automatically resizing array
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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
#include <assert.h>
#include <new>
#include "m_alloc.h"


template <class T>
class TArray
{
public:
	////////
	// This is a dummy constructor that does nothing. The purpose of this
	// is so you can create a global TArray in the data segment that gets
	// used by code before startup without worrying about the constructor
	// resetting it after it's already been used. You MUST NOT use it for
	// heap- or stack-allocated TArrays.
	enum ENoInit
	{
		NoInit
	};
	TArray (ENoInit dummy)
	{
	}
	////////
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
		Array = (T *)M_Malloc (sizeof(T)*max);
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
				if (Count > 0)
				{
					DoDelete (0, Count-1);
				}
				free (Array);
			}
			DoCopy (other);
		}
		return *this;
	}
	~TArray ()
	{
		if (Array)
		{
			if (Count > 0)
			{
				DoDelete (0, Count-1);
			}
			free (Array);
			Array = NULL;
			Count = 0;
			Most = 0;
		}
	}
	T &operator[] (unsigned int index) const
	{
		return Array[index];
	}
	unsigned int Push (const T &item)
	{
		Grow (1);
		::new((void*)&Array[Count]) T(item);
		return Count++;
	}
	bool Pop (T &item)
	{
		if (Count > 0)
		{
			item = Array[--Count];
			Array[Count].~T();
			return true;
		}
		return false;
	}
	void Delete (unsigned int index)
	{
		if (index < Count)
		{
			Array[index].~T();
			memmove (&Array[index], &Array[index+1], sizeof(T)*(Count - index - 1));
			Count--;
		}
	}
	// Inserts an item into the array, shifting elements as needed
	void Insert (unsigned int index, const T &item)
	{
		if (index >= Count)
		{
			// Inserting somewhere past the end of the array, so we can
			// just add it without moving things.
			Resize (index + 1);
			::new ((void *)&Array[index]) T(item);
		}
		else
		{
			// Inserting somewhere in the middle of the array,
			// so make room for it
			Resize (Count + 1);

			// Now move items from the index and onward out of the way
			memmove (&Array[index+1], &Array[index], sizeof(T)*(Count - index - 1));

			// And put the new element in
			::new ((void *)&Array[index]) T(item);
		}
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
				DoResize ();
			}
		}
	}
	// Grow Array to be large enough to hold amount more entries without
	// further growing.
	void Grow (unsigned int amount)
	{
		if (Count + amount > Most)
		{
			const unsigned int choicea = Count + amount;
			const unsigned int choiceb = Most = (Most >= 16) ? Most + Most / 2 : 16;
			Most = (choicea > choiceb ? choicea : choiceb);
			DoResize ();
		}
	}
	// Resize Array so that it has exactly amount entries in use.
	void Resize (unsigned int amount)
	{
		if (Count < amount)
		{
			// Adding new entries
			Grow (amount - Count);
			for (unsigned int i = Count; i < amount; ++i)
			{
				::new((void *)&Array[i]) T;
			}
		}
		else if (Count != amount)
		{
			// Deleting old entries
			DoDelete (amount, Count - 1);
		}
		Count = amount;
	}
	// Reserves amount entries at the end of the array, but does nothing
	// with them.
	unsigned int Reserve (unsigned int amount)
	{
		Grow (amount);
		unsigned int place = Count;
		Count += amount;
		for (unsigned int i = place; i < Count; ++i)
		{
			::new((void *)&Array[i]) T;
		}
		return place;
	}
	unsigned int Size () const
	{
		return Count;
	}
	unsigned int Max () const
	{
		return Most;
	}
	void Clear ()
	{
		if (Count > 0)
		{
			DoDelete (0, Count-1);
			Count = 0;
		}
	}
private:
	T *Array;
	unsigned int Most;
	unsigned int Count;

	void DoCopy (const TArray<T> &other)
	{
		Most = Count = other.Count;
		if (Count != 0)
		{
			Array = (T *)M_Malloc (sizeof(T)*Most);
			for (unsigned int i = 0; i < Count; ++i)
			{
				Array[i] = other.Array[i];
			}
		}
		else
		{
			Array = NULL;
		}
	}

	void DoResize ()
	{
		size_t allocsize = sizeof(T)*Most;
		Array = (T *)M_Realloc (Array, allocsize);
	}

	void DoDelete (unsigned int first, unsigned int last)
	{
		assert (last != ~0u);
		for (unsigned int i = first; i <= last; ++i)
		{
			Array[i].~T();
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
	T GetVal (unsigned int index)
	{
		if (index >= this->Size())
		{
			return 0;
		}
		return (*this)[index];
	}
	void SetVal (unsigned int index, T val)
	{
		if ((int)index < 0) return;	// These always result in an out of memory condition.

		if (index >= this->Size())
		{
			this->Resize (index + 1);
		}
		(*this)[index] = val;
	}
};

#endif //__TARRAY_H__
