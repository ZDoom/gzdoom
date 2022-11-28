/*
** templates.h
** Some useful template functions
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __TEMPLATES_H__
#define __TEMPLATES_H__

#ifdef _MSC_VER
#pragma once
#endif

#include <stdlib.h>
#include <utility>

//==========================================================================
//
// BinarySearch
//
// Searches an array sorted in ascending order for an element matching
// the desired key.
//
// Template parameters:
//		ClassType -		The class to be searched
//		KeyType -		The type of the key contained in the class
//
// Function parameters:
//		first -			Pointer to the first element in the array
//		max -			The number of elements in the array
//		keyptr -		Pointer to the key member of ClassType
//		key -			The key value to look for
//
// Returns:
//		A pointer to the element with a matching key or NULL if none found.
//==========================================================================

template<class ClassType, class KeyType>
inline
const ClassType *BinarySearch (const ClassType *first, int max,
	const KeyType ClassType::*keyptr, const KeyType key)
{
	int min = 0;
	--max;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		const ClassType *probe = &first[mid];
		const KeyType &seekey = probe->*keyptr;
		if (seekey == key)
		{
			return probe;
		}
		else if (seekey < key)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//==========================================================================
//
// BinarySearchFlexible
//
// THIS DOES NOT WORK RIGHT WITH VISUAL C++
// ONLY ONE COMPTYPE SEEMS TO BE USED FOR ANY INSTANCE OF THIS FUNCTION
// IN A DEBUG BUILD. RELEASE MIGHT BE DIFFERENT--I DIDN'T BOTHER TRYING.
//
// Similar to BinarySearch, except another function is used to copmare
// items in the array.
//
// Template parameters:
//		IndexType -		The type used to index the array (int, size_t, etc.)
//		KeyType -		The type of the key
//		CompType -		A class with a static DoCompare(IndexType, KeyType) method.
//
// Function parameters:
//		max -			The number of elements in the array
//		key -			The key value to look for
//		noIndex -		The value to return if no matching element is found.
//
// Returns:
//		The index of the matching element or noIndex.
//==========================================================================

template<class IndexType, class KeyType, class CompType>
inline
IndexType BinarySearchFlexible (IndexType max, const KeyType key, IndexType noIndex)
{
	IndexType min = 0;
	--max;

	while (min <= max)
	{
		IndexType mid = (min + max) / 2;
		int lexx = CompType::DoCompare (mid, key);
		if (lexx == 0)
		{
			return mid;
		}
		else if (lexx < 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return noIndex;
}

//==========================================================================
//
// MIN
//
// Returns the minimum of a and b.
//==========================================================================

#ifdef MIN
#undef MIN
#endif

template<class T>
inline
const T MIN (const T a, const T b)
{
	return a < b ? a : b;
}

//==========================================================================
//
// MAX
//
// Returns the maximum of a and b.
//==========================================================================

#ifdef MAX
#undef MAX
#endif

template<class T>
inline
const T MAX (const T a, const T b)
{
	return a > b ? a : b;
}

//==========================================================================
//
// clamp
//
// Clamps in to the range [min,max].
//==========================================================================

template<class T>
inline
T clamp (const T in, const T min, const T max)
{
	return in <= min ? min : in >= max ? max : in;
}

//==========================================================================
//
// swapvalues
//
// Swaps the values of a and b.
//==========================================================================

template<class T>
inline
void swapvalues (T &a, T &b)
{
	T temp = std::move(a); a = std::move(b); b = std::move(temp);
}

#endif //__TEMPLATES_H__
