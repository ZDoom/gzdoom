/*
** dynarray.cpp
**
** internal data types for dynamic arrays
**
**---------------------------------------------------------------------------
** Copyright 2016-2017 Christoph Oelckers
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "tarray.h"
#include "dobject.h"
#include "vm.h"
#include "types.h"

// We need one specific type for each of the 8 integral VM types and instantiate the needed functions for each of them.
// Dynamic arrays cannot hold structs because for every type there'd need to be an internal implementation which is impossible.

typedef TArray<uint8_t> FDynArray_I8;
typedef TArray<uint16_t> FDynArray_I16;
typedef TArray<uint32_t> FDynArray_I32;
typedef TArray<float> FDynArray_F32;
typedef TArray<double> FDynArray_F64;
typedef TArray<void*> FDynArray_Ptr;
typedef TArray<DObject*> FDynArray_Obj;
typedef TArray<FString> FDynArray_String;

//-----------------------------------------------------
//
// Int8 array
//
//-----------------------------------------------------

template<class T> void ArrayCopy(T *self, T *other)
{
	*self = *other;
}

template<class T> void ArrayMove(T *self, T* other)
{
	*self = std::move(*other);
}

template<class T, class U> int ArrayFind(T *self, U val)
{
	return self->Find(static_cast<typename T::value_type>(val));
}

template<class T, class U> int ArrayPush(T *self, U val)
{
	return self->Push(static_cast<typename T::value_type>(val));
}

template<class T> int ArrayPop(T *self)
{
	return self->Pop();
}

template<class T> void ArrayDelete(T *self, int index, int count)
{
	self->Delete(index, count);
}

template<class T, class U, int fill = 1> void ArrayInsert(T *self, int index, U val)
{
	//int oldSize = self->Size();
	self->Insert(index, static_cast<typename T::value_type>(val));
	// Is this even necessary? All Insert does is inserting one defined element into the array and moving the rest.
	// It never creates empty tailing entries. fillcount in the macro will always be 0
	//if (fill) { DYNARRAY_FILL_ITEMS_SKIP(1); }
}

template<class T> void ArrayShrinkToFit(T *self)
{
	self->ShrinkToFit();
}

template<class T> void ArrayGrow(T *self, int amount)
{
	self->Grow(amount);
}

template<class T, int fill = 1> void ArrayResize(T *self, int amount)
{
	int oldSize = self->Size();
	self->Resize(amount);
	if (fill)
	{
		// This must ensure that all new entries get cleared.
		const int fillCount = int(self->Size() - oldSize);
		if (fillCount > 0) memset(&(*self)[oldSize], 0, sizeof(*self)[0] * fillCount);
	}
}

template<class T> unsigned int ArrayReserve(T *self, int amount)
{
	return self->Reserve(amount);
}

template<class T> int ArrayMax(T *self)
{
	return self->Max();
}

template<class T> void ArrayClear(T *self)
{
	self->Clear();
}

// without this the two-argument templates cannot be used in macros.
#define COMMA ,

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Copy, ArrayCopy<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_POINTER(other, FDynArray_I8);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Move, ArrayMove<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_POINTER(other, FDynArray_I8);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Find, ArrayFind<FDynArray_I8 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Push, ArrayPush<FDynArray_I8 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Pop, ArrayPop<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Delete, ArrayDelete<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Insert, ArrayInsert<FDynArray_I8 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(index);
	PARAM_INT(val);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, ShrinkToFit, ArrayShrinkToFit<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Grow, ArrayGrow<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Resize, ArrayResize<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Reserve, ArrayReserve<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Max, ArrayMax<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I8, Clear, ArrayClear<FDynArray_I8>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	self->Clear();
	return 0;
}

//-----------------------------------------------------
//
// Int16 array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Copy, ArrayCopy<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_POINTER(other, FDynArray_I16);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Move, ArrayMove<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_POINTER(other, FDynArray_I16);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Find, ArrayFind<FDynArray_I16 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Push, ArrayPush<FDynArray_I16 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Pop, ArrayPop<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Delete, ArrayDelete<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Insert, ArrayInsert<FDynArray_I16 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(index);
	PARAM_INT(val);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, ShrinkToFit, ArrayShrinkToFit<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Grow, ArrayGrow<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Resize, ArrayResize<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Reserve, ArrayReserve<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Max, ArrayMax<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I16, Clear, ArrayClear<FDynArray_I16>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	self->Clear();
	return 0;
}

//-----------------------------------------------------
//
// Int32 array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Copy, ArrayCopy<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_POINTER(other, FDynArray_I32);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Move, ArrayMove<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_POINTER(other, FDynArray_I32);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Find, ArrayFind<FDynArray_I32 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Push, ArrayPush<FDynArray_I32 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Pop, ArrayPop<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Delete, ArrayDelete<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Insert, ArrayInsert<FDynArray_I32 COMMA int>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(index);
	PARAM_INT(val);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, ShrinkToFit, ArrayShrinkToFit<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Grow, ArrayGrow<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Resize, ArrayResize<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Reserve, ArrayReserve<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Max, ArrayMax<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_I32, Clear, ArrayClear<FDynArray_I32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	self->Clear();
	return 0;
}

//-----------------------------------------------------
//
// Float32 array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Copy, ArrayCopy<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_POINTER(other, FDynArray_F32);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Move, ArrayMove<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_POINTER(other, FDynArray_F32);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Find, ArrayFind<FDynArray_F32 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Find((float)val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Push, ArrayPush<FDynArray_F32 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Push((float)val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Pop, ArrayPop<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Delete, ArrayDelete<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Insert, ArrayInsert<FDynArray_F32 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(index);
	PARAM_FLOAT(val);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, ShrinkToFit, ArrayShrinkToFit<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Grow, ArrayGrow<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Resize, ArrayResize<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Reserve, ArrayReserve<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Max, ArrayMax<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F32, Clear, ArrayClear<FDynArray_F32>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	self->Clear();
	return 0;
}

//-----------------------------------------------------
//
// Float64 array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Copy, ArrayCopy<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_POINTER(other, FDynArray_F64);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Move, ArrayMove<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_POINTER(other, FDynArray_F64);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Find, ArrayFind<FDynArray_F64 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Push, ArrayPush<FDynArray_F64 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Pop, ArrayPop<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Delete, ArrayDelete<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Insert, ArrayInsert<FDynArray_F64 COMMA double>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(index);
	PARAM_FLOAT(val);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, ShrinkToFit, ArrayShrinkToFit<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Grow, ArrayGrow<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Resize, ArrayResize<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Reserve, ArrayReserve<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Max, ArrayMax<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_F64, Clear, ArrayClear<FDynArray_F64>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	self->Clear();
	return 0;
}

//-----------------------------------------------------
//
// Pointer array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Copy, ArrayCopy<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(other, FDynArray_Ptr);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Move, ArrayMove<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(other, FDynArray_Ptr);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Find, ArrayFind<FDynArray_Ptr COMMA void*>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(val, void);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Push, ArrayPush<FDynArray_Ptr COMMA void*>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(val, void);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Pop, ArrayPop<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Delete, ArrayDelete<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Insert, ArrayInsert<FDynArray_Ptr COMMA void*>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(index);
	PARAM_POINTER(val, void);
	ArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, ShrinkToFit, ArrayShrinkToFit<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Grow, ArrayGrow<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Resize, ArrayResize<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Reserve, ArrayReserve<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Max, ArrayMax<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Ptr, Clear, ArrayClear<FDynArray_Ptr>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	self->Clear();
	return 0;
}


//-----------------------------------------------------
//
// Object array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Copy, ArrayCopy<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_POINTER(other, FDynArray_Obj);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Move, ArrayMove<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_POINTER(other, FDynArray_Obj);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Find, ArrayFind<FDynArray_Obj COMMA DObject*>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_OBJECT(val, DObject);
	ACTION_RETURN_INT(self->Find(val));
}

int ObjArrayPush(FDynArray_Obj *self, DObject *obj)
{
	GC::WriteBarrier(obj);
	return self->Push(obj);
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Push, ObjArrayPush)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_OBJECT(val, DObject);
	ACTION_RETURN_INT(ObjArrayPush(self, val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Pop, ArrayPop<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Delete, ArrayDelete<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

void ObjArrayInsert(FDynArray_Obj *self,int index, DObject *obj)
{
	GC::WriteBarrier(obj);
	self->Insert(index, obj);
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Insert, ObjArrayInsert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(index);
	PARAM_OBJECT(val, DObject);
	ObjArrayInsert(self, index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, ShrinkToFit, ArrayShrinkToFit<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Grow, ArrayGrow<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Resize, ArrayResize<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	ArrayResize(self, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Reserve, ArrayReserve<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Max, ArrayMax<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_Obj, Clear, ArrayClear<FDynArray_Obj>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	self->Clear();
	return 0;
}


//-----------------------------------------------------
//
// String array
//
//-----------------------------------------------------

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Copy, ArrayCopy<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_POINTER(other, FDynArray_String);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Move, ArrayMove<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_POINTER(other, FDynArray_String);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Find, ArrayFind<FDynArray_String COMMA const FString &>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_STRING(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Push, ArrayPush<FDynArray_String COMMA const FString &>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_STRING(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Pop, ArrayPop<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Delete, ArrayDelete<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(index);
	PARAM_INT(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Insert, ArrayInsert<FDynArray_String COMMA const FString & COMMA 0>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(index);
	PARAM_STRING(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, ShrinkToFit, ArrayShrinkToFit<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Grow, ArrayGrow<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Resize, ArrayResize<FDynArray_String COMMA 0>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Reserve, ArrayReserve<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Max, ArrayMax<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION_NATIVE(FDynArray_String, Clear, ArrayClear<FDynArray_String>)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	self->Clear();
	return 0;
}

DEFINE_FIELD_NAMED_X(DynArray_I8, FArray, Count, Size)		
DEFINE_FIELD_NAMED_X(DynArray_I16, FArray, Count, Size)		
DEFINE_FIELD_NAMED_X(DynArray_I32, FArray, Count, Size)		
DEFINE_FIELD_NAMED_X(DynArray_F32, FArray, Count, Size)		
DEFINE_FIELD_NAMED_X(DynArray_F64, FArray, Count, Size)		
DEFINE_FIELD_NAMED_X(DynArray_Ptr, FArray, Count, Size)	
DEFINE_FIELD_NAMED_X(DynArray_Obj, FArray, Count, Size)
DEFINE_FIELD_NAMED_X(DynArray_String, FArray, Count, Size)
