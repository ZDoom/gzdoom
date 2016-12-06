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
#include "a_artifacts.h"
#include "a_weaponpiece.h"
#include "d_player.h"
#include "doomerrors.h"
#include "fragglescript/t_fs.h"
#include "a_keys.h"
#include "a_health.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
EXTERN_CVAR(Bool, strictdecorate);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FTypeTable TypeTable;
PSymbolTable GlobalSymbols;
TArray<PClass *> PClass::AllClasses;
bool PClass::bShutdown;

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
PStateLabel *TypeStateLabel;
PStruct *TypeVector2;
PStruct *TypeVector3;
PStruct *TypeColorStruct;
PStruct *TypeStringStruct;
PPointer *TypeNullPtr;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// A harmless non-NULL FlatPointer for classes without pointers.
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
		for (PType *ty = TypeTable.TypeHash[i]; ty != NULL; ty = ty->HashNext)
		{
			Printf(" -> %s", ty->IsKindOf(RUNTIME_CLASS(PNamedType)) ? static_cast<PNamedType*>(ty)->TypeName.GetChars(): ty->GetClass()->TypeName.GetChars());
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

/* PClassType *************************************************************/

IMPLEMENT_CLASS(PClassType, false, false)

//==========================================================================
//
// PClassType Constructor
//
//==========================================================================

PClassType::PClassType()
: TypeTableType(NULL)
{
}

//==========================================================================
//
// PClassType :: DeriveData
//
//==========================================================================

void PClassType::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassType)));
	Super::DeriveData(newclass);
	static_cast<PClassType *>(newclass)->TypeTableType = TypeTableType;
}

/* PClassClass ************************************************************/

IMPLEMENT_CLASS(PClassClass, false, false)

//==========================================================================
//
// PClassClass Constructor
//
// The only thing we want to do here is automatically set TypeTableType
// to PClass.
//
//==========================================================================

PClassClass::PClassClass()
{
	TypeTableType = RUNTIME_CLASS(PClass);
}

/* PType ******************************************************************/

IMPLEMENT_CLASS(PType, true, true)

IMPLEMENT_POINTERS_START(PType)
	IMPLEMENT_POINTER(HashNext)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PType Parameterized Constructor
//
//==========================================================================

PType::PType(unsigned int size, unsigned int align)
: Size(size), Align(align), HashNext(NULL)
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
// PType :: PropagateMark
//
//==========================================================================

size_t PType::PropagateMark()
{
	size_t marked = Symbols.MarkSymbols();
	return marked + Super::PropagateMark();
}

//==========================================================================
//
// PType :: AddConversion
//
//==========================================================================

bool PType::AddConversion(PType *target, void (*convertconst)(ZCC_ExprConstant *, class FSharedStringArena &))
{
	// Make sure a conversion hasn't already been registered
	for (unsigned i = 0; i < Conversions.Size(); ++i)
	{
		if (Conversions[i].TargetType == target)
			return false;
	}
	Conversions.Push(Conversion(target, convertconst));
	return true;
}

//==========================================================================
//
// PType :: FindConversion
//
// Returns <0 if there is no path to target. Otherwise, returns the distance
// to target and fills slots (if non-NULL) with the necessary conversions
// to get there. A result of 0 means this is the target.
//
//==========================================================================

int PType::FindConversion(PType *target, const PType::Conversion **slots, int numslots)
{
	if (this == target)
	{
		return 0;
	}
	// The queue is implemented as a ring buffer
	VisitQueue queue;
	VisitedNodeSet visited;

	// Use a breadth-first search to find the shortest path to the target.
	MarkPred(NULL, -1, -1);
	queue.Push(this);
	visited.Insert(this);
	while (!queue.IsEmpty())
	{
		PType *t = queue.Pop();
		if (t == target)
		{ // found it
			if (slots != NULL)
			{
				if (t->Distance >= numslots)
				{ // Distance is too far for the output
					return -2;
				}
				t->FillConversionPath(slots);
			}
			return t->Distance + 1;
		}
		for (unsigned i = 0; i < t->Conversions.Size(); ++i)
		{
			PType *succ = t->Conversions[i].TargetType;
			if (!visited.Check(succ))
			{
				succ->MarkPred(t, i, t->Distance + 1);
				visited.Insert(succ);
				queue.Push(succ);
			}
		}
	}
	return -1;
}

//==========================================================================
//
// PType :: FillConversionPath
//
// Traces backwards from the target type to the original type and fills in
// the conversions necessary to get between them. slots must point to an
// array large enough to contain the entire path.
//
//==========================================================================

void PType::FillConversionPath(const PType::Conversion **slots)
{
	for (PType *node = this; node->Distance >= 0; node = node->PredType)
	{
		assert(node->PredType != NULL);
		slots[node->Distance] = &node->PredType->Conversions[node->PredConv];
	}
}

//==========================================================================
//
// PType :: VisitQueue :: Push
//
//==========================================================================

void PType::VisitQueue::Push(PType *type)
{
	Queue[In] = type;
	Advance(In);
	assert(!IsEmpty() && "Queue overflowed");
}

//==========================================================================
//
// PType :: VisitQueue :: Pop
//
//==========================================================================

PType *PType::VisitQueue::Pop()
{
	if (IsEmpty())
	{
		return NULL;
	}
	PType *node = Queue[Out];
	Advance(Out);
	return node;
}

//==========================================================================
//
// PType :: VisitedNodeSet :: Insert
//
//==========================================================================

void PType::VisitedNodeSet::Insert(PType *node)
{
	assert(!Check(node) && "Node was already inserted");
	size_t buck = Hash(node) & (countof(Buckets) - 1);
	node->VisitNext = Buckets[buck];
	Buckets[buck] = node;
}

//==========================================================================
//
// PType :: VisitedNodeSet :: Check
//
//==========================================================================

bool PType::VisitedNodeSet::Check(const PType *node)
{
	size_t buck = Hash(node) & (countof(Buckets) - 1);
	for (const PType *probe = Buckets[buck]; probe != NULL; probe = probe->VisitNext)
	{
		if (probe == node)
		{
			return true;
		}
	}
	return false;
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
// Set up TypeTableType values for every PType child and create basic types.
//
//==========================================================================

void ReleaseGlobalSymbols()
{
	TypeTable.Clear();
	GlobalSymbols.ReleaseSymbols();
}

void PType::StaticInit()
{
	// Add types to the global symbol table.
	atterm(ReleaseGlobalSymbols);

	// Set up TypeTable hash keys.
	RUNTIME_CLASS(PErrorType)->TypeTableType = RUNTIME_CLASS(PErrorType);
	RUNTIME_CLASS(PVoidType)->TypeTableType = RUNTIME_CLASS(PVoidType);
	RUNTIME_CLASS(PInt)->TypeTableType = RUNTIME_CLASS(PInt);
	RUNTIME_CLASS(PBool)->TypeTableType = RUNTIME_CLASS(PBool);
	RUNTIME_CLASS(PFloat)->TypeTableType = RUNTIME_CLASS(PFloat);
	RUNTIME_CLASS(PString)->TypeTableType = RUNTIME_CLASS(PString);
	RUNTIME_CLASS(PName)->TypeTableType = RUNTIME_CLASS(PName);
	RUNTIME_CLASS(PSound)->TypeTableType = RUNTIME_CLASS(PSound);
	RUNTIME_CLASS(PSpriteID)->TypeTableType = RUNTIME_CLASS(PSpriteID);
	RUNTIME_CLASS(PTextureID)->TypeTableType = RUNTIME_CLASS(PTextureID);
	RUNTIME_CLASS(PColor)->TypeTableType = RUNTIME_CLASS(PColor);
	RUNTIME_CLASS(PPointer)->TypeTableType = RUNTIME_CLASS(PPointer);
	RUNTIME_CLASS(PClassPointer)->TypeTableType = RUNTIME_CLASS(PClassPointer);
	RUNTIME_CLASS(PEnum)->TypeTableType = RUNTIME_CLASS(PEnum);
	RUNTIME_CLASS(PArray)->TypeTableType = RUNTIME_CLASS(PArray);
	RUNTIME_CLASS(PDynArray)->TypeTableType = RUNTIME_CLASS(PDynArray);
	RUNTIME_CLASS(PMap)->TypeTableType = RUNTIME_CLASS(PMap);
	RUNTIME_CLASS(PStruct)->TypeTableType = RUNTIME_CLASS(PStruct);
	RUNTIME_CLASS(PNativeStruct)->TypeTableType = RUNTIME_CLASS(PNativeStruct);
	RUNTIME_CLASS(PPrototype)->TypeTableType = RUNTIME_CLASS(PPrototype);
	RUNTIME_CLASS(PClass)->TypeTableType = RUNTIME_CLASS(PClass);
	RUNTIME_CLASS(PStatePointer)->TypeTableType = RUNTIME_CLASS(PStatePointer);
	RUNTIME_CLASS(PStateLabel)->TypeTableType = RUNTIME_CLASS(PStateLabel);

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

	TypeColorStruct = NewStruct("@ColorStruct", nullptr);	//This name is intentionally obfuscated so that it cannot be used explicitly. The point of this type is to gain access to the single channels of a color value.
	TypeStringStruct = NewNativeStruct(NAME_String, nullptr);
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



	GlobalSymbols.AddSymbol(new PSymbolType(NAME_sByte, TypeSInt8));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Byte, TypeUInt8));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Short, TypeSInt16));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_uShort, TypeUInt16));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Int, TypeSInt32));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_uInt, TypeUInt32));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Bool, TypeBool));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Float, TypeFloat64));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Double, TypeFloat64));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Float32, TypeFloat32));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Float64, TypeFloat64));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_String, TypeString));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Name, TypeName));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Sound, TypeSound));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Color, TypeColor));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_State, TypeState));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Vector2, TypeVector2));
	GlobalSymbols.AddSymbol(new PSymbolType(NAME_Vector3, TypeVector3));
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

IMPLEMENT_CLASS(PNamedType, true, true)

IMPLEMENT_POINTERS_START(PNamedType)
	IMPLEMENT_POINTER(Outer)
IMPLEMENT_POINTERS_END

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

//==========================================================================
//
// PNamedType :: QualifiedName
//
//==========================================================================

FString PNamedType::QualifiedName() const
{
	FString out;
	if (Outer != nullptr) out = Outer->QualifiedName();
	out << "::" << TypeName;
	return out;
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
		int maxval = (1 << ((8 * size) - 1)) - 1;
		int minval = -maxval - 1;
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, minval));
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this, maxval));
	}
	else
	{
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, 0u));
		Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this, (1u << (8 * size)) - 1));
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
		*(BYTE *)addr = val;
	}
	else if (Size == 2)
	{
		*(WORD *)addr = val;
	}
	else if (Size == 8)
	{
		*(QWORD *)addr = val;
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
		return Unsigned ? *(BYTE *)addr : *(SBYTE *)addr;
	}
	else if (Size == 2)
	{
		return Unsigned ? *(WORD *)addr : *(SWORD *)addr;
	}
	else if (Size == 8)
	{ // truncated output
		return (int)*(QWORD *)addr;
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
	// Override the default max set by PInt's constructor
	PSymbolConstNumeric *maxsym = static_cast<PSymbolConstNumeric *>(Symbols.FindSymbol(NAME_Max, false));
	assert(maxsym != NULL && maxsym->IsKindOf(RUNTIME_CLASS(PSymbolConstNumeric)));
	maxsym->Value = 1;
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
	if (base != nullptr) new((BYTE *)base + offset) FString;
	if (special != NULL)
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

IMPLEMENT_CLASS(PPointer, false, true)

IMPLEMENT_POINTERS_START(PPointer)
	IMPLEMENT_POINTER(PointedType)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PPointer - Default Constructor
//
//==========================================================================

PPointer::PPointer()
: PBasicType(sizeof(void *), alignof(void *)), PointedType(NULL), IsConst(false)
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
	SetOps();
}

//==========================================================================
//
// PPointer :: GetStoreOp
//
//==========================================================================

void PPointer::SetOps()
{
	storeOp = OP_SP;
	loadOp = (PointedType && PointedType->IsKindOf(RUNTIME_CLASS(PClass))) ? OP_LO : OP_LP;
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
// PPointer :: SetDefaultValue
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
		ar(key, *(DObject **)addr);
	}
	else
	{
		assert(0 && "Pointer points to a type we don't handle");
		I_Error("Attempt to save pointer to unhandled type");
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
		bool res = false;
		::Serialize(ar, key, *(DObject **)addr, nullptr, &res);
		return res;
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
	if (ptype == NULL)
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

IMPLEMENT_CLASS(PClassPointer,false, true)

IMPLEMENT_POINTERS_START(PClassPointer)
	IMPLEMENT_POINTER(ClassRestriction)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PClassPointer - Default Constructor
//
//==========================================================================

PClassPointer::PClassPointer()
: PPointer(RUNTIME_CLASS(PClass)), ClassRestriction(NULL)
{
	mDescriptiveName = "ClassPointer";
}

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
	if (ptype == NULL)
	{
		ptype = new PClassPointer(restrict);
		TypeTable.AddType(ptype, RUNTIME_CLASS(PClassPointer), (intptr_t)RUNTIME_CLASS(PClass), (intptr_t)restrict, bucket);
	}
	return static_cast<PClassPointer *>(ptype);
}

/* PEnum ******************************************************************/

IMPLEMENT_CLASS(PEnum, false, true)

IMPLEMENT_POINTERS_START(PEnum)
	IMPLEMENT_POINTER(ValueType)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PEnum - Default Constructor
//
//==========================================================================

PEnum::PEnum()
: ValueType(NULL)
{
	mDescriptiveName = "Enum";
}

//==========================================================================
//
// PEnum - Parameterized Constructor
//
//==========================================================================

PEnum::PEnum(FName name, PTypeBase *outer)
: PNamedType(name, outer), ValueType(NULL)
{
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
	PType *etype = TypeTable.FindType(RUNTIME_CLASS(PEnum), (intptr_t)outer, (intptr_t)name, &bucket);
	if (etype == NULL)
	{
		etype = new PEnum(name, outer);
		TypeTable.AddType(etype, RUNTIME_CLASS(PEnum), (intptr_t)outer, (intptr_t)name, bucket);
	}
	return static_cast<PEnum *>(etype);
}

/* PArray *****************************************************************/

IMPLEMENT_CLASS(PArray, false, true)

IMPLEMENT_POINTERS_START(PArray)
	IMPLEMENT_POINTER(ElementType)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PArray - Default Constructor
//
//==========================================================================

PArray::PArray()
: ElementType(NULL), ElementCount(0)
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
		const BYTE *addrb = (const BYTE *)addr;
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
		BYTE *addrb = (BYTE *)addr;
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
	if (atype == NULL)
	{
		atype = new PArray(type, count);
		TypeTable.AddType(atype, RUNTIME_CLASS(PArray), (intptr_t)type, count, bucket);
	}
	return (PArray *)atype;
}

/* PDynArray **************************************************************/

IMPLEMENT_CLASS(PDynArray, false, true)

IMPLEMENT_POINTERS_START(PDynArray)
	IMPLEMENT_POINTER(ElementType)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PDynArray - Default Constructor
//
//==========================================================================

PDynArray::PDynArray()
: ElementType(NULL)
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

PDynArray::PDynArray(PType *etype)
: ElementType(etype)
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
	if (atype == NULL)
	{
		atype = new PDynArray(type);
		TypeTable.AddType(atype, RUNTIME_CLASS(PDynArray), (intptr_t)type, 0, bucket);
	}
	return (PDynArray *)atype;
}

/* PMap *******************************************************************/

IMPLEMENT_CLASS(PMap, false, true)

IMPLEMENT_POINTERS_START(PMap)
	IMPLEMENT_POINTER(KeyType)
	IMPLEMENT_POINTER(ValueType)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// PMap - Default Constructor
//
//==========================================================================

PMap::PMap()
: KeyType(NULL), ValueType(NULL)
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
	if (maptype == NULL)
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
		if (!(field->Flags & VARF_Transient))
		{
			field->Type->WriteValue(ar, field->SymbolName.GetChars(), (const BYTE *)addr + field->Offset);
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
		if (sym == NULL)
		{
			DPrintf(DMSG_ERROR, "Cannot find field %s in %s\n",
				label, TypeName.GetChars());
		}
		else if (!sym->IsKindOf(RUNTIME_CLASS(PField)))
		{
			DPrintf(DMSG_ERROR, "Symbol %s in %s is not a field\n",
				label, TypeName.GetChars());
		}
		else
		{
			readsomething |= static_cast<const PField *>(sym)->Type->ReadValue(ar, nullptr,
				(BYTE *)addr + static_cast<const PField *>(sym)->Offset);
		}
	}
	return readsomething || !foundsomething;
}

//==========================================================================
//
// PStruct :: AddField
//
// Appends a new field to the end of a struct. Returns either the new field
// or NULL if a symbol by that name already exists.
//
//==========================================================================

PField *PStruct::AddField(FName name, PType *type, DWORD flags)
{
	PField *field = new PField(name, type, flags);

	// The new field is added to the end of this struct, alignment permitting.
	field->Offset = (Size + (type->Align - 1)) & ~(type->Align - 1);

	// Enlarge this struct to enclose the new field.
	Size = unsigned(field->Offset + type->Size);

	// This struct's alignment is the same as the largest alignment of any of
	// its fields.
	Align = MAX(Align, type->Align);

	if (Symbols.AddSymbol(field) == NULL)
	{ // name is already in use
		delete field;
		return NULL;
	}
	Fields.Push(field);

	return field;
}

//==========================================================================
//
// PStruct :: AddField
//
// Appends a new native field to the struct. Returns either the new field
// or NULL if a symbol by that name already exists.
//
//==========================================================================

PField *PStruct::AddNativeField(FName name, PType *type, size_t address, DWORD flags, int bitvalue)
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
// PStruct :: PropagateMark
//
//==========================================================================

size_t PStruct::PropagateMark()
{
	GC::MarkArray(Fields);
	return Fields.Size() * sizeof(void*) + Super::PropagateMark();
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
	PType *stype = TypeTable.FindType(RUNTIME_CLASS(PStruct), (intptr_t)outer, (intptr_t)name, &bucket);
	if (stype == NULL)
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

PNativeStruct::PNativeStruct(FName name)
	: PStruct(name, nullptr)
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
	PType *stype = TypeTable.FindType(RUNTIME_CLASS(PNativeStruct), (intptr_t)outer, (intptr_t)name, &bucket);
	if (stype == NULL)
	{
		stype = new PNativeStruct(name);
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
: PSymbol(NAME_None), Offset(0), Type(NULL), Flags(0)
{
}


PField::PField(FName name, PType *type, DWORD flags, size_t offset, int bitvalue)
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
	if (proto == NULL)
	{
		proto = new PPrototype(rettypes, argtypes);
		TypeTable.AddType(proto, RUNTIME_CLASS(PPrototype), (intptr_t)&argtypes, (intptr_t)&rettypes, bucket);
	}
	return static_cast<PPrototype *>(proto);
}

/* PFunction **************************************************************/

IMPLEMENT_CLASS(PFunction, false, false)

//==========================================================================
//
// PFunction :: PropagataMark
//
//==========================================================================

size_t PFunction::PropagateMark()
{
	for (unsigned i = 0; i < Variants.Size(); ++i)
	{
		GC::Mark(Variants[i].Proto);
		GC::Mark(Variants[i].Implementation);
	}
	return Variants.Size() * sizeof(Variants[0]) + Super::PropagateMark();
}

//==========================================================================
//
// PFunction :: AddVariant
//
// Adds a new variant for this function. Does not check if a matching
// variant already exists.
//
//==========================================================================

unsigned PFunction::AddVariant(PPrototype *proto, TArray<DWORD> &argflags, TArray<FName> &argnames, VMFunction *impl, int flags, int useflags)
{
	Variant variant;

	// I do not think we really want to deal with overloading here...
	assert(Variants.Size() == 0);

	variant.Flags = flags;
	variant.UseFlags = useflags;
	variant.Proto = proto;
	variant.ArgFlags = std::move(argflags);
	variant.ArgNames = std::move(argnames);
	variant.Implementation = impl;
	if (impl != nullptr) impl->Proto = proto;

	// SelfClass can differ from OwningClass, but this is variant-dependent.
	// Unlike the owner there can be cases where different variants can have different SelfClasses.
	// (Of course only if this ever gets enabled...)
	if (flags & VARF_Method)
	{
		assert(proto->ArgumentTypes.Size() > 0);
		auto selftypeptr = dyn_cast<PPointer>(proto->ArgumentTypes[0]);
		assert(selftypeptr != nullptr);
		variant.SelfClass = dyn_cast<PStruct>(selftypeptr->PointedType);
		assert(variant.SelfClass != nullptr);
	}
	else
	{
		variant.SelfClass = nullptr;
	}

	return Variants.Push(variant);
}

/* PClass *****************************************************************/

IMPLEMENT_CLASS(PClass, false, true)

IMPLEMENT_POINTERS_START(PClass)
	IMPLEMENT_POINTER(ParentClass)
IMPLEMENT_POINTERS_END

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
	if (type != NULL)
	{
		RecurseWriteFields(type->ParentClass, ar, addr);
		// Don't write this part if it has no non-transient variables
		for (unsigned i = 0; i < type->Fields.Size(); ++i)
		{
			if (!(type->Fields[i]->Flags & VARF_Transient))
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
			for (parent = this; parent != NULL; parent = parent->ParentClass)
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

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != NULL)
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
// Frees FlatPointers belonging to all classes. Only really needed to avoid
// memory leak warnings at exit.
//
//==========================================================================

void PClass::StaticShutdown ()
{
	TArray<size_t *> uniqueFPs(64);
	unsigned int i, j;

	FS_Close();	// this must be done before the classes get deleted.
	for (i = 0; i < PClass::AllClasses.Size(); ++i)
	{
		PClass *type = PClass::AllClasses[i];
		PClass::AllClasses[i] = NULL;
		if (type->FlatPointers != &TheEnd && type->FlatPointers != type->Pointers)
		{
			// FlatPointers are shared by many classes, so we must check for
			// duplicates and only delete those that are unique.
			for (j = 0; j < uniqueFPs.Size(); ++j)
			{
				if (type->FlatPointers == uniqueFPs[j])
				{
					break;
				}
			}
			if (j == uniqueFPs.Size())
			{
				uniqueFPs.Push(const_cast<size_t *>(type->FlatPointers));
			}
		}
		type->Destroy();
	}
	for (i = 0; i < uniqueFPs.Size(); ++i)
	{
		delete[] uniqueFPs[i];
	}
	TypeTable.Clear();
	bShutdown = true;

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
// PClass and PClassClass have intermingling dependencies on their
// definitions. To sort this out, we explicitly define them before
// proceeding with the RegisterClass loop in StaticInit().
//
//==========================================================================

void PClass::StaticBootstrap()
{
	PClassClass *clscls = new PClassClass;
	PClassClass::RegistrationInfo.SetupClass(clscls);

	PClassClass *cls = new PClassClass;
	PClass::RegistrationInfo.SetupClass(cls);

	// The PClassClass constructor initialized these to NULL, because the
	// PClass metadata had not been created yet. Now it has, so we know what
	// they should be and can insert them into the type table successfully.
	clscls->TypeTableType = cls;
	cls->TypeTableType = cls;
	clscls->InsertIntoHash();
	cls->InsertIntoHash();

	// Create parent objects before we go so that these definitions are complete.
	clscls->ParentClass = PClassType::RegistrationInfo.ParentType->RegisterClass();
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
	HashNext = nullptr;
	Defaults = nullptr;
	bRuntimeClass = false;
	bExported = false;
	bDecorateClass = false;
	ConstructNative = nullptr;
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
	if (Defaults != NULL)
	{
		M_Free(Defaults);
		Defaults = NULL;
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
		&PClassInventory::RegistrationInfo,
		&PClassAmmo::RegistrationInfo,
		&PClassHealth::RegistrationInfo,
		&PClassPuzzleItem::RegistrationInfo,
		&PClassWeapon::RegistrationInfo,
		&PClassPlayerPawn::RegistrationInfo,
		&PClassType::RegistrationInfo,
		&PClassClass::RegistrationInfo,
	};

	// Skip classes that have already been registered
	if (MyClass != NULL)
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
	assert(MyClass == NULL);
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

	found = TypeTable.FindType(RUNTIME_CLASS(PClass), (intptr_t)Outer, TypeName, &bucket);
	if (found != NULL)
	{ // This type has already been inserted
		I_Error("Tried to register class '%s' more than once.\n", TypeName.GetChars());
	}
	else
	{
		TypeTable.AddType(this, RUNTIME_CLASS(PClass), (intptr_t)Outer, TypeName, bucket);
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
	for (const PClass *type = this; type != NULL; type = type->ParentClass)
	{
		if (type->TypeName == name)
		{
			return type;
		}
	}
	return NULL;
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
		return NULL;
	}
	return static_cast<PClass *>(TypeTable.FindType(RUNTIME_CLASS(PClass),
		/*FIXME:Outer*/0, zaname, NULL));
}

//==========================================================================
//
// PClass :: CreateNew
//
// Create a new object that this class represents
//
//==========================================================================

DObject *PClass::CreateNew() const
{
	BYTE *mem = (BYTE *)M_Malloc (Size);
	assert (mem != NULL);

	// Set this object's defaults before constructing it.
	if (Defaults != NULL)
		memcpy (mem, Defaults, Size);
	else
		memset (mem, 0, Size);

	ConstructNative (mem);
	((DObject *)mem)->SetClass (const_cast<PClass *>(this));
	InitializeSpecials(mem);
	return (DObject *)mem;
}

//==========================================================================
//
// PClass :: InitializeSpecials
//
// Initialize special fields (e.g. strings) of a newly-created instance.
//
//==========================================================================

void PClass::InitializeSpecials(void *addr) const
{
	// Once we reach a native class, we can stop going up the family tree,
	// since native classes handle initialization natively.
	if (!bRuntimeClass)
	{
		return;
	}
	assert(ParentClass != NULL);
	ParentClass->InitializeSpecials(addr);
	for (auto tao : SpecialInits)
	{
		tao.first->InitializeValue((BYTE*)addr + tao.second, Defaults == nullptr? nullptr : Defaults + tao.second);
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

void PClass::DestroySpecials(void *addr) const
{
	// Once we reach a native class, we can stop going up the family tree,
	// since native classes handle deinitialization natively.
	if (!bRuntimeClass)
	{
		return;
	}
	assert(ParentClass != NULL);
	ParentClass->DestroySpecials(addr);
	for (auto tao : SpecialInits)
	{
		tao.first->DestroyValue((BYTE *)addr + tao.second);
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
}

//==========================================================================
//
// PClassActor :: InitializeNativeDefaults
//
//==========================================================================

void PClass::InitializeDefaults()
{
	assert(Defaults == NULL);
	Defaults = (BYTE *)M_Malloc(Size);
	if (ParentClass->Defaults != NULL)
	{
		memcpy(Defaults, ParentClass->Defaults, ParentClass->Size);
		if (Size > ParentClass->Size)
		{
			memset(Defaults + ParentClass->Size, 0, Size - ParentClass->Size);
		}
	}
	else
	{
		memset(Defaults, 0, Size);
	}

	if (bRuntimeClass)
	{
		// Copy parent values from the parent defaults.
		assert(ParentClass != NULL);
		ParentClass->InitializeSpecials(Defaults);

		for (const PField *field : Fields)
		{
			if (!(field->Flags & VARF_Native))
			{
				field->Type->SetDefaultValue(Defaults, unsigned(field->Offset), &SpecialInits);
			}
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
	assert (size >= Size);
	PClass *type;
	bool notnew;
	size_t bucket;

	PClass *existclass = static_cast<PClass *>(TypeTable.FindType(RUNTIME_CLASS(PClass), /*FIXME:Outer*/0, name, &bucket));

	// This is a placeholder so fill it in
	if (existclass != nullptr)
	{
		if (existclass->Size == TentativeClass)
		{
			if (!IsDescendantOf(existclass->ParentClass))
			{
				I_Error("%s must inherit from %s but doesn't.", name.GetChars(), existclass->ParentClass->TypeName.GetChars());
			}

			if (size == TentativeClass)
			{
				// see if we can reuse the existing class. This is only possible if the inheritance is identical. Otherwise it needs to be replaced.
				if (this == existclass->ParentClass)
				{
					existclass->ObjectFlags &= OF_Transient;
					return existclass;
				}
			}
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
		notnew = false;
	}

	// Create a new type object of the same type as us. (We may be a derived class of PClass.)
	type = static_cast<PClass *>(GetClass()->CreateNew());

	Derive(type, name);
	type->Size = size;
	if (size != TentativeClass)
	{
		type->InitializeDefaults();
		type->Virtuals = Virtuals;
		DeriveData(type);
	}
	if (!notnew)
	{
		type->InsertIntoHash();
	}
	else
	{
		TypeTable.ReplaceType(type, existclass, bucket);
		StaticPointerSubstitution(existclass, type, true);	// replace the old one, also in the actor defaults.
		// Delete the old class from the class lists, both the full one and the actor list.
		auto index = PClassActor::AllActorClasses.Find(static_cast<PClassActor*>(existclass));
		if (index < PClassActor::AllActorClasses.Size()) PClassActor::AllActorClasses.Delete(index);
		index = PClass::AllClasses.Find(existclass);
		if (index < PClass::AllClasses.Size()) PClass::AllClasses.Delete(index);
		// Now we can destroy the old class as nothing should reference it anymore
		existclass->Destroy();
	}
	return type;
}

//==========================================================================
//
// PClass :: AddField
//
//==========================================================================

PField *PClass::AddField(FName name, PType *type, DWORD flags)
{
	unsigned oldsize = Size;
	PField *field = Super::AddField(name, type, flags);

	// Only initialize the defaults if they have already been created.
	// For ZScript this is not the case, it will first define all fields before
	// setting up any defaults for any class.
	if (field != nullptr && !(flags & VARF_Native) && Defaults != nullptr)
	{
		Defaults = (BYTE *)M_Realloc(Defaults, Size);
		memset(Defaults + oldsize, 0, Size - oldsize);
	}
	return field;
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
		return NULL;
	}
	size_t bucket;

	PType *found = TypeTable.FindType(RUNTIME_CLASS(PClass),
		/*FIXME:Outer*/0, name, &bucket);

	if (found != NULL)
	{
		return static_cast<PClass *>(found);
	}
	PClass *type = static_cast<PClass *>(GetClass()->CreateNew());
	DPrintf(DMSG_SPAMMY, "Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	Derive(type, name);
	type->Size = TentativeClass;
	TypeTable.AddType(type, RUNTIME_CLASS(PClass), (intptr_t)type->Outer, name, bucket);
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
	if (FlatPointers != NULL)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == NULL)
	{ // No parent (i.e. DObject: FlatPointers is the same as Pointers.
		if (Pointers == NULL)
		{ // No pointers: Make FlatPointers a harmless non-NULL.
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
			size_t *flat = new size_t[numPointers + numSuperPointers + ScriptPointers.Size() + 1];
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
	auto cls = PClass::FindActor(clsname);
	if (!cls) return nullptr;
	auto func = dyn_cast<PFunction>(cls->Symbols.FindSymbol(funcname, true));
	if (!func) return nullptr;
	return func->Variants[0].Implementation;
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
	if (bucketnum != NULL)
	{
		*bucketnum = bucket;
	}
	for (PType *type = TypeHash[bucket]; type != NULL; type = type->HashNext)
	{
		if (type->GetClass()->TypeTableType == metatype && type->IsMatch(parm1, parm2))
		{
			return type;
		}
	}
	return NULL;
}

//==========================================================================
//
// FTypeTable :: ReplaceType
//
// Replaces an existing type in the table with a new version of the same
// type. For use when redefining actors in DECORATE. Does nothing if the
// old version is not in the table.
//
//==========================================================================

void FTypeTable::ReplaceType(PType *newtype, PType *oldtype, size_t bucket)
{
	for (PType **type_p = &TypeHash[bucket]; *type_p != NULL; type_p = &(*type_p)->HashNext)
	{
		PType *type = *type_p;
		if (type == oldtype)
		{
			newtype->HashNext = type->HashNext;
			type->HashNext = NULL;
			*type_p = newtype;
			break;
		}
	}
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
	assert(metatype == type->GetClass()->TypeTableType && "Metatype does not match passed object");
	assert(FindType(metatype, parm1, parm2, &bucketcheck) == NULL && "Type must not be inserted more than once");
	assert(bucketcheck == bucket && "Passed bucket was wrong");
#endif
	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
	GC::WriteBarrier(type);
}

//==========================================================================
//
// FTypeTable :: AddType - Simple Version
//
//==========================================================================

void FTypeTable::AddType(PType *type)
{
	PClass *metatype;
	intptr_t parm1, parm2;
	size_t bucket;

	metatype = type->GetClass()->TypeTableType;
	type->GetTypeIDs(parm1, parm2);
	bucket = Hash(metatype, parm1, parm2) % HASH_SIZE;
	assert(FindType(metatype, parm1, parm2, NULL) == NULL && "Type must not be inserted more than once");

	type->HashNext = TypeHash[bucket];
	TypeHash[bucket] = type;
	GC::WriteBarrier(type);
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
// FTypeTable :: Mark
//
// Mark all types in this table for the garbage collector.
//
//==========================================================================

void FTypeTable::Mark()
{
	for (int i = HASH_SIZE - 1; i >= 0; --i)
	{
		if (TypeHash[i] != NULL)
		{
			GC::Mark(TypeHash[i]);
		}
	}
}

//==========================================================================
//
// FTypeTable :: Clear
//
// Removes everything from the table. We let the garbage collector worry
// about deleting them.
//
//==========================================================================

void FTypeTable::Clear()
{
	memset(TypeHash, 0, sizeof(TypeHash));
}

#include "c_dispatch.h"
CCMD(typetable)
{
	DumpTypeTable();
}

// Symbol tables ------------------------------------------------------------

IMPLEMENT_CLASS(PTypeBase, true, false);
IMPLEMENT_CLASS(PSymbol, true, false);
IMPLEMENT_CLASS(PSymbolConst, false, false);
IMPLEMENT_CLASS(PSymbolConstNumeric, false, false);
IMPLEMENT_CLASS(PSymbolConstString, false, false);
IMPLEMENT_CLASS(PSymbolTreeNode, false, false)
IMPLEMENT_CLASS(PSymbolType, false, true)

IMPLEMENT_POINTERS_START(PSymbolType)
	IMPLEMENT_POINTER(Type)
IMPLEMENT_POINTERS_END

IMPLEMENT_CLASS(PSymbolVMFunction, false, true)

IMPLEMENT_POINTERS_START(PSymbolVMFunction)
	IMPLEMENT_POINTER(Function)
IMPLEMENT_POINTERS_END

//==========================================================================
//
//
//
//==========================================================================

PSymbol::~PSymbol()
{
}

PSymbolTable::PSymbolTable()
: ParentSymbolTable(NULL)
{
}

PSymbolTable::PSymbolTable(PSymbolTable *parent)
: ParentSymbolTable(parent)
{
}

PSymbolTable::~PSymbolTable ()
{
	ReleaseSymbols();
}

size_t PSymbolTable::MarkSymbols()
{
	size_t count = 0;
	MapType::Iterator it(Symbols);
	MapType::Pair *pair;

	while (it.NextPair(pair))
	{
		GC::Mark(pair->Value);
		count++;
	}
	return count * sizeof(*pair);
}

void PSymbolTable::ReleaseSymbols()
{
	// The GC will take care of deleting the symbols. We just need to
	// clear our references to them.
	Symbols.Clear();
}

void PSymbolTable::SetParentTable (PSymbolTable *parent)
{
	ParentSymbolTable = parent;
}

PSymbol *PSymbolTable::FindSymbol (FName symname, bool searchparents) const
{
	PSymbol * const *value = Symbols.CheckKey(symname);
	if (value == NULL && searchparents && ParentSymbolTable != NULL)
	{
		return ParentSymbolTable->FindSymbol(symname, searchparents);
	}
	return value != NULL ? *value : NULL;
}

PSymbol *PSymbolTable::FindSymbolInTable(FName symname, PSymbolTable *&symtable)
{
	PSymbol * const *value = Symbols.CheckKey(symname);
	if (value == NULL)
	{
		if (ParentSymbolTable != NULL)
		{
			return ParentSymbolTable->FindSymbolInTable(symname, symtable);
		}
		symtable = NULL;
		return NULL;
	}
	symtable = this;
	return *value;
}

PSymbol *PSymbolTable::AddSymbol (PSymbol *sym)
{
	// Symbols that already exist are not inserted.
	if (Symbols.CheckKey(sym->SymbolName) != NULL)
	{
		return NULL;
	}
	Symbols.Insert(sym->SymbolName, sym);
	return sym;
}

PSymbol *PSymbolTable::ReplaceSymbol(PSymbol *newsym)
{
	// If a symbol with a matching name exists, take its place and return it.
	PSymbol **symslot = Symbols.CheckKey(newsym->SymbolName);
	if (symslot != NULL)
	{
		PSymbol *oldsym = *symslot;
		*symslot = newsym;
		return oldsym;
	}
	// Else, just insert normally and return NULL since there was no
	// symbol to replace.
	Symbols.Insert(newsym->SymbolName, newsym);
	return NULL;
}
