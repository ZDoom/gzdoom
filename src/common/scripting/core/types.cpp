/*
** types.cpp
** Implements the VM type hierarchy
**
**---------------------------------------------------------------------------
** Copyright 2008-2016 Randy Heit
** Copyright 2016-2017 Cheistoph Oelckers
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

#include <cinttypes>

#include "vmintern.h"
#include "s_soundinternal.h"
#include "types.h"
#include "printf.h"
#include "textureid.h"
#include "maps.h"
#include "palettecontainer.h"
#include "texturemanager.h"
#include "i_interface.h"


FTypeTable TypeTable;

PErrorType *TypeError;
PErrorType *TypeAuto;
PVoidType *TypeVoid;
PInt *TypeSInt8,  *TypeUInt8;
PInt *TypeSInt16, *TypeUInt16;
PInt *TypeSInt32, *TypeUInt32;
PBool *TypeBool;
PFloat *TypeFloat32, *TypeFloat64;
PString *TypeString;
PName *TypeName;
PSound *TypeSound;
PColor *TypeColor;
PTextureID *TypeTextureID;
PTranslationID* TypeTranslationID;
PSpriteID *TypeSpriteID;
PStatePointer *TypeState;
PPointer *TypeFont;
PStateLabel *TypeStateLabel;
PStruct *TypeVector2;
PStruct *TypeVector3;
PStruct* TypeVector4;
PStruct* TypeQuaternion;
PStruct* TypeFVector2;
PStruct* TypeFVector3;
PStruct* TypeFVector4;
PStruct* TypeFQuaternion;
PStruct *TypeColorStruct;
PStruct *TypeStringStruct;
PStruct* TypeQuaternionStruct;
PPointer *TypeNullPtr;
PPointer *TypeVoidPtr;
PPointer *TypeRawFunction;
PPointer* TypeVMFunction;


// CODE --------------------------------------------------------------------

void DumpTypeTable()
{
	int used = 0;
	int min = INT_MAX;
	int max = 0;
	int all = 0;
	int lens[10] = {0};
	for (size_t i = 0; i < countof(TypeTable.TypeHash); ++i)
	{
		int len = 0;
		Printf("%4zu:", i);
		for (PType *ty = TypeTable.TypeHash[i]; ty != nullptr; ty = ty->HashNext)
		{
			Printf(" -> %s", ty->DescriptiveName());
			len++;
			all++;
		}
		if (len != 0)
		{
			used++;
			if (len < min)
				min = len;
			if (len > max)
				max = len;
		}
		if (len < (int)countof(lens))
		{
			lens[len]++;
		}
		Printf("\n");
	}
	Printf("Used buckets: %d/%zu (%.2f%%) for %d entries\n", used, countof(TypeTable.TypeHash), double(used)/countof(TypeTable.TypeHash)*100, all);
	Printf("Min bucket size: %d\n", min);
	Printf("Max bucket size: %d\n", max);
	Printf("Avg bucket size: %.2f\n", double(all) / used);
	int j,k;
	for (k = countof(lens)-1; k > 0; --k)
		if (lens[k])
			break;
	for (j = 0; j <= k; ++j)
		Printf("Buckets of len %d: %d (%.2f%%)\n", j, lens[j], j!=0?double(lens[j])/used*100:-1.0);
}

/* PType ******************************************************************/

//==========================================================================
//
// PType Parameterized Constructor
//
//==========================================================================

PType::PType(unsigned int size, unsigned int align)
: Size(size), Align(align), HashNext(nullptr)
{
	mDescriptiveName = "Type";
	loadOp = OP_NOP;
	storeOp = OP_NOP;
	moveOp = OP_NOP;
	RegType = REGT_NIL;
	RegCount = 1;
}

//==========================================================================
//
// PType Destructor
//
//==========================================================================

PType::~PType()
{
}

//==========================================================================
//
// PType :: WriteValue
//
//==========================================================================

void PType::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	assert(0 && "Cannot write value for this type");
}

//==========================================================================
//
// PType :: ReadValue
//
//==========================================================================

bool PType::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	assert(0 && "Cannot read value for this type");
	return false;
}

//==========================================================================
//
// PType :: SetDefaultValue
//
//==========================================================================

void PType::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *stroffs)
{
}

//==========================================================================
//
// PType :: SetPointer*
//
//==========================================================================

void PType::SetPointer(void *base, unsigned offset, TArray<size_t> *stroffs)
{
}

void PType::SetPointerArray(void *base, unsigned offset, TArray<size_t> *stroffs)
{
}

void PType::SetPointerMap(void *base, unsigned offset, TArray<std::pair<size_t,PType *>> *ptrofs)
{
}

//==========================================================================
//
// PType :: InitializeValue
//
//==========================================================================

void PType::InitializeValue(void *addr, const void *def) const
{
}

//==========================================================================
//
// PType :: DestroyValue
//
//==========================================================================

void PType::DestroyValue(void *addr) const
{
}

//==========================================================================
//
// PType :: SetValue
//
//==========================================================================

void PType::SetValue(void *addr, int val)
{
	assert(0 && "Cannot set int value for this type");
}

void PType::SetValue(void *addr, double val)
{
	assert(0 && "Cannot set float value for this type");
}

//==========================================================================
//
// PType :: GetValue
//
//==========================================================================

int PType::GetValueInt(void *addr) const
{
	assert(0 && "Cannot get value for this type");
	return 0;
}

double PType::GetValueFloat(void *addr) const
{
	assert(0 && "Cannot get value for this type");
	return 0;
}

//==========================================================================
//
// PType :: IsMatch
//
//==========================================================================

bool PType::IsMatch(intptr_t id1, intptr_t id2) const
{
	return false;
}

//==========================================================================
//
// PType :: GetTypeIDs
//
//==========================================================================

void PType::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = 0;
	id2 = 0;
}

//==========================================================================
//
// PType :: GetTypeIDs
//
//==========================================================================

const char *PType::DescriptiveName() const
{
	return mDescriptiveName.GetChars();
}

//==========================================================================
//
// PType :: StaticInit												STATIC
//
//==========================================================================

void PType::StaticInit()
{
	// Create types and add them type the type table.
	TypeTable.AddType(TypeError = new PErrorType, NAME_None);
	TypeTable.AddType(TypeAuto = new PErrorType(2), NAME_None);
	TypeTable.AddType(TypeVoid = new PVoidType, NAME_Void);
	TypeTable.AddType(TypeSInt8 = new PInt(1, false), NAME_Int);
	TypeTable.AddType(TypeUInt8 = new PInt(1, true), NAME_Int);
	TypeTable.AddType(TypeSInt16 = new PInt(2, false), NAME_Int);
	TypeTable.AddType(TypeUInt16 = new PInt(2, true), NAME_Int);
	TypeTable.AddType(TypeSInt32 = new PInt(4, false), NAME_Int);
	TypeTable.AddType(TypeUInt32 = new PInt(4, true), NAME_Int);
	TypeTable.AddType(TypeBool = new PBool, NAME_Bool);
	TypeTable.AddType(TypeFloat32 = new PFloat(4), NAME_Float);
	TypeTable.AddType(TypeFloat64 = new PFloat(8), NAME_Float);
	TypeTable.AddType(TypeString = new PString, NAME_String);
	TypeTable.AddType(TypeName = new PName, NAME_Name);
	TypeTable.AddType(TypeSound = new PSound, NAME_Sound);
	TypeTable.AddType(TypeColor = new PColor, NAME_Color);
	TypeTable.AddType(TypeState = new PStatePointer, NAME_Pointer);
	TypeTable.AddType(TypeStateLabel = new PStateLabel, NAME_Label);
	TypeTable.AddType(TypeNullPtr = new PPointer, NAME_Pointer);
	TypeTable.AddType(TypeSpriteID = new PSpriteID, NAME_SpriteID);
	TypeTable.AddType(TypeTextureID = new PTextureID, NAME_TextureID);
	TypeTable.AddType(TypeTranslationID = new PTranslationID, NAME_TranslationID);

	TypeVoidPtr = NewPointer(TypeVoid, false);
	TypeRawFunction = new PPointer;
		TypeRawFunction->mDescriptiveName = "Raw Function Pointer";
	TypeVMFunction = NewPointer(NewStruct("VMFunction", nullptr, true));
	TypeColorStruct = NewStruct("@ColorStruct", nullptr);	//This name is intentionally obfuscated so that it cannot be used explicitly. The point of this type is to gain access to the single channels of a color value.
	TypeStringStruct = NewStruct("Stringstruct", nullptr, true);
	TypeQuaternionStruct = NewStruct("QuatStruct", nullptr, true);
	TypeFont = NewPointer(NewStruct("Font", nullptr, true));
#ifdef __BIG_ENDIAN__
	TypeColorStruct->AddField(NAME_a, TypeUInt8);
	TypeColorStruct->AddField(NAME_r, TypeUInt8);
	TypeColorStruct->AddField(NAME_g, TypeUInt8);
	TypeColorStruct->AddField(NAME_b, TypeUInt8);
#else
	TypeColorStruct->AddField(NAME_b, TypeUInt8);
	TypeColorStruct->AddField(NAME_g, TypeUInt8);
	TypeColorStruct->AddField(NAME_r, TypeUInt8);
	TypeColorStruct->AddField(NAME_a, TypeUInt8);
#endif

	TypeVector2 = new PStruct(NAME_Vector2, nullptr);
	TypeVector2->AddField(NAME_X, TypeFloat64);
	TypeVector2->AddField(NAME_Y, TypeFloat64);
	TypeTable.AddType(TypeVector2, NAME_Struct);
	TypeVector2->loadOp = OP_LV2;
	TypeVector2->storeOp = OP_SV2;
	TypeVector2->moveOp = OP_MOVEV2;
	TypeVector2->RegType = REGT_FLOAT;
	TypeVector2->RegCount = 2;
	TypeVector2->isOrdered = true;

	TypeVector3 = new PStruct(NAME_Vector3, nullptr);
	TypeVector3->AddField(NAME_X, TypeFloat64);
	TypeVector3->AddField(NAME_Y, TypeFloat64);
	TypeVector3->AddField(NAME_Z, TypeFloat64);
	// allow accessing xy as a vector2. This is not supposed to be serialized so it's marked transient
	TypeVector3->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeVector3, NAME_Struct);
	TypeVector3->loadOp = OP_LV3;
	TypeVector3->storeOp = OP_SV3;
	TypeVector3->moveOp = OP_MOVEV3;
	TypeVector3->RegType = REGT_FLOAT;
	TypeVector3->RegCount = 3;
	TypeVector3->isOrdered = true;

	TypeVector4 = new PStruct(NAME_Vector4, nullptr);
	TypeVector4->AddField(NAME_X, TypeFloat64);
	TypeVector4->AddField(NAME_Y, TypeFloat64);
	TypeVector4->AddField(NAME_Z, TypeFloat64);
	TypeVector4->AddField(NAME_W, TypeFloat64);
	// allow accessing xyz as a vector3. This is not supposed to be serialized so it's marked transient
	TypeVector4->Symbols.AddSymbol(Create<PField>(NAME_XYZ, TypeVector3, VARF_Transient, 0));
	TypeVector4->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeVector4, NAME_Struct);
	TypeVector4->loadOp = OP_LV4;
	TypeVector4->storeOp = OP_SV4;
	TypeVector4->moveOp = OP_MOVEV4;
	TypeVector4->RegType = REGT_FLOAT;
	TypeVector4->RegCount = 4;
	TypeVector4->isOrdered = true;


	TypeFVector2 = new PStruct(NAME_FVector2, nullptr);
	TypeFVector2->AddField(NAME_X, TypeFloat32);
	TypeFVector2->AddField(NAME_Y, TypeFloat32);
	TypeTable.AddType(TypeFVector2, NAME_Struct);
	TypeFVector2->loadOp = OP_LFV2;
	TypeFVector2->storeOp = OP_SFV2;
	TypeFVector2->moveOp = OP_MOVEV2;
	TypeFVector2->RegType = REGT_FLOAT;
	TypeFVector2->RegCount = 2;
	TypeFVector2->isOrdered = true;

	TypeFVector3 = new PStruct(NAME_FVector3, nullptr);
	TypeFVector3->AddField(NAME_X, TypeFloat32);
	TypeFVector3->AddField(NAME_Y, TypeFloat32);
	TypeFVector3->AddField(NAME_Z, TypeFloat32);
	// allow accessing xy as a vector2
	TypeFVector3->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeFVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeFVector3, NAME_Struct);
	TypeFVector3->loadOp = OP_LFV3;
	TypeFVector3->storeOp = OP_SFV3;
	TypeFVector3->moveOp = OP_MOVEV3;
	TypeFVector3->RegType = REGT_FLOAT;
	TypeFVector3->RegCount = 3;
	TypeFVector3->isOrdered = true;

	TypeFVector4 = new PStruct(NAME_FVector4, nullptr);
	TypeFVector4->AddField(NAME_X, TypeFloat32);
	TypeFVector4->AddField(NAME_Y, TypeFloat32);
	TypeFVector4->AddField(NAME_Z, TypeFloat32);
	TypeFVector4->AddField(NAME_W, TypeFloat32);
	// allow accessing xyz as a vector3
	TypeFVector4->Symbols.AddSymbol(Create<PField>(NAME_XYZ, TypeFVector3, VARF_Transient, 0));
	TypeFVector4->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeFVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeFVector4, NAME_Struct);
	TypeFVector4->loadOp = OP_LFV4;
	TypeFVector4->storeOp = OP_SFV4;
	TypeFVector4->moveOp = OP_MOVEV4;
	TypeFVector4->RegType = REGT_FLOAT;
	TypeFVector4->RegCount = 4;
	TypeFVector4->isOrdered = true;


	TypeQuaternion = new PStruct(NAME_Quat, nullptr);
	TypeQuaternion->AddField(NAME_X, TypeFloat64);
	TypeQuaternion->AddField(NAME_Y, TypeFloat64);
	TypeQuaternion->AddField(NAME_Z, TypeFloat64);
	TypeQuaternion->AddField(NAME_W, TypeFloat64);
	// allow vector access.
	TypeQuaternion->Symbols.AddSymbol(Create<PField>(NAME_XYZ, TypeVector3, VARF_Transient, 0));
	TypeQuaternion->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeQuaternion, NAME_Struct);
	TypeQuaternion->loadOp = OP_LV4;
	TypeQuaternion->storeOp = OP_SV4;
	TypeQuaternion->moveOp = OP_MOVEV4;
	TypeQuaternion->RegType = REGT_FLOAT;
	TypeQuaternion->RegCount = 4;
	TypeQuaternion->isOrdered = true;

	TypeFQuaternion = new PStruct(NAME_FQuat, nullptr);
	TypeFQuaternion->AddField(NAME_X, TypeFloat32);
	TypeFQuaternion->AddField(NAME_Y, TypeFloat32);
	TypeFQuaternion->AddField(NAME_Z, TypeFloat32);
	TypeFQuaternion->AddField(NAME_W, TypeFloat32);
	// allow accessing xyz as a vector3
	TypeFQuaternion->Symbols.AddSymbol(Create<PField>(NAME_XYZ, TypeFVector3, VARF_Transient, 0));
	TypeFQuaternion->Symbols.AddSymbol(Create<PField>(NAME_XY, TypeFVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeFQuaternion, NAME_Struct);
	TypeFQuaternion->loadOp = OP_LFV4;
	TypeFQuaternion->storeOp = OP_SFV4;
	TypeFQuaternion->moveOp = OP_MOVEV4;
	TypeFQuaternion->RegType = REGT_FLOAT;
	TypeFQuaternion->RegCount = 4;
	TypeFQuaternion->isOrdered = true;


	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_sByte, TypeSInt8));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Byte, TypeUInt8));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Short, TypeSInt16));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_uShort, TypeUInt16));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Int, TypeSInt32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_uInt, TypeUInt32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Bool, TypeBool));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Float, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Double, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Float32, TypeFloat32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Float64, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_String, TypeString));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Name, TypeName));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Sound, TypeSound));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Color, TypeColor));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_State, TypeState));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Vector2, TypeVector2));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Vector3, TypeVector3));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Vector4, TypeVector4));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_Quat, TypeQuaternion));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_FVector2, TypeFVector2));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_FVector3, TypeFVector3));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_FVector4, TypeFVector4));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(Create<PSymbolType>(NAME_FQuat, TypeFQuaternion));
}


/* PBasicType *************************************************************/

//==========================================================================
//
// PBasicType Parameterized Constructor
//
//==========================================================================

PBasicType::PBasicType(unsigned int size, unsigned int align)
: PType(size, align)
{
	mDescriptiveName = "BasicType";
	Flags |= TYPE_Scalar;
}

/* PCompoundType **********************************************************/

//==========================================================================
//
// PBasicType Parameterized Constructor
//
//==========================================================================

PCompoundType::PCompoundType(unsigned int size, unsigned int align)
	: PType(size, align)
{
	mDescriptiveName = "CompoundType";
}

/* PContainerType *************************************************************/

//==========================================================================
//
// PContainerType :: IsMatch
//
//==========================================================================

bool PContainerType::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PTypeBase *outer = (const PTypeBase *)id1;
	FName name = (ENamedName)(intptr_t)id2;

	return Outer == outer && TypeName == name;
}

//==========================================================================
//
// PContainerType :: GetTypeIDs
//
//==========================================================================

void PContainerType::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)Outer;
	id2 = TypeName.GetIndex();
}

/* PInt *******************************************************************/

//==========================================================================
//
// PInt Parameterized Constructor
//
//==========================================================================

PInt::PInt(unsigned int size, bool unsign, bool compatible)
: PBasicType(size, size), Unsigned(unsign), IntCompatible(compatible)
{
	mDescriptiveName.Format("%cInt%d", unsign? 'U':'S', size);
	Flags |= TYPE_Int;

	MemberOnly = (size < 4);
	if (!unsign)
	{
		int maxval = (1u << ((8 * size) - 1)) - 1; // compute as unsigned to prevent overflow before -1
		int minval = -maxval - 1;
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(NAME_Min, this, minval));
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(NAME_Max, this, maxval));
	}
	else
	{
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(NAME_Min, this, 0u));
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(NAME_Max, this, (uint32_t) (((uint64_t) 1u << (size * 8)) - 1uL)));
	}
	SetOps();
}

void PInt::SetOps()
{
	moveOp = OP_MOVE;
	RegType = REGT_INT;
	if (Size == 4)
	{
		storeOp = OP_SW;
		loadOp = OP_LW;
	}
	else if (Size == 1)
	{
		storeOp = OP_SB;
		loadOp = Unsigned ? OP_LBU : OP_LB;
	}
	else if (Size == 2)
	{
		storeOp = OP_SH;
		loadOp = Unsigned ? OP_LHU : OP_LH;
	}
	else
	{
		assert(0 && "Unhandled integer size");
		storeOp = OP_NOP;
	}
}

//==========================================================================
//
// PInt :: WriteValue
//
//==========================================================================

void PInt::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (Size == 8 && Unsigned)
	{
		// this is a special case that cannot be represented by an int64_t.
		uint64_t val = *(uint64_t*)addr;
		ar(key, val);
	}
	else
	{
		int64_t val;
		switch (Size)
		{
		case 1:
			val = Unsigned ? *(uint8_t*)addr : *(int8_t*)addr;
			break;

		case 2:
			val = Unsigned ? *(uint16_t*)addr : *(int16_t*)addr;
			break;

		case 4:
			val = Unsigned ? *(uint32_t*)addr : *(int32_t*)addr;
			break;

		case 8:
			val = *(int64_t*)addr;
			break;

		default:
			return;	// something invalid
		}
		ar(key, val);
	}
}

//==========================================================================
//
// PInt :: ReadValue
//
//==========================================================================

bool PInt::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	NumericValue val;

	ar(key, val);
	if (val.type == NumericValue::NM_invalid) return false;	// not found or usable
	if (val.type == NumericValue::NM_float) val.signedval = (int64_t)val.floatval;

	// No need to check the unsigned state here. Downcasting to smaller types will yield the same result for both.
	switch (Size)
	{
	case 1:
		*(uint8_t*)addr = (uint8_t)val.signedval;
		break;

	case 2:
		*(uint16_t*)addr = (uint16_t)val.signedval;
		break;

	case 4:
		*(uint32_t*)addr = (uint32_t)val.signedval;
		break;

	case 8:
		*(uint64_t*)addr = (uint64_t)val.signedval;
		break;

	default:
		return false;	// something invalid
	}

	return true;
}

//==========================================================================
//
// PInt :: SetValue
//
//==========================================================================

void PInt::SetValue(void *addr, int val)
{
	assert(((intptr_t)addr & (Align - 1)) == 0 && "unaligned address");
	if (Size == 4)
	{
		*(int *)addr = val;
	}
	else if (Size == 1)
	{
		*(uint8_t *)addr = val;
	}
	else if (Size == 2)
	{
		*(uint16_t *)addr = val;
	}
	else if (Size == 8)
	{
		*(uint64_t *)addr = val;
	}
	else
	{
		assert(0 && "Unhandled integer size");
	}
}

void PInt::SetValue(void *addr, double val)
{
	SetValue(addr, (int)val);
}

//==========================================================================
//
// PInt :: GetValueInt
//
//==========================================================================

int PInt::GetValueInt(void *addr) const
{
	assert(((intptr_t)addr & (Align - 1)) == 0 && "unaligned address");
	if (Size == 4)
	{
		return *(int *)addr;
	}
	else if (Size == 1)
	{
		return Unsigned ? *(uint8_t *)addr : *(int8_t *)addr;
	}
	else if (Size == 2)
	{
		return Unsigned ? *(uint16_t *)addr : *(int16_t *)addr;
	}
	else if (Size == 8)
	{ // truncated output
		return (int)*(uint64_t *)addr;
	}
	else
	{
		assert(0 && "Unhandled integer size");
		return 0;
	}
}

//==========================================================================
//
// PInt :: GetValueFloat
//
//==========================================================================

double PInt::GetValueFloat(void *addr) const
{
	return GetValueInt(addr);
}

//==========================================================================
//
// PInt :: GetStoreOp
//
//==========================================================================

/* PBool ******************************************************************/

//==========================================================================
//
// PInt :: SetValue
//
//==========================================================================

void PBool::SetValue(void *addr, int val)
{
	*(bool*)addr = !!val;
}

void PBool::SetValue(void *addr, double val)
{
	*(bool*)addr = val != 0.;
}

int PBool::GetValueInt(void *addr) const
{
	return *(bool *)addr;
}

double PBool::GetValueFloat(void *addr) const
{
	return *(bool *)addr;
}

//==========================================================================
//
// PBool Default Constructor
//
//==========================================================================

PBool::PBool()
: PInt(sizeof(bool), true)
{
	mDescriptiveName = "Bool";
	MemberOnly = false;
	Flags |= TYPE_IntNotInt;
}

/* PFloat *****************************************************************/

//==========================================================================
//
// PFloat Parameterized Constructor
//
//==========================================================================

PFloat::PFloat(unsigned int size)
: PBasicType(size, size)
{
	mDescriptiveName.Format("Float%d", size);
	Flags |= TYPE_Float;
	if (size == 8)
	{
		if (sizeof(void*) == 4)
		{
			// Some ABIs for 32-bit platforms define alignment of double type as 4 bytes
			// Intel POSIX (System V ABI) and PowerPC Macs are examples of those
			struct AlignmentCheck { uint8_t i; double d; };
			Align = static_cast<unsigned int>(offsetof(AlignmentCheck, d));
		}

		SetDoubleSymbols();
	}
	else
	{
		assert(size == 4);
		MemberOnly = true;
		SetSingleSymbols();
	}
	SetOps();
}

//==========================================================================
//
// PFloat :: SetDoubleSymbols
//
// Setup constant values for 64-bit floats.
//
//==========================================================================

void PFloat::SetDoubleSymbols()
{
	static const SymbolInitF symf[] =
	{
		{ NAME_Min_Normal,		DBL_MIN },
		{ NAME_Max,				DBL_MAX },
		{ NAME_Epsilon,			DBL_EPSILON },
		{ NAME_Equal_Epsilon,	EQUAL_EPSILON },
		{ NAME_NaN,				std::numeric_limits<double>::quiet_NaN() },
		{ NAME_Infinity,		std::numeric_limits<double>::infinity() },
		{ NAME_Min_Denormal,	std::numeric_limits<double>::denorm_min() }
	};
	static const SymbolInitI symi[] =
	{
		{ NAME_Dig,				DBL_DIG },
		{ NAME_Min_Exp,			DBL_MIN_EXP },
		{ NAME_Max_Exp,			DBL_MAX_EXP },
		{ NAME_Mant_Dig,		DBL_MANT_DIG },
		{ NAME_Min_10_Exp,		DBL_MIN_10_EXP },
		{ NAME_Max_10_Exp,		DBL_MAX_10_EXP }
	};
	SetSymbols(symf, countof(symf));
	SetSymbols(symi, countof(symi));
}

//==========================================================================
//
// PFloat :: SetSingleSymbols
//
// Setup constant values for 32-bit floats.
//
//==========================================================================

void PFloat::SetSingleSymbols()
{
	static const SymbolInitF symf[] =
	{
		{ NAME_Min_Normal,		FLT_MIN },
		{ NAME_Max,				FLT_MAX },
		{ NAME_Epsilon,			FLT_EPSILON },
		{ NAME_Equal_Epsilon,	(float)EQUAL_EPSILON },
		{ NAME_NaN,				std::numeric_limits<float>::quiet_NaN() },
		{ NAME_Infinity,		std::numeric_limits<float>::infinity() },
		{ NAME_Min_Denormal,	std::numeric_limits<float>::denorm_min() }
	};
	static const SymbolInitI symi[] =
	{
		{ NAME_Dig,				FLT_DIG },
		{ NAME_Min_Exp,			FLT_MIN_EXP },
		{ NAME_Max_Exp,			FLT_MAX_EXP },
		{ NAME_Mant_Dig,		FLT_MANT_DIG },
		{ NAME_Min_10_Exp,		FLT_MIN_10_EXP },
		{ NAME_Max_10_Exp,		FLT_MAX_10_EXP }
	};
	SetSymbols(symf, countof(symf));
	SetSymbols(symi, countof(symi));
}

//==========================================================================
//
// PFloat :: SetSymbols
//
//==========================================================================

void PFloat::SetSymbols(const PFloat::SymbolInitF *sym, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(sym[i].Name, this, sym[i].Value));
	}
}

void PFloat::SetSymbols(const PFloat::SymbolInitI *sym, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		Symbols.AddSymbol(Create<PSymbolConstNumeric>(sym[i].Name, this, sym[i].Value));
	}
}

//==========================================================================
//
// PFloat :: WriteValue
//
//==========================================================================

void PFloat::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (Size == 8)
	{
		ar(key, *(double*)addr);
	}
	else
	{
		ar(key, *(float*)addr);
	}
}

//==========================================================================
//
// PFloat :: ReadValue
//
//==========================================================================

bool PFloat::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	NumericValue val;

	ar(key, val);
	if (val.type == NumericValue::NM_invalid) return false;	// not found or usable
	else if (val.type == NumericValue::NM_signed) val.floatval = (double)val.signedval;
	else if (val.type == NumericValue::NM_unsigned) val.floatval = (double)val.unsignedval;

	if (Size == 8)
	{
		*(double*)addr = val.floatval;
	}
	else
	{
		*(float*)addr = (float)val.floatval;
	}
	return true;
}

//==========================================================================
//
// PFloat :: SetValue
//
//==========================================================================

void PFloat::SetValue(void *addr, int val)
{
	return SetValue(addr, (double)val);
}

void PFloat::SetValue(void *addr, double val)
{
	assert(((intptr_t)addr & (Align - 1)) == 0 && "unaligned address");
	if (Size == 4)
	{
		*(float *)addr = (float)val;
	}
	else
	{
		assert(Size == 8);
		*(double *)addr = val;
	}
}

//==========================================================================
//
// PFloat :: GetValueInt
//
//==========================================================================

int PFloat::GetValueInt(void *addr) const
{
	return int(GetValueFloat(addr));
}

//==========================================================================
//
// PFloat :: GetValueFloat
//
//==========================================================================

double PFloat::GetValueFloat(void *addr) const
{
	assert(((intptr_t)addr & (Align - 1)) == 0 && "unaligned address");
	if (Size == 4)
	{
		return *(float *)addr;
	}
	else
	{
		assert(Size == 8);
		return *(double *)addr;
	}
}

//==========================================================================
//
// PFloat :: GetStoreOp
//
//==========================================================================

void PFloat::SetOps()
{
	if (Size == 4)
	{
		storeOp = OP_SSP;
		loadOp = OP_LSP;
	}
	else
	{
		assert(Size == 8);
		storeOp = OP_SDP;
		loadOp = OP_LDP;
	}
	moveOp = OP_MOVEF;
	RegType = REGT_FLOAT;
}

/* PString ****************************************************************/

//==========================================================================
//
// PString Default Constructor
//
//==========================================================================

PString::PString()
: PBasicType(sizeof(FString), alignof(FString))
{
	mDescriptiveName = "String";
	storeOp = OP_SS;
	loadOp = OP_LS;
	moveOp = OP_MOVES;
	RegType = REGT_STRING;

}

//==========================================================================
//
// PString :: WriteValue
//
//==========================================================================

void PString::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	ar(key, *(FString*)addr);
}

//==========================================================================
//
// PString :: ReadValue
//
//==========================================================================

bool PString::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	const char *cptr;
	ar.StringPtr(key, cptr);
	if (cptr == nullptr)
	{
		return false;
	}
	else
	{
		*(FString*)addr = cptr;
		return true;
	}
}

//==========================================================================
//
// PString :: SetDefaultValue
//
//==========================================================================

void PString::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	if (base != nullptr) new((uint8_t *)base + offset) FString;
	if (special != nullptr)
	{
		special->Push(std::make_pair(this, offset));
	}
}

//==========================================================================
//
// PString :: InitializeValue
//
//==========================================================================

void PString::InitializeValue(void *addr, const void *def) const
{
	if (def != nullptr)
	{
		new(addr) FString(*(FString *)def);
	}
	else
	{
		new(addr) FString;
	}
}

//==========================================================================
//
// PString :: DestroyValue
//
//==========================================================================

void PString::DestroyValue(void *addr) const
{
	((FString *)addr)->~FString();
}

/* PName ******************************************************************/

//==========================================================================
//
// PName Default Constructor
//
//==========================================================================

PName::PName()
: PInt(sizeof(FName), true, false)
{
	mDescriptiveName = "Name";
	Flags |= TYPE_IntNotInt;
	static_assert(sizeof(FName) == alignof(FName), "Name not properly aligned");
}

//==========================================================================
//
// PName :: WriteValue
//
//==========================================================================

void PName::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	const char *cptr = ((const FName*)addr)->GetChars();
	ar.StringPtr(key, cptr);
}

//==========================================================================
//
// PName :: ReadValue
//
//==========================================================================

bool PName::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	const char *cptr;
	ar.StringPtr(key, cptr);
	if (cptr == nullptr)
	{
		return false;
	}
	else
	{
		*(FName*)addr = FName(cptr);
		return true;
	}
}

/* PStatePointer **********************************************************/

//==========================================================================
//
// PStatePointer Default Constructor
//
//==========================================================================

PStatePointer::PStatePointer()
{
	mDescriptiveName = "Pointer<State>";
	PointedType = NewStruct(NAME_State, nullptr, true);
	IsConst = true;
}

//==========================================================================
//
// PStatePointer :: WriteValue
//
//==========================================================================

void PStatePointer::WriteValue(FSerializer& ar, const char* key, const void* addr) const
{
	ar.StatePointer(key, const_cast<void*>(addr), nullptr);
}

//==========================================================================
//
// PStatePointer :: ReadValue
//
//==========================================================================

bool PStatePointer::ReadValue(FSerializer& ar, const char* key, void* addr) const
{
	bool res = false;
	ar.StatePointer(key, addr, &res);
	return res;
}


/* PSpriteID ******************************************************************/

//==========================================================================
//
// PName Default Constructor
//
//==========================================================================

PSpriteID::PSpriteID()
	: PInt(sizeof(int), true, true)
{
	Flags |= TYPE_IntNotInt;
	mDescriptiveName = "SpriteID";
}

//==========================================================================
//
// PName :: WriteValue
//
//==========================================================================

void PSpriteID::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	int32_t val = *(int*)addr;
	ar.Sprite(key, val, nullptr);
}

//==========================================================================
//
// PName :: ReadValue
//
//==========================================================================

bool PSpriteID::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	int32_t val = 0;
	ar.Sprite(key, val, nullptr);
	*(int*)addr = val;
	return true;
}

/* PTextureID ******************************************************************/

//==========================================================================
//
// PTextureID Default Constructor
//
//==========================================================================

PTextureID::PTextureID()
	: PInt(sizeof(FTextureID), true, false)
{
	mDescriptiveName = "TextureID";
	Flags |= TYPE_IntNotInt;
	static_assert(sizeof(FTextureID) == alignof(FTextureID), "TextureID not properly aligned");
}

//==========================================================================
//
// PTextureID :: WriteValue
//
//==========================================================================

void PTextureID::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	FTextureID val = *(FTextureID*)addr;
	ar(key, val);
}

//==========================================================================
//
// PTextureID :: ReadValue
//
//==========================================================================

bool PTextureID::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	FTextureID val;
	ar(key, val);
	*(FTextureID*)addr = val;
	return true;
}

/* PTranslationID ******************************************************************/

//==========================================================================
//
// PTranslationID Default Constructor
//
//==========================================================================

PTranslationID::PTranslationID()
	: PInt(sizeof(FTranslationID), true, false)
{
	mDescriptiveName = "TranslationID";
	Flags |= TYPE_IntNotInt;
	static_assert(sizeof(FTranslationID) == alignof(FTranslationID), "TranslationID not properly aligned");
}

//==========================================================================
//
// PTranslationID :: WriteValue
//
//==========================================================================

void PTranslationID::WriteValue(FSerializer& ar, const char* key, const void* addr) const
{
	FTranslationID val = *(FTranslationID*)addr;
	ar(key, val);
}

//==========================================================================
//
// PTranslationID :: ReadValue
//
//==========================================================================

bool PTranslationID::ReadValue(FSerializer& ar, const char* key, void* addr) const
{
	FTranslationID val;
	ar(key, val);
	*(FTranslationID*)addr = val;
	return true;
}

/* PSound *****************************************************************/

//==========================================================================
//
// PSound Default Constructor
//
//==========================================================================

PSound::PSound()
: PInt(sizeof(FSoundID), true)
{
	mDescriptiveName = "Sound";
	Flags |= TYPE_IntNotInt;
	static_assert(sizeof(FSoundID) == alignof(FSoundID), "SoundID not properly aligned");
}

//==========================================================================
//
// PSound :: WriteValue
//
//==========================================================================

void PSound::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	const char *cptr = soundEngine->GetSoundName(*(const FSoundID *)addr);
	ar.StringPtr(key, cptr);
}

//==========================================================================
//
// PSound :: ReadValue
//
//==========================================================================

bool PSound::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	const char *cptr;
	ar.StringPtr(key, cptr);
	if (cptr == nullptr)
	{
		return false;
	}
	else
	{
		*(FSoundID *)addr = S_FindSound(cptr);
		return true;
	}
}

/* PColor *****************************************************************/

//==========================================================================
//
// PColor Default Constructor
//
//==========================================================================

PColor::PColor()
: PInt(sizeof(PalEntry), true)
{
	mDescriptiveName = "Color";
	Flags |= TYPE_IntNotInt;
	static_assert(sizeof(PalEntry) == alignof(PalEntry), "PalEntry not properly aligned");
}

/* PStateLabel *****************************************************************/

//==========================================================================
//
// PStateLabel Default Constructor
//
//==========================================================================

PStateLabel::PStateLabel()
	: PInt(sizeof(int), false, false)
{
	Flags |= TYPE_IntNotInt;
	mDescriptiveName = "StateLabel";
}

/* PPointer ***************************************************************/

//==========================================================================
//
// PPointer - Default Constructor
//
//==========================================================================

PPointer::PPointer()
: PBasicType(sizeof(void *), alignof(void *)), PointedType(nullptr), IsConst(false)
{
	mDescriptiveName = "NullPointer";
	loadOp = OP_LP;
	storeOp = OP_SP;
	moveOp = OP_MOVEA;
	RegType = REGT_POINTER;
	Flags |= TYPE_Pointer;
}

//==========================================================================
//
// PPointer - Parameterized Constructor
//
//==========================================================================

PPointer::PPointer(PType *pointsat, bool isconst)
: PBasicType(sizeof(void *), alignof(void *)), PointedType(pointsat), IsConst(isconst)
{
	if (pointsat != nullptr)
	{
		mDescriptiveName.Format("Pointer<%s%s>", pointsat->DescriptiveName(), isconst ? "readonly " : "");
		mVersion = pointsat->mVersion;
	}
	else
	{
		mDescriptiveName = "Pointer";
		mVersion = 0;
	}
	loadOp = OP_LP;
	storeOp = OP_SP;
	moveOp = OP_MOVEA;
	RegType = REGT_POINTER;
	Flags |= TYPE_Pointer;
}

//==========================================================================
//
// PPointer :: IsMatch
//
//==========================================================================

bool PPointer::IsMatch(intptr_t id1, intptr_t id2) const
{
	assert(id2 == 0 || id2 == 1);
	PType *pointat = (PType *)id1;

	return pointat == PointedType && (!!id2) == IsConst;
}

//==========================================================================
//
// PPointer :: GetTypeIDs
//
//==========================================================================

void PPointer::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)PointedType;
	id2 = 0;
}

//==========================================================================
//
// PPointer :: WriteValue
//
//==========================================================================

void PPointer::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (writer != nullptr)
	{
		writer(ar, key, addr);
	}
	else
	{
		I_Error("Attempt to save pointer to unhandled type %s", PointedType->DescriptiveName());
	}
}

//==========================================================================
//
// PPointer :: ReadValue
//
//==========================================================================

bool PPointer::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	if (reader != nullptr)
	{
		return reader(ar, key, addr);
	}
	return false;
}

/* PObjectPointer **********************************************************/

//==========================================================================
//
// PPointer :: GetStoreOp
//
//==========================================================================

PObjectPointer::PObjectPointer(PClass *cls, bool isconst)
	: PPointer(cls->VMType, isconst)
{
	loadOp = OP_LO;
	Flags |= TYPE_ObjectPointer;
	// Non-destroyed thinkers are always guaranteed to be linked into the thinker chain so we don't need the write barrier for them.
	if (cls && !cls->IsDescendantOf(NAME_Thinker)) storeOp = OP_SO;
}

//==========================================================================
//
// PPointer :: SetPointer
//
//==========================================================================

void PObjectPointer::SetPointer(void *base, unsigned offset, TArray<size_t> *special)
{
	// Add to the list of pointers for this class.
	special->Push(offset);
}

//==========================================================================
//
// PPointer :: WriteValue
//
//==========================================================================

void PObjectPointer::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	ar(key, *(DObject **)addr);
}

//==========================================================================
//
// PPointer :: ReadValue
//
//==========================================================================

bool PObjectPointer::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	bool res;
	::Serialize(ar, key, *(DObject **)addr, nullptr, &res);
	return res;
}

//==========================================================================
//
// NewPointer
//
// Returns a PPointer to an object of the specified type
//
//==========================================================================

PPointer *NewPointer(PType *type, bool isconst)
{
	auto cp = PType::toClass(type);
	if (cp) return NewPointer(cp->Descriptor, isconst);

	size_t bucket;
	PType *ptype = TypeTable.FindType(NAME_Pointer, (intptr_t)type, isconst ? 1 : 0, &bucket);
	if (ptype == nullptr)
	{
		ptype = new PPointer(type, isconst);
		TypeTable.AddType(ptype, NAME_Pointer, (intptr_t)type, isconst ? 1 : 0, bucket);
	}
	return static_cast<PPointer *>(ptype);
}

PPointer *NewPointer(PClass *cls, bool isconst)
{
	assert(cls->VMType != nullptr);

	auto type = cls->VMType;
	size_t bucket;
	PType *ptype = TypeTable.FindType(NAME_Pointer, (intptr_t)type, isconst ? 1 : 0, &bucket);
	if (ptype == nullptr)
	{
		ptype = new PObjectPointer(cls, isconst);
		TypeTable.AddType(ptype, NAME_Pointer, (intptr_t)type, isconst ? 1 : 0, bucket);
	}
	return static_cast<PPointer *>(ptype);
}



/* PClassPointer **********************************************************/

//==========================================================================
//
// PClassPointer - Parameterized Constructor
//
//==========================================================================

PClassPointer::PClassPointer(PClass *restrict)
: PPointer(restrict->VMType), ClassRestriction(restrict)
{
	if (restrict) mDescriptiveName.Format("ClassPointer<%s>", restrict->TypeName.GetChars());
	else mDescriptiveName = "ClassPointer";
	loadOp = OP_LP;
	storeOp = OP_SP;
	Flags |= TYPE_ClassPointer;
	if (restrict) mVersion = restrict->VMType->mVersion;
	else mVersion = 0;
}

//==========================================================================
//
// PPointer :: WriteValue
//
//==========================================================================

void PClassPointer::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	ar(key, *(PClass **)addr);
}

//==========================================================================
//
// PPointer :: ReadValue
//
//==========================================================================

bool PClassPointer::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	::Serialize(ar, key, *(PClass **)addr, (PClass**)nullptr);
	return false;
}

//==========================================================================
//
// PClassPointer - isCompatible
//
//==========================================================================

bool PClassPointer::isCompatible(PType *type)
{
	auto other = PType::toClassPointer(type);
	return (other != nullptr && other->ClassRestriction->IsDescendantOf(ClassRestriction));
}

//==========================================================================
//
// PClassPointer :: SetPointer
//
//==========================================================================

void PClassPointer::SetPointer(void *base, unsigned offset, TArray<size_t> *special)
{
}

//==========================================================================
//
// PClassPointer :: IsMatch
//
//==========================================================================

bool PClassPointer::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PClass *classat = (const PClass *)id2;
	return classat == ClassRestriction;
}

//==========================================================================
//
// PClassPointer :: GetTypeIDs
//
//==========================================================================

void PClassPointer::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = 0;
	id2 = (intptr_t)ClassRestriction;
}

//==========================================================================
//
// NewClassPointer
//
// Returns a PClassPointer for the restricted type.
//
//==========================================================================

PClassPointer *NewClassPointer(PClass *restrict)
{
	size_t bucket;
	PType *ptype = TypeTable.FindType(NAME_Class, 0, (intptr_t)restrict, &bucket);
	if (ptype == nullptr)
	{
		ptype = new PClassPointer(restrict);
		TypeTable.AddType(ptype, NAME_Class, 0, (intptr_t)restrict, bucket);
	}
	return static_cast<PClassPointer *>(ptype);
}

/* PEnum ******************************************************************/

//==========================================================================
//
// PEnum - Parameterized Constructor
//
//==========================================================================

PEnum::PEnum(FName name, PTypeBase *outer)
: PInt(4, false), Outer(outer), EnumName(name)
{
	Flags |= TYPE_IntNotInt;
	mDescriptiveName.Format("Enum<%s>", name.GetChars());
}

//==========================================================================
//
// NewEnum
//
// Returns a PEnum for the given name and container, making sure not to
// create duplicates.
//
//==========================================================================

PEnum *NewEnum(FName name, PTypeBase *outer)
{
	size_t bucket;
	if (outer == nullptr) outer = Namespaces.GlobalNamespace;
	PType *etype = TypeTable.FindType(NAME_Enum, (intptr_t)outer, name.GetIndex(), &bucket);
	if (etype == nullptr)
	{
		etype = new PEnum(name, outer);
		TypeTable.AddType(etype, NAME_Enum, (intptr_t)outer, name.GetIndex(), bucket);
	}
	return static_cast<PEnum *>(etype);
}

/* PArray *****************************************************************/

//==========================================================================
//
// PArray - Parameterized Constructor
//
//==========================================================================

PArray::PArray(PType *etype, unsigned int ecount)
: ElementType(etype), ElementCount(ecount)
{
	mDescriptiveName.Format("Array<%s>[%d]", etype->DescriptiveName(), ecount);

	Align = etype->Align;
	// Since we are concatenating elements together, the element size should
	// also be padded to the nearest alignment.
	ElementSize = (etype->Size + (etype->Align - 1)) & ~(etype->Align - 1);
	Size = ElementSize * ecount;
	Flags |= TYPE_Array;
}

//==========================================================================
//
// PArray :: IsMatch
//
//==========================================================================

bool PArray::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PType *elemtype = (const PType *)id1;
	unsigned int count = (unsigned int)(intptr_t)id2;

	return elemtype == ElementType && count == ElementCount;
}

//==========================================================================
//
// PArray :: GetTypeIDs
//
//==========================================================================

void PArray::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)ElementType;
	id2 = ElementCount;
}

//==========================================================================
//
// PArray :: WriteValue
//
//==========================================================================

void PArray::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (ar.BeginArray(key))
	{
		const uint8_t *addrb = (const uint8_t *)addr;
		for (unsigned i = 0; i < ElementCount; ++i)
		{
			ElementType->WriteValue(ar, nullptr, addrb);
			addrb += ElementSize;
		}
		ar.EndArray();
	}
}

//==========================================================================
//
// PArray :: ReadValue
//
//==========================================================================

bool PArray::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	if (ar.BeginArray(key))
	{
		bool readsomething = false;
		unsigned count = ar.ArraySize();
		unsigned loop = min(count, ElementCount);
		uint8_t *addrb = (uint8_t *)addr;
		for(unsigned i=0;i<loop;i++)
		{
			readsomething |= ElementType->ReadValue(ar, nullptr, addrb);
			addrb += ElementSize;
		}
		if (loop < count)
		{
			DPrintf(DMSG_WARNING, "Array on disk (%u) is bigger than in memory (%u)\n",
				count, ElementCount);
		}
		ar.EndArray();
		return readsomething;
	}
	return false;
}

//==========================================================================
//
// PArray :: SetDefaultValue
//
//==========================================================================

void PArray::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	for (unsigned i = 0; i < ElementCount; ++i)
	{
		ElementType->SetDefaultValue(base, offset + i*ElementSize, special);
	}
}

//==========================================================================
//
// PArray :: SetDefaultValue
//
//==========================================================================

void PArray::SetPointer(void *base, unsigned offset, TArray<size_t> *special)
{
	for (unsigned i = 0; i < ElementCount; ++i)
	{
		ElementType->SetPointer(base, offset + i*ElementSize, special);
	}
}

//==========================================================================
//
// PArray :: SetPointerArray
//
//==========================================================================

void PArray::SetPointerArray(void *base, unsigned offset, TArray<size_t> *special)
{
	if (ElementType->isStruct() || ElementType->isDynArray())
	{
		for (unsigned int i = 0; i < ElementCount; ++i)
		{
			ElementType->SetPointerArray(base, offset + ElementSize * i, special);
		}
	}
}

//==========================================================================
//
// PArray :: SetPointerMap
//
//==========================================================================

void PArray::SetPointerMap(void *base, unsigned offset, TArray<std::pair<size_t,PType *>> *special)
{
	if(ElementType->isStruct() || ElementType->isMap())
	{
		for (unsigned int i = 0; i < ElementCount; ++i)
		{
			ElementType->SetPointerMap(base, offset + ElementSize * i, special);
		}
	}
}

//==========================================================================
//
// NewArray
//
// Returns a PArray for the given type and size, making sure not to create
// duplicates.
//
//==========================================================================

PArray *NewArray(PType *type, unsigned int count)
{
	size_t bucket;
	PType *atype = TypeTable.FindType(NAME_Array, (intptr_t)type, count, &bucket);
	if (atype == nullptr)
	{
		atype = new PArray(type, count);
		TypeTable.AddType(atype, NAME_Array, (intptr_t)type, count, bucket);
	}
	return (PArray *)atype;
}

/* PArray *****************************************************************/

//==========================================================================
//
// PArray - Parameterized Constructor
//
//==========================================================================

PStaticArray::PStaticArray(PType *etype)
	: PArray(etype, 0)
{
	mDescriptiveName.Format("ResizableArray<%s>", etype->DescriptiveName());
}

//==========================================================================
//
// PArray :: IsMatch
//
//==========================================================================

bool PStaticArray::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PType *elemtype = (const PType *)id1;
	unsigned int count = (unsigned int)(intptr_t)id2;

	return elemtype == ElementType && count == 0;
}

//==========================================================================
//
// PArray :: GetTypeIDs
//
//==========================================================================

void PStaticArray::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)ElementType;
	id2 = 0;
}

//==========================================================================
//
// NewStaticArray
//
// Returns a PArray for the given type and size, making sure not to create
// duplicates.
//
//==========================================================================

PStaticArray *NewStaticArray(PType *type)
{
	size_t bucket;
	PType *atype = TypeTable.FindType(NAME_StaticArray, (intptr_t)type, 0, &bucket);
	if (atype == nullptr)
	{
		atype = new PStaticArray(type);
		TypeTable.AddType(atype, NAME_StaticArray, (intptr_t)type, 0, bucket);
	}
	return (PStaticArray *)atype;
}

/* PDynArray **************************************************************/

//==========================================================================
//
// PDynArray - Parameterized Constructor
//
//==========================================================================

enum OverrideFunctionRetType {
	OFN_RET_VOID,
	OFN_RET_VAL,
	OFN_RET_KEY,
	OFN_RET_BOOL,
	OFN_RET_VAL_BOOL,
	OFN_RET_INT,
};
enum OverrideFunctionArgType {
	OFN_ARG_VOID,
	OFN_ARG_KEY,
	OFN_ARG_VAL,
	OFN_ARG_KEY_VAL,
	OFN_ARG_ELEM,
	OFN_ARG_INT_ELEM,
};

template<OverrideFunctionRetType RetType, OverrideFunctionArgType ArgType , int ExtraFlags = 0, class MT>
void CreateOverrideFunction(MT *self, FName name)
{
	auto Fn = Create<PFunction>(self->BackingType, name);
	auto NativeFn = FindFunction(self->BackingType, name.GetChars());

	assert(NativeFn);
	assert(NativeFn->VMPointer);

	TArray<PType*> ret;
	TArray<PType*> args;
	TArray<uint32_t> argflags;
	TArray<FName> argnames;

	if constexpr(RetType == OFN_RET_VAL)
	{
		ret.Push(self->ValueType);
	}
	else if constexpr(RetType == OFN_RET_KEY)
	{
		ret.Push(self->KeyType);
	}
	else if constexpr(RetType == OFN_RET_BOOL)
	{
		ret.Push(TypeBool);
	}
	else if constexpr(RetType == OFN_RET_VAL_BOOL)
	{
		ret.Push(self->ValueType);
		ret.Push(TypeBool);
	}
	else if constexpr(RetType == OFN_RET_INT)
	{
		ret.Push(TypeSInt32);
	}

	args.Push(NewPointer(self->BackingType));
	argnames.Push(NAME_self);
	argflags.Push(VARF_Implicit | VARF_ReadOnly);

	if constexpr(ArgType == OFN_ARG_KEY)
	{
		args.Push(self->KeyType);
		argflags.Push(0);
		argnames.Push(NAME_Key);
	}
	else if constexpr(ArgType == OFN_ARG_VAL)
	{

		args.Push(self->ValueType);
		argflags.Push(0);
		argnames.Push(NAME_Value);
	}
	else if constexpr(ArgType == OFN_ARG_KEY_VAL)
	{
		args.Push(self->KeyType);
		args.Push(self->ValueType);
		argflags.Push(0);
		argflags.Push(0);
		argnames.Push(NAME_Key);
		argnames.Push(NAME_Value);
	}
	else if constexpr(ArgType == OFN_ARG_ELEM)
	{
		args.Push(self->ElementType);
		argflags.Push(0);
		argnames.Push(NAME_Item);
	}
	else if constexpr(ArgType == OFN_ARG_INT_ELEM)
	{
		args.Push(TypeSInt32);
		args.Push(self->ElementType);
		argflags.Push(0);
		argflags.Push(0);
		argnames.Push(NAME_Index);
		argnames.Push(NAME_Item);
	}

	Fn->AddVariant(NewPrototype(ret, args), argflags, argnames, *NativeFn->VMPointer, VARF_Method | VARF_Native | ExtraFlags, SUF_ACTOR | SUF_OVERLAY | SUF_WEAPON | SUF_ITEM);
	self->FnOverrides.Insert(name, Fn);
}

PDynArray::PDynArray(PType *etype,PStruct *backing)
: ElementType(etype), BackingType(backing)
{
	mDescriptiveName.Format("DynArray<%s>", etype->DescriptiveName());
	Size = sizeof(FArray);
	Align = alignof(FArray);
	CreateOverrideFunction<OFN_RET_INT ,  OFN_ARG_ELEM     , VARF_ReadOnly> (this, NAME_Find);
	CreateOverrideFunction<OFN_RET_INT ,  OFN_ARG_ELEM     > (this, NAME_Push);
	CreateOverrideFunction<OFN_RET_VOID , OFN_ARG_INT_ELEM > (this, NAME_Insert);
}

//==========================================================================
//
// PDynArray :: IsMatch
//
//==========================================================================

bool PDynArray::IsMatch(intptr_t id1, intptr_t id2) const
{
	assert(id2 == 0);
	const PType *elemtype = (const PType *)id1;

	return elemtype == ElementType;
}

//==========================================================================
//
// PDynArray :: GetTypeIDs
//
//==========================================================================

void PDynArray::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)ElementType;
	id2 = 0;
}

//==========================================================================
//
// PDynArray :: InitializeValue
//
//==========================================================================

void PDynArray::InitializeValue(void *addr, const void *deff) const
{
	const FArray *def = (const FArray*)deff;
	FArray *aray = (FArray*)addr;

	if (def == nullptr || def->Count == 0)
	{
		// Empty arrays do not need construction.
		*aray = { nullptr, 0, 0 };
	}
	else if (ElementType->GetRegType() != REGT_STRING)
	{
		// These are just integral values which can be done without any constructor hackery.
		size_t blocksize = ElementType->Size * def->Count;
		aray->Array = M_Malloc(blocksize);
		memcpy(aray->Array, def->Array, blocksize);
		aray->Most = aray->Count = def->Count;
	}
	else
	{
		// non-empty string arrays require explicit construction.
		new(addr) TArray<FString>(*(TArray<FString>*)def);
	}
}

//==========================================================================
//
// PDynArray :: DestroyValue
//
//==========================================================================

void PDynArray::DestroyValue(void *addr) const
{
	FArray *aray = (FArray*)addr;

	if (aray->Array != nullptr)
	{
		if (ElementType->GetRegType() != REGT_STRING)
		{
			M_Free(aray->Array);
		}
		else
		{
			// Damn those cursed strings again. :(
			((TArray<FString>*)addr)->~TArray<FString>();
		}
	}
	aray->Count = aray->Most = 0;
	aray->Array = nullptr;
}

//==========================================================================
//
// PDynArray :: SetDefaultValue
//
//==========================================================================

void PDynArray::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	if (base != nullptr) memset((char*)base + offset, 0, sizeof(FArray));	// same as constructing an empty array.
	if (special != nullptr)
	{
		special->Push(std::make_pair(this, offset));
	}
}

//==========================================================================
//
// PDynArray :: SetPointer
//
//==========================================================================

void PDynArray::SetPointerArray(void *base, unsigned offset, TArray<size_t> *special)
{
	if (ElementType->isObjectPointer())
	{
		// Add to the list of pointer arrays for this class.
		special->Push(offset);
	}
}

//==========================================================================
//
// PDynArray :: WriteValue
//
//==========================================================================

void PDynArray::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	FArray *aray = (FArray*)addr;
	// We may skip an empty array only if it gets stored under a named key.
	// If no name is given, i.e. it's part of an outer array's element list, even empty arrays must be stored,
	// because otherwise the array would lose its entry.
	if (aray->Count > 0 || key == nullptr)	
	{
		if (ar.BeginArray(key))
		{
			const uint8_t *addrb = (const uint8_t *)aray->Array;
			for (unsigned i = 0; i < aray->Count; ++i)
			{
				ElementType->WriteValue(ar, nullptr, addrb);
				addrb += ElementType->Size;
			}
			ar.EndArray();
		}
	}
}

//==========================================================================
//
// PDynArray :: ReadValue
//
//==========================================================================

bool PDynArray::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	FArray *aray = (FArray*)addr;
	DestroyValue(addr);	// note that even after calling this we still got a validly constructed empty array.

	if (ar.BeginArray(key))
	{
		bool readsomething = false;
		unsigned count = ar.ArraySize();

		size_t blocksize = ElementType->Size * count;
		aray->Array = M_Malloc(blocksize);
		memset(aray->Array, 0, blocksize);
		aray->Most = aray->Count = count;

		uint8_t *addrb = (uint8_t *)aray->Array;
		for (unsigned i = 0; i<count; i++)
		{
			// Strings must be constructed first.
			if (ElementType->GetRegType() == REGT_STRING) new(addrb) FString;
			readsomething |= ElementType->ReadValue(ar, nullptr, addrb);
			addrb += ElementType->Size;
		}
		ar.EndArray();
		return readsomething;
	}
	return false;
}

//==========================================================================
//
// NewDynArray
//
// Creates a new DynArray of the given type, making sure not to create a
// duplicate.
//
//==========================================================================

PDynArray *NewDynArray(PType *type)
{
	size_t bucket;
	PType *atype = TypeTable.FindType(NAME_DynArray, (intptr_t)type, 0, &bucket);
	if (atype == nullptr)
	{
		FString backingname;

		switch (type->GetRegType())
		{
		case REGT_INT:
			backingname.Format("DynArray_I%d", type->Size * 8);
			break;

		case REGT_FLOAT:
			backingname.Format("DynArray_F%d", type->Size * 8);
			break;

		case REGT_STRING:
			backingname = "DynArray_String";
			break;

		case REGT_POINTER:
			if (type->isObjectPointer())
				backingname = "DynArray_Obj";
			else
				backingname = "DynArray_Ptr";
			break;

		default:
			I_Error("Unsupported dynamic array requested");
			break;
		}

		auto backing = NewStruct(backingname, nullptr, true);
		atype = new PDynArray(type, backing);
		TypeTable.AddType(atype, NAME_DynArray, (intptr_t)type, 0, bucket);
	}
	return (PDynArray *)atype;
}

/* PMap *******************************************************************/

//==========================================================================
//
// PMap - Parameterized Constructor
//
//==========================================================================

PMap::PMap(PType *keytype, PType *valtype, PStruct *backing, int backing_class)
: KeyType(keytype), ValueType(valtype), BackingType(backing), BackingClass((decltype(BackingClass)) backing_class)
{
	mDescriptiveName.Format("Map<%s, %s>", keytype->DescriptiveName(), valtype->DescriptiveName());
	Size = sizeof(ZSFMap);
	Align = alignof(ZSFMap);
	CreateOverrideFunction< OFN_RET_VAL      , OFN_ARG_KEY     > (this, NAME_Get);
	CreateOverrideFunction< OFN_RET_VAL      , OFN_ARG_KEY     , VARF_ReadOnly> (this, NAME_GetIfExists);
	CreateOverrideFunction< OFN_RET_BOOL     , OFN_ARG_KEY     , VARF_ReadOnly> (this, NAME_CheckKey);
	CreateOverrideFunction< OFN_RET_VAL_BOOL , OFN_ARG_KEY     , VARF_ReadOnly> (this, NAME_CheckValue);
	CreateOverrideFunction< OFN_RET_VOID     , OFN_ARG_KEY_VAL > (this, NAME_Insert);
	CreateOverrideFunction< OFN_RET_VOID     , OFN_ARG_KEY     > (this, NAME_InsertNew);
	CreateOverrideFunction< OFN_RET_VOID     , OFN_ARG_KEY     > (this, NAME_Remove);
}

//==========================================================================
//
// PMap :: IsMatch
//
//==========================================================================

bool PMap::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PType *keyty = (const PType *)id1;
	const PType *valty = (const PType *)id2;

	return keyty == KeyType && valty == ValueType;
}

//==========================================================================
//
// PMap :: GetTypeIDs
//
//==========================================================================

void PMap::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)KeyType;
	id2 = (intptr_t)ValueType;
}

//==========================================================================
//
// PMap :: InitializeValue
//
//==========================================================================

#define FOR_EACH_MAP_TYPE(FN) \
		case PMap::MAP_I32_I8: \
			FN( uint32_t , uint8_t ) \
		case PMap::MAP_I32_I16: \
			FN( uint32_t , uint16_t ) \
		case PMap::MAP_I32_I32: \
			FN( uint32_t , uint32_t ) \
		case PMap::MAP_I32_F32: \
			FN( uint32_t , float ) \
		case PMap::MAP_I32_F64: \
			FN( uint32_t , double ) \
		case PMap::MAP_I32_OBJ: \
			FN( uint32_t , DObject* ) \
		case PMap::MAP_I32_PTR: \
			FN( uint32_t , void* ) \
		case PMap::MAP_I32_STR: \
			FN( uint32_t , FString ) \
		case PMap::MAP_STR_I8: \
			FN( FString , uint8_t ) \
		case PMap::MAP_STR_I16: \
			FN( FString , uint16_t ) \
		case PMap::MAP_STR_I32: \
			FN( FString , uint32_t ) \
		case PMap::MAP_STR_F32: \
			FN( FString , float ) \
		case PMap::MAP_STR_F64: \
			FN( FString , double ) \
		case PMap::MAP_STR_OBJ: \
			FN( FString , DObject* ) \
		case PMap::MAP_STR_PTR: \
			FN( FString , void* ) \
		case PMap::MAP_STR_STR: \
			FN( FString , FString )

void PMap::Construct(void * addr) const {
	switch(BackingClass)
	{
		#define MAP_CONSTRUCT(KT, VT) new(addr) ZSMap< KT, VT >(); break;
		FOR_EACH_MAP_TYPE(MAP_CONSTRUCT)
		#undef MAP_CONSTRUCT
	};
}


void PMap::InitializeValue(void *addr, const void *def) const
{
	Construct(addr);
}

//==========================================================================
//
// PMap :: DestroyValue
//
//==========================================================================

void PMap::DestroyValue(void *addr) const
{
	switch(BackingClass)
	{
		#define MAP_DESTRUCT(KT, VT) static_cast<ZSMap< KT, VT >*>(addr)->~ZSMap(); break;
		FOR_EACH_MAP_TYPE(MAP_DESTRUCT)
		#undef MAP_DESTRUCT
	}
}

//==========================================================================
//
// PMap :: SetDefaultValue
//
//==========================================================================

void PMap::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	if (base != nullptr)
	{
		Construct(((uint8_t*)base)+offset); // is this needed? string/dynarray do this initialization if base != nullptr, but their initialization doesn't need to allocate
											// it might lead to double allocations (and memory leakage) for Map if both base and special != nullptr
	}
	if (special != nullptr)
	{
		special->Push(std::make_pair(this, offset));
	}
	else
	{
		I_Error("null special");
	}
}

//==========================================================================
//
// PMap :: SetPointer
//
//==========================================================================

void PMap::SetPointerMap(void *base, unsigned offset, TArray<std::pair<size_t,PType *>> *special)
{
	if (ValueType->isObjectPointer())
	{
		// Add to the list of pointer arrays for this class.
		special->Push(std::make_pair(offset,KeyType));
	}
}

//==========================================================================
//
// PMap :: WriteValue
//
//==========================================================================

template<typename M>
static void PMapValueWriter(FSerializer &ar, const M *map, const PMap *m)
{
	TMapConstIterator<typename M::KeyType, typename M::ValueType> it(*map);
	const typename M::Pair * p;
	while(it.NextPair(p))
	{
		if constexpr(std::is_same_v<typename M::KeyType,FString>)
		{
			m->ValueType->WriteValue(ar,p->Key.GetChars(),static_cast<const void *>(&p->Value));
		}
		else if constexpr(std::is_same_v<typename M::KeyType,uint32_t>)
		{
			if(m->KeyType->Flags & 8 /*TYPE_IntNotInt*/)
			{
				if(m->KeyType == TypeName)
				{
					m->ValueType->WriteValue(ar,FName(ENamedName(p->Key)).GetChars(),static_cast<const void *>(&p->Value));
				}
				else if(m->KeyType == TypeSound)
				{
					m->ValueType->WriteValue(ar,soundEngine->GetSoundName(FSoundID::fromInt(p->Key)),static_cast<const void *>(&p->Value));
				}
				else if(m->KeyType == TypeTextureID)
				{
					if(!!(p->Key & 0x8000000))
					{ // invalid
						m->ValueType->WriteValue(ar,"invalid",static_cast<const void *>(&p->Value));
					}
					else if(p->Key == 0 || p->Key >= (unsigned)TexMan.NumTextures())
					{ // null
						m->ValueType->WriteValue(ar,"null",static_cast<const void *>(&p->Value));
					}
					else
					{
						FTextureID tid;
						tid.SetIndex(p->Key);
						FGameTexture *tex = TexMan.GetGameTexture(tid);
						int lump = tex->GetSourceLump();
						unsigned useType = static_cast<unsigned>(tex->GetUseType());

						FString name;

						if (TexMan.GetLinkedTexture(lump) == tex)
						{
							name = fileSystem.GetFileFullName(lump);
						}
						else
						{
							name = tex->GetName().GetChars();
						}

						name.AppendFormat(":%u",useType);

						m->ValueType->WriteValue(ar,name.GetChars(),static_cast<const void *>(&p->Value));
					}
				}
				else
				{ // bool/color/enum/sprite/translationID
					FString key;
					key.Format("%u",p->Key);
					m->ValueType->WriteValue(ar,key.GetChars(),static_cast<const void *>(&p->Value));
				}
			}
			else
			{
				FString key;
				key.Format("%u",p->Key);
				m->ValueType->WriteValue(ar,key.GetChars(),static_cast<const void *>(&p->Value));
			}
		}
		//else unknown key type
	}
}

void PMap::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	if(ar.BeginObject(key))
	{
		switch(BackingClass)
		{
			#define MAP_WRITE(KT, VT) PMapValueWriter(ar, static_cast<const ZSMap< KT, VT >*>(addr), this); break;
			FOR_EACH_MAP_TYPE(MAP_WRITE)
			#undef MAP_WRITE
		}
		ar.EndObject();
	}
}

//==========================================================================
//
// PMap :: ReadValue
//
//==========================================================================


template<typename M>
static bool PMapValueReader(FSerializer &ar, M *map, const PMap *m)
{
	const char * k;
	while((k = ar.GetKey()))
	{
		typename M::ValueType * val = nullptr;
		if constexpr(std::is_same_v<typename M::KeyType,FString>)
		{
			val = &map->InsertNew(k);
		}
		else if constexpr(std::is_same_v<typename M::KeyType,uint32_t>)
		{
			if(m->KeyType->Flags & 8 /*TYPE_IntNotInt*/)
			{
				if(m->KeyType == TypeName)
				{
					val = &map->InsertNew(FName(k).GetIndex());
				}
				else if(m->KeyType == TypeSound)
				{
					val = &map->InsertNew(S_FindSound(k).index());
				}
				else if(m->KeyType == TypeTextureID)
				{
					FString s(k);
					FTextureID tex;
					if(s.Compare("invalid") == 0)
					{
						tex.SetInvalid();
					}
					else if(s.Compare("null") == 0)
					{
						tex.SetNull();
					}
					else
					{
						ptrdiff_t sep = s.LastIndexOf(":");
						if(sep < 0)
						{
							ar.EndObject();
							return false;
						}
						FString texName = s.Left(sep);
						FString useType = s.Mid(sep + 1);

						tex = TexMan.GetTextureID(texName.GetChars(), (ETextureType) useType.ToULong());
					}
					val = &map->InsertNew(tex.GetIndex());
				}
				else if(m->KeyType == TypeTranslationID)
				{
					FString s(k);
					if(!s.IsInt())
					{
						ar.EndObject();
						return false;
					}
					int v = (int)s.ToULong();

					if (sysCallbacks.RemapTranslation) v = sysCallbacks.RemapTranslation(FTranslationID::fromInt(v)).index();

					val = &map->InsertNew(v);
				}
				else
				{ // bool/color/enum/sprite
					FString s(k);
					if(!s.IsInt())
					{
						ar.EndObject();
						return false;
					}
					val = &map->InsertNew(static_cast<uint32_t>(s.ToULong()));
				}
			}
			else
			{
				FString s(k);
				if(!s.IsInt())
				{
					ar.EndObject();
					return false;
				}
				val = &map->InsertNew(static_cast<uint32_t>(s.ToULong()));
			}
		}
		if (!m->ValueType->ReadValue(ar,nullptr,static_cast<void*>(val)))
		{
			ar.EndObject();
			return false;
		}
	}
	ar.EndObject();
	return true;
}


bool PMap::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	DestroyValue(addr);
	InitializeValue(addr, nullptr);
	if(ar.BeginObject(key))
	{
		switch(BackingClass)
		{
			#define MAP_READ(KT, VT) return PMapValueReader(ar, static_cast<ZSMap< KT, VT >*>(addr), this);
			FOR_EACH_MAP_TYPE(MAP_READ)
			#undef MAP_READ
		}
	}
	return false;
}

//==========================================================================
//
// NewMap
//
// Returns a PMap for the given key and value types, ensuring not to create
// duplicates.
//
//==========================================================================

int PMapBackingClass(PType *keytype, PType *valuetype, FString &backingName) {
	int backingClass;
	auto key_rtype = keytype->GetRegType();
	auto value_rtype = valuetype->GetRegType();
	if (key_rtype == REGT_INT)
	{
		if(keytype->Size != 4)
		{
			I_Error("Unsupported map requested");
		}
		else
		{
			backingName += "I32_";
			backingClass = PMap::MAP_I32_I8;
		}
	}
	else if (key_rtype == REGT_STRING)
	{
		backingName += "Str_";
		backingClass = PMap::MAP_STR_I8;
	}
	else
	{
		I_Error("Unsupported map requested");
	}
	switch (valuetype->GetRegType())
	{
	case REGT_INT:
		backingName.AppendFormat("I%d", valuetype->Size * 8);
		backingClass += (valuetype->Size >> 1);
		break;
	case REGT_FLOAT:
		backingName.AppendFormat("F%d", valuetype->Size * 8);
		backingClass += PMap::MAP_I32_F32 + (valuetype->Size == 8);
		break;
	case REGT_STRING:
		backingName += "Str";
		backingClass += PMap::MAP_I32_STR;
		break;

	case REGT_POINTER:
		if (valuetype->isObjectPointer())
		{
			backingName += "Obj";
			backingClass += PMap::MAP_I32_OBJ;
		}
		else
		{
			backingName += "Ptr";
			backingClass += PMap::MAP_I32_PTR;
		}
		break;
	default:
		I_Error("Unsupported map requested");
		break;
	}
	return backingClass;
}

PMap *NewMap(PType *keyType, PType *valueType)
{
	size_t bucket;
	PType *mapType = TypeTable.FindType(NAME_Map, (intptr_t)keyType, (intptr_t)valueType, &bucket);
	if (mapType == nullptr)
	{
		FString backingName = "Map_";
		int backingClass = PMapBackingClass(keyType, valueType, backingName);
		
		auto backing = NewStruct(backingName, nullptr, true);
		mapType = new PMap(keyType, valueType, backing, backingClass);
		TypeTable.AddType(mapType, NAME_Map, (intptr_t)keyType, (intptr_t)valueType, bucket);
		
	}
	return (PMap *)mapType;
}

/* PMapIterator ***********************************************************/

//==========================================================================
//
// PMapIterator - Parameterized Constructor
//
//==========================================================================

PMapIterator::PMapIterator(PType *keytype, PType *valtype, PStruct *backing, int backing_class)
	: KeyType(keytype), ValueType(valtype), BackingType(backing), BackingClass((decltype(BackingClass)) backing_class)
{
	mDescriptiveName.Format("MapIterator<%s, %s>", keytype->DescriptiveName(), valtype->DescriptiveName());
	Size = sizeof(ZSFMap);
	Align = alignof(ZSFMap);
	CreateOverrideFunction<OFN_RET_KEY,  OFN_ARG_VOID>(this, NAME_GetKey);
	CreateOverrideFunction<OFN_RET_VAL,  OFN_ARG_VOID>(this, NAME_GetValue);
	CreateOverrideFunction<OFN_RET_VOID, OFN_ARG_VAL>(this, NAME_SetValue);
}

//==========================================================================
//
// PMapIterator :: IsMatch
//
//==========================================================================

bool PMapIterator::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PType *keyty = (const PType *)id1;
	const PType *valty = (const PType *)id2;

	return keyty == KeyType && valty == ValueType;
}

//==========================================================================
//
// PMapIterator :: GetTypeIDs
//
//==========================================================================

void PMapIterator::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)KeyType;
	id2 = (intptr_t)ValueType;
}

//==========================================================================
//
// PMapIterator :: InitializeValue
//
//==========================================================================

void PMapIterator::Construct(void * addr) const {
	switch(BackingClass)
	{
		#define MAP_IT_CONSTRUCT(KT, VT) new(addr) ZSMapIterator< KT, VT >(); break;
		FOR_EACH_MAP_TYPE(MAP_IT_CONSTRUCT)
		#undef MAP_IT_CONSTRUCT
	};
}

void PMapIterator::InitializeValue(void *addr, const void *def) const
{
	Construct(addr);
}

//==========================================================================
//
// PMapIterator :: DestroyValue
//
//==========================================================================

void PMapIterator::DestroyValue(void *addr) const
{
	switch(BackingClass)
	{
		#define MAP_IT_DESTROY(KT, VT) static_cast<ZSMapIterator< KT, VT >*>(addr)->~ZSMapIterator(); break;
		FOR_EACH_MAP_TYPE(MAP_IT_DESTROY)
		#undef MAP_IT_DESTROY
	}
}

//==========================================================================
//
// PMapIterator :: SetDefaultValue
//
//==========================================================================

void PMapIterator::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	if (base != nullptr)
	{
		Construct(((uint8_t*)base)+offset);
	}
	if (special != nullptr)
	{
		special->Push(std::make_pair(this, offset));
	}
	else
	{
		I_Error("null special");
	}
}

//==========================================================================
//
// PMapIterator :: WriteValue
//
//==========================================================================

void PMapIterator::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	ar.BeginObject(key);
	ar.EndObject();
}

//==========================================================================
//
// PMapIterator :: ReadValue
//
//==========================================================================

bool PMapIterator::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	DestroyValue(addr);
	InitializeValue(addr, nullptr);
	ar.BeginObject(key);
	ar.EndObject();
	return true;
}

//==========================================================================
//
// NewMapIterator
//
// Returns a PMapIterator for the given key and value types, ensuring not to create
// duplicates.
//
//==========================================================================

PMapIterator *NewMapIterator(PType *keyType, PType *valueType)
{
	size_t bucket;
	PType *mapIteratorType = TypeTable.FindType(NAME_MapIterator, (intptr_t)keyType, (intptr_t)valueType, &bucket);
	if (mapIteratorType == nullptr)
	{
		FString backingName = "MapIterator_";
		int backingClass = PMapBackingClass(keyType, valueType, backingName);

		auto backing = NewStruct(backingName, nullptr, true);
		mapIteratorType = new PMapIterator(keyType, valueType, backing, backingClass);
		TypeTable.AddType(mapIteratorType, NAME_MapIterator, (intptr_t)keyType, (intptr_t)valueType, bucket);
	}
	return (PMapIterator *)mapIteratorType;
}

/* PFunctionPointer *******************************************************/

//==========================================================================
//
// PFunctionPointer - Parameterized Constructor
//
//==========================================================================

static FString MakeFunctionPointerDescriptiveName(PPrototype * proto,const TArray<uint32_t> &ArgFlags, int scope)
{
	FString mDescriptiveName;

	mDescriptiveName = "Function<";
	switch(scope)
	{
	case FScopeBarrier::Side_PlainData:
		mDescriptiveName += "clearscope ";
		break;
	case FScopeBarrier::Side_Play:
		mDescriptiveName += "play ";
		break;
	case FScopeBarrier::Side_UI:
		mDescriptiveName += "ui ";
		break;
	}
	if(proto->ReturnTypes.Size() > 0)
	{
		mDescriptiveName += proto->ReturnTypes[0]->DescriptiveName();

		const unsigned n = proto->ReturnTypes.Size();
		for(unsigned i = 1; i < n; i++)
		{
			mDescriptiveName += ", ";
			mDescriptiveName += proto->ReturnTypes[i]->DescriptiveName();
		}
		mDescriptiveName += " (";
	}
	else
	{
		mDescriptiveName += "void (";
	}
	if(proto->ArgumentTypes.Size() > 0)
	{
		if(ArgFlags[0] == VARF_Out) mDescriptiveName += "out ";
		mDescriptiveName += proto->ArgumentTypes[0]->DescriptiveName();
		const unsigned n = proto->ArgumentTypes.Size();
		for(unsigned i = 1; i < n; i++)
		{
			mDescriptiveName += ", ";
			if(ArgFlags[i] == VARF_Out) mDescriptiveName += "out ";
			mDescriptiveName += proto->ArgumentTypes[i]->DescriptiveName();
		}
		mDescriptiveName += ")>";
	}
	else
	{
		mDescriptiveName += "void)>";
	}

	return mDescriptiveName;
}


FString PFunctionPointer::GenerateNameForError(const PFunction * from)
{
	return MakeFunctionPointerDescriptiveName(from->Variants[0].Proto, from->Variants[0].ArgFlags, FScopeBarrier::SideFromFlags(from->Variants[0].Flags));
}

PFunctionPointer::PFunctionPointer(PPrototype * proto, TArray<uint32_t> && argflags, int scope)
	: PPointer(proto ? (PType*) proto : TypeVoid, false), ArgFlags(std::move(argflags)), Scope(scope)
{
	if(!proto)
	{
		mDescriptiveName = "Function<void>";
	}
	else
	{
		mDescriptiveName = MakeFunctionPointerDescriptiveName(proto, ArgFlags, scope);
	}

	Flags |= TYPE_FunctionPointer;

	if(proto)
	{
		assert(Scope != -1); // for now, a scope is always required

		TArray<FName> ArgNames;
		TArray<uint32_t> ArgFlags2(ArgFlags); // AddVariant calls std::move on this, so it needs to be a copy,
											  // but it takes it as a regular reference, so it needs to be a full variable instead of a temporary
		ArgNames.Resize(ArgFlags.Size());
		FakeFunction = Create<PFunction>();
		FakeFunction->AddVariant(proto, ArgFlags2, ArgNames, nullptr, FScopeBarrier::FlagsFromSide(Scope), 0);
	}
	else
	{
		FakeFunction = nullptr;
	}
}

void PFunctionPointer::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	auto p = *(const PFunction**)(addr);
	if(p)
	{
		FunctionPointerValue val;
		FunctionPointerValue *fpv = &val;
		val.ClassName = FString((p->OwningClass ? p->OwningClass->TypeName : NAME_None).GetChars());
		val.FunctionName = FString(p->SymbolName.GetChars());
		SerializeFunctionPointer(ar, key, fpv);
	}
	else
	{
		FunctionPointerValue *fpv = nullptr;
		SerializeFunctionPointer(ar, key, fpv);
	}
}

PFunction *NativeFunctionPointerCast(PFunction *from, const PFunctionPointer *to);

bool PFunctionPointer::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	FunctionPointerValue val;
	FunctionPointerValue *fpv = &val;
	SerializeFunctionPointer(ar, key, fpv);

	PFunction ** fn = (PFunction**)(addr);

	if(fpv)
	{
		auto cls = PClass::FindClass(val.ClassName);
		if(!cls)
		{
			*fn = nullptr;
			Printf(TEXTCOLOR_RED "Function Pointer ('%s::%s'): '%s' is not a valid class\n",
				val.ClassName.GetChars(),
				val.FunctionName.GetChars(),
				val.ClassName.GetChars()
			);
			ar.mErrors++;
			return false;
		}
		auto sym = cls->FindSymbol(FName(val.FunctionName), true);
		if(!sym)
		{
			*fn = nullptr;
			Printf(TEXTCOLOR_RED "Function Pointer ('%s::%s'): symbol '%s' does not exist in class '%s'\n",
				val.ClassName.GetChars(),
				val.FunctionName.GetChars(),
				val.FunctionName.GetChars(),
				val.ClassName.GetChars()
			);
			ar.mErrors++;
			return false;
		}
		PFunction* p = dyn_cast<PFunction>(sym);
		if(!p)
		{
			*fn = nullptr;
			Printf(TEXTCOLOR_RED "Function Pointer (%s::%s): '%s' in class '%s' is a variable, not a function\n",
				val.ClassName.GetChars(),
				val.FunctionName.GetChars(),
				val.FunctionName.GetChars(),
				val.ClassName.GetChars()
			);
			ar.mErrors++;
			return false;
		}
		*fn = NativeFunctionPointerCast(p, this);
		if(!*fn)
		{
			if((p->Variants[0].Flags & (VARF_Action | VARF_Virtual)) != 0)
			{
				*fn = nullptr;
				Printf(TEXTCOLOR_RED "Function Pointer (%s::%s): function '%s' in class '%s' is %s, not a static function\n",
					val.ClassName.GetChars(),
					val.FunctionName.GetChars(),
					val.FunctionName.GetChars(),
					val.ClassName.GetChars(),
					(p->GetImplicitArgs() == 1 ? "a virtual function" : "an action function")
				);
			}
			else
			{
				FString fn_name = MakeFunctionPointerDescriptiveName(p->Variants[0].Proto,p->Variants[0].ArgFlags, FScopeBarrier::SideFromFlags(p->Variants[0].Flags));
				Printf(TEXTCOLOR_RED "Function Pointer (%s::%s) has incompatible type (Pointer is '%s', Function is '%s')\n",
							val.ClassName.GetChars(),
							val.FunctionName.GetChars(),
							fn_name.GetChars(),
							mDescriptiveName.GetChars()
				);
			}
			ar.mErrors++;
			return false;
		}
		return true;
	}
	else
	{
		*fn = nullptr;
	}
	return true;
}

bool PFunctionPointer::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PPrototype * proto = (const PPrototype*) id1;
	const PFunctionPointer::FlagsAndScope * flags_and_scope = (const PFunctionPointer::FlagsAndScope *) id2;
	return (proto == (PointedType == TypeVoid ? nullptr : PointedType))
		&& (Scope == flags_and_scope->Scope)
		&& (ArgFlags == *flags_and_scope->ArgFlags);
}

void PFunctionPointer::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{	//NOT SUPPORTED
	assert(0 && "GetTypeIDs not supported for PFunctionPointer");
}

PFunctionPointer * NewFunctionPointer(PPrototype * proto, TArray<uint32_t> && argflags, int scope)
{
	size_t bucket;

	PFunctionPointer::FlagsAndScope flags_and_scope { &argflags, scope };

	PType *fn = TypeTable.FindType(NAME_Function, (intptr_t)proto, (intptr_t)&flags_and_scope, &bucket);
	if (fn == nullptr)
	{
		fn = new PFunctionPointer(proto, std::move(argflags), scope);
		flags_and_scope.ArgFlags = &static_cast<PFunctionPointer *>(fn)->ArgFlags;
		TypeTable.AddType(fn, NAME_Function, (intptr_t)proto, (intptr_t)&flags_and_scope, bucket);
	}
	return (PFunctionPointer *)fn;
}

/* PStruct ****************************************************************/

//==========================================================================
//
// PStruct - Parameterized Constructor
//
//==========================================================================

PStruct::PStruct(FName name, PTypeBase *outer, bool isnative, int fileno)
: PContainerType(name, outer)
{
	mDescriptiveName.Format("%sStruct<%s>", isnative? "Native" : "", name.GetChars());
	Size = 0;
	isNative = isnative;
	mDefFileNo = fileno;
}

//==========================================================================
//
// PStruct :: SetDefaultValue
//
//==========================================================================

void PStruct::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special)
{
	auto it = Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		auto field = dyn_cast<PField>(pair->Value);
		if (field && !(field->Flags & VARF_Transient))
		{
			field->Type->SetDefaultValue(base, unsigned(offset + field->Offset), special);
		}
	}
}

//==========================================================================
//
// PStruct :: SetPointer
//
//==========================================================================

void PStruct::SetPointer(void *base, unsigned offset, TArray<size_t> *special)
{
	auto it = Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		auto field = dyn_cast<PField>(pair->Value);
		if (field && !(field->Flags & VARF_Transient))
		{
			field->Type->SetPointer(base, unsigned(offset + field->Offset), special);
		}
	}
}

//==========================================================================
//
// PStruct :: SetPointerArray
//
//==========================================================================

void PStruct::SetPointerArray(void *base, unsigned offset, TArray<size_t> *special)
{
	auto it = Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		auto field = dyn_cast<PField>(pair->Value);
		if (field && !(field->Flags & VARF_Transient))
		{
			field->Type->SetPointerArray(base, unsigned(offset + field->Offset), special);
		}
	}
}

//==========================================================================
//
// PStruct :: SetPointerMap
//
//==========================================================================

void PStruct::SetPointerMap(void *base, unsigned offset, TArray<std::pair<size_t,PType *>> *special)
{
	auto it = Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		auto field = dyn_cast<PField>(pair->Value);
		if (field && !(field->Flags & VARF_Transient))
		{
			field->Type->SetPointerMap(base, unsigned(offset + field->Offset), special);
		}
	}
}

//==========================================================================
//
// PStruct :: WriteValue
//
//==========================================================================

void PStruct::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (ar.BeginObject(key))
	{
		Symbols.WriteFields(ar, addr);
		ar.EndObject();
	}
}

//==========================================================================
//
// PStruct :: ReadValue
//
//==========================================================================

bool PStruct::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	if (ar.BeginObject(key))
	{
		bool ret = Symbols.ReadFields(ar, addr, DescriptiveName());
		ar.EndObject();
		return ret;
	}
	return false;
}

//==========================================================================
//
// PStruct :: AddField
//
// Appends a new field to the end of a struct. Returns either the new field
// or nullptr if a symbol by that name already exists.
//
//==========================================================================

PField *PStruct::AddField(FName name, PType *type, uint32_t flags)
{
	assert(type->Size > 0);
	return Symbols.AddField(name, type, flags, Size, &Align, mDefFileNo);
}

//==========================================================================
//
// PStruct :: AddField
//
// Appends a new native field to the struct. Returns either the new field
// or nullptr if a symbol by that name already exists.
//
//==========================================================================

PField *PStruct::AddNativeField(FName name, PType *type, size_t address, uint32_t flags, int bitvalue)
{
	return Symbols.AddNativeField(name, type, address, flags, bitvalue, mDefFileNo);
}

//==========================================================================
//
// NewStruct
// Returns a PStruct for the given name and container, making sure not to
// create duplicates.
//
//==========================================================================

PStruct *NewStruct(FName name, PTypeBase *outer, bool native, int fileno)
{
	size_t bucket;
	if (outer == nullptr) outer = Namespaces.GlobalNamespace;
	PType *stype = TypeTable.FindType(NAME_Struct, (intptr_t)outer, name.GetIndex(), &bucket);
	if (stype == nullptr)
	{
		stype = new PStruct(name, outer, native, fileno);
		TypeTable.AddType(stype, NAME_Struct, (intptr_t)outer, name.GetIndex(), bucket);
	}
	return static_cast<PStruct *>(stype);
}


/* PPrototype *************************************************************/

//==========================================================================
//
// PPrototype - Parameterized Constructor
//
//==========================================================================

PPrototype::PPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes)
: ArgumentTypes(argtypes), ReturnTypes(rettypes)
{
	for (auto& type: ArgumentTypes)
	{
		if (type == TypeFVector2)
		{
			type = TypeVector2;
		}
		else if (type == TypeFVector3)
		{
			type = TypeVector3;
		}
	}

	for (auto& type : ReturnTypes)
	{
		if (type == TypeFVector2)
		{
			type = TypeVector2;
		}
		else if (type == TypeFVector3)
		{
			type = TypeVector3;
		}
	}
}

//==========================================================================
//
// PPrototype :: IsMatch
//
//==========================================================================

bool PPrototype::IsMatch(intptr_t id1, intptr_t id2) const
{
	const TArray<PType *> *args = (const TArray<PType *> *)id1;
	const TArray<PType *> *rets = (const TArray<PType *> *)id2;

	return *args == ArgumentTypes && *rets == ReturnTypes;
}

//==========================================================================
//
// PPrototype :: GetTypeIDs
//
//==========================================================================

void PPrototype::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)&ArgumentTypes;
	id2 = (intptr_t)&ReturnTypes;
}

//==========================================================================
//
// NewPrototype
//
// Returns a PPrototype for the given return and argument types, making sure
// not to create duplicates.
//
//==========================================================================

PPrototype *NewPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes)
{
	size_t bucket;
	PType *proto = TypeTable.FindType(NAME_Prototype, (intptr_t)&argtypes, (intptr_t)&rettypes, &bucket);
	if (proto == nullptr)
	{
		proto = new PPrototype(rettypes, argtypes);
		TypeTable.AddType(proto, NAME_Prototype, (intptr_t)&argtypes, (intptr_t)&rettypes, bucket);
	}
	return static_cast<PPrototype *>(proto);
}

/* PClass *****************************************************************/

//==========================================================================
//
//
//
//==========================================================================

PClassType::PClassType(PClass *cls, int fileno)
{
	assert(cls->VMType == nullptr);
	Descriptor = cls;
	TypeName = cls->TypeName;
	if (cls->ParentClass != nullptr)
	{
		ParentType = cls->ParentClass->VMType;
		assert(ParentType != nullptr);
		Symbols.SetParentTable(&ParentType->Symbols);
		ScopeFlags = ParentType->ScopeFlags;
	}
	cls->VMType = this;
	mDefFileNo = fileno;
	mDescriptiveName.Format("Class<%s>", cls->TypeName.GetChars());
}

//==========================================================================
//
// PClass :: AddField
//
//==========================================================================

PField *PClassType::AddField(FName name, PType *type, uint32_t flags)
{
	return Descriptor->AddField(name, type, flags, mDefFileNo);
}

//==========================================================================
//
// PClass :: AddNativeField
//
//==========================================================================

PField *PClassType::AddNativeField(FName name, PType *type, size_t address, uint32_t flags, int bitvalue)
{
	auto field = Symbols.AddNativeField(name, type, address, flags, bitvalue, mDefFileNo);
	if (field != nullptr) Descriptor->Fields.Push(field);
	return field;
}

//==========================================================================
//
//
//
//==========================================================================

PClassType *NewClassType(PClass *cls, int fileno)
{
	size_t bucket;
	PType *ptype = TypeTable.FindType(NAME_Object, 0, cls->TypeName.GetIndex(), &bucket);
	if (ptype == nullptr)
	{
		ptype = new PClassType(cls, fileno);
		TypeTable.AddType(ptype, NAME_Object, 0, cls->TypeName.GetIndex(), bucket);
	}
	return static_cast<PClassType *>(ptype);
}


/* FTypeTable **************************************************************/

//==========================================================================
//
// FTypeTable :: FindType
//
//==========================================================================

PType *FTypeTable::FindType(FName type_name, intptr_t parm1, intptr_t parm2, size_t *bucketnum)
{
	size_t bucket = Hash(type_name, parm1, parm2) % HASH_SIZE;
	if (bucketnum != nullptr)
	{
		*bucketnum = bucket;
	}
	for (PType *type = TypeHash[bucket]; type != nullptr; type = type->HashNext)
	{
		if (type->TypeTableType == type_name && type->IsMatch(parm1, parm2))
		{
			return type;
		}
	}
	return nullptr;
}

//==========================================================================
//
// FTypeTable :: AddType - Fully Parameterized Version
//
//==========================================================================

void FTypeTable::AddType(PType *type, FName type_name, intptr_t parm1, intptr_t parm2, size_t bucket)
{
#ifdef _DEBUG
	size_t bucketcheck;
	assert(FindType(type_name, parm1, parm2, &bucketcheck) == nullptr && "Type must not be inserted more than once");
	assert(bucketcheck == bucket && "Passed bucket was wrong");
#endif
	type->TypeTableType = type_name;
	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
}

//==========================================================================
//
// FTypeTable :: AddType - Simple Version
//
//==========================================================================

void FTypeTable::AddType(PType *type, FName type_name)
{
	intptr_t parm1, parm2;
	size_t bucket;

	// Type table stuff id only needed to let all classes hash to the same group. For all other types this is pointless.
	type->TypeTableType = type_name;
	type->GetTypeIDs(parm1, parm2);
	bucket = Hash(type_name, parm1, parm2) % HASH_SIZE;
	assert(FindType(type_name, parm1, parm2, nullptr) == nullptr && "Type must not be inserted more than once");

	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
}

//==========================================================================
//
// FTypeTable :: Hash												STATIC
//
//==========================================================================

size_t FTypeTable::Hash(FName p1, intptr_t p2, intptr_t p3)
{
	size_t i1 = (size_t)p1.GetIndex();

	// Swap the high and low halves of i1. The compiler should be smart enough
	// to transform this into a ROR or ROL.
	i1 = (i1 >> (sizeof(size_t)*4)) | (i1 << (sizeof(size_t)*4));

	if (p1 != NAME_Prototype && p1 != NAME_Function)
	{
		size_t i2 = (size_t)p2;
		size_t i3 = (size_t)p3;
		return (~i1 ^ i2) + i3 * 961748927;	// i3 is multiplied by a prime
	}
	else if(p1 == NAME_Prototype)
	{ // Prototypes need to hash the TArrays at p2 and p3
		const TArray<PType *> *a2 = (const TArray<PType *> *)p2;
		const TArray<PType *> *a3 = (const TArray<PType *> *)p3;
		for (unsigned i = 0; i < a2->Size(); ++i)
		{
			i1 = (i1 * 961748927) + (size_t)((*a2)[i]);
		}
		for (unsigned i = 0; i < a3->Size(); ++i)
		{
			i1 = (i1 * 961748927) + (size_t)((*a3)[i]);
		}
		return i1;
	}
	else // if(p1 == NAME_Function)
	{ // functions need custom hashing as well
		size_t i2 = (size_t)p2;
		const PFunctionPointer::FlagsAndScope * flags_and_scope = (const PFunctionPointer::FlagsAndScope *) p3;
		const TArray<uint32_t> * a3 = flags_and_scope->ArgFlags;
		i1 = (~i1 ^ i2);
		for (unsigned i = 0; i < a3->Size(); ++i)
		{
			i1 = (i1 * 961748927) + (size_t)((*a3)[i]);
		}
		return (i1 * 961748927) + (size_t)flags_and_scope->Scope;
	}
}

//==========================================================================
//
// FTypeTable :: Clear
//
//==========================================================================

void FTypeTable::Clear()
{
	for (size_t i = 0; i < countof(TypeTable.TypeHash); ++i)
	{
		for (PType *ty = TypeTable.TypeHash[i]; ty != nullptr;)
		{
			auto next = ty->HashNext;
			delete ty;
			ty = next;
		}
	}
	memset(TypeHash, 0, sizeof(TypeHash));
}

#include "c_dispatch.h"
CCMD(typetable)
{
	DumpTypeTable();
}

