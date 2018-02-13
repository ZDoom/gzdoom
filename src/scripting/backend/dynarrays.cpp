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

// We need one specific type for each of the 7 integral VM types and instantiate the needed functions for each of them.
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

DEFINE_ACTION_FUNCTION(FDynArray_I8, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_POINTER(other, FDynArray_I8);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_POINTER(other, FDynArray_I8);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(index);
	PARAM_INT(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I8);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_I8, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_I16, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_POINTER(other, FDynArray_I16);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_POINTER(other, FDynArray_I16);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(index);
	PARAM_INT(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I16);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_I16, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_I32, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_POINTER(other, FDynArray_I32);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_POINTER(other, FDynArray_I32);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(index);
	PARAM_INT(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_I32);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_I32, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_F32, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_POINTER(other, FDynArray_F32);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_POINTER(other, FDynArray_F32);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Find((float)val));
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Push((float)val));
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(index);
	PARAM_FLOAT(val);
	self->Insert(index, (float)val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F32);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_F32, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_F64, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_POINTER(other, FDynArray_F64);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_POINTER(other, FDynArray_F64);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_FLOAT(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(index);
	PARAM_FLOAT(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_F64);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_F64, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(other, FDynArray_Ptr);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(other, FDynArray_Ptr);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(val, void);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_POINTER(val, void);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(index);
	PARAM_POINTER(val, void);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Ptr);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_Ptr, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_POINTER(other, FDynArray_Obj);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_POINTER(other, FDynArray_Obj);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_OBJECT(val, DObject);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_OBJECT(val, DObject);
	GC::WriteBarrier(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(index);
	PARAM_OBJECT(val, DObject);
	GC::WriteBarrier(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_Obj);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_Obj, Clear)
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

DEFINE_ACTION_FUNCTION(FDynArray_String, Copy)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_POINTER(other, FDynArray_String);
	*self = *other;
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Move)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_POINTER(other, FDynArray_String);
	*self = std::move(*other);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Find)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_STRING(val);
	ACTION_RETURN_INT(self->Find(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Push)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_STRING(val);
	ACTION_RETURN_INT(self->Push(val));
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Pop)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	ACTION_RETURN_BOOL(self->Pop());
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Delete)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(index);
	PARAM_INT_DEF(count);
	self->Delete(index, count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Insert)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(index);
	PARAM_STRING(val);
	self->Insert(index, val);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, ShrinkToFit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	self->ShrinkToFit();
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Grow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	self->Grow(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Resize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	self->Resize(count);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Reserve)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	PARAM_INT(count);
	ACTION_RETURN_INT(self->Reserve(count));
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Max)
{
	PARAM_SELF_STRUCT_PROLOGUE(FDynArray_String);
	ACTION_RETURN_INT(self->Max());
}

DEFINE_ACTION_FUNCTION(FDynArray_String, Clear)
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
