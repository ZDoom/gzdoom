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

TArray<PClass *> PClass::m_RuntimeActors;
TArray<PClass *> PClass::m_Types;
PClass *PClass::TypeHash[PClass::HASH_SIZE];
bool PClass::bShutdown;

// A harmless non-NULL FlatPointer for classes without pointers.
static const size_t TheEnd = ~(size_t)0;

static int STACK_ARGS cregcmp (const void *a, const void *b) NO_SANITIZE
{
	// VC++ introduces NULLs in the sequence. GCC seems to work as expected and not do it.
	const ClassReg *class1 = *(const ClassReg **)a;
	const ClassReg *class2 = *(const ClassReg **)b;
	if (class1 == NULL) return 1;
	if (class2 == NULL) return -1;
	return strcmp (class1->Name, class2->Name);
}

void PClass::StaticInit ()
{
	atterm (StaticShutdown);

	// Sort classes by name to remove dependance on how the compiler ordered them.
	REGINFO *head = &CRegHead;
	REGINFO *tail = &CRegTail;

	// MinGW's linker is linking the object files backwards for me now...
	if (head > tail)
	{
		swapvalues (head, tail);
	}
	qsort (head + 1, tail - head - 1, sizeof(REGINFO), cregcmp);

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != NULL)
	{
		((ClassReg *)*probe)->RegisterClass ();
	}
}

void PClass::ClearRuntimeData ()
{
	StaticShutdown();

	m_RuntimeActors.Clear();
	m_Types.Clear();
	memset(TypeHash, 0, sizeof(TypeHash));
	bShutdown = false;

	// Immediately reinitialize the internal classes
	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != NULL)
	{
		((ClassReg *)*probe)->RegisterClass ();
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
		type->FlatPointers = NULL;

		// For runtime classes, this call will also delete the PClass.
		PClass::StaticFreeData (type);
	}
	for (i = 0; i < uniqueFPs.Size(); ++i)
	{
		delete[] uniqueFPs[i];
	}
	bShutdown = true;
}

void PClass::StaticFreeData (PClass *type)
{
	if (type->Defaults != NULL)
	{
		M_Free(type->Defaults);
		type->Defaults = NULL;
	}
	type->FreeStateList ();

	if (type->ActorInfo != NULL)
	{
		if (type->ActorInfo->OwnedStates != NULL)
		{
			delete[] type->ActorInfo->OwnedStates;
			type->ActorInfo->OwnedStates = NULL;
		}
		if (type->ActorInfo->DamageFactors != NULL)
		{
			delete type->ActorInfo->DamageFactors;
			type->ActorInfo->DamageFactors = NULL;
		}
		if (type->ActorInfo->PainChances != NULL)
		{
			delete type->ActorInfo->PainChances;
			type->ActorInfo->PainChances = NULL;
		}
		if (type->ActorInfo->ColorSets != NULL)
		{
			delete type->ActorInfo->ColorSets;
			type->ActorInfo->ColorSets = NULL;
		}
		delete type->ActorInfo;
		type->ActorInfo = NULL;
	}
	if (type->bRuntimeClass)
	{
		delete type;
	}
	else
	{
		type->Symbols.ReleaseSymbols();
	}
}

void ClassReg::RegisterClass () const
{
	assert (MyClass != NULL);

	// Add type to list
	MyClass->ClassIndex = PClass::m_Types.Push (MyClass);

	MyClass->TypeName = FName(Name+1);
	MyClass->ParentClass = ParentType;
	MyClass->Size = SizeOf;
	MyClass->Pointers = Pointers;
	MyClass->ConstructNative = ConstructNative;
	MyClass->InsertIntoHash ();
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
const PClass *PClass::FindClass (FName zaname)
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
			return cls;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

// Create a new object that this class represents
DObject *PClass::CreateNew () const
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
		type = new PClass;
		notnew = false;
	}

	type->TypeName = name;
	type->ParentClass = this;
	type->Size = size;
	type->Pointers = NULL;
	type->ConstructNative = ConstructNative;
	if (!notnew)
	{
		type->ClassIndex = m_Types.Push (type);
	}
	type->Meta = Meta;

	// Set up default instance of the new class.
	type->Defaults = (BYTE *)M_Malloc(size);
	memcpy (type->Defaults, Defaults, Size);
	if (size > Size)
	{
		memset (type->Defaults + Size, 0, size - Size);
	}

	type->FlatPointers = NULL;
	type->bRuntimeClass = true;
	type->ActorInfo = NULL;
	type->Symbols.SetParentTable (&this->Symbols);
	if (!notnew) type->InsertIntoHash();

	// If this class has an actor info, then any classes derived from it
	// also need an actor info.
	if (this->ActorInfo != NULL)
	{
		FActorInfo *info = type->ActorInfo = new FActorInfo;
		info->Class = type;
		info->GameFilter = GAME_Any;
		info->SpawnID = 0;
		info->ConversationID = 0;
		info->DoomEdNum = -1;
		info->OwnedStates = NULL;
		info->NumOwnedStates = 0;
		info->Replacement = NULL;
		info->Replacee = NULL;
		info->StateList = NULL;
		info->DamageFactors = NULL;
		info->PainChances = NULL;
		info->PainFlashes = NULL;
		info->ColorSets = NULL;
		m_RuntimeActors.Push (type);
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
// is found. CreateDerivedClass will automatically fill
// in the placeholder when the actual class is defined.
const PClass *PClass::FindClassTentative (FName name)
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
	PClass *type = new PClass;
	DPrintf("Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	type->TypeName = name;
	type->ParentClass = this;
	type->Size = -1;
	type->Pointers = NULL;
	type->ConstructNative = NULL;
	type->ClassIndex = m_Types.Push (type);
	type->Defaults = NULL;
	type->FlatPointers = NULL;
	type->bRuntimeClass = true;
	type->ActorInfo = NULL;
	type->InsertIntoHash();
	return type;
}

// This is used by DECORATE to assign ActorInfos to internal classes
void PClass::InitializeActorInfo ()
{
	Symbols.SetParentTable (&ParentClass->Symbols);
	Defaults = (BYTE *)M_Malloc(Size);
	if (ParentClass->Defaults != NULL) 
	{
		memcpy (Defaults, ParentClass->Defaults, ParentClass->Size);
		if (Size > ParentClass->Size)
		{
			memset (Defaults + ParentClass->Size, 0, Size - ParentClass->Size);
		}
	}
	else
	{
		memset (Defaults, 0, Size);
	}

	FActorInfo *info = ActorInfo = new FActorInfo;
	info->Class = this;
	info->GameFilter = GAME_Any;
	info->SpawnID = 0;
	info->ConversationID = 0;
	info->DoomEdNum = -1;
	info->OwnedStates = NULL;
	info->NumOwnedStates = 0;
	info->Replacement = NULL;
	info->Replacee = NULL;
	info->StateList = NULL;
	info->DamageFactors = NULL;
	info->PainChances = NULL;
	info->PainFlashes = NULL;
	info->ColorSets = NULL;
	m_RuntimeActors.Push (this);
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

void PClass::FreeStateList ()
{
	if (ActorInfo != NULL && ActorInfo->StateList != NULL)
	{
		ActorInfo->StateList->Destroy();
		M_Free (ActorInfo->StateList);
		ActorInfo->StateList = NULL;
	}
}

const PClass *PClass::NativeClass() const
{
	const PClass *cls = this;

	while (cls && cls->bRuntimeClass)
		cls = cls->ParentClass;

	return cls;
}

PClass *PClass::GetReplacement() const
{
	return ActorInfo->GetReplacement()->Class;
}

// Symbol tables ------------------------------------------------------------

PSymbol::~PSymbol()
{
}

PSymbolTable::~PSymbolTable ()
{
	ReleaseSymbols();
}

void PSymbolTable::ReleaseSymbols()
{
	for (unsigned int i = 0; i < Symbols.Size(); ++i)
	{
		delete Symbols[i];
	}
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
