#ifndef __TEMPLATES_H__
#define __TEMPLATES_H__

#ifdef _MSC_VER
#pragma once
#endif

#include <stdlib.h>

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
// swap
//
// Swaps the values of a and b.
//==========================================================================

template<class T>
inline
void swap (T &a, T &b)
{
	T temp = a; a = b; b = temp;
}

#endif //__TEMPLATES_H__