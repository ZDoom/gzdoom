/*
** dobject.cpp
** Implements the base class DObject, which most other classes derive from
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
#include "a_sharedglobal.h"

#include "autosegs.h"

PClass DObject::_StaticType;
ClassReg DObject::RegistrationInfo =
{
	&DObject::_StaticType,			// MyClass
	"DObject",						// Name
	NULL,							// ParentType
	sizeof(DObject),				// SizeOf
	NULL,							// Pointers
	&DObject::InPlaceConstructor	// ConstructNative
};
_DECLARE_TI(DObject)

static cycle_t StaleCycles;
static int StaleCount;

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
			delete[] meta->Value.String;
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

int FMetaTable::GetMetaInt (DWORD id, int def) const
{
	FMetaData *meta = FindMeta (META_Int, id);
	return meta != NULL ? meta->Value.Int : def;
}

void FMetaTable::SetMetaFixed (DWORD id, fixed_t parm)
{
	FMetaData *meta = FindMetaDef (META_Fixed, id);
	meta->Value.Fixed = parm;
}

fixed_t FMetaTable::GetMetaFixed (DWORD id, fixed_t def) const
{
	FMetaData *meta = FindMeta (META_Fixed, id);
	return meta != NULL ? meta->Value.Fixed : def;
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
	// This is by no means speed-optimized. But it's an informational console
	// command that will be executed infrequently, so I don't mind.
	struct DumpInfo
	{
		const PClass *Type;
		DumpInfo *Next;
		DumpInfo *Children;

		static DumpInfo *FindType (DumpInfo *root, const PClass *type)
		{
			if (root == NULL)
			{
				return root;
			}
			if (root->Type == type)
			{
				return root;
			}
			if (root->Next != NULL)
			{
				return FindType (root->Next, type);
			}
			if (root->Children != NULL)
			{
				return FindType (root->Children, type);
			}
			return NULL;
		}

		static DumpInfo *AddType (DumpInfo **root, const PClass *type)
		{
			DumpInfo *info, *parentInfo;

			if (*root == NULL)
			{
				info = new DumpInfo;
				info->Type = type;
				info->Next = NULL;
				info->Children = *root;
				*root = info;
				return info;
			}
			if (type->ParentClass == (*root)->Type)
			{
				parentInfo = *root;
			}
			else if (type == (*root)->Type)
			{
				return *root;
			}
			else
			{
				parentInfo = FindType (*root, type->ParentClass);
				if (parentInfo == NULL)
				{
					parentInfo = AddType (root, type->ParentClass);
				}
			}
			// Has this type already been added?
			for (info = parentInfo->Children; info != NULL; info = info->Next)
			{
				if (info->Type == type)
				{
					return info;
				}
			}
			info = new DumpInfo;
			info->Type = type;
			info->Next = parentInfo->Children;
			info->Children = NULL;
			parentInfo->Children = info;
			return info;
		}

		static void PrintTree (DumpInfo *root, int level)
		{
			Printf ("%*c%s\n", level, ' ', root->Type->TypeName.GetChars());
			if (root->Children != NULL)
			{
				PrintTree (root->Children, level + 2);
			}
			if (root->Next != NULL)
			{
				PrintTree (root->Next, level);
			}
		}

		static void FreeTree (DumpInfo *root)
		{
			if (root->Children != NULL)
			{
				FreeTree (root->Children);
			}
			if (root->Next != NULL)
			{
				FreeTree (root->Next);
			}
			delete root;
		}
	};

	unsigned int i;
	int shown, omitted;
	DumpInfo *tree = NULL;
	const PClass *root = NULL;
	bool showall = true;

	if (argv.argc() > 1)
	{
		root = PClass::FindClass (argv[1]);
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

	shown = omitted = 0;
	DumpInfo::AddType (&tree, root != NULL ? root : RUNTIME_CLASS(DObject));
	for (i = 0; i < PClass::m_Types.Size(); i++)
	{
		PClass *cls = PClass::m_Types[i];
		if (root == NULL ||
			(cls->IsDescendantOf (root) &&
			(showall || cls == root ||
			cls->ActorInfo != root->ActorInfo)))
		{
			DumpInfo::AddType (&tree, cls);
//			Printf (" %s\n", PClass::m_Types[i]->Name + 1);
			shown++;
		}
		else
		{
			omitted++;
		}
	}
	DumpInfo::PrintTree (tree, 2);
	DumpInfo::FreeTree (tree);
	Printf ("%d classes shown, %d omitted\n", shown, omitted);
}

TArray<DObject *> DObject::Objects (TArray<DObject *>::NoInit);
TArray<unsigned int> DObject::FreeIndices (TArray<unsigned int>::NoInit);
TArray<DObject *> DObject::ToDestroy (TArray<DObject *>::NoInit);
bool DObject::Inactive;

void DObject::InPlaceConstructor (void *mem)
{
	new ((EInPlace *)mem) DObject;
}

DObject::DObject ()
: ObjectFlags(0), Class(0)
{
	if (FreeIndices.Pop (Index))
		Objects[Index] = this;
	else
		Index = Objects.Push (this);
}

DObject::DObject (PClass *inClass)
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
			const PClass *info = current->GetClass();
			const size_t *offsets = info->FlatPointers;
			if (offsets == NULL)
			{
				const_cast<PClass *>(info)->BuildFlatPointers();
				offsets = info->FlatPointers;
			}
			while (*offsets != ~(size_t)0)
			{
				if (*(DObject **)((BYTE *)current + *offsets) == old)
				{
					*(DObject **)((BYTE *)current + *offsets) = notOld;
				}
				offsets++;
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

	if (sectors != NULL)
	{
		for (i = 0; i < (unsigned int)numsectors; ++i)
		{
			if (sectors[i].SoundTarget == old)
			{
				sectors[i].SoundTarget = static_cast<AActor *>(notOld);
			}
			if (sectors[i].CeilingSkyBox == old)
			{
				sectors[i].CeilingSkyBox = static_cast<ASkyViewpoint *>(notOld);
			}
			if (sectors[i].FloorSkyBox == old)
			{
				sectors[i].FloorSkyBox = static_cast<ASkyViewpoint *>(notOld);
			}
		}
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
			const PClass *info = current->GetClass();
			const size_t *offsets = info->FlatPointers;
			if (offsets == NULL)
			{
				const_cast<PClass *>(info)->BuildFlatPointers();
				offsets = info->FlatPointers;
			}
			while (*offsets != ~(size_t)0)
			{
				j = destroycount;
				do
				{
					if (*(DObject **)((BYTE *)current + *offsets) == *(destroybase + j))
					{
						*(DObject **)((BYTE *)current + *offsets) = NULL;
					}
				} while (++j);
				offsets++;
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

	for (i = 0; i < (unsigned int)numsectors; ++i)
	{
		j = destroycount;
		do
		{
			if (sectors[i].SoundTarget == *(destroybase + j))
			{
				sectors[i].SoundTarget = NULL;
			}
		} while (++j);
	}
}

void DObject::StaticShutdown ()
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
			StaticType()->TypeName.GetChars());
	}
}

ADD_STAT (destroys)
{
	FString out;
	out.Format ("Pointer fixing: %d in %04.1f ms",
		StaleCount, SecondsPerCycle * (double)StaleCycles * 1000);
	return out;
}
