#include <stdlib.h>
#include <string.h>

#include "dobject.h"
#include "m_alloc.h"
#include "doomstat.h"		// Ideally, DObjects can be used independant of Doom.
#include "d_player.h"		// See p_user.cpp to find out why this doesn't work.
#include "z_zone.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "r_state.h"

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

void TypeInfo::RegisterType ()
{
	// Add type to list
	if (m_NumTypes == m_MaxTypes)
	{
		m_MaxTypes = m_MaxTypes ? m_MaxTypes*2 : 32;
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
	return NULL;
}

// Case-insensitive search
const TypeInfo *TypeInfo::IFindType (const char *name)
{
	int i;

	for (i = 0; i < TypeInfo::m_NumTypes; i++)
	{
		if (stricmp (TypeInfo::m_Types[i]->Name + 1, name) == 0)
			return TypeInfo::m_Types[i];
	}
	return NULL;
}

CCMD (dumpclasses)
{
	const TypeInfo *root;
	int i;
	int shown, omitted;
	bool showall = true;

	if (argc > 1)
	{
		root = TypeInfo::IFindType (argv[1]);
		if (root == NULL)
		{
			Printf (PRINT_HIGH, "Class '%s' not found\n", argv[1]);
			return;
		}
		if (stricmp (argv[1], "Actor") == 0)
		{
			if (argc < 3 || stricmp (argv[2], "all") != 0)
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
			Printf (PRINT_HIGH, " %s\n", TypeInfo::m_Types[i]->Name + 1);
			shown++;
		}
		else
		{
			omitted++;
		}
	}
	Printf (PRINT_HIGH, "%d classes shown, %d omitted\n", shown, omitted);
}

TArray<DObject *> DObject::Objects;
TArray<size_t> DObject::FreeIndices;
TArray<DObject *> DObject::ToDestroy;
bool DObject::Inactive;

DObject::DObject ()
{
	ObjectFlags = 0;
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
			int i;

			for (i = ToDestroy.Size() - 1; i >= 0; i--)
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
}

void DObject::EndFrame ()
{
	if (ToDestroy.Size ())
	{
		DestroyScan ();
		//Printf (PRINT_HIGH, "Destroyed %d objects\n", ToDestroy.Size());

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
}

void DObject::RemoveFromArray ()
{
	if (Objects.Size () == Index + 1)
	{
		DObject *dummy;
		Objects.Pop (dummy);
	}
	else
	{
		Objects[Index] = NULL;
		FreeIndices.Push (Index);
	}
}

// Search for references to a single object and NULL them.
// It should not be listed in ToDestroy.
void DObject::DestroyScan (DObject *obj)
{
	size_t i, highest;
	highest = Objects.Size ();

	for (i = 0; i < highest; i++)
	{
		DObject *current = Objects[i];
		if (current)
		{
			const TypeInfo *info = RUNTIME_TYPE(current);
			while (info)
			{
				const size_t *offsets = info->Pointers;
				if (offsets)
				{
					while (*offsets != ~0)
					{
						if (*((DObject **)(((byte *)current) + *offsets)) == obj)
						{
							*((DObject **)(((byte *)current) + *offsets)) = NULL;
						}
						offsets++;
					}
				}
				info = info->ParentType;
			}
		}
	}

	if (obj->IsKindOf (RUNTIME_CLASS(APlayerPawn)))
	{
		AActor *actor = static_cast<AActor *>(obj);
		for (i = 0; i < (size_t)numsectors; i++)
		{
			if (sectors[i].soundtarget == actor)
				sectors[i].soundtarget = NULL;
		}
	}

	// This is an ugly hack, but it's the best I can do for now.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
			players[i].FixPointers (obj);
	}
}

// Search for references to all objects scheduled for
// destruction and NULL them.
void DObject::DestroyScan ()
{
	size_t i, highest;
	int j, destroycount;
	DObject **destroybase;
	destroycount = (int)ToDestroy.Size ();
	if (destroycount == 0)
		return;
	destroybase = &ToDestroy[0] + destroycount;
	destroycount = -destroycount;
	highest = Objects.Size ();

	for (i = 0; i < highest; i++)
	{
		DObject *current = Objects[i];
		if (current)
		{
			const TypeInfo *info = RUNTIME_TYPE(current);
			while (info)
			{
				const size_t *offsets = info->Pointers;
				if (offsets)
				{
					while (*offsets != ~0)
					{
						j = destroycount;
						do
						{
							if (*((DObject **)(((byte *)current) + *offsets)) == *(destroybase + j))
								*((DObject **)(((byte *)current) + *offsets)) = NULL;
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
		if ((*(destroybase + j))->IsKindOf (RUNTIME_CLASS(APlayerPawn)))
		{
			AActor *actor = static_cast<AActor *>(*(destroybase + j));
			for (i = 0; i < (size_t)numsectors; i++)
			{
				if (sectors[i].soundtarget == actor)
					sectors[i].soundtarget = NULL;
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
				players[i].FixPointers (*(destroybase + j));
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
