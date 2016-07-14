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
#include "farchive.h"
#include "actor.h"
#include "templates.h"
#include "autosegs.h"
#include "v_text.h"
#include "a_pickups.h"
#include "a_weaponpiece.h"
#include "d_player.h"
#include "fragglescript/t_fs.h"

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
PStatePointer *TypeState;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// A harmless non-NULL FlatPointer for classes without pointers.
static const size_t TheEnd = ~(size_t)0;

// CODE --------------------------------------------------------------------

IMPLEMENT_CLASS(PErrorType)
IMPLEMENT_CLASS(PVoidType)

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

IMPLEMENT_CLASS(PClassType)

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
// PClassType :: Derive
//
//==========================================================================

void PClassType::Derive(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassType)));
	Super::Derive(newclass);
	static_cast<PClassType *>(newclass)->TypeTableType = TypeTableType;
}

/* PClassClass ************************************************************/

IMPLEMENT_CLASS(PClassClass)

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

IMPLEMENT_ABSTRACT_POINTY_CLASS(PType)
 DECLARE_POINTER(HashNext)
END_POINTERS

//==========================================================================
//
// PType Default Constructor
//
//==========================================================================

PType::PType()
: Size(0), Align(1), HashNext(NULL)
{
}

//==========================================================================
//
// PType Parameterized Constructor
//
//==========================================================================

PType::PType(unsigned int size, unsigned int align)
: Size(size), Align(align), HashNext(NULL)
{
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

void PType::WriteValue(FArchive &ar, const void *addr) const
{
	assert(0 && "Cannot write value for this type");
}

//==========================================================================
//
// PType :: ReadValue
//
//==========================================================================

bool PType::ReadValue(FArchive &ar, void *addr) const
{
	assert(0 && "Cannot read value for this type");
	SkipValue(ar);
	return false;
}

//==========================================================================
//
// PType :: SkipValue												STATIC
//
//==========================================================================

void PType::SkipValue(FArchive &ar)
{
	BYTE tag;
	ar << tag;
	SkipValue(ar, tag);
}

void PType::SkipValue(FArchive &ar, int tag)
{
	assert(ar.IsLoading() && "SkipValue passed an archive that is writing");
	BYTE buff[8];

	switch (tag)
	{
	case VAL_Zero: case VAL_One:
		break;

	case VAL_Int8: case VAL_UInt8:
		ar.Read(buff, 1);
		break;

	case VAL_Int16: case VAL_UInt16:
		ar.Read(buff, 2);
		break;

	case VAL_Int32: case VAL_UInt32: case VAL_Float32:
		ar.Read(buff, 4);
		break;

	case VAL_Int64: case VAL_UInt64: case VAL_Float64:
		ar.Read(buff, 8);
		break;

	case VAL_Name:
		ar.ReadName();
		break;

	case VAL_Object:
	{
		DObject *skipper;
		ar << skipper;
		break;
	}
	case VAL_State:
	{
		FState *skipper;
		ar << skipper;
		break;
	}
	case VAL_String:
	{
		FString skipper;
		ar << skipper;
		break;
	}
	case VAL_Array:
	{
		DWORD count = ar.ReadCount();
		while (count-- > 0)
		{
			SkipValue(ar);
		}
		break;
	}
	case VAL_Struct:
	{
		const char *label;
		for (label = ar.ReadName(); label != NULL; label = ar.ReadName())
		{
			SkipValue(ar);
		}
		break;
	}
	case VAL_Class:
	{
		PClass *type;
		for (ar.UserReadClass(type); type != NULL; ar.UserReadClass(type))
		{
			SkipValue(ar, VAL_Struct);
		}
		break;
	}
	}
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
// PType :: GetStoreOp
//
//==========================================================================

int PType::GetStoreOp() const
{
	assert(0 && "Cannot store this type");
	return OP_NOP;
}

//==========================================================================
//
// PType :: GetLoadOp
//
//==========================================================================

int PType::GetLoadOp() const
{
	assert(0 && "Cannot load this type");
	return OP_NOP;
}

//==========================================================================
//
// PType :: GetRegType
//
//==========================================================================

int PType::GetRegType() const
{
	assert(0 && "No register for this type");
	return REGT_NIL;
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
	RUNTIME_CLASS(PColor)->TypeTableType = RUNTIME_CLASS(PColor);
	RUNTIME_CLASS(PPointer)->TypeTableType = RUNTIME_CLASS(PPointer);
	RUNTIME_CLASS(PClassPointer)->TypeTableType = RUNTIME_CLASS(PClassPointer);
	RUNTIME_CLASS(PEnum)->TypeTableType = RUNTIME_CLASS(PEnum);
	RUNTIME_CLASS(PArray)->TypeTableType = RUNTIME_CLASS(PArray);
	RUNTIME_CLASS(PDynArray)->TypeTableType = RUNTIME_CLASS(PDynArray);
	RUNTIME_CLASS(PVector)->TypeTableType = RUNTIME_CLASS(PVector);
	RUNTIME_CLASS(PMap)->TypeTableType = RUNTIME_CLASS(PMap);
	RUNTIME_CLASS(PStruct)->TypeTableType = RUNTIME_CLASS(PStruct);
	RUNTIME_CLASS(PPrototype)->TypeTableType = RUNTIME_CLASS(PPrototype);
	RUNTIME_CLASS(PClass)->TypeTableType = RUNTIME_CLASS(PClass);
	RUNTIME_CLASS(PStatePointer)->TypeTableType = RUNTIME_CLASS(PStatePointer);
	
	// Create types and add them type the type table.
	TypeTable.AddType(TypeError = new PErrorType);
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
}


/* PBasicType *************************************************************/

IMPLEMENT_ABSTRACT_CLASS(PBasicType)

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
}

/* PCompoundType **********************************************************/

IMPLEMENT_ABSTRACT_CLASS(PCompoundType)

/* PNamedType *************************************************************/

IMPLEMENT_ABSTRACT_POINTY_CLASS(PNamedType)
 DECLARE_POINTER(Outer)
END_POINTERS

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

IMPLEMENT_CLASS(PInt)

//==========================================================================
//
// PInt Default Constructor
//
//==========================================================================

PInt::PInt()
: PBasicType(4, 4), Unsigned(false)
{
	Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Min, this, -0x7FFFFFFF - 1));
	Symbols.AddSymbol(new PSymbolConstNumeric(NAME_Max, this,  0x7FFFFFFF));
}

//==========================================================================
//
// PInt Parameterized Constructor
//
//==========================================================================

PInt::PInt(unsigned int size, bool unsign)
: PBasicType(size, size), Unsigned(unsign)
{
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
}

//==========================================================================
//
// PInt :: WriteValue
//
// Write the value using the minimum byte size needed to represent it. This
// means that the value as written is not necessarily of the same type as
// stored, but the signedness information is preserved.
//
//==========================================================================

void PInt::WriteValue(FArchive &ar, const void *addr) const
{
	BYTE bval;

	// The process for bytes is the same whether signed or unsigned, since
	// they can't be compacted into a representation with fewer bytes.
	if (Size == 1)
	{
		bval = *(BYTE *)addr;
	}
	else if (Unsigned)
	{
		unsigned val;
		if (Size == 8)
		{
			QWORD qval = *(QWORD *)addr;
			if (qval & 0xFFFFFFFF00000000llu)
			{ // Value needs 64 bits
				ar.WriteByte(VAL_UInt64);
				ar.WriteInt64(qval);
				return;
			}
			// Value can fit in 32 bits or less
			val = (unsigned)qval;
			goto check_u32;
		}
		else if (Size == 4)
		{
			val = *(DWORD *)addr;
check_u32:	if (val & 0xFFFF0000u)
			{ // Value needs 32 bits
				ar.WriteByte(VAL_UInt32);
				ar.WriteInt32(val);
				return;
			}
			// Value can fit in 16 bits or less
			goto check_u16;
		}
		else// if (Size == 2)
		{
			val = *(WORD *)addr;
check_u16:	if (val & 0xFFFFFF00u)
			{ // Value needs 16 bits
				ar.WriteByte(VAL_UInt16);
				ar.WriteInt16(val);
				return;
			}
			// Value can fit in 8 bits
			bval = (BYTE)val;
		}
	}
	else // Signed
	{
		int val;
		if (Size == 8)
		{
			SQWORD qval = *(SQWORD *)addr;
			INT_MIN;
			if (qval < (-0x7FFFFFFF - 1) || qval > 0x7FFFFFFF)
			{ // Value needs 64 bits
				ar.WriteByte(VAL_Int64);
				ar.WriteInt64(qval);
				return;
			}
			// Value can fit in 32 bits or less
			val = (int)qval;
			goto check_s32;
		}
		else if (Size == 4)
		{
			val = *(SDWORD *)addr;
check_s32:	if (val < -0x8000 || val > 0x7FFF)
			{ // Value needs 32 bits
				ar.WriteByte(VAL_Int32);
				ar.WriteInt32(val);
				return;
			}
			// Value can fit in 16 bits or less
			goto check_s16;
		}
		else// if (Size == 2)
		{
			val = *(SWORD *)addr;
check_s16:	if (val < -0x80 || val > 0x7F)
			{ // Value needs 16 bits
				ar.WriteByte(VAL_Int16);
				ar.WriteInt16(val);
				return;
			}
			// Value can fit in 8 bits
			bval = (BYTE)val;
		}
	}
	// If we get here, the value fits in a byte. Values of 0 and 1 are
	// optimized away into the tag so they don't require any extra space
	// to store.
	if (bval & 0xFE)
	{
		BYTE out[2] = { Unsigned ? VAL_UInt8 : VAL_Int8, bval };
		ar.Write(out, 2);
	}
	else
	{
		ar.WriteByte(VAL_Zero + bval);
	}
}

//==========================================================================
//
// PInt :: ReadValue
//
//==========================================================================

bool PInt::ReadValue(FArchive &ar, void *addr) const
{
	union
	{
		QWORD uval;
		SQWORD sval;
	};
	BYTE tag;
	union
	{
		BYTE val8;
		WORD val16;
		DWORD val32;
		float single;
		double dbl;
	};

	ar << tag;
	switch (tag)
	{
	case VAL_Zero:		uval = 0; break;
	case VAL_One:		uval = 1; break;
	case VAL_Int8:		ar << val8;	sval = (SBYTE)val8;	break;
	case VAL_UInt8:		ar << val8; uval = val8; break;
	case VAL_Int16:		ar << val16; sval = (SWORD)val16; break;
	case VAL_UInt16:	ar << val16; uval = val16; break;
	case VAL_Int32:		ar << val32; sval = (SDWORD)val32; break;
	case VAL_UInt32:	ar << val32; uval = val32; break;
	case VAL_Int64:		ar << sval; break;
	case VAL_UInt64:	ar << uval; break;
	case VAL_Float32:	ar << single; sval = (SQWORD)single; break;
	case VAL_Float64:	ar << dbl; sval = (SQWORD)dbl; break;
	default:			SkipValue(ar, tag); return false;		// Incompatible type
	}
	switch (Size)
	{
	case 1:	*(BYTE *)addr = (BYTE)uval; break;
	case 2: *(WORD *)addr = (WORD)uval; break;
	case 4: *(DWORD *)addr = (DWORD)uval; break;
	case 8: *(QWORD *)addr = uval; break;
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

int PInt::GetStoreOp() const
{
	if (Size == 4)
	{
		return OP_SW;
	}
	else if (Size == 1)
	{
		return OP_SB;
	}
	else if (Size == 2)
	{
		return OP_SH;
	}
	else
	{
		assert(0 && "Unhandled integer size");
		return OP_NOP;
	}
}

//==========================================================================
//
// PInt :: GetLoadOp
//
//==========================================================================

int PInt::GetLoadOp() const
{
	if (Size == 4)
	{
		return OP_LW;
	}
	else if (Size == 1)
	{
		return Unsigned ? OP_LBU : OP_LB;
	}
	else if (Size == 2)
	{
		return Unsigned ? OP_LHU : OP_LH;
	}
	else
	{
		assert(0 && "Unhandled integer size");
		return OP_NOP;
	}
}

//==========================================================================
//
// PInt :: GetRegType
//
//==========================================================================

int PInt::GetRegType() const
{
	return REGT_INT;
}

/* PBool ******************************************************************/

IMPLEMENT_CLASS(PBool)

//==========================================================================
//
// PBool Default Constructor
//
//==========================================================================

PBool::PBool()
: PInt(sizeof(bool), true)
{
	// Override the default max set by PInt's constructor
	PSymbolConstNumeric *maxsym = static_cast<PSymbolConstNumeric *>(Symbols.FindSymbol(NAME_Max, false));
	assert(maxsym != NULL && maxsym->IsKindOf(RUNTIME_CLASS(PSymbolConstNumeric)));
	maxsym->Value = 1;
}

/* PFloat *****************************************************************/

IMPLEMENT_CLASS(PFloat)

//==========================================================================
//
// PFloat Default Constructor
//
//==========================================================================

PFloat::PFloat()
: PBasicType(8, 8)
{
	SetDoubleSymbols();
}

//==========================================================================
//
// PFloat Parameterized Constructor
//
//==========================================================================

PFloat::PFloat(unsigned int size)
: PBasicType(size, size)
{
	if (size == 8)
	{
		SetDoubleSymbols();
	}
	else
	{
		assert(size == 4);
		SetSingleSymbols();
	}
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

void PFloat::WriteValue(FArchive &ar, const void *addr) const
{
	float singleprecision;
	if (Size == 8)
	{
		// If it can be written as single precision without information
		// loss, then prefer that over writing a full-sized double.
		double doubleprecision = *(double *)addr;
		singleprecision = (float)doubleprecision;
		if (singleprecision != doubleprecision)
		{
			ar.WriteByte(VAL_Float64);
			ar << doubleprecision;
			return;
		}
	}
	else
	{
		singleprecision = *(float *)addr;
	}
	ar.WriteByte(VAL_Float32);
	ar << singleprecision;
}

//==========================================================================
//
// PFloat :: ReadValue
//
//==========================================================================

static bool ReadValueDbl(FArchive &ar, double *addr, unsigned tag)
{
	double val;
	union
	{
		BYTE val8;
		WORD val16;
		DWORD val32;
		QWORD val64;
		fixed_t fix;
		float single;
		angle_t ang;
	};

	switch (tag)
	{
	case VAL_Zero:		val = 0; break;
	case VAL_One:		val = 1; break;
	case VAL_Int8:		ar << val8;	val = (SBYTE)val8;	break;
	case VAL_UInt8:		ar << val8; val = val8; break;
	case VAL_Int16:		ar << val16; val = (SWORD)val16; break;
	case VAL_UInt16:	ar << val16; val = val16; break;
	case VAL_Int32:		ar << val32; val = (SDWORD)val32; break;
	case VAL_UInt32:	ar << val32; val = val32; break;
	case VAL_Int64:		ar << val64; val = (double)(SQWORD)val64; break;
	case VAL_UInt64:	ar << val64; val = (double)val64; break;
	case VAL_Float32:	ar << single; val = single; break;
	case VAL_Float64:	ar << val; break;
	default:			PType::SkipValue(ar, tag); return false;	// Incompatible type
	}
	*(double *)addr = val;
	return true;
}

bool PFloat::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	double val;
	if (ReadValueDbl(ar, &val, tag))
	{
		if (Size == 4)
		{
			*(float *)addr = (float)val;
		}
		else
		{
			*(double *)addr = val;
		}
		return true;
	}
	return false;
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

int PFloat::GetStoreOp() const
{
	if (Size == 4)
	{
		return OP_SSP;
	}
	else
	{
		assert(Size == 8);
		return OP_SDP;
	}
}

//==========================================================================
//
// PFloat :: GetLoadOp
//
//==========================================================================

int PFloat::GetLoadOp() const
{
	if (Size == 4)
	{
		return OP_LSP;
	}
	else
	{
		assert(Size == 8);
		return OP_LDP;
	}
	assert(0 && "Cannot load this type");
	return OP_NOP;
}

//==========================================================================
//
// PFloat :: GetRegType
//
//==========================================================================

int PFloat::GetRegType() const
{
	return REGT_FLOAT;
}

/* PString ****************************************************************/

IMPLEMENT_CLASS(PString)

//==========================================================================
//
// PString Default Constructor
//
//==========================================================================

PString::PString()
: PBasicType(sizeof(FString), __alignof(FString))
{
}

//==========================================================================
//
// PString :: GetRegType
//
//==========================================================================

int PString::GetRegType() const
{
	return REGT_STRING;
}

//==========================================================================
//
// PString :: WriteValue
//
//==========================================================================

void PString::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_String);
	ar.WriteString(*(const FString *)addr);
}

//==========================================================================
//
// PString :: ReadValue
//
//==========================================================================

bool PString::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag == VAL_String)
	{
		ar << *(FString *)addr;
	}
	else if (tag == VAL_Name)
	{
		const char *str = ar.ReadName();
		*(FString *)addr = str;
	}
	else
	{
		SkipValue(ar, tag);
		return false;
	}
	return true;
}

//==========================================================================
//
// PString :: SetDefaultValue
//
//==========================================================================

void PString::SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const
{
	new((BYTE *)base + offset) FString;
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
	new(addr) FString(*(FString *)def);
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

IMPLEMENT_CLASS(PName)

//==========================================================================
//
// PName Default Constructor
//
//==========================================================================

PName::PName()
: PInt(sizeof(FName), true)
{
	assert(sizeof(FName) == __alignof(FName));
}

//==========================================================================
//
// PName :: WriteValue
//
//==========================================================================

void PName::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_Name);
	ar.WriteName(((const FName *)addr)->GetChars());
}

//==========================================================================
//
// PName :: ReadValue
//
//==========================================================================

bool PName::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag == VAL_Name)
	{
		*(FName *)addr = FName(ar.ReadName());
	}
	else if (tag == VAL_String)
	{
		FString str;
		ar << str;
		*(FName *)addr = FName(str);
	}
	else
	{
		SkipValue(ar, tag);
		return false;
	}
	return true;
}

/* PSound *****************************************************************/

IMPLEMENT_CLASS(PSound)

//==========================================================================
//
// PSound Default Constructor
//
//==========================================================================

PSound::PSound()
: PInt(sizeof(FSoundID), true)
{
	assert(sizeof(FSoundID) == __alignof(FSoundID));
}

//==========================================================================
//
// PSound :: WriteValue
//
//==========================================================================

void PSound::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_Name);
	ar.WriteName(*(const FSoundID *)addr);
}

//==========================================================================
//
// PSound :: ReadValue
//
//==========================================================================

bool PSound::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;

	ar << tag;
	if (tag == VAL_Name)
	{
		const char *str = ar.ReadName();
		*(FSoundID *)addr = FSoundID(str);
	}
	else if (tag == VAL_String)
	{
		FString str;
		ar << str;
		*(FSoundID *)addr = FSoundID(str);
	}
	else
	{
		SkipValue(ar, tag);
		return false;
	}
	return true;
}

/* PColor *****************************************************************/

IMPLEMENT_CLASS(PColor)

//==========================================================================
//
// PColor Default Constructor
//
//==========================================================================

PColor::PColor()
: PInt(sizeof(PalEntry), true)
{
	assert(sizeof(PalEntry) == __alignof(PalEntry));
}

/* PStatePointer **********************************************************/

IMPLEMENT_CLASS(PStatePointer)

//==========================================================================
//
// PStatePointer Default Constructor
//
//==========================================================================

PStatePointer::PStatePointer()
: PBasicType(sizeof(FState *), __alignof(FState *))
{
}

//==========================================================================
//
// PStatePointer :: GetStoreOp
//
//==========================================================================

int PStatePointer::GetStoreOp() const
{
	return OP_SP;
}

//==========================================================================
//
// PStatePointer :: GetLoadOp
//
//==========================================================================

int PStatePointer::GetLoadOp() const
{
	return OP_LP;
}

//==========================================================================
//
// PStatePointer :: GetRegType
//
//==========================================================================

int PStatePointer::GetRegType() const
{
	return REGT_POINTER;
}

//==========================================================================
//
// PStatePointer :: WriteValue
//
//==========================================================================

void PStatePointer::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_State);
	ar << *(FState **)addr;
}

//==========================================================================
//
// PStatePointer :: ReadValue
//
//==========================================================================

bool PStatePointer::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag == VAL_State)
	{
		ar << *(FState **)addr;
		return true;
	}
	SkipValue(ar, tag);
	return false;
}

/* PPointer ***************************************************************/

IMPLEMENT_POINTY_CLASS(PPointer)
 DECLARE_POINTER(PointedType)
END_POINTERS

//==========================================================================
//
// PPointer - Default Constructor
//
//==========================================================================

PPointer::PPointer()
: PBasicType(sizeof(void *), __alignof(void *)), PointedType(NULL)
{
}

//==========================================================================
//
// PPointer - Parameterized Constructor
//
//==========================================================================

PPointer::PPointer(PType *pointsat)
: PBasicType(sizeof(void *), __alignof(void *)), PointedType(pointsat)
{
}

//==========================================================================
//
// PPointer :: GetStoreOp
//
//==========================================================================

int PPointer::GetStoreOp() const
{
	return OP_SP;
}

//==========================================================================
//
// PPointer :: GetLoadOp
//
//==========================================================================

int PPointer::GetLoadOp() const
{
	return PointedType->IsKindOf(RUNTIME_CLASS(PClass)) ? OP_LO : OP_LP;
}

//==========================================================================
//
// PPointer :: GetRegType
//
//==========================================================================

int PPointer::GetRegType() const
{
	return REGT_POINTER;
}

//==========================================================================
//
// PPointer :: IsMatch
//
//==========================================================================

bool PPointer::IsMatch(intptr_t id1, intptr_t id2) const
{
	assert(id2 == 0);
	PType *pointat = (PType *)id1;

	return pointat == PointedType;
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

void PPointer::WriteValue(FArchive &ar, const void *addr) const
{
	if (PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
	{
		ar.WriteByte(VAL_Object);
		ar << *(DObject **)addr;
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

bool PPointer::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag == VAL_Object && PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
	{
		ar << *(DObject **)addr;
		return true;
	}
	SkipValue(ar, tag);
	return false;
}

//==========================================================================
//
// NewPointer
//
// Returns a PPointer to an object of the specified type
//
//==========================================================================

PPointer *NewPointer(PType *type)
{
	size_t bucket;
	PType *ptype = TypeTable.FindType(RUNTIME_CLASS(PPointer), (intptr_t)type, 0, &bucket);
	if (ptype == NULL)
	{
		ptype = new PPointer(type);
		TypeTable.AddType(ptype, RUNTIME_CLASS(PPointer), (intptr_t)type, 0, bucket);
	}
	return static_cast<PPointer *>(ptype);
}


/* PClassPointer **********************************************************/

IMPLEMENT_POINTY_CLASS(PClassPointer)
 DECLARE_POINTER(ClassRestriction)
END_POINTERS

//==========================================================================
//
// PClassPointer - Default Constructor
//
//==========================================================================

PClassPointer::PClassPointer()
: PPointer(RUNTIME_CLASS(PClass)), ClassRestriction(NULL)
{
}

//==========================================================================
//
// PClassPointer - Parameterized Constructor
//
//==========================================================================

PClassPointer::PClassPointer(PClass *restrict)
: PPointer(RUNTIME_CLASS(PClass)), ClassRestriction(restrict)
{
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

IMPLEMENT_POINTY_CLASS(PEnum)
 DECLARE_POINTER(ValueType)
END_POINTERS

//==========================================================================
//
// PEnum - Default Constructor
//
//==========================================================================

PEnum::PEnum()
: ValueType(NULL)
{
}

//==========================================================================
//
// PEnum - Parameterized Constructor
//
//==========================================================================

PEnum::PEnum(FName name, DObject *outer)
: PNamedType(name, outer), ValueType(NULL)
{
}

//==========================================================================
//
// NewEnum
//
// Returns a PEnum for the given name and container, making sure not to
// create duplicates.
//
//==========================================================================

PEnum *NewEnum(FName name, DObject *outer)
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

IMPLEMENT_POINTY_CLASS(PArray)
 DECLARE_POINTER(ElementType)
END_POINTERS

//==========================================================================
//
// PArray - Default Constructor
//
//==========================================================================

PArray::PArray()
: ElementType(NULL), ElementCount(0)
{
}

//==========================================================================
//
// PArray - Parameterized Constructor
//
//==========================================================================

PArray::PArray(PType *etype, unsigned int ecount)
: ElementType(etype), ElementCount(ecount)
{
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

void PArray::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_Array);
	ar.WriteCount(ElementCount);
	const BYTE *addrb = (const BYTE *)addr;
	for (unsigned i = 0; i < ElementCount; ++i)
	{
		ElementType->WriteValue(ar, addrb);
		addrb += ElementSize;
	}
}

//==========================================================================
//
// PArray :: ReadValue
//
//==========================================================================

bool PArray::ReadValue(FArchive &ar, void *addr) const
{
	bool readsomething = false;
	BYTE tag;

	ar << tag;
	if (tag == VAL_Array)
	{
		unsigned count = ar.ReadCount();
		unsigned i;
		BYTE *addrb = (BYTE *)addr;
		for (i = 0; i < MIN(count, ElementCount); ++i)
		{
			readsomething |= ElementType->ReadValue(ar, addrb);
			addrb += ElementSize;
		}
		if (i < ElementCount)
		{
			DPrintf("Array on disk (%u) is bigger than in memory (%u)\n",
				count, ElementCount);
			for (; i < ElementCount; ++i)
			{
				SkipValue(ar);
			}
		}
		return readsomething;
	}
	SkipValue(ar, tag);
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

/* PVector ****************************************************************/

IMPLEMENT_CLASS(PVector)

//==========================================================================
//
// PVector - Default Constructor
//
//==========================================================================

PVector::PVector()
: PArray(TypeFloat32, 3)
{
}

//==========================================================================
//
// PVector - Parameterized Constructor
//
//==========================================================================

PVector::PVector(unsigned int size)
: PArray(TypeFloat32, size)
{
	assert(size >= 2 && size <= 4);
}

//==========================================================================
//
// NewVector
//
// Returns a PVector with the given dimension, making sure not to create
// duplicates.
//
//==========================================================================

PVector *NewVector(unsigned int size)
{
	size_t bucket;
	PType *type = TypeTable.FindType(RUNTIME_CLASS(PVector), (intptr_t)TypeFloat32, size, &bucket);
	if (type == NULL)
	{
		type = new PVector(size);
		TypeTable.AddType(type, RUNTIME_CLASS(PVector), (intptr_t)TypeFloat32, size, bucket);
	}
	return (PVector *)type;
}

/* PDynArray **************************************************************/

IMPLEMENT_POINTY_CLASS(PDynArray)
 DECLARE_POINTER(ElementType)
END_POINTERS

//==========================================================================
//
// PDynArray - Default Constructor
//
//==========================================================================

PDynArray::PDynArray()
: ElementType(NULL)
{
	Size = sizeof(FArray);
	Align = __alignof(FArray);
}

//==========================================================================
//
// PDynArray - Parameterized Constructor
//
//==========================================================================

PDynArray::PDynArray(PType *etype)
: ElementType(etype)
{
	Size = sizeof(FArray);
	Align = __alignof(FArray);
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

IMPLEMENT_POINTY_CLASS(PMap)
 DECLARE_POINTER(KeyType)
 DECLARE_POINTER(ValueType)
END_POINTERS

//==========================================================================
//
// PMap - Default Constructor
//
//==========================================================================

PMap::PMap()
: KeyType(NULL), ValueType(NULL)
{
	Size = sizeof(FMap);
	Align = __alignof(FMap);
}

//==========================================================================
//
// PMap - Parameterized Constructor
//
//==========================================================================

PMap::PMap(PType *keytype, PType *valtype)
: KeyType(keytype), ValueType(valtype)
{
	Size = sizeof(FMap);
	Align = __alignof(FMap);
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

IMPLEMENT_CLASS(PStruct)

//==========================================================================
//
// PStruct - Default Constructor
//
//==========================================================================

PStruct::PStruct()
{
}

//==========================================================================
//
// PStruct - Parameterized Constructor
//
//==========================================================================

PStruct::PStruct(FName name, DObject *outer)
: PNamedType(name, outer)
{
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
		if (!(field->Flags & VARF_Native))
		{
			field->Type->SetDefaultValue(base, offset + field->Offset, special);
		}
	}
}

//==========================================================================
//
// PStruct :: WriteValue
//
//==========================================================================

void PStruct::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_Struct);
	WriteFields(ar, addr, Fields);
}

//==========================================================================
//
// PStruct :: ReadValue
//
//==========================================================================

bool PStruct::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag == VAL_Struct)
	{
		return ReadFields(ar, addr);
	}
	SkipValue(ar, tag);
	return true;
}

//==========================================================================
//
// PStruct :: WriteFields											STATIC
//
//==========================================================================

void PStruct::WriteFields(FArchive &ar, const void *addr, const TArray<PField *> &fields)
{
	for (unsigned i = 0; i < fields.Size(); ++i)
	{
		const PField *field = fields[i];
		// Skip fields with native serialization
		if (!(field->Flags & VARF_Native))
		{
			ar.WriteName(field->SymbolName);
			field->Type->WriteValue(ar, (const BYTE *)addr + field->Offset);
		}
	}
	ar.WriteName(NULL);
}

//==========================================================================
//
// PStruct :: ReadFields
//
//==========================================================================

bool PStruct::ReadFields(FArchive &ar, void *addr) const
{
	bool readsomething = false;
	const char *label = ar.ReadName();
	if (label == NULL)
	{ // If there is nothing to restore, we count it as success.
		return true;
	}
	for (; label != NULL; label = ar.ReadName())
	{
		const PSymbol *sym = Symbols.FindSymbol(FName(label, true), true);
		if (sym == NULL)
		{
			DPrintf("Cannot find field %s in %s\n",
				label, TypeName.GetChars());
			SkipValue(ar);
		}
		else if (!sym->IsKindOf(RUNTIME_CLASS(PField)))
		{
			DPrintf("Symbol %s in %s is not a field\n",
				label, TypeName.GetChars());
			SkipValue(ar);
		}
		else
		{
			readsomething |= static_cast<const PField *>(sym)->Type->ReadValue(ar,
				(BYTE *)addr + static_cast<const PField *>(sym)->Offset);
		}
	}
	return readsomething;
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
	PField *field = new PField(name, type);

	// The new field is added to the end of this struct, alignment permitting.
	field->Offset = (Size + (type->Align - 1)) & ~(type->Align - 1);

	// Enlarge this struct to enclose the new field.
	Size = field->Offset + type->Size;

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

PStruct *NewStruct(FName name, DObject *outer)
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

/* PField *****************************************************************/

IMPLEMENT_CLASS(PField)

//==========================================================================
//
// PField - Default Constructor
//
//==========================================================================

PField::PField()
: PSymbol(NAME_None), Offset(0), Type(NULL), Flags(0)
{
}

/* PPrototype *************************************************************/

IMPLEMENT_CLASS(PPrototype)

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

IMPLEMENT_CLASS(PFunction)

//==========================================================================
//
// PFunction :: PropagataMark
//
//==========================================================================

size_t PFunction::PropagateMark()
{
	for (unsigned i = 0; i < Variants.Size(); ++i)
	{
		//GC::Mark(Variants[i].Proto);
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

unsigned PFunction::AddVariant(PPrototype *proto, TArray<DWORD> &argflags, VMFunction *impl)
{
	Variant variant;

	//variant.Proto = proto;
	variant.ArgFlags = argflags;
	variant.Implementation = impl;
	impl->Proto = proto;
	return Variants.Push(variant);
}

/* PClass *****************************************************************/

IMPLEMENT_POINTY_CLASS(PClass)
 DECLARE_POINTER(ParentClass)
END_POINTERS

//==========================================================================
//
// PClass :: WriteValue
//
// Similar to PStruct's version, except it also needs to traverse parent
// classes.
//
//==========================================================================

static void RecurseWriteFields(const PClass *type, FArchive &ar, const void *addr)
{
	if (type != NULL)
	{
		RecurseWriteFields(type->ParentClass, ar, addr);
		// Don't write this part if it has no non-native variables
		for (unsigned i = 0; i < type->Fields.Size(); ++i)
		{
			if (!(type->Fields[i]->Flags & VARF_Native))
			{
				// Tag this section with the class it came from in case
				// a more-derived class has variables that shadow a less-
				// derived class. Whether or not that is a language feature
				// that will actually be allowed remains to be seen.
				ar.UserWriteClass(const_cast<PClass *>(type));
				PStruct::WriteFields(ar, addr, type->Fields);
				break;
			}
		}
	}
}

void PClass::WriteValue(FArchive &ar, const void *addr) const
{
	ar.WriteByte(VAL_Class);
	RecurseWriteFields(this, ar, addr);
	ar.UserWriteClass(NULL);
}

//==========================================================================
//
// PClass :: ReadValue
//
//==========================================================================

bool PClass::ReadValue(FArchive &ar, void *addr) const
{
	BYTE tag;
	ar << tag;
	if (tag != VAL_Class)
	{
		SkipValue(ar, tag);
		return false;
	}
	else
	{
		bool readsomething = false;
		PClass *type;
		for (ar.UserReadClass(type); type != NULL; ar.UserReadClass(type))
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
			if (parent != NULL)
			{
				readsomething |= type->ReadFields(ar, addr);
			}
			else
			{
				DPrintf("Unknown superclass %s of class %s\n",
					type->TypeName.GetChars(), TypeName.GetChars());
				SkipValue(ar, VAL_Struct);
			}
		}
		return readsomething;
	}
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

	while (*++probe != NULL)
	{
		((ClassReg *)*probe)->MyClass = NULL;
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
	ParentClass = NULL;
	Pointers = NULL;
	FlatPointers = NULL;
	HashNext = NULL;
	Defaults = NULL;
	bRuntimeClass = false;
	ConstructNative = NULL;

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
		&PClassWeaponPiece::RegistrationInfo,
		&PClassPowerupGiver::RegistrationInfo,
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

	if (metaclasses[MetaClassNum]->MyClass == NULL)
	{ // Make sure the meta class is already registered before registering this one
		metaclasses[MetaClassNum]->RegisterClass();
	}
	cls = static_cast<PClass *>(metaclasses[MetaClassNum]->MyClass->CreateNew());

	SetupClass(cls);
	cls->InsertIntoHash();
	if (ParentType != NULL)
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
	  // ... but there is no need whatsoever to make it a fatal error!
		if (!strictdecorate) Printf (TEXTCOLOR_RED"Tried to register class '%s' more than once.\n", TypeName.GetChars());
		else I_Error("Tried to register class '%s' more than once.\n", TypeName.GetChars());
		TypeTable.ReplaceType(this, found, bucket);
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
	if (Defaults != NULL)
	{
		InitializeSpecials(mem);
	}
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
		tao.first->InitializeValue((BYTE*)addr + tao.second, Defaults + tao.second);
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

void PClass::Derive(PClass *newclass)
{
	newclass->ParentClass = this;
	newclass->ConstructNative = ConstructNative;

	// Set up default instance of the new class.
	newclass->Defaults = (BYTE *)M_Malloc(newclass->Size);
	if (Defaults) memcpy(newclass->Defaults, Defaults, Size);
	if (newclass->Size > Size)
	{
		memset(newclass->Defaults + Size, 0, newclass->Size - Size);
	}

	newclass->Symbols.SetParentTable(&this->Symbols);
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
	if (existclass != NULL && (existclass->Size == TentativeClass))
	{
		type = const_cast<PClass*>(existclass);
		if (!IsDescendantOf(type->ParentClass))
		{
			I_Error("%s must inherit from %s but doesn't.", name.GetChars(), type->ParentClass->TypeName.GetChars());
		}
		DPrintf("Defining placeholder class %s\n", name.GetChars());
		notnew = true;
	}
	else
	{
		notnew = false;
	}

	// Create a new type object of the same type as us. (We may be a derived class of PClass.)
	type = static_cast<PClass *>(GetClass()->CreateNew());

	type->TypeName = name;
	type->Size = size;
	type->bRuntimeClass = true;
	Derive(type);
	DeriveData(type);
	if (!notnew)
	{
		type->InsertIntoHash();
	}
	else
	{
		PClassActor::AllActorClasses.Pop();	// remove the newly added class from the list
		// todo: replace all affected fields
		for (unsigned i = 0; i < PClassActor::AllActorClasses.Size(); i++)
		{
			PClassActor::AllActorClasses[i]->ReplaceClassRef(existclass, type);
			if (PClassActor::AllActorClasses[i] == existclass)
				PClassActor::AllActorClasses[i] = static_cast<PClassActor*>(type);
		}
		TypeTable.ReplaceType(type, existclass, bucket);
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
	if (field != NULL)
	{
	Defaults = (BYTE *)M_Realloc(Defaults, Size);
	memset(Defaults + oldsize, 0, Size - oldsize);
		// If this is a native class, then we must not initialize and
		// destroy any of its members. We do, however, initialize the
		// default instance since it's not a normal instance of the class.
		type->SetDefaultValue(Defaults, field->Offset,
			bRuntimeClass ? &SpecialInits : NULL);
	}
	return field;
}

//==========================================================================
//
// PClass :: FindClassTentative
//
// Like FindClass but creates a placeholder if no class is found.
// CreateDerivedClass will automatically fill in the placeholder when the
// actual class is defined.
//
//==========================================================================

PClass *PClass::FindClassTentative(FName name, bool fatal)
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
	DPrintf("Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	type->TypeName = name;
	type->ParentClass = this;
	type->Size = TentativeClass;
	type->bRuntimeClass = true;
	TypeTable.AddType(type, RUNTIME_CLASS(PClass), (intptr_t)type->Outer, name, bucket);
	return type;
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
	{ // No parent: FlatPointers is the same as Pointers.
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
		if (Pointers == NULL)
		{ // No new pointers: Just use the same FlatPointers as the parent.
			FlatPointers = ParentClass->FlatPointers;
		}
		else
		{ // New pointers: Create a new FlatPointers array and add them.
			int numPointers, numSuperPointers;

			// Count pointers defined by this class.
			for (numPointers = 0; Pointers[numPointers] != ~(size_t)0; numPointers++)
			{ }
			// Count pointers defined by superclasses.
			for (numSuperPointers = 0; ParentClass->FlatPointers[numSuperPointers] != ~(size_t)0; numSuperPointers++)
			{ }

			// Concatenate them into a new array
			size_t *flat = new size_t[numPointers + numSuperPointers + 1];
			if (numSuperPointers > 0)
			{
				memcpy (flat, ParentClass->FlatPointers, sizeof(size_t)*numSuperPointers);
			}
			memcpy (flat + numSuperPointers, Pointers, sizeof(size_t)*(numPointers+1));
			FlatPointers = flat;
		}
	}
}

//==========================================================================
//
// PClass :: NativeClass
//
// Finds the underlying native type underlying this class.
//
//==========================================================================

const PClass *PClass::NativeClass() const
{
	const PClass *cls = this;

	while (cls && cls->bRuntimeClass)
		cls = cls->ParentClass;

	return cls;
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

IMPLEMENT_ABSTRACT_CLASS(PSymbol);
IMPLEMENT_CLASS(PSymbolConst);
IMPLEMENT_CLASS(PSymbolConstNumeric);
IMPLEMENT_CLASS(PSymbolConstString);
IMPLEMENT_POINTY_CLASS(PSymbolType)
 DECLARE_POINTER(Type)
END_POINTERS
IMPLEMENT_POINTY_CLASS(PSymbolVMFunction)
 DECLARE_POINTER(Function)
END_POINTERS
IMPLEMENT_CLASS(PSymbolTreeNode)

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
