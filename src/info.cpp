// info.cpp: Keep track of available actors and their states
//
// This is completely different from the Doom original.


#include "m_fixed.h"
#include "c_dispatch.h"
#include "autosegs.h"

#include "info.h"
#include "gi.h"

#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"

// Each state is owned by an actor. Actors can own any number of
// states, but a single state cannot be owned by more than one
// actor. States are archived by recording the actor they belong
// to and the index into that actor's list of states.

// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

FArchive &operator<< (FArchive &arc, FState *&state)
{
	if (arc.IsStoring ())
	{
		if (state == NULL)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (NULL_STATE_INDEX);
			return arc;
		}

		FActorInfo *info = RUNTIME_CLASS(AActor)->ActorInfo;

		if (state >= info->OwnedStates &&
			state < info->OwnedStates + info->NumOwnedStates)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (state - info->OwnedStates);
			return arc;
		}

		TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;
		while (++reg != NULL)
		{
			if (state >= reg->OwnedStates &&
				state < reg->OwnedStates + reg->NumOwnedStates)
			{
				arc.UserWriteClass (reg->Class);
				arc.WriteCount (state - reg->OwnedStates);
				return arc;
			}
		}
		I_Error ("Cannot find owner for state %p\n", state);
	}
	else
	{
		const TypeInfo *info;
		DWORD ofs;

		arc.UserReadClass (info);
		ofs = arc.ReadCount ();
		if (ofs == NULL_STATE_INDEX && info == RUNTIME_CLASS(AActor))
		{
			state = NULL;
		}
		else if (info->ActorInfo != NULL)
		{
			state = info->ActorInfo->OwnedStates + ofs;
		}
		else
		{
			state = NULL;
		}
	}
	return arc;
}

// Change sprite names to indices
static void ProcessStates (FState *states, int numstates)
{
	int sprite = -1;

	if (states == NULL)
		return;
if (states->sprite.name[0] == 'D' && states->sprite.name[1] == 'E'
	&& states->sprite.name[2] == 'M')
states=states;
	while (--numstates >= 0)
	{
		if (sprite == -1 || strncmp (sprites[sprite].name, states->sprite.name, 4) != 0)
		{
			size_t i;

			sprite = -1;
			for (i = 0; i < sprites.Size (); ++i)
			{
				if (strncmp (sprites[i].name, states->sprite.name, 4) == 0)
				{
					sprite = i;
					break;
				}
			}
			if (sprite == -1)
			{
				spritedef_t temp;
				strncpy (temp.name, states->sprite.name, 4);
				temp.name[5] = 0;
				sprite = sprites.Push (temp);
			}
		}
		states->sprite.index = sprite;
		states++;
	}
}

void FActorInfo::StaticInit ()
{
	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;

	// Attach FActorInfo structures to every actor's TypeInfo
	while (++reg != NULL)
	{
		reg->Class->ActorInfo = reg;
		if (reg->OwnedStates &&
			(unsigned)reg->OwnedStates->sprite.index < sprites.Size ())
		{
			Printf ("\x1c+%s is stateless. Fix its default list.\n",
				reg->Class->Name + 1);
		}
		ProcessStates (reg->OwnedStates, reg->NumOwnedStates);
	}

	// Now build default instances of every actor
	reg.Reset ();
	while (++reg != NULL)
	{
		reg->BuildDefaults ();
	}
}

// Called whenever a new game is started
void FActorInfo::StaticGameSet ()
{
	memset (SpawnableThings, 0, sizeof(SpawnableThings));
	DoomEdMap.Empty ();

	// For every actor valid for this game, add it to the
	// SpawnableThings array and DoomEdMap
	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;
	while (++reg != NULL)
	{
		if (reg->GameFilter == GAME_Any ||
			(reg->GameFilter & gameinfo.gametype))
		{
			if (reg->SpawnID != 0)
			{
				SpawnableThings[reg->SpawnID] = reg->Class;
			}
			if (reg->DoomEdNum != -1)
			{
				DoomEdMap.AddType (reg->DoomEdNum, reg->Class);
			}
		}
	}

	// Now run every AT_GAME_SET function
	TAutoSegIteratorNoArrow<void (*)(), &GRegHead, &GRegTail> setters;
	while (++setters != NULL)
	{
		((void (*)())setters) ();
	}
}

// Called immediately after StaticGameSet, but only if the game
// speed has changed.

void FActorInfo::StaticSpeedSet ()
{
	TAutoSegIteratorNoArrow<void (*)(int), &SRegHead, &SRegTail> setters;
	while (++setters != NULL)
	{
		((void (*)(int))setters) (GameSpeed);
	}
}

FDoomEdMap DoomEdMap;

FDoomEdMap::FDoomEdEntry *FDoomEdMap::DoomEdHash[DOOMED_HASHSIZE];

void FDoomEdMap::AddType (int doomednum, const TypeInfo *type)
{
	int hash = doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry *entry = DoomEdHash[hash];
	while (entry && entry->DoomEdNum != doomednum)
	{
		entry = entry->HashNext;
	}
	if (entry == NULL)
	{
		entry = new FDoomEdEntry;
		entry->HashNext = DoomEdHash[hash];
		entry->DoomEdNum = doomednum;
		DoomEdHash[hash] = entry;
	}
	entry->Type = type;
}

void FDoomEdMap::DelType (int doomednum)
{
	int hash = doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry **prev = &DoomEdHash[hash];
	FDoomEdEntry *entry = *prev;
	while (entry && entry->DoomEdNum != doomednum)
	{
		prev = &entry->HashNext;
		entry = entry->HashNext;
	}
	if (entry != NULL)
	{
		*prev = entry->HashNext;
		delete entry;
	}
}

void FDoomEdMap::Empty ()
{
	int bucket;

	for (bucket = 0; bucket < DOOMED_HASHSIZE; ++bucket)
	{
		FDoomEdEntry *probe = DoomEdHash[bucket];

		while (probe != NULL)
		{
			FDoomEdEntry *next = probe->HashNext;
			delete probe;
			probe = next;
		}
		DoomEdHash[bucket] = NULL;
	}
}

const TypeInfo *FDoomEdMap::FindType (int doomednum) const
{
	int hash = doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry *entry = DoomEdHash[hash];
	while (entry && entry->DoomEdNum != doomednum)
		entry = entry->HashNext;
	return entry ? entry->Type : NULL;
}

struct EdSorting
{
	const TypeInfo *Type;
	int DoomEdNum;
};

static int STACK_ARGS sortnums (const void *a, const void *b)
{
	return ((const EdSorting *)a)->DoomEdNum -
		((const EdSorting *)b)->DoomEdNum;
}

void FDoomEdMap::DumpMapThings ()
{
	TArray<EdSorting> infos (TypeInfo::m_NumTypes);
	int i;

	for (i = 0; i < DOOMED_HASHSIZE; ++i)
	{
		FDoomEdEntry *probe = DoomEdHash[i];

		while (probe != NULL)
		{
			EdSorting sorting = { probe->Type, probe->DoomEdNum };
			infos.Push (sorting);
			probe = probe->HashNext;
		}
	}

	if (infos.Size () == 0)
	{
		Printf ("No map things registered\n");
	}
	else
	{
		qsort (&infos[0], infos.Size (), sizeof(EdSorting), sortnums);

		for (i = 0; i < (int)infos.Size (); ++i)
		{
			Printf ("%6d %s\n",
				infos[i].DoomEdNum, infos[i].Type->Name + 1);
		}
	}
}

CCMD (dumpmapthings)
{
	FDoomEdMap::DumpMapThings ();
}

BOOL CheckCheatmode ();

CCMD (summon)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		// Don't use FindType, because we want a case-insensitive search
		const TypeInfo *type = TypeInfo::IFindType (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMON);
		Net_WriteString (type->Name + 1);
	}
}
