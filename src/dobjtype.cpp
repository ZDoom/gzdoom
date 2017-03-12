/*
** dobjtype.cpp
** Implements the type information class
**
**---------------------------------------------------------------------------
** Copyright 1998-2010 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <float.h>
#include <limits>

#include "dobject.h"
#include "i_system.h"
#include "serializer.h"
#include "actor.h"
#include "templates.h"
#include "autosegs.h"
#include "v_text.h"
#include "a_pickups.h"
#include "d_player.h"
#include "doomerrors.h"
#include "fragglescript/t_fs.h"
#include "a_keys.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
EXTERN_CVAR(Bool, strictdecorate);

// PUBLIC DATA DEFINITIONS -------------------------------------------------
FMemArena ClassDataAllocator(32768);	// use this for all static class data that can be released in bulk when the type system is shut down.

FTypeTable TypeTable;
TArray<PClass *> PClass::AllClasses;
TArray<VMFunction**> PClass::FunctionPtrList;
bool PClass::bShutdown;
bool PClass::bVMOperational;

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
PSpriteID *TypeSpriteID;
PStatePointer *TypeState;
PPointer *TypeFont;
PStateLabel *TypeStateLabel;
PStruct *TypeVector2;
PStruct *TypeVector3;
PStruct *TypeColorStruct;
PStruct *TypeStringStruct;
PPointer *TypeNullPtr;
PPointer *TypeVoidPtr;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// A harmless non-nullptr FlatPointer for classes without pointers.
static const size_t TheEnd = ~(size_t)0;

// CODE --------------------------------------------------------------------

IMPLEMENT_CLASS(PErrorType, false, false)
IMPLEMENT_CLASS(PVoidType, false, false)

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
	Printf("Used buckets: %d/%lu (%.2f%%) for %d entries\n", used, countof(TypeTable.TypeHash), double(used)/countof(TypeTable.TypeHash)*100, all);
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

IMPLEMENT_CLASS(PType, true, false)

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

void PType::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *stroffs) const
{
}

//==========================================================================
//
// PType :: SetDefaultValue
//
//==========================================================================

void PType::SetPointer(void *base, unsigned offset, TArray<size_t> *stroffs) const
{
}

void PType::SetPointerArray(void *base, unsigned offset, TArray<size_t> *stroffs) const
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
	TypeTable.AddType(TypeError = new PErrorType);
	TypeTable.AddType(TypeAuto = new PErrorType(2));
	TypeTable.AddType(TypeVoid = new PVoidType);
	TypeTable.AddType(TypeSInt8 = new PInt(1, false));
	TypeTable.AddType(TypeUInt8 = new PInt(1, true));
	TypeTable.AddType(TypeSInt16 = new PInt(2, false));
	TypeTable.AddType(TypeUInt16 = new PInt(2, true));
	TypeTable.AddType(TypeSInt32 = new PInt(4, false));
	TypeTable.AddType(TypeUInt32 = new PInt(4, true));
	TypeTable.AddType(TypeBool = new PBool);
	TypeTable.AddType(TypeFloat32 = new PFloat(4));
	TypeTable.AddType(TypeFloat64 = new PFloat(8));
	TypeTable.AddType(TypeString = new PString);
	TypeTable.AddType(TypeName = new PName);
	TypeTable.AddType(TypeSound = new PSound);
	TypeTable.AddType(TypeColor = new PColor);
	TypeTable.AddType(TypeState = new PStatePointer);
	TypeTable.AddType(TypeStateLabel = new PStateLabel);
	TypeTable.AddType(TypeNullPtr = new PPointer);
	TypeTable.AddType(TypeSpriteID = new PSpriteID);
	TypeTable.AddType(TypeTextureID = new PTextureID);

	TypeVoidPtr = NewPointer(TypeVoid, false);
	TypeColorStruct = NewStruct("@ColorStruct", nullptr);	//This name is intentionally obfuscated so that it cannot be used explicitly. The point of this type is to gain access to the single channels of a color value.
	TypeStringStruct = NewNativeStruct("Stringstruct", nullptr);
	TypeFont = NewPointer(NewNativeStruct("Font", nullptr));
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
	TypeTable.AddType(TypeVector2);
	TypeVector2->loadOp = OP_LV2;
	TypeVector2->storeOp = OP_SV2;
	TypeVector2->moveOp = OP_MOVEV2;
	TypeVector2->RegType = REGT_FLOAT;
	TypeVector2->RegCount = 2;

	TypeVector3 = new PStruct(NAME_Vector3, nullptr);
	TypeVector3->AddField(NAME_X, TypeFloat64);
	TypeVector3->AddField(NAME_Y, TypeFloat64);
	TypeVector3->AddField(NAME_Z, TypeFloat64);
	// allow accessing xy as a vector2. This is not supposed to be serialized so it's marked transient
	TypeVector3->Symbols.AddSymbol(new PField(NAME_XY, TypeVector2, VARF_Transient, 0));
	TypeTable.AddType(TypeVector3);
	TypeVector3->loadOp = OP_LV3;
	TypeVector3->storeOp = OP_SV3;
	TypeVector3->moveOp = OP_MOVEV3;
	TypeVector3->RegType = REGT_FLOAT;
	TypeVector3->RegCount = 3;



	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_sByte, TypeSInt8));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Byte, TypeUInt8));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Short, TypeSInt16));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_uShort, TypeUInt16));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Int, TypeSInt32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_uInt, TypeUInt32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Bool, TypeBool));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Float, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Double, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Float32, TypeFloat32));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Float64, TypeFloat64));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_String, TypeString));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Name, TypeName));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Sound, TypeSound));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Color, TypeColor));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_State, TypeState));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Vector2, TypeVector2));
	Namespaces.GlobalNamespace->Symbols.AddSymbol(new PSymbolType(NAME_Vector3, TypeVector3));
}


/* PBasicType *************************************************************/

IMPLEMENT_CLASS(PBasicType, true, false)

//==========================================================================
//
// PBasicType Default Constructor
//
//==========================================================================

PBasicType::PBasicType()
{
}

//==========================================================================
//
// PBasicType Parameterized Constructor
//
//==========================================================================

PBasicType::PBasicType(unsigned int size, unsigned int align)
: PType(size, align)
{
	mDescriptiveName = "BasicType";
}

/* PCompoundType **********************************************************/

IMPLEMENT_CLASS(PCompoundType, true, false)

/* PNamedType *************************************************************/

IMPLEMENT_CLASS(PNamedType, true, false)

//==========================================================================
//
// PNamedType :: IsMatch
//
//==========================================================================

bool PNamedType::IsMatch(intptr_t id1, intptr_t id2) const
{
	const DObject *outer = (const DObject *)id1;
	FName name = (ENamedName)(intptr_t)id2;
	
	return Outer == outer && TypeName == name;
}

//==========================================================================
//
// PNamedType :: GetTypeIDs
//
//==========================================================================

void PNamedType::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)Outer;
	id2 = TypeName;
}

/* PInt *******************************************************************/

IMPLEMENT_CLASS(PInt, false, false)

//==========================================================================
//
// PInt Default Constructor
//
//==========================================================================

PInt::PInt()
: PBasicType(4, 4), Unsigned(false), IntCompatible(true)
{
	mDescriptiveName = "SInt32";
	Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, -0x7FFFFFFF - 1));
	Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this,  0x7FFFFFFF));
	SetOps();
}

//==========================================================================
//
// PInt Parameterized Constructor
//
//==========================================================================

PInt::PInt(unsigned int size, bool unsign, bool compatible)
: PBasicType(size, size), Unsigned(unsign), IntCompatible(compatible)
{
	mDescriptiveName.Format("%cInt%d", unsign? 'U':'S', size);

	MemberOnly = (size < 4);
	if (!unsign)
	{
		int maxval = (1u << ((8 * size) - 1)) - 1; // compute as unsigned to prevent overflow before -1
		int minval = -maxval - 1;
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, minval));
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this, maxval));
	}
	else
	{
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, 0u));
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this, (1u << ((8 * size) - 1))));
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

IMPLEMENT_CLASS(PBool, false, false)

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
}

/* PFloat *****************************************************************/

IMPLEMENT_CLASS(PFloat, false, false)

//==========================================================================
//
// PFloat Default Constructor
//
//==========================================================================

PFloat::PFloat()
: PBasicType(8, 8)
{
	mDescriptiveName = "Float";
	SetDoubleSymbols();
	SetOps();
}

//==========================================================================
//
// PFloat Parameterized Constructor
//
//==========================================================================

PFloat::PFloat(unsigned int size)
: PBasicType(size, size)
{
	mDescriptiveName.Format("Float%d", size);
	if (size == 8)
	{
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
		Symbols.AddSymbol(new PSymbolConstNumeric(sym[i].Name, this, sym[i].Value));
	}
}

void PFloat::SetSymbols(const PFloat::SymbolInitI *sym, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		Symbols.AddSymbol(new PSymbolConstNumeric(sym[i].Name, this, sym[i].Value));
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
	return xs_ToInt(GetValueFloat(addr));
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

IMPLEMENT_CLASS(PString, false, false)

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

void PString::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const
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

IMPLEMENT_CLASS(PName, false, false)

//==========================================================================
//
// PName Default Constructor
//
//==========================================================================

PName::PName()
: PInt(sizeof(FName), true, false)
{
	mDescriptiveName = "Name";
	assert(sizeof(FName) == alignof(FName));
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

/* PSpriteID ******************************************************************/

IMPLEMENT_CLASS(PSpriteID, false, false)

//==========================================================================
//
// PName Default Constructor
//
//==========================================================================

PSpriteID::PSpriteID()
	: PInt(sizeof(int), true, true)
{
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
	int32_t val;
	ar.Sprite(key, val, nullptr);
	*(int*)addr = val;
	return true;
}

/* PTextureID ******************************************************************/

IMPLEMENT_CLASS(PTextureID, false, false)

//==========================================================================
//
// PTextureID Default Constructor
//
//==========================================================================

PTextureID::PTextureID()
	: PInt(sizeof(FTextureID), true, false)
{
	mDescriptiveName = "TextureID";
	assert(sizeof(FTextureID) == alignof(FTextureID));
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

/* PSound *****************************************************************/

IMPLEMENT_CLASS(PSound, false, false)

//==========================================================================
//
// PSound Default Constructor
//
//==========================================================================

PSound::PSound()
: PInt(sizeof(FSoundID), true)
{
	mDescriptiveName = "Sound";
	assert(sizeof(FSoundID) == alignof(FSoundID));
}

//==========================================================================
//
// PSound :: WriteValue
//
//==========================================================================

void PSound::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	const char *cptr = *(const FSoundID *)addr;
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
		*(FSoundID *)addr = FSoundID(cptr);
		return true;
	}
}

/* PColor *****************************************************************/

IMPLEMENT_CLASS(PColor, false, false)

//==========================================================================
//
// PColor Default Constructor
//
//==========================================================================

PColor::PColor()
: PInt(sizeof(PalEntry), true)
{
	mDescriptiveName = "Color";
	assert(sizeof(PalEntry) == alignof(PalEntry));
}

/* PStateLabel *****************************************************************/

IMPLEMENT_CLASS(PStateLabel, false, false)

//==========================================================================
//
// PStateLabel Default Constructor
//
//==========================================================================

PStateLabel::PStateLabel()
	: PInt(sizeof(int), false, false)
{
	mDescriptiveName = "StateLabel";
}

/* PPointer ***************************************************************/

IMPLEMENT_CLASS(PPointer, false, false)

//==========================================================================
//
// PPointer - Default Constructor
//
//==========================================================================

PPointer::PPointer()
: PBasicType(sizeof(void *), alignof(void *)), PointedType(nullptr), IsConst(false)
{
	mDescriptiveName = "NullPointer";
	SetOps();
}

//==========================================================================
//
// PPointer - Parameterized Constructor
//
//==========================================================================

PPointer::PPointer(PType *pointsat, bool isconst)
: PBasicType(sizeof(void *), alignof(void *)), PointedType(pointsat), IsConst(isconst)
{
	mDescriptiveName.Format("Pointer<%s%s>", pointsat->DescriptiveName(), isconst? "readonly " : "");
	mVersion = pointsat->mVersion;
	SetOps();
}

//==========================================================================
//
// PPointer :: GetStoreOp
//
//==========================================================================

void PPointer::SetOps()
{
	loadOp = (PointedType && PointedType->IsKindOf(RUNTIME_CLASS(PClass))) ? OP_LO : OP_LP;
	// Non-destroyed thinkers are always guaranteed to be linked into the thinker chain so we don't need the write barrier for them.
	storeOp = (loadOp == OP_LO && !static_cast<PClass*>(PointedType)->IsDescendantOf(RUNTIME_CLASS(DThinker))) ? OP_SO : OP_SP;
	moveOp = OP_MOVEA;
	RegType = REGT_POINTER;
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
// PPointer :: SetPointer
//
//==========================================================================

void PPointer::SetPointer(void *base, unsigned offset, TArray<size_t> *special) const
{
	if (PointedType != nullptr && PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
	{
		// Add to the list of pointers for this class.
		special->Push(offset);
	}
}

//==========================================================================
//
// PPointer :: WriteValue
//
//==========================================================================

void PPointer::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
	{
		auto pt = static_cast<PClass*>(PointedType);

		if (pt->IsDescendantOf(RUNTIME_CLASS(PClass)))
		{
			ar(key, *(PClass **)addr);
		}
		else
		{
			ar(key, *(DObject **)addr);
		}
	}
	else if (writer != nullptr)
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
	if (PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
	{
		auto pt = static_cast<PClass*>(PointedType);
		bool res = true;

		if (pt->IsDescendantOf(RUNTIME_CLASS(PClass)))
		{
			::Serialize(ar, key, *(PClass **)addr, (PClass**)nullptr);
		}
		else
		{
			::Serialize(ar, key, *(DObject **)addr, nullptr, &res);
		}
		return res;
	}
	else if (reader != nullptr)
	{
		return reader(ar, key, addr);
	}
	return false;
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
	size_t bucket;
	PType *ptype = TypeTable.FindType(RUNTIME_CLASS(PPointer), (intptr_t)type, isconst ? 1 : 0, &bucket);
	if (ptype == nullptr)
	{
		ptype = new PPointer(type, isconst);
		TypeTable.AddType(ptype, RUNTIME_CLASS(PPointer), (intptr_t)type, isconst ? 1 : 0, bucket);
	}
	return static_cast<PPointer *>(ptype);
}

/* PStatePointer **********************************************************/

IMPLEMENT_CLASS(PStatePointer, false, false)

//==========================================================================
//
// PStatePointer Default Constructor
//
//==========================================================================

PStatePointer::PStatePointer()
{
	mDescriptiveName = "Pointer<State>";
	PointedType = NewNativeStruct(NAME_State, nullptr);
	IsConst = true;
}

//==========================================================================
//
// PStatePointer :: WriteValue
//
//==========================================================================

void PStatePointer::WriteValue(FSerializer &ar, const char *key, const void *addr) const
{
	ar(key, *(FState **)addr);
}

//==========================================================================
//
// PStatePointer :: ReadValue
//
//==========================================================================

bool PStatePointer::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	bool res = false;
	::Serialize(ar, key, *(FState **)addr, nullptr, &res);
	return res;
}



/* PClassPointer **********************************************************/

IMPLEMENT_CLASS(PClassPointer,false, false)

//==========================================================================
//
// PClassPointer - Parameterized Constructor
//
//==========================================================================

PClassPointer::PClassPointer(PClass *restrict)
: PPointer(RUNTIME_CLASS(PClass)), ClassRestriction(restrict)
{
	if (restrict) mDescriptiveName.Format("ClassPointer<%s>", restrict->TypeName.GetChars());
	else mDescriptiveName = "ClassPointer";
	// class pointers do not need write barriers because all classes are stored in the global type table and won't get collected.
	// This means we can use the cheapoer non-barriered opcodes here.
	loadOp = OP_LOS;
	storeOp = OP_SP;
	mVersion = restrict->mVersion;
}

//==========================================================================
//
// PClassPointer - isCompatible
//
//==========================================================================

bool PClassPointer::isCompatible(PType *type)
{
	auto other = dyn_cast<PClassPointer>(type);
	return (other != nullptr && other->ClassRestriction->IsDescendantOf(ClassRestriction));
}

//==========================================================================
//
// PClassPointer :: SetPointer
//
//==========================================================================

void PClassPointer::SetPointer(void *base, unsigned offset, TArray<size_t> *special) const
{
	// Class pointers do not get added to FlatPointers because they are released from the GC.
}

//==========================================================================
//
// PClassPointer :: IsMatch
//
//==========================================================================

bool PClassPointer::IsMatch(intptr_t id1, intptr_t id2) const
{
	const PType *pointat = (const PType *)id1;
	const PClass *classat = (const PClass *)id2;

	assert(pointat->IsKindOf(RUNTIME_CLASS(PClass)));
	return classat == ClassRestriction;
}

//==========================================================================
//
// PClassPointer :: GetTypeIDs
//
//==========================================================================

void PClassPointer::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	assert(PointedType == RUNTIME_CLASS(PClass));
	id1 = (intptr_t)PointedType;
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
	PType *ptype = TypeTable.FindType(RUNTIME_CLASS(PClassPointer), (intptr_t)RUNTIME_CLASS(PClass), (intptr_t)restrict, &bucket);
	if (ptype == nullptr)
	{
		ptype = new PClassPointer(restrict);
		TypeTable.AddType(ptype, RUNTIME_CLASS(PClassPointer), (intptr_t)RUNTIME_CLASS(PClass), (intptr_t)restrict, bucket);
	}
	return static_cast<PClassPointer *>(ptype);
}

/* PEnum ******************************************************************/

IMPLEMENT_CLASS(PEnum, false, false)

//==========================================================================
//
// PEnum - Default Constructor
//
//==========================================================================

PEnum::PEnum()
: PInt(4, false)
{
	mDescriptiveName = "Enum";
}

//==========================================================================
//
// PEnum - Parameterized Constructor
//
//==========================================================================

PEnum::PEnum(FName name, PTypeBase *outer)
: PInt(4, false)
{
	EnumName = name;
	Outer = outer;
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
	PType *etype = TypeTable.FindType(RUNTIME_CLASS(PEnum), (intptr_t)outer, (intptr_t)name, &bucket);
	if (etype == nullptr)
	{
		etype = new PEnum(name, outer);
		TypeTable.AddType(etype, RUNTIME_CLASS(PEnum), (intptr_t)outer, (intptr_t)name, bucket);
	}
	return static_cast<PEnum *>(etype);
}

/* PArray *****************************************************************/

IMPLEMENT_CLASS(PArray, false, false)

//==========================================================================
//
// PArray - Default Constructor
//
//==========================================================================

PArray::PArray()
: ElementType(nullptr), ElementCount(0)
{
	mDescriptiveName = "Array";
}

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
		unsigned loop = MIN(count, ElementCount);
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

void PArray::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const
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

void PArray::SetPointer(void *base, unsigned offset, TArray<size_t> *special) const
{
	for (unsigned i = 0; i < ElementCount; ++i)
	{
		ElementType->SetPointer(base, offset + i*ElementSize, special);
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
	PType *atype = TypeTable.FindType(RUNTIME_CLASS(PArray), (intptr_t)type, count, &bucket);
	if (atype == nullptr)
	{
		atype = new PArray(type, count);
		TypeTable.AddType(atype, RUNTIME_CLASS(PArray), (intptr_t)type, count, bucket);
	}
	return (PArray *)atype;
}

/* PArray *****************************************************************/

IMPLEMENT_CLASS(PResizableArray, false, false)

//==========================================================================
//
// PArray - Default Constructor
//
//==========================================================================

PResizableArray::PResizableArray()
{
	mDescriptiveName = "ResizableArray";
}

//==========================================================================
//
// PArray - Parameterized Constructor
//
//==========================================================================

PResizableArray::PResizableArray(PType *etype)
	: PArray(etype, 0)
{
	mDescriptiveName.Format("ResizableArray<%s>", etype->DescriptiveName());
}

//==========================================================================
//
// PArray :: IsMatch
//
//==========================================================================

bool PResizableArray::IsMatch(intptr_t id1, intptr_t id2) const
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

void PResizableArray::GetTypeIDs(intptr_t &id1, intptr_t &id2) const
{
	id1 = (intptr_t)ElementType;
	id2 = 0;
}

//==========================================================================
//
// NewResizableArray
//
// Returns a PArray for the given type and size, making sure not to create
// duplicates.
//
//==========================================================================

PResizableArray *NewResizableArray(PType *type)
{
	size_t bucket;
	PType *atype = TypeTable.FindType(RUNTIME_CLASS(PResizableArray), (intptr_t)type, 0, &bucket);
	if (atype == nullptr)
	{
		atype = new PResizableArray(type);
		TypeTable.AddType(atype, RUNTIME_CLASS(PResizableArray), (intptr_t)type, 0, bucket);
	}
	return (PResizableArray *)atype;
}

/* PDynArray **************************************************************/

IMPLEMENT_CLASS(PDynArray, false, false)

//==========================================================================
//
// PDynArray - Default Constructor
//
//==========================================================================

PDynArray::PDynArray()
: ElementType(nullptr)
{
	mDescriptiveName = "DynArray";
	Size = sizeof(FArray);
	Align = alignof(FArray);
}

//==========================================================================
//
// PDynArray - Parameterized Constructor
//
//==========================================================================

PDynArray::PDynArray(PType *etype,PStruct *backing)
: ElementType(etype), BackingType(backing)
{
	mDescriptiveName.Format("DynArray<%s>", etype->DescriptiveName());
	Size = sizeof(FArray);
	Align = alignof(FArray);
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

void PDynArray::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const
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

void PDynArray::SetPointerArray(void *base, unsigned offset, TArray<size_t> *special) const
{
	if (ElementType->IsKindOf(RUNTIME_CLASS(PPointer)) && !ElementType->IsKindOf(RUNTIME_CLASS(PClassPointer)) && static_cast<PPointer*>(ElementType)->PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
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
	if (aray->Count > 0)
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
	PType *atype = TypeTable.FindType(RUNTIME_CLASS(PDynArray), (intptr_t)type, 0, &bucket);
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
			backingname = "DynArray_Ptr";
			break;

		default:
			I_Error("Unsupported dynamic array requested");
			break;
		}

		auto backing = NewNativeStruct(backingname, nullptr);
		atype = new PDynArray(type, backing);
		TypeTable.AddType(atype, RUNTIME_CLASS(PDynArray), (intptr_t)type, 0, bucket);
	}
	return (PDynArray *)atype;
}

/* PMap *******************************************************************/

IMPLEMENT_CLASS(PMap, false, false)

//==========================================================================
//
// PMap - Default Constructor
//
//==========================================================================

PMap::PMap()
: KeyType(nullptr), ValueType(nullptr)
{
	mDescriptiveName = "Map";
	Size = sizeof(FMap);
	Align = alignof(FMap);
}

//==========================================================================
//
// PMap - Parameterized Constructor
//
//==========================================================================

PMap::PMap(PType *keytype, PType *valtype)
: KeyType(keytype), ValueType(valtype)
{
	mDescriptiveName.Format("Map<%s, %s>", keytype->DescriptiveName(), valtype->DescriptiveName());
	Size = sizeof(FMap);
	Align = alignof(FMap);
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
// NewMap
//
// Returns a PMap for the given key and value types, ensuring not to create
// duplicates.
//
//==========================================================================

PMap *NewMap(PType *keytype, PType *valuetype)
{
	size_t bucket;
	PType *maptype = TypeTable.FindType(RUNTIME_CLASS(PMap), (intptr_t)keytype, (intptr_t)valuetype, &bucket);
	if (maptype == nullptr)
	{
		maptype = new PMap(keytype, valuetype);
		TypeTable.AddType(maptype, RUNTIME_CLASS(PMap), (intptr_t)keytype, (intptr_t)valuetype, bucket);
	}
	return (PMap *)maptype;
}

/* PStruct ****************************************************************/

IMPLEMENT_CLASS(PStruct, false, false)

//==========================================================================
//
// PStruct - Default Constructor
//
//==========================================================================

PStruct::PStruct()
{
	mDescriptiveName = "Struct";
	Size = 0;
}

//==========================================================================
//
// PStruct - Parameterized Constructor
//
//==========================================================================

PStruct::PStruct(FName name, PTypeBase *outer)
: PNamedType(name, outer)
{
	mDescriptiveName.Format("Struct<%s>", name.GetChars());
	Size = 0;
	HasNativeFields = false;
}

//==========================================================================
//
// PStruct :: SetDefaultValue
//
//==========================================================================

void PStruct::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const
{
	for (const PField *field : Fields)
	{
		if (!(field->Flags & VARF_Transient))
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

void PStruct::SetPointer(void *base, unsigned offset, TArray<size_t> *special) const
{
	for (const PField *field : Fields)
	{
		if (!(field->Flags & VARF_Transient))
		{
			field->Type->SetPointer(base, unsigned(offset + field->Offset), special);
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
		WriteFields(ar, addr, Fields);
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
		bool ret = ReadFields(ar, addr);
		ar.EndObject();
		return ret;
	}
	return false;
}

//==========================================================================
//
// PStruct :: WriteFields											STATIC
//
//==========================================================================

void PStruct::WriteFields(FSerializer &ar, const void *addr, const TArray<PField *> &fields)
{
	for (unsigned i = 0; i < fields.Size(); ++i)
	{
		const PField *field = fields[i];
		// Skip fields without or with native serialization
		if (!(field->Flags & (VARF_Transient|VARF_Meta)))
		{
			field->Type->WriteValue(ar, field->SymbolName.GetChars(), (const uint8_t *)addr + field->Offset);
		}
	}
}

//==========================================================================
//
// PStruct :: ReadFields
//
//==========================================================================

bool PStruct::ReadFields(FSerializer &ar, void *addr) const
{
	bool readsomething = false;
	bool foundsomething = false;
	const char *label;
	while ((label = ar.GetKey()))
	{
		foundsomething = true;

		const PSymbol *sym = Symbols.FindSymbol(FName(label, true), true);
		if (sym == nullptr)
		{
			DPrintf(DMSG_ERROR, "Cannot find field %s in %s\n",
				label, TypeName.GetChars());
		}
		else if (!sym->IsKindOf(RUNTIME_CLASS(PField)))
		{
			DPrintf(DMSG_ERROR, "Symbol %s in %s is not a field\n",
				label, TypeName.GetChars());
		}
		else if ((static_cast<const PField *>(sym)->Flags & (VARF_Transient | VARF_Meta)))
		{
			DPrintf(DMSG_ERROR, "Symbol %s in %s is not a serializable field\n",
				label, TypeName.GetChars());
		}
		else
		{
			readsomething |= static_cast<const PField *>(sym)->Type->ReadValue(ar, nullptr,
				(uint8_t *)addr + static_cast<const PField *>(sym)->Offset);
		}
	}
	return readsomething || !foundsomething;
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
	PField *field = new PField(name, type, flags);

	// The new field is added to the end of this struct, alignment permitting.
	field->Offset = (Size + (type->Align - 1)) & ~(type->Align - 1);

	// Enlarge this struct to enclose the new field.
	Size = unsigned(field->Offset + type->Size);

	// This struct's alignment is the same as the largest alignment of any of
	// its fields.
	Align = MAX(Align, type->Align);

	if (Symbols.AddSymbol(field) == nullptr)
	{ // name is already in use
		field->Destroy();
		return nullptr;
	}
	Fields.Push(field);

	return field;
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
	PField *field = new PField(name, type, flags|VARF_Native|VARF_Transient, address, bitvalue);

	if (Symbols.AddSymbol(field) == nullptr)
	{ // name is already in use
		field->Destroy();
		return nullptr;
	}
	Fields.Push(field);
	HasNativeFields = true;
	return field;
}

//==========================================================================
//
// NewStruct
// Returns a PStruct for the given name and container, making sure not to
// create duplicates.
//
//==========================================================================

PStruct *NewStruct(FName name, PTypeBase *outer)
{
	size_t bucket;
	if (outer == nullptr) outer = Namespaces.GlobalNamespace;
	PType *stype = TypeTable.FindType(RUNTIME_CLASS(PStruct), (intptr_t)outer, (intptr_t)name, &bucket);
	if (stype == nullptr)
	{
		stype = new PStruct(name, outer);
		TypeTable.AddType(stype, RUNTIME_CLASS(PStruct), (intptr_t)outer, (intptr_t)name, bucket);
	}
	return static_cast<PStruct *>(stype);
}

/* PNativeStruct ****************************************************************/

IMPLEMENT_CLASS(PNativeStruct, false, false)

//==========================================================================
//
// PNativeStruct - Parameterized Constructor
//
//==========================================================================

PNativeStruct::PNativeStruct(FName name, PTypeBase *outer)
	: PStruct(name, outer)
{
	mDescriptiveName.Format("NativeStruct<%s>", name.GetChars());
	Size = 0;
	HasNativeFields = true;
}

//==========================================================================
//
// NewNativeStruct
// Returns a PNativeStruct for the given name and container, making sure not to
// create duplicates.
//
//==========================================================================

PNativeStruct *NewNativeStruct(FName name, PTypeBase *outer)
{
	size_t bucket;
	if (outer == nullptr) outer = Namespaces.GlobalNamespace;
	PType *stype = TypeTable.FindType(RUNTIME_CLASS(PNativeStruct), (intptr_t)outer, (intptr_t)name, &bucket);
	if (stype == nullptr)
	{
		stype = new PNativeStruct(name, outer);
		TypeTable.AddType(stype, RUNTIME_CLASS(PNativeStruct), (intptr_t)outer, (intptr_t)name, bucket);
	}
	return static_cast<PNativeStruct *>(stype);
}

/* PField *****************************************************************/

IMPLEMENT_CLASS(PField, false, false)

//==========================================================================
//
// PField - Default Constructor
//
//==========================================================================

PField::PField()
: PSymbol(NAME_None), Offset(0), Type(nullptr), Flags(0)
{
}


PField::PField(FName name, PType *type, uint32_t flags, size_t offset, int bitvalue)
	: PSymbol(name), Offset(offset), Type(type), Flags(flags)
{
	if (bitvalue != 0)
	{
		BitValue = 0;
		unsigned val = bitvalue;
		while ((val >>= 1)) BitValue++;

		if (type->IsA(RUNTIME_CLASS(PInt)) && unsigned(BitValue) < 8u * type->Size)
		{
			// map to the single bytes in the actual variable. The internal bit instructions operate on 8 bit values.
#ifndef __BIG_ENDIAN__
			Offset += BitValue / 8;
#else
			Offset += type->Size - 1 - BitValue / 8;
#endif
			BitValue &= 7;
			Type = TypeBool;
		}
		else
		{
			// Just abort. Bit fields should only be defined internally.
			I_Error("Trying to create an invalid bit field element: %s", name.GetChars());
		}
	}
	else BitValue = -1;
}

VersionInfo PField::GetVersion()
{
	VersionInfo Highest = { 0,0,0 };
	if (!(Flags & VARF_Deprecated)) Highest = mVersion;
	if (Type->mVersion > Highest) Highest = Type->mVersion;
	return Highest;
}

/* PProperty *****************************************************************/

IMPLEMENT_CLASS(PProperty, false, false)

//==========================================================================
//
// PField - Default Constructor
//
//==========================================================================

PProperty::PProperty()
	: PSymbol(NAME_None)
{
}

PProperty::PProperty(FName name, TArray<PField *> &fields)
	: PSymbol(name)
{
	Variables = std::move(fields);
}

/* PPrototype *************************************************************/

IMPLEMENT_CLASS(PPrototype, false, false)

//==========================================================================
//
// PPrototype - Default Constructor
//
//==========================================================================

PPrototype::PPrototype()
{
}

//==========================================================================
//
// PPrototype - Parameterized Constructor
//
//==========================================================================

PPrototype::PPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes)
: ArgumentTypes(argtypes), ReturnTypes(rettypes)
{
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
// PPrototype :: PropagateMark
//
//==========================================================================

size_t PPrototype::PropagateMark()
{
	GC::MarkArray(ArgumentTypes);
	GC::MarkArray(ReturnTypes);
	return (ArgumentTypes.Size() + ReturnTypes.Size()) * sizeof(void*) +
		Super::PropagateMark();
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
	PType *proto = TypeTable.FindType(RUNTIME_CLASS(PPrototype), (intptr_t)&argtypes, (intptr_t)&rettypes, &bucket);
	if (proto == nullptr)
	{
		proto = new PPrototype(rettypes, argtypes);
		TypeTable.AddType(proto, RUNTIME_CLASS(PPrototype), (intptr_t)&argtypes, (intptr_t)&rettypes, bucket);
	}
	return static_cast<PPrototype *>(proto);
}

/* PClass *****************************************************************/

IMPLEMENT_CLASS(PClass, false, false)

//==========================================================================
//
// PClass :: WriteValue
//
// Similar to PStruct's version, except it also needs to traverse parent
// classes.
//
//==========================================================================

static void RecurseWriteFields(const PClass *type, FSerializer &ar, const void *addr)
{
	if (type != nullptr)
	{
		RecurseWriteFields(type->ParentClass, ar, addr);
		// Don't write this part if it has no non-transient variables
		for (unsigned i = 0; i < type->Fields.Size(); ++i)
		{
			if (!(type->Fields[i]->Flags & (VARF_Transient|VARF_Meta)))
			{
				// Tag this section with the class it came from in case
				// a more-derived class has variables that shadow a less-
				// derived class. Whether or not that is a language feature
				// that will actually be allowed remains to be seen.
				FString key;
				key.Format("class:%s", type->TypeName.GetChars());
				if (ar.BeginObject(key.GetChars()))
				{
					PStruct::WriteFields(ar, addr, type->Fields);
					ar.EndObject();
				}
				break;
			}
		}
	}
}

void PClass::WriteValue(FSerializer &ar, const char *key,const void *addr) const
{
	if (ar.BeginObject(key))
	{
		RecurseWriteFields(this, ar, addr);
		ar.EndObject();
	}
}

// Same as WriteValue, but does not create a new object in the serializer
// This is so that user variables do not contain unnecessary subblocks.
void PClass::WriteAllFields(FSerializer &ar, const void *addr) const
{
	RecurseWriteFields(this, ar, addr);
}

//==========================================================================
//
// PClass :: ReadValue
//
//==========================================================================

bool PClass::ReadValue(FSerializer &ar, const char *key, void *addr) const
{
	if (ar.BeginObject(key))
	{
		bool ret = ReadAllFields(ar, addr);
		ar.EndObject();
		return ret;
	}
	return true;
}

bool PClass::ReadAllFields(FSerializer &ar, void *addr) const
{
	bool readsomething = false;
	bool foundsomething = false;
	const char *key;
	key = ar.GetKey();
	if (strcmp(key, "classtype"))
	{
		// this does not represent a DObject
		Printf(TEXTCOLOR_RED "trying to read user variables but got a non-object (first key is '%s')", key);
		ar.mErrors++;
		return false;
	}
	while ((key = ar.GetKey()))
	{
		if (strncmp(key, "class:", 6))
		{
			// We have read all user variable blocks.
			break;
		}
		foundsomething = true;
		PClass *type = PClass::FindClass(key + 6);
		if (type != nullptr)
		{
			// Only read it if the type is related to this one.
			const PClass *parent;
			for (parent = this; parent != nullptr; parent = parent->ParentClass)
			{
				if (parent == type)
				{
					break;
				}
			}
			if (parent != nullptr)
			{
				if (ar.BeginObject(nullptr))
				{
					readsomething |= type->ReadFields(ar, addr);
					ar.EndObject();
				}
			}
			else
			{
				DPrintf(DMSG_ERROR, "Unknown superclass %s of class %s\n",
					type->TypeName.GetChars(), TypeName.GetChars());
			}
		}
		else
		{
			DPrintf(DMSG_ERROR, "Unknown superclass %s of class %s\n",
				key+6, TypeName.GetChars());
		}
	}
	return readsomething || !foundsomething;
}

//==========================================================================
//
// cregcmp
//
// Sorter to keep built-in types in a deterministic order. (Needed?)
//
//==========================================================================

static int cregcmp (const void *a, const void *b) NO_SANITIZE
{
	const PClass *class1 = *(const PClass **)a;
	const PClass *class2 = *(const PClass **)b;
	return strcmp(class1->TypeName, class2->TypeName);
}

//==========================================================================
//
// PClass :: StaticInit												STATIC
//
// Creates class metadata for all built-in types.
//
//==========================================================================

void PClass::StaticInit ()
{
	atterm (StaticShutdown);

	StaticBootstrap();
	Namespaces.GlobalNamespace = Namespaces.NewNamespace(0);

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != nullptr)
	{
		((ClassReg *)*probe)->RegisterClass ();
	}

	// Keep built-in classes in consistant order. I did this before, though
	// I'm not sure if this is really necessary to maintain any sort of sync.
	qsort(&AllClasses[0], AllClasses.Size(), sizeof(AllClasses[0]), cregcmp);

	// Set all symbol table relations here so that they are valid right from the start.
	for (auto c : AllClasses)
	{
		if (c->ParentClass != nullptr)
		{
			c->Symbols.SetParentTable(&c->ParentClass->Symbols);
		}
	}

}

//==========================================================================
//
// PClass :: StaticShutdown											STATIC
//
// Frees all static class data.
//
//==========================================================================

void PClass::StaticShutdown ()
{
	// delete all variables containing pointers to script functions.
	for (auto p : FunctionPtrList)
	{
		*p = nullptr;
	}
	FunctionPtrList.Clear();
	VMFunction::DeleteAll();

	// Make a full garbage collection here so that all destroyed but uncollected higher level objects 
	// that still exist are properly taken down before the low level data is deleted.
	GC::FullGC();

	// From this point onward no scripts may be called anymore because the data needed by the VM is getting deleted now.
	// This flags DObject::Destroy not to call any scripted OnDestroy methods anymore.
	bVMOperational = false;

	// PendingWeapon must be cleared manually because it is not subjected to the GC if it contains WP_NOCHANGE, which is just RUNTIME_CLASS(AWWeapon).
	// But that will get cleared here, confusing the GC if the value is left in.
	for (auto &p : players)
	{
		p.PendingWeapon = nullptr;
	}

	// Unless something went wrong, anything left here should be class and type objects only, which do not own any scripts.
	bShutdown = true;
	TypeTable.Clear();
	Namespaces.ReleaseSymbols();
	ClassDataAllocator.FreeAllBlocks();
	AllClasses.Clear();
	PClassActor::AllActorClasses.Clear();

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != nullptr)
	{
		auto cr = ((ClassReg *)*probe);
		cr->MyClass = nullptr;
	}
	
}

//==========================================================================
//
// PClass :: StaticBootstrap										STATIC
//
//==========================================================================

void PClass::StaticBootstrap()
{
	PClass *cls = new PClass;
	PClass::RegistrationInfo.SetupClass(cls);

	// The PClassClass constructor initialized these to nullptr, because the
	// PClass metadata had not been created yet. Now it has, so we know what
	// they should be and can insert them into the type table successfully.
	cls->InsertIntoHash();

	// Create parent objects before we go so that these definitions are complete.
	cls->ParentClass = PClass::RegistrationInfo.ParentType->RegisterClass();
}

//==========================================================================
//
// PClass Constructor
//
//==========================================================================

PClass::PClass()
{
	Size = sizeof(DObject);
	ParentClass = nullptr;
	Pointers = nullptr;
	FlatPointers = nullptr;
	ArrayPointers = nullptr;
	HashNext = nullptr;
	Defaults = nullptr;
	bRuntimeClass = false;
	bExported = false;
	bDecorateClass = false;
	ConstructNative = nullptr;
	Meta = nullptr;
	mDescriptiveName = "Class";

	PClass::AllClasses.Push(this);
}

//==========================================================================
//
// PClass Destructor
//
//==========================================================================

PClass::~PClass()
{
	if (Defaults != nullptr)
	{
		M_Free(Defaults);
		Defaults = nullptr;
	}
	if (Meta != nullptr)
	{
		M_Free(Meta);
		Meta = nullptr;
	}
}

//==========================================================================
//
// ClassReg :: RegisterClass
//
// Create metadata describing the built-in class this struct is intended
// for.
//
//==========================================================================

PClass *ClassReg::RegisterClass()
{
	static ClassReg *const metaclasses[] =
	{
		&PClass::RegistrationInfo,
		&PClassActor::RegistrationInfo,
	};

	// Skip classes that have already been registered
	if (MyClass != nullptr)
	{
		return MyClass;
	}

	// Add type to list
	PClass *cls;

	if (MetaClassNum >= countof(metaclasses))
	{
		assert(0 && "Class registry has an invalid meta class identifier");
	}

	if (metaclasses[MetaClassNum]->MyClass == nullptr)
	{ // Make sure the meta class is already registered before registering this one
		metaclasses[MetaClassNum]->RegisterClass();
	}
	cls = static_cast<PClass *>(metaclasses[MetaClassNum]->MyClass->CreateNew());

	SetupClass(cls);
	cls->InsertIntoHash();
	if (ParentType != nullptr)
	{
		cls->ParentClass = ParentType->RegisterClass();
	}
	return cls;
}

//==========================================================================
//
// ClassReg :: SetupClass
//
// Copies the class-defining parameters from a ClassReg to the Class object
// created for it.
//
//==========================================================================

void ClassReg::SetupClass(PClass *cls)
{
	assert(MyClass == nullptr);
	MyClass = cls;
	cls->TypeName = FName(Name+1);
	cls->Size = SizeOf;
	cls->Pointers = Pointers;
	cls->ConstructNative = ConstructNative;
	cls->mDescriptiveName.Format("Class<%s>", cls->TypeName.GetChars());
}

//==========================================================================
//
// PClass :: InsertIntoHash
//
// Add class to the type table.
//
//==========================================================================

void PClass::InsertIntoHash ()
{
	size_t bucket;
	PType *found;

	found = TypeTable.FindType(RUNTIME_CLASS(PClass), 0, TypeName, &bucket);
	if (found != nullptr)
	{ // This type has already been inserted
		I_Error("Tried to register class '%s' more than once.\n", TypeName.GetChars());
	}
	else
	{
		TypeTable.AddType(this, RUNTIME_CLASS(PClass), 0, TypeName, bucket);
	}
}

//==========================================================================
//
// PClass :: FindParentClass
//
// Finds a parent class that matches the given name, including itself.
//
//==========================================================================

const PClass *PClass::FindParentClass(FName name) const
{
	for (const PClass *type = this; type != nullptr; type = type->ParentClass)
	{
		if (type->TypeName == name)
		{
			return type;
		}
	}
	return nullptr;
}

//==========================================================================
//
// PClass :: FindClass
//
// Find a type, passed the name as a name.
//
//==========================================================================

PClass *PClass::FindClass (FName zaname)
{
	if (zaname == NAME_None)
	{
		return nullptr;
	}
	return static_cast<PClass *>(TypeTable.FindType(RUNTIME_CLASS(PClass), 0, zaname, nullptr));
}

//==========================================================================
//
// PClass :: CreateNew
//
// Create a new object that this class represents
//
//==========================================================================

DObject *PClass::CreateNew()
{
	uint8_t *mem = (uint8_t *)M_Malloc (Size);
	assert (mem != nullptr);

	// Set this object's defaults before constructing it.
	if (Defaults != nullptr)
		memcpy (mem, Defaults, Size);
	else
		memset (mem, 0, Size);

	if (ConstructNative == nullptr)
	{
		M_Free(mem);
		I_Error("Attempt to instantiate abstract class %s.", TypeName.GetChars());
	}
	ConstructNative (mem);
	((DObject *)mem)->SetClass (const_cast<PClass *>(this));
	InitializeSpecials(mem, Defaults, &PClass::SpecialInits);
	return (DObject *)mem;
}

//==========================================================================
//
// PClass :: InitializeSpecials
//
// Initialize special fields (e.g. strings) of a newly-created instance.
//
//==========================================================================

void PClass::InitializeSpecials(void *addr, void *defaults, TArray<FTypeAndOffset> PClass::*Inits)
{
	// Once we reach a native class, we can stop going up the family tree,
	// since native classes handle initialization natively.
	if ((!bRuntimeClass && Inits == &PClass::SpecialInits) || ParentClass == nullptr)
	{
		return;
	}
	ParentClass->InitializeSpecials(addr, defaults, Inits);
	for (auto tao : (this->*Inits))
	{
		tao.first->InitializeValue((char*)addr + tao.second, defaults == nullptr? nullptr : ((char*)defaults) + tao.second);
	}
}

//==========================================================================
//
// PClass :: DestroySpecials
//
// Destroy special fields (e.g. strings) of an instance that is about to be
// deleted.
//
//==========================================================================

void PClass::DestroySpecials(void *addr)
{
	// Once we reach a native class, we can stop going up the family tree,
	// since native classes handle deinitialization natively.
	if (!bRuntimeClass)
	{
		return;
	}
	assert(ParentClass != nullptr);
	ParentClass->DestroySpecials(addr);
	for (auto tao : SpecialInits)
	{
		tao.first->DestroyValue((uint8_t *)addr + tao.second);
	}
}

//==========================================================================
//
// PClass :: Derive
//
// Copies inheritable values into the derived class and other miscellaneous setup.
//
//==========================================================================

void PClass::Derive(PClass *newclass, FName name)
{
	newclass->bRuntimeClass = true;
	newclass->ParentClass = this;
	newclass->ConstructNative = ConstructNative;
	newclass->Symbols.SetParentTable(&this->Symbols);
	newclass->TypeName = name;
	newclass->mDescriptiveName.Format("Class<%s>", name.GetChars());
	newclass->mVersion = mVersion;
	newclass->MetaSize = MetaSize;

}

//==========================================================================
//
// PClassActor :: InitializeNativeDefaults
//
//==========================================================================

void PClass::InitializeDefaults()
{
	if (IsKindOf(RUNTIME_CLASS(PClassActor)))
	{
		assert(Defaults == nullptr);
		Defaults = (uint8_t *)M_Malloc(Size);

		// run the constructor on the defaults to set the vtbl pointer which is needed to run class-aware functions on them.
		// Temporarily setting bSerialOverride prevents linking into the thinker chains.
		auto s = DThinker::bSerialOverride;
		DThinker::bSerialOverride = true;
		ConstructNative(Defaults);
		DThinker::bSerialOverride = s;
		// We must unlink the defaults from the class list because it's just a static block of data to the engine.
		DObject *optr = (DObject*)Defaults;
		GC::Root = optr->ObjNext;
		optr->ObjNext = nullptr;
		optr->SetClass(this);

		// Copy the defaults from the parent but leave the DObject part alone because it contains important data.
		if (ParentClass->Defaults != nullptr)
		{
			memcpy(Defaults + sizeof(DObject), ParentClass->Defaults + sizeof(DObject), ParentClass->Size - sizeof(DObject));
			if (Size > ParentClass->Size)
			{
				memset(Defaults + ParentClass->Size, 0, Size - ParentClass->Size);
			}
		}
		else
		{
			memset(Defaults + sizeof(DObject), 0, Size - sizeof(DObject));
		}

		assert(MetaSize >= ParentClass->MetaSize);
		if (MetaSize != 0)
		{
			Meta = (uint8_t*)M_Malloc(MetaSize);

			// Copy the defaults from the parent but leave the DObject part alone because it contains important data.
			if (ParentClass->Meta != nullptr)
			{
				memcpy(Meta, ParentClass->Meta, ParentClass->MetaSize);
				if (MetaSize > ParentClass->MetaSize)
				{
					memset(Meta + ParentClass->MetaSize, 0, MetaSize - ParentClass->MetaSize);
				}
			}
			else
			{
				memset(Meta, 0, MetaSize);
			}

			if (MetaSize > 0) memcpy(Meta, ParentClass->Meta, ParentClass->MetaSize);
			else memset(Meta, 0, MetaSize);
		}
	}

	if (bRuntimeClass)
	{
		// Copy parent values from the parent defaults.
		assert(ParentClass != nullptr);
		if (Defaults != nullptr) ParentClass->InitializeSpecials(Defaults, ParentClass->Defaults, &PClass::SpecialInits);
		if (Meta != nullptr) ParentClass->InitializeSpecials(Meta, ParentClass->Meta, &PClass::MetaInits);
		for (const PField *field : Fields)
		{
			if (!(field->Flags & VARF_Native) && !(field->Flags & VARF_Meta))
			{
				field->Type->SetDefaultValue(Defaults, unsigned(field->Offset), &SpecialInits);
			}
		}
	}
	if (Meta != nullptr) ParentClass->InitializeSpecials(Meta, ParentClass->Meta, &PClass::MetaInits);
	for (const PField *field : Fields)
	{
		if (!(field->Flags & VARF_Native) && (field->Flags & VARF_Meta))
		{
			field->Type->SetDefaultValue(Meta, unsigned(field->Offset), &MetaInits);
		}
	}
}

//==========================================================================
//
// PClass :: DeriveData
//
// Copies inheritable data to the child class.
//
//==========================================================================

void PClass::DeriveData(PClass *newclass)
{
}

//==========================================================================
//
// PClass :: CreateDerivedClass
//
// Create a new class based on an existing class
//
//==========================================================================

PClass *PClass::CreateDerivedClass(FName name, unsigned int size)
{
	assert(size >= Size);
	PClass *type;
	bool notnew;

	const PClass *existclass = FindClass(name);

	if (existclass != nullptr)
	{
		// This is a placeholder so fill it in
		if (existclass->Size == TentativeClass)
		{
			type = const_cast<PClass*>(existclass);
			if (!IsDescendantOf(type->ParentClass))
			{
				I_Error("%s must inherit from %s but doesn't.", name.GetChars(), type->ParentClass->TypeName.GetChars());
			}
			DPrintf(DMSG_SPAMMY, "Defining placeholder class %s\n", name.GetChars());
			notnew = true;
		}
		else
		{
			// a different class with the same name already exists. Let the calling code deal with this.
			return nullptr;
		}
	}
	else
	{
		type = static_cast<PClass *>(GetClass()->CreateNew());
		notnew = false;
	}

	type->TypeName = name;
	type->bRuntimeClass = true;
	Derive(type, name);
	type->Size = size;
	if (size != TentativeClass)
	{
		type->InitializeDefaults();
		type->Virtuals = Virtuals;
		DeriveData(type);
	}
	else
		type->ObjectFlags &= OF_Transient;

	if (!notnew)
	{
		type->InsertIntoHash();
	}
	return type;
}

//==========================================================================
//
// PStruct :: AddField
//
// Appends a new metadata field to the end of a struct. Returns either the new field
// or nullptr if a symbol by that name already exists.
//
//==========================================================================

PField *PClass::AddMetaField(FName name, PType *type, uint32_t flags)
{
	PField *field = new PField(name, type, flags);

	// The new field is added to the end of this struct, alignment permitting.
	field->Offset = (MetaSize + (type->Align - 1)) & ~(type->Align - 1);

	// Enlarge this struct to enclose the new field.
	MetaSize = unsigned(field->Offset + type->Size);

	// This struct's alignment is the same as the largest alignment of any of
	// its fields.
	Align = MAX(Align, type->Align);

	if (Symbols.AddSymbol(field) == nullptr)
	{ // name is already in use
		field->Destroy();
		return nullptr;
	}
	Fields.Push(field);

	return field;
}

//==========================================================================
//
// PClass :: AddField
//
//==========================================================================

PField *PClass::AddField(FName name, PType *type, uint32_t flags)
{
	if (!(flags & VARF_Meta))
	{
		unsigned oldsize = Size;
		PField *field = Super::AddField(name, type, flags);

		// Only initialize the defaults if they have already been created.
		// For ZScript this is not the case, it will first define all fields before
		// setting up any defaults for any class.
		if (field != nullptr && !(flags & VARF_Native) && Defaults != nullptr)
		{
			Defaults = (uint8_t *)M_Realloc(Defaults, Size);
			memset(Defaults + oldsize, 0, Size - oldsize);
		}
		return field;
	}
	else
	{
		unsigned oldsize = MetaSize;
		PField *field = AddMetaField(name, type, flags);

		// Only initialize the defaults if they have already been created.
		// For ZScript this is not the case, it will first define all fields before
		// setting up any defaults for any class.
		if (field != nullptr && !(flags & VARF_Native) && Meta != nullptr)
		{
			Meta = (uint8_t *)M_Realloc(Meta, MetaSize);
			memset(Meta + oldsize, 0, MetaSize - oldsize);
		}
		return field;
	}
}

//==========================================================================
//
// PClass :: FindClassTentative
//
// Like FindClass but creates a placeholder if no class is found.
// This will be filled in when the actual class is constructed.
//
//==========================================================================

PClass *PClass::FindClassTentative(FName name)
{
	if (name == NAME_None)
	{
		return nullptr;
	}
	size_t bucket;

	PType *found = TypeTable.FindType(RUNTIME_CLASS(PClass),
		/*FIXME:Outer*/0, name, &bucket);

	if (found != nullptr)
	{
		return static_cast<PClass *>(found);
	}
	PClass *type = static_cast<PClass *>(GetClass()->CreateNew());
	DPrintf(DMSG_SPAMMY, "Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	Derive(type, name);
	type->Size = TentativeClass;
	TypeTable.AddType(type, RUNTIME_CLASS(PClass), 0, name, bucket);
	return type;
}

//==========================================================================
//
// PClass :: FindVirtualIndex
//
// Compares a prototype with the existing list of virtual functions
// and returns an index if something matching is found.
//
//==========================================================================

int PClass::FindVirtualIndex(FName name, PPrototype *proto)
{
	for (unsigned i = 0; i < Virtuals.Size(); i++)
	{
		if (Virtuals[i]->Name == name)
		{
			auto vproto = Virtuals[i]->Proto;
			if (vproto->ReturnTypes.Size() != proto->ReturnTypes.Size() ||
				vproto->ArgumentTypes.Size() != proto->ArgumentTypes.Size())
			{
				continue;	// number of parameters does not match, so it's incompatible
			}
			bool fail = false;
			// The first argument is self and will mismatch so just skip it.
			for (unsigned a = 1; a < proto->ArgumentTypes.Size(); a++)
			{
				if (proto->ArgumentTypes[a] != vproto->ArgumentTypes[a])
				{
					fail = true;
					break;
				}
			}
			if (fail) continue;

			for (unsigned a = 0; a < proto->ReturnTypes.Size(); a++)
			{
				if (proto->ReturnTypes[a] != vproto->ReturnTypes[a])
				{
					fail = true;
					break;
				}
			}
			if (!fail) return i;
		}
	}
	return -1;
}

//==========================================================================
//
// PClass :: BuildFlatPointers
//
// Create the FlatPointers array, if it doesn't exist already.
// It comprises all the Pointers from superclasses plus this class's own
// Pointers. If this class does not define any new Pointers, then
// FlatPointers will be set to the same array as the super class.
//
//==========================================================================

void PClass::BuildFlatPointers ()
{
	if (FlatPointers != nullptr)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == nullptr)
	{ // No parent (i.e. DObject: FlatPointers is the same as Pointers.
		if (Pointers == nullptr)
		{ // No pointers: Make FlatPointers a harmless non-nullptr.
			FlatPointers = &TheEnd;
		}
		else
		{
			FlatPointers = Pointers;
		}
	}
	else
	{
		ParentClass->BuildFlatPointers ();

		TArray<size_t> ScriptPointers;

		// Collect all pointers in scripted fields. These are not part of the Pointers list.
		for (auto field : Fields)
		{
			if (!(field->Flags & VARF_Native))
			{
				field->Type->SetPointer(Defaults, unsigned(field->Offset), &ScriptPointers);
			}
		}

		if (Pointers == nullptr && ScriptPointers.Size() == 0)
		{ // No new pointers: Just use the same FlatPointers as the parent.
			FlatPointers = ParentClass->FlatPointers;
		}
		else
		{ // New pointers: Create a new FlatPointers array and add them.
			int numPointers, numSuperPointers;

			if (Pointers != nullptr)
			{
				// Count pointers defined by this class.
				for (numPointers = 0; Pointers[numPointers] != ~(size_t)0; numPointers++)
				{
				}
			}
			else numPointers = 0;

			// Count pointers defined by superclasses.
			for (numSuperPointers = 0; ParentClass->FlatPointers[numSuperPointers] != ~(size_t)0; numSuperPointers++)
			{ }

			// Concatenate them into a new array
			size_t *flat = (size_t*)ClassDataAllocator.Alloc(sizeof(size_t) * (numPointers + numSuperPointers + ScriptPointers.Size() + 1));
			if (numSuperPointers > 0)
			{
				memcpy (flat, ParentClass->FlatPointers, sizeof(size_t)*numSuperPointers);
			}
			if (numPointers > 0)
			{
				memcpy(flat + numSuperPointers, Pointers, sizeof(size_t)*numPointers);
			}
			if (ScriptPointers.Size() > 0)
			{
				memcpy(flat + numSuperPointers + numPointers, &ScriptPointers[0], sizeof(size_t) * ScriptPointers.Size());
			}
			flat[numSuperPointers + numPointers + ScriptPointers.Size()] = ~(size_t)0;
			FlatPointers = flat;
		}
	}
}

//==========================================================================
//
// PClass :: BuildArrayPointers
//
// same as above, but creates a list to dynamic object arrays
//
//==========================================================================

void PClass::BuildArrayPointers()
{
	if (ArrayPointers != nullptr)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == nullptr)
	{ // No parent (i.e. DObject: FlatPointers is the same as Pointers.
		ArrayPointers = &TheEnd;
	}
	else
	{
		ParentClass->BuildArrayPointers();

		TArray<size_t> ScriptPointers;

		// Collect all arrays to pointers in scripted fields.
		for (auto field : Fields)
		{
			if (!(field->Flags & VARF_Native))
			{
				field->Type->SetPointerArray(Defaults, unsigned(field->Offset), &ScriptPointers);
			}
		}

		if (ScriptPointers.Size() == 0)
		{ // No new pointers: Just use the same ArrayPointers as the parent.
			ArrayPointers = ParentClass->ArrayPointers;
		}
		else
		{ // New pointers: Create a new FlatPointers array and add them.
			int numSuperPointers;

			// Count pointers defined by superclasses.
			for (numSuperPointers = 0; ParentClass->ArrayPointers[numSuperPointers] != ~(size_t)0; numSuperPointers++)
			{
			}

			// Concatenate them into a new array
			size_t *flat = (size_t*)ClassDataAllocator.Alloc(sizeof(size_t) * (numSuperPointers + ScriptPointers.Size() + 1));
			if (numSuperPointers > 0)
			{
				memcpy(flat, ParentClass->ArrayPointers, sizeof(size_t)*numSuperPointers);
			}
			if (ScriptPointers.Size() > 0)
			{
				memcpy(flat + numSuperPointers, &ScriptPointers[0], sizeof(size_t) * ScriptPointers.Size());
			}
			flat[numSuperPointers + ScriptPointers.Size()] = ~(size_t)0;
			ArrayPointers = flat;
		}
	}
}

//==========================================================================
//
// PClass :: NativeClass
//
// Finds the native type underlying this class.
//
//==========================================================================

const PClass *PClass::NativeClass() const
{
	const PClass *cls = this;

	while (cls && cls->bRuntimeClass)
		cls = cls->ParentClass;

	return cls;
}

VMFunction *PClass::FindFunction(FName clsname, FName funcname)
{
	auto cls = PClass::FindClass(clsname);
	if (!cls) return nullptr;
	auto func = dyn_cast<PFunction>(cls->Symbols.FindSymbol(funcname, true));
	if (!func) return nullptr;
	return func->Variants[0].Implementation;
}

void PClass::FindFunction(VMFunction **pptr, FName clsname, FName funcname)
{
	auto cls = PClass::FindClass(clsname);
	if (!cls) return;
	auto func = dyn_cast<PFunction>(cls->Symbols.FindSymbol(funcname, true));
	if (!func) return;
	*pptr = func->Variants[0].Implementation;
	FunctionPtrList.Push(pptr);
}


/* FTypeTable **************************************************************/

//==========================================================================
//
// FTypeTable :: FindType
//
//==========================================================================

PType *FTypeTable::FindType(PClass *metatype, intptr_t parm1, intptr_t parm2, size_t *bucketnum)
{
	size_t bucket = Hash(metatype, parm1, parm2) % HASH_SIZE;
	if (bucketnum != nullptr)
	{
		*bucketnum = bucket;
	}
	for (PType *type = TypeHash[bucket]; type != nullptr; type = type->HashNext)
	{
		if (type->TypeTableType == metatype && type->IsMatch(parm1, parm2))
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

void FTypeTable::AddType(PType *type, PClass *metatype, intptr_t parm1, intptr_t parm2, size_t bucket)
{
#ifdef _DEBUG
	size_t bucketcheck;
	assert(FindType(metatype, parm1, parm2, &bucketcheck) == nullptr && "Type must not be inserted more than once");
	assert(bucketcheck == bucket && "Passed bucket was wrong");
#endif
	type->TypeTableType = metatype;
	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
	type->Release();
}

//==========================================================================
//
// FTypeTable :: AddType - Simple Version
//
//==========================================================================

void FTypeTable::AddType(PType *type)
{
	intptr_t parm1, parm2;
	size_t bucket;

	// Type table stuff id only needed to let all classes hash to the same group. For all other types this is pointless.
	type->TypeTableType = type->GetClass();
	PClass *metatype = type->TypeTableType;
	type->GetTypeIDs(parm1, parm2);
	bucket = Hash(metatype, parm1, parm2) % HASH_SIZE;
	assert(FindType(metatype, parm1, parm2, nullptr) == nullptr && "Type must not be inserted more than once");

	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
	type->Release();
}

//==========================================================================
//
// FTypeTable :: Hash												STATIC
//
//==========================================================================

size_t FTypeTable::Hash(const PClass *p1, intptr_t p2, intptr_t p3)
{
	size_t i1 = (size_t)p1;

	// Swap the high and low halves of i1. The compiler should be smart enough
	// to transform this into a ROR or ROL.
	i1 = (i1 >> (sizeof(size_t)*4)) | (i1 << (sizeof(size_t)*4));

	if (p1 != RUNTIME_CLASS(PPrototype))
	{
		size_t i2 = (size_t)p2;
		size_t i3 = (size_t)p3;
		return (~i1 ^ i2) + i3 * 961748927;	// i3 is multiplied by a prime
	}
	else
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

