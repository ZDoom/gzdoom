/*
** dobjtype.cpp
** Implements the type information class
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include "dobject.h"
#include "i_system.h"
#include "actor.h"
#include "templates.h"
#include "autosegs.h"
#include "v_text.h"
#include "a_pickups.h"
#include "d_player.h"

IMPLEMENT_POINTY_CLASS(PClass)
 DECLARE_POINTER(ParentClass)
END_POINTERS

TArray<PClassActor *> PClass::m_RuntimeActors;
TArray<PClass *> PClass::m_Types;
PClass *PClass::TypeHash[PClass::HASH_SIZE];
bool PClass::bShutdown;

// A harmless non-NULL FlatPointer for classes without pointers.
static const size_t TheEnd = ~(size_t)0;

static int STACK_ARGS cregcmp (const void *a, const void *b)
{
	const PClass *class1 = *(const PClass **)a;
	const PClass *class2 = *(const PClass **)b;
	return strcmp(class1->TypeName, class2->TypeName);
}

void PClass::StaticInit ()
{
	atterm (StaticShutdown);

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != NULL)
	{
		((ClassReg *)*probe)->RegisterClass ();
	}

	// Keep actors in consistant order. I did this before, though I'm not
	// sure if this is really necessary to maintain any sort of sync.
	qsort(&m_Types[0], m_Types.Size(), sizeof(m_Types[0]), cregcmp);
	for (unsigned int i = 0; i < m_Types.Size(); ++i)
	{
		m_Types[i]->ClassIndex = i;
	}
}

void PClass::StaticShutdown ()
{
	TArray<size_t *> uniqueFPs(64);
	unsigned int i, j;

	for (i = 0; i < PClass::m_Types.Size(); ++i)
	{
		PClass *type = PClass::m_Types[i];
		PClass::m_Types[i] = NULL;
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
	bShutdown = true;
}

PClass::PClass()
{
	Size = sizeof(DObject);
	ParentClass = NULL;
	Pointers = NULL;
	FlatPointers = NULL;
	HashNext = NULL;
	Defaults = NULL;
	bRuntimeClass = false;
	ClassIndex = ~0;
	ConstructNative = NULL;
}

PClass::~PClass()
{
	Symbols.ReleaseSymbols();
	if (Defaults != NULL)
	{
		M_Free(Defaults);
		Defaults = NULL;
	}
}

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
	};

	// MyClass may have already been created by a previous recursive call.
	// Or this may be a recursive call for a previously created class.
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

	if (this == &PClass::RegistrationInfo)
	{
		cls = new PClass;
	}
	else
	{
		if (metaclasses[MetaClassNum]->MyClass == NULL)
		{ // Make sure the meta class is already registered before registering this one
			metaclasses[MetaClassNum]->RegisterClass();
		}
		cls = static_cast<PClass *>(metaclasses[MetaClassNum]->MyClass->CreateNew());
	}

	MyClass = cls;
	PClass::m_Types.Push(cls);
	cls->TypeName = FName(Name+1);
	cls->Size = SizeOf;
	cls->Pointers = Pointers;
	cls->ConstructNative = ConstructNative;
	cls->InsertIntoHash();
	if (ParentType != NULL)
	{
		cls->ParentClass = ParentType->RegisterClass();
	}
	return cls;
}

void PClass::InsertIntoHash ()
{
	// Add class to hash table. Classes are inserted into each bucket
	// in ascending order by name index.
	unsigned int bucket = TypeName % HASH_SIZE;
	PClass **hashpos = &TypeHash[bucket];
	while (*hashpos != NULL)
	{
		int lexx = int(TypeName) - int((*hashpos)->TypeName);

		if (lexx > 0)
		{ // This type should come later in the chain
			hashpos = &((*hashpos)->HashNext);
		}
		else if (lexx == 0)
		{ // This type has already been inserted
		  // ... but there is no need whatsoever to make it a fatal error!
			Printf (TEXTCOLOR_RED"Tried to register class '%s' more than once.\n", TypeName.GetChars());
			break;
		}
		else
		{ // Type comes right here
			break;
		}
	}
	HashNext = *hashpos;
	*hashpos = this;
}

// Find a type, passed the name as a name
PClass *PClass::FindClass (FName zaname)
{
	if (zaname == NAME_None)
	{
		return NULL;
	}

	PClass *cls = TypeHash[zaname % HASH_SIZE];

	while (cls != 0)
	{
		int lexx = int(zaname) - int(cls->TypeName);
		if (lexx > 0)
		{
			cls = cls->HashNext;
		}
		else if (lexx == 0)
		{
			return cls->Size<0? NULL : cls;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

// Create a new object that this class represents
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
	return (DObject *)mem;
}

// Copies inheritable values into the derived class and other miscellaneous setup.
void PClass::Derive(PClass *newclass)
{
	newclass->ParentClass = this;
	newclass->ConstructNative = ConstructNative;

	// Set up default instance of the new class.
	newclass->Defaults = (BYTE *)M_Malloc(newclass->Size);
	memcpy(newclass->Defaults, Defaults, Size);
	if (newclass->Size > Size)
	{
		memset(newclass->Defaults + Size, 0, newclass->Size - Size);
	}

	newclass->Symbols.SetParentTable(&this->Symbols);
}

// Create a new class based on an existing class
PClass *PClass::CreateDerivedClass (FName name, unsigned int size)
{
	assert (size >= Size);
	PClass *type;
	bool notnew;

	const PClass *existclass = FindClass(name);

	// This is a placeholder so fill it in
	if (existclass != NULL && existclass->Size == (unsigned)-1)
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
		// Create a new type object of the same type as us. (We may be a derived class of PClass.)
		type = static_cast<PClass *>(GetClass()->CreateNew());
		notnew = false;
	}

	type->TypeName = name;
	type->Size = size;
	type->bRuntimeClass = true;
	Derive(type);
	if (!notnew)
	{
		type->ClassIndex = m_Types.Push (type);
		type->InsertIntoHash();
	}

	// If this class is for an actor, push it onto the RuntimeActors stack.
	if (type->IsKindOf(RUNTIME_CLASS(PClassActor)))
	{
		m_RuntimeActors.Push(static_cast<PClassActor *>(type));
	}
	return type;
}

// Add <extension> bytes to the end of this class. Returns the
// previous size of the class.
unsigned int PClass::Extend(unsigned int extension)
{
	assert(this->bRuntimeClass);

	unsigned int oldsize = Size;
	Size += extension;
	Defaults = (BYTE *)M_Realloc(Defaults, Size);
	memset(Defaults + oldsize, 0, extension);
	return oldsize;
}

// Like FindClass but creates a placeholder if no class
// is found. CreateDerivedClass will automatcally fill in
// the placeholder when the actual class is defined.
PClass *PClass::FindClassTentative (FName name)
{
	if (name == NAME_None)
	{
		return NULL;
	}

	PClass *cls = TypeHash[name % HASH_SIZE];

	while (cls != 0)
	{
		int lexx = int(name) - int(cls->TypeName);
		if (lexx > 0)
		{
			cls = cls->HashNext;
		}
		else if (lexx == 0)
		{
			return cls;
		}
		else
		{
			break;
		}
	}
	PClass *type = static_cast<PClass *>(GetClass()->CreateNew());
	DPrintf("Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	type->TypeName = name;
	type->ParentClass = this;
	type->Size = -1;
	type->ClassIndex = m_Types.Push (type);
	type->bRuntimeClass = true;
	type->InsertIntoHash();
	return type;
}

// Create the FlatPointers array, if it doesn't exist already.
// It comprises all the Pointers from superclasses plus this class's own Pointers.
// If this class does not define any new Pointers, then FlatPointers will be set
// to the same array as the super class's.
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

const PClass *PClass::NativeClass() const
{
	const PClass *cls = this;

	while (cls && cls->bRuntimeClass)
		cls = cls->ParentClass;

	return cls;
}

size_t PClass::PropagateMark()
{
	size_t marked;

	// Mark symbols
	marked = Symbols.MarkSymbols();

	return marked + Super::PropagateMark();
}

// Symbol tables ------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(PSymbol);
IMPLEMENT_CLASS(PSymbolConst);
IMPLEMENT_CLASS(PSymbolVariable);
IMPLEMENT_POINTY_CLASS(PSymbolActionFunction)
 DECLARE_POINTER(Function)
END_POINTERS
IMPLEMENT_POINTY_CLASS(PSymbolVMFunction)
 DECLARE_POINTER(Function)
END_POINTERS

PSymbol::~PSymbol()
{
}

PSymbolTable::PSymbolTable()
: ParentSymbolTable(NULL)
{
}

PSymbolTable::~PSymbolTable ()
{
	ReleaseSymbols();
}

size_t PSymbolTable::MarkSymbols()
{
	for (unsigned int i = 0; i < Symbols.Size(); ++i)
	{
		GC::Mark(Symbols[i]);
	}
	return Symbols.Size() * sizeof(Symbols[0]);
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
	int min, max;

	min = 0;
	max = (int)Symbols.Size() - 1;

	while (min <= max)
	{
		unsigned int mid = (min + max) / 2;
		PSymbol *sym = Symbols[mid];

		if (sym->SymbolName == symname)
		{
			return sym;
		}
		else if (sym->SymbolName < symname)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	if (searchparents && ParentSymbolTable != NULL)
	{
		return ParentSymbolTable->FindSymbol (symname, true);
	}
	return NULL;
}

PSymbol *PSymbolTable::AddSymbol (PSymbol *sym)
{
	// Insert it in sorted order.
	int min, max, mid;

	min = 0;
	max = (int)Symbols.Size() - 1;

	while (min <= max)
	{
		mid = (min + max) / 2;
		PSymbol *tsym = Symbols[mid];

		if (tsym->SymbolName == sym->SymbolName)
		{ // A symbol with this name already exists in the table
			return NULL;
		}
		else if (tsym->SymbolName < sym->SymbolName)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}

	// Good. The symbol is not in the table yet.
	Symbols.Insert (MAX(min, max), sym);
	return sym;
}
