#include <stdlib.h>
#include <string.h>

#include "dobject.h"
#include "m_alloc.h"
#include "doomstat.h"		// Ideally, DObjects can be used independant of Doom.
#include "d_player.h"		// See p_user.cpp to find out why this doesn't work.
#include "z_zone.h"

ClassInit::ClassInit (TypeInfo *type)
{
	type->RegisterType ();
}

TypeInfo **TypeInfo::m_Types;
unsigned short TypeInfo::m_NumTypes;
unsigned short TypeInfo::m_MaxTypes;

void TypeInfo::RegisterType ()
{
	if (m_NumTypes == m_MaxTypes)
	{
		m_MaxTypes = m_MaxTypes ? m_MaxTypes*2 : 32;
		m_Types = (TypeInfo **)Realloc (m_Types, m_MaxTypes * sizeof(*m_Types));
	}
	m_Types[m_NumTypes] = this;
	TypeIndex = m_NumTypes;
	m_NumTypes++;
}

const TypeInfo *TypeInfo::FindType (const char *name)
{
	unsigned short i;

	for (i = 0; i != m_NumTypes; i++)
		if (!strcmp (name, m_Types[i]->Name))
			return m_Types[i];

	return NULL;
}

TypeInfo DObject::_StaticType(NULL, "DObject", NULL, sizeof(DObject));

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
							break;
						}
						offsets++;
					}
				}
				info = info->ParentType;
			}
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
	size_t i, j, highest, destroycount;
	destroycount = ToDestroy.Size ();
	if (destroycount == 0)
		return;
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
						for (j = 0; j < destroycount; j++)
							if (*((DObject **)(((byte *)current) + *offsets)) == ToDestroy[j])
								*((DObject **)(((byte *)current) + *offsets)) = NULL;
						offsets++;
					}
				}
				info = info->ParentType;
			}
		}
		// This is an ugly hack, but it's the best I can do for now.
		for (j = 0; j < MAXPLAYERS; j++)
		{
			size_t k;
			if (playeringame[j])
				for (k = 0; k < destroycount; k++)
					players[j].FixPointers (ToDestroy[k]);
		}
	}
}

void STACK_ARGS DObject::StaticShutdown ()
{
	Inactive = true;
}
