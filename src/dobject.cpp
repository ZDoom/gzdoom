/*
** dobject.cpp
** Implements the base class DObject, which most other classes derive from
**
**---------------------------------------------------------------------------
** Copyright 1998-2005 Randy Heit
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

#include <stdlib.h>
#include <string.h>

#include "cmdlib.h"
#include "actor.h"
#include "dobject.h"
#include "m_alloc.h"
#include "doomstat.h"		// Ideally, DObjects can be used independant of Doom.
#include "d_player.h"		// See p_user.cpp to find out why this doesn't work.
#include "g_game.h"			// Needed for bodyque.
#include "c_dispatch.h"
#include "i_system.h"
#include "r_state.h"
#include "stats.h"

TArray<TypeInfo *> TypeInfo::m_RuntimeActors;
TypeInfo **TypeInfo::m_Types;
unsigned short TypeInfo::m_NumTypes;
unsigned short TypeInfo::m_MaxTypes;
unsigned int TypeInfo::TypeHash[256];	// Why can't I use TypeInfo::HASH_SIZE?

#if defined(_MSC_VER) || defined(__GNUC__)
#include "autosegs.h"

TypeInfo DObject::_StaticType =
{
	"DObject",
	NULL,
	sizeof(DObject),
};

void TypeInfo::StaticInit ()
{
	TAutoSegIterator<TypeInfo *, &CRegHead, &CRegTail> probe;

	while (++probe != NULL)
	{
		probe->RegisterType ();
	}
}
#else
TypeInfo DObject::_StaticType(NULL, "DObject", NULL, sizeof(DObject));
#endif

static cycle_t StaleCycles;
static int StaleCount;

void TypeInfo::RegisterType ()
{
	// Add type to list
	if (m_NumTypes == m_MaxTypes)
	{
		m_MaxTypes = m_MaxTypes ? m_MaxTypes*2 : 256;
		m_Types = (TypeInfo **)Realloc (m_Types, m_MaxTypes * sizeof(*m_Types));
	}
	m_Types[m_NumTypes] = this;
	TypeIndex = m_NumTypes;
	m_NumTypes++;

	// Add type to hash table. Types are inserted into each bucket
	// lexicographically, and the prefix character is ignored.
	unsigned int bucket = MakeKey (Name+1) % HASH_SIZE;
	unsigned int *hashpos = &TypeHash[bucket];
	while (*hashpos != 0)
	{
		int lexx = strcmp (Name+1, m_Types[*hashpos-1]->Name+1);
		// (The Lexx is the most powerful weapon of destruction
		//  in the two universes.)

		if (lexx > 0)
		{ // This type should come later in the chain
			hashpos = &m_Types[*hashpos-1]->HashNext;
		}
		else if (lexx == 0)
		{ // This type has already been inserted
			I_FatalError ("Class %s already registered", Name);
		}
		else
		{ // Type comes right here
			break;
		}
	}
	HashNext = *hashpos;
	*hashpos = TypeIndex + 1;
}

// Case-sensitive search (preferred)
const TypeInfo *TypeInfo::FindType (const char *name)
{
	if (name != NULL)
	{
		unsigned int index = TypeHash[MakeKey (name) % HASH_SIZE];

		while (index != 0)
		{
			int lexx = strcmp (name, m_Types[index-1]->Name + 1);
			if (lexx > 0)
			{
				index = m_Types[index-1]->HashNext;
			}
			else if (lexx == 0)
			{
				return m_Types[index-1];
			}
			else
			{
				break;
			}
		}
	}
	return NULL;
}

// Case-insensitive search
const TypeInfo *TypeInfo::IFindType (const char *name)
{
	if (name != NULL)
	{
		for (int i = 0; i < TypeInfo::m_NumTypes; i++)
		{
			if (stricmp (TypeInfo::m_Types[i]->Name + 1, name) == 0)
				return TypeInfo::m_Types[i];
		}
	}
	return NULL;
}

// Create a new object that this type represents
DObject *TypeInfo::CreateNew () const
{
	BYTE *mem = (BYTE *)Malloc (SizeOf);
	ConstructNative (mem);
	((DObject *)mem)->SetClass (const_cast<TypeInfo *>(this));

	// If this is a scripted extension of a class but not an actor,
	// initialize any extended space to zero. Actors have defaults, so
	// we can initialize them better
	if (ActorInfo != NULL)
	{
		AActor *actor = (AActor *)mem;
		memcpy (&(actor->x), &(((AActor *)ActorInfo->Defaults)->x), SizeOf - ((BYTE *)&actor->x - (BYTE *)actor));
	}
	else if (ParentType != 0 &&
		ConstructNative == ParentType->ConstructNative &&
		SizeOf > ParentType->SizeOf)
	{
		memset (mem + ParentType->SizeOf, 0, SizeOf - ParentType->SizeOf);
	}
	return (DObject *)mem;
}

// Create a new type based on an existing type
TypeInfo *TypeInfo::CreateDerivedClass (char *name, unsigned int size)
{
	TypeInfo *type = new TypeInfo;

	type->Name = name;
	type->ParentType = this;
	type->SizeOf = size;
	type->Pointers = NULL;
	type->ConstructNative = ConstructNative;
	type->RegisterType();
	type->Meta = Meta;

	// If this class has an actor info, then any classes derived from it
	// also need an actor info.
	if (this->ActorInfo != NULL)
	{
		FActorInfo *info = type->ActorInfo = new FActorInfo;
		info->Class = type;
		info->Defaults = new BYTE[size];
		info->GameFilter = GAME_Any;
		info->SpawnID = 0;
		info->DoomEdNum = -1;
		info->OwnedStates = NULL;
		info->NumOwnedStates = 0;

		memcpy (info->Defaults, ActorInfo->Defaults, SizeOf);
		if (size > SizeOf)
		{
			memset (info->Defaults + SizeOf, 0, size - SizeOf);
		}
		m_RuntimeActors.Push (type);
	}
	else
	{
		type->ActorInfo = NULL;
	}
	return type;
}

FMetaTable::~FMetaTable ()
{
	FreeMeta ();
}

FMetaTable::FMetaTable (const FMetaTable &other)
{
	Meta = NULL;
	CopyMeta (&other);
}

FMetaTable &FMetaTable::operator = (const FMetaTable &other)
{
	CopyMeta (&other);
	return *this;
}

void FMetaTable::FreeMeta ()
{
	while (Meta != NULL)
	{
		FMetaData *meta = Meta;

		switch (meta->Type)
		{
		case META_String:
			delete meta->Value.String;
			break;
		default:
			break;
		}
		Meta = meta->Next;
		delete meta;
	}
}

void FMetaTable::CopyMeta (const FMetaTable *other)
{
	const FMetaData *meta_src;
	FMetaData **meta_dest;

	FreeMeta ();

	meta_src = other->Meta;
	meta_dest = &Meta;
	while (meta_src != NULL)
	{
		FMetaData *newmeta = new FMetaData (meta_src->Type, meta_src->ID);
		switch (meta_src->Type)
		{
		case META_String:
			newmeta->Value.String = copystring (meta_src->Value.String);
			break;
		default:
			newmeta->Value = meta_src->Value;
			break;
		}
		*meta_dest = newmeta;
		meta_dest = &newmeta->Next;
		meta_src = meta_src->Next;
	}
	*meta_dest = NULL;
}

FMetaData *FMetaTable::FindMeta (EMetaType type, DWORD id) const
{
	FMetaData *meta = Meta;

	while (meta != NULL)
	{
		if (meta->ID == id && meta->Type == type)
		{
			return meta;
		}
		meta = meta->Next;
	}
	return NULL;
}

FMetaData *FMetaTable::FindMetaDef (EMetaType type, DWORD id)
{
	FMetaData *meta = FindMeta (type, id);
	if (meta == NULL)
	{
		meta = new FMetaData (type, id);
		meta->Next = Meta;
		meta->Value.String = NULL;
		Meta = meta;
	}
	return meta;
}

void FMetaTable::SetMetaInt (DWORD id, int parm)
{
	FMetaData *meta = FindMetaDef (META_Int, id);
	meta->Value.Int = parm;
}

int FMetaTable::GetMetaInt (DWORD id) const
{
	FMetaData *meta = FindMeta (META_Int, id);
	return meta != NULL ? meta->Value.Int : 0;
}

void FMetaTable::SetMetaFixed (DWORD id, fixed_t parm)
{
	FMetaData *meta = FindMetaDef (META_Fixed, id);
	meta->Value.Fixed = parm;
}

fixed_t FMetaTable::GetMetaFixed (DWORD id) const
{
	FMetaData *meta = FindMeta (META_Fixed, id);
	return meta != NULL ? meta->Value.Fixed : 0;
}

void FMetaTable::SetMetaString (DWORD id, const char *parm)
{
	FMetaData *meta = FindMetaDef (META_String, id);
	ReplaceString (&meta->Value.String, parm);
}

const char *FMetaTable::GetMetaString (DWORD id) const
{
	FMetaData *meta = FindMeta (META_String, id);
	return meta != NULL ? meta->Value.String : NULL;
}

CCMD (dumpclasses)
{
	const TypeInfo *root;
	int i;
	int shown, omitted;
	bool showall = true;

	if (argv.argc() > 1)
	{
		root = TypeInfo::IFindType (argv[1]);
		if (root == NULL)
		{
			Printf ("Class '%s' not found\n", argv[1]);
			return;
		}
		if (stricmp (argv[1], "Actor") == 0)
		{
			if (argv.argc() < 3 || stricmp (argv[2], "all") != 0)
			{
				showall = false;
			}
		}
	}
	else
	{
		root = NULL;
	}

	shown = omitted = 0;
	for (i = 0; i < TypeInfo::m_NumTypes; i++)
	{
		if (root == NULL ||
			(TypeInfo::m_Types[i]->IsDescendantOf (root) &&
			 (showall || TypeInfo::m_Types[i] == root ||
			  TypeInfo::m_Types[i]->ActorInfo != root->ActorInfo)))
		{
			Printf (" %s\n", TypeInfo::m_Types[i]->Name + 1);
			shown++;
		}
		else
		{
			omitted++;
		}
	}
	Printf ("%d classes shown, %d omitted\n", shown, omitted);
}

TArray<DObject *> DObject::Objects;
TArray<size_t> DObject::FreeIndices;
TArray<DObject *> DObject::ToDestroy;
bool DObject::Inactive;

DObject::DObject ()
: ObjectFlags(0), Class(0)
{
	if (FreeIndices.Pop (Index))
		Objects[Index] = this;
	else
		Index = Objects.Push (this);
}

DObject::DObject (TypeInfo *inClass)
: ObjectFlags(0), Class(inClass)
{
	if (FreeIndices.Pop (Index))
		Objects[Index] = this;
	else
		Index = Objects.Push (this);
}

DObject::~DObject ()
{
	if (!Inactive)
	{
		if (!(ObjectFlags & OF_MassDestruction))
		{
			RemoveFromArray ();
			DestroyScan (this);
		}
		else if (!(ObjectFlags & OF_Cleanup))
		{
			// object is queued for deletion, but is not being deleted
			// by the destruction process, so remove it from the
			// ToDestroy array and do other necessary stuff.
			unsigned int i;

			for (i = ToDestroy.Size() - 1; i-- > 0; )
			{
				if (ToDestroy[i] == this)
				{
					ToDestroy[i] = NULL;
					break;
				}
			}
			DestroyScan (this);
		}
	}
}

void DObject::Destroy ()
{
	if (!Inactive)
	{
		if (!(ObjectFlags & OF_MassDestruction))
		{
			RemoveFromArray ();
			ObjectFlags |= OF_MassDestruction;
			ToDestroy.Push (this);
		}
	}
	else
		delete this;
}

void DObject::BeginFrame ()
{
	StaleCycles = 0;
	StaleCount = 0;
}

void DObject::EndFrame ()
{
	clock (StaleCycles);
	if (ToDestroy.Size ())
	{
		StaleCount += (int)ToDestroy.Size ();
		DestroyScan ();
		//Printf ("Destroyed %d objects\n", ToDestroy.Size());

		DObject *obj;
		while (ToDestroy.Pop (obj))
		{
			if (obj)
			{
				obj->ObjectFlags |= OF_Cleanup;
				delete obj;
			}
		}
	}
	unclock (StaleCycles);
}

void DObject::RemoveFromArray ()
{
	if (Objects.Size() == Index + 1)
	{
		DObject *dummy;
		Objects.Pop (dummy);
	}
	else if (Objects.Size() > Index)
	{
		Objects[Index] = NULL;
		FreeIndices.Push (Index);
	}
}

void DObject::PointerSubstitution (DObject *old, DObject *notOld)
{
	unsigned int i, highest;
	highest = Objects.Size ();

	for (i = 0; i <= highest; i++)
	{
		DObject *current = i < highest ? Objects[i] : &bglobal;
		if (current)
		{
			const TypeInfo *info = NATIVE_TYPE(current);
			while (info)
			{
				DObject *DObject::* const *offsets = info->Pointers;
				if (offsets)
				{
					while (*offsets != 0)
					{
						if (current->**offsets == old)
						{
							current->**offsets = notOld;
						}
						offsets++;
					}
				}
				info = info->ParentType;
			}
		}
	}

	for (i = 0; i < BODYQUESIZE; ++i)
	{
		if (bodyque[i] == old)
		{
			bodyque[i] = static_cast<AActor *>(notOld);
		}
	}

	// This is an ugly hack, but it's the best I can do for now.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
			players[i].FixPointers (old, notOld);
	}
}

// Search for references to a single object and NULL them.
// It should not be listed in ToDestroy.
void DObject::DestroyScan (DObject *obj)
{
	PointerSubstitution (obj, NULL);
}

// Search for references to all objects scheduled for
// destruction and NULL them.
void DObject::DestroyScan ()
{
	unsigned int i, highest;
	int j, destroycount;
	DObject **destroybase;
	destroycount = (int)ToDestroy.Size ();
	if (destroycount == 0)
		return;
	destroybase = &ToDestroy[0] + destroycount;
	destroycount = -destroycount;
	highest = Objects.Size ();

	for (i = 0; i <= highest; i++)
	{
		DObject *current = i < highest ? Objects[i] : &bglobal;
		if (current)
		{
			const TypeInfo *info = NATIVE_TYPE(current);
			while (info)
			{
				DObject *DObject::* const *offsets = info->Pointers;
				if (offsets)
				{
					while (*offsets != 0)
					{
						j = destroycount;
						do
						{
							if (current->**offsets == *(destroybase + j))
								current->**offsets = 0;
						} while (++j);
						offsets++;
					}
				}
				info = info->ParentType;
			}
		}
	}

	j = destroycount;
	do
	{
		for (i = 0; i < BODYQUESIZE; ++i)
		{
			if (bodyque[i] == *(destroybase + j))
			{
				bodyque[i] = NULL;
			}
		}

	} while (++j);

	// This is an ugly hack, but it's the best I can do for now.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			j = destroycount;
			do
			{
				players[i].FixPointers (*(destroybase + j), NULL);
			} while (++j);
		}
	}
}

void STACK_ARGS DObject::StaticShutdown ()
{
	Inactive = true;
}

void DObject::Serialize (FArchive &arc)
{
	ObjectFlags |= OF_SerialSuccess;
}

void DObject::CheckIfSerialized () const
{
	if (!(ObjectFlags & OF_SerialSuccess))
	{
		I_Error (
			"BUG: %s::Serialize\n"
			"(or one of its superclasses) needs to call\n"
			"Super::Serialize\n",
			StaticType ()->Name);
	}
}

FArchive &operator<< (FArchive &arc, const TypeInfo * &info)
{
	if (arc.IsStoring ())
	{
		arc.UserWriteClass (info);
	}
	else
	{
		arc.UserReadClass (info);
	}
	return arc;
}

ADD_STAT (destroys, out)
{
	sprintf (out, "Pointer fixing: %d in %04.1f ms",
		StaleCount, SecondsPerCycle * (double)StaleCycles * 1000);
}
