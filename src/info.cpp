/*
** info.cpp
** Keeps track of available actors and their states
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
** This is completely different from Doom's info.c.
**
*/


#include "info.h"
#include "m_fixed.h"
#include "c_dispatch.h"
#include "autosegs.h"

#include "gi.h"

#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"
#include "templates.h"
#include "cmdlib.h"

extern void LoadDecorations ();

// Each state is owned by an actor. Actors can own any number of
// states, but a single state cannot be owned by more than one
// actor. States are archived by recording the actor they belong
// to and the index into that actor's list of states.

// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

//==========================================================================
//
//
//==========================================================================

FArchive &operator<< (FArchive &arc, FState *&state)
{
	const PClass *info;

	if (arc.IsStoring ())
	{
		if (state == NULL)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (NULL_STATE_INDEX);
			return arc;
		}

		info = FState::StaticFindStateOwner (state);

		if (info != NULL)
		{
			arc.UserWriteClass (info);
			arc.WriteCount ((DWORD)(state - info->ActorInfo->OwnedStates));
		}
		else
		{
			/* this was never working as intended.
			I_Error ("Cannot find owner for state %p:\n"
					 "%s %c%c %3d [%p] -> %p", state,
					 sprites[state->sprite].name,
					 state->GetFrame() + 'A',
					 state->GetFullbright() ? '*' : ' ',
					 state->GetTics(),
					 state->GetAction(),
					 state->GetNextState());
			*/
		}
	}
	else
	{
		const PClass *info;
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

//==========================================================================
//
// Find the actor that a state belongs to.
//
//==========================================================================

const PClass *FState::StaticFindStateOwner (const FState *state)
{
	for (unsigned int i = 0; i < PClass::m_RuntimeActors.Size(); ++i)
	{
		FActorInfo *info = PClass::m_RuntimeActors[i]->ActorInfo;
		if (state >= info->OwnedStates &&
			state <  info->OwnedStates + info->NumOwnedStates)
		{
			return info->Class;
		}
	}

	return NULL;
}

//==========================================================================
//
// Find the actor that a state belongs to, but restrict the search to
// the specified type and its ancestors.
//
//==========================================================================

const PClass *FState::StaticFindStateOwner (const FState *state, const FActorInfo *info)
{
	while (info != NULL)
	{
		if (state >= info->OwnedStates &&
			state <  info->OwnedStates + info->NumOwnedStates)
		{
			return info->Class;
		}
		info = info->Class->ParentClass->ActorInfo;
	}
	return NULL;
}

//==========================================================================
//
//
//==========================================================================

int GetSpriteIndex(const char * spritename)
{
	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strncmp (sprites[i].name, spritename, 4) == 0)
		{
			return (int)i;
		}
	}
	spritedef_t temp;
	strncpy (temp.name, spritename, 4);
	temp.name[4] = 0;
	temp.numframes = 0;
	temp.spriteframes = 0;
	return (int)sprites.Push (temp);
}


//==========================================================================
//
//
//==========================================================================

void FActorInfo::StaticInit ()
{
	if (sprites.Size() == 0)
	{
		spritedef_t temp;

		// Sprite 0 is always TNT1
		memcpy (temp.name, "TNT1", 5);
		temp.numframes = 0;
		temp.spriteframes = 0;
		sprites.Push (temp);

		// Sprite 1 is always ----
		memcpy (temp.name, "----", 5);
		sprites.Push (temp);
	}

	Printf ("LoadDecorations: Load external actors.\n");
	LoadDecorations ();
}

//==========================================================================
//
// Called after Dehacked patches are applied
//
//==========================================================================

void FActorInfo::StaticSetActorNums ()
{
	memset (SpawnableThings, 0, sizeof(SpawnableThings));
	DoomEdMap.Empty ();

	for (unsigned int i = 0; i < PClass::m_RuntimeActors.Size(); ++i)
	{
		PClass::m_RuntimeActors[i]->ActorInfo->RegisterIDs ();
	}
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::RegisterIDs ()
{
	if (GameFilter == GAME_Any || (GameFilter & gameinfo.gametype))
	{
		if (SpawnID != 0)
		{
			SpawnableThings[SpawnID] = Class;
		}
		if (DoomEdNum != -1)
		{
			DoomEdMap.AddType (DoomEdNum, Class);
		}
	}
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacement ()
{
	if (Replacement == NULL)
	{
		return this;
	}
	// The Replacement field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacement;
	Replacement = NULL;
	FActorInfo *rep = savedrep->GetReplacement ();
	Replacement = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacee ()
{
	if (Replacee == NULL)
	{
		return this;
	}
	// The Replacee field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacee;
	Replacee = NULL;
	FActorInfo *rep = savedrep->GetReplacee ();
	Replacee = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetDamageFactor(FName type, fixed_t factor)
{
	if (factor != FRACUNIT) 
	{
		if (DamageFactors == NULL) DamageFactors=new DmgFactors;
		DamageFactors->Insert(type, factor);
	}
	else 
	{
		if (DamageFactors != NULL) 
			DamageFactors->Remove(type);
	}
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetPainChance(FName type, int chance)
{
	if (chance >= 0) 
	{
		if (PainChances == NULL) PainChances=new PainChanceList;
		PainChances->Insert(type, MIN(chance, 255));
	}
	else 
	{
		if (PainChances != NULL) 
			PainChances->Remove(type);
	}
}

//==========================================================================
//
//
//==========================================================================

FStateLabel *FStateLabels::FindLabel (FName label)
{
	return const_cast<FStateLabel *>(BinarySearch<FStateLabel, FName> (Labels, NumLabels, &FStateLabel::Label, label));
}

void FStateLabels::Destroy ()
{
	for(int i=0; i<NumLabels;i++)
	{
		if (Labels[i].Children != NULL)
		{
			Labels[i].Children->Destroy();
			free (Labels[i].Children);	// These are malloc'd, not new'd!
			Labels[i].Children=NULL;
		}
	}
}


//===========================================================================
//
// HasStates
//
// Checks whether the actor has special death states.
//
//===========================================================================

bool AActor::HasSpecialDeathStates () const
{
	const FActorInfo *info = GetClass()->ActorInfo;

	if (info->StateList != NULL)
	{
		FStateLabel *slabel = info->StateList->FindLabel (NAME_Death);
		if (slabel != NULL && slabel->Children != NULL)
		{
			for(int i=0;i<slabel->Children->NumLabels;i++)
			{
				if (slabel->Children->Labels[i].State != NULL) return true;
			}
		}
	}
	return false;
}

//===========================================================================
//
// FindState (one name version)
//
// Finds a state with the exact specified name.
//
//===========================================================================

FState *AActor::FindState (FName label) const
{
	const FActorInfo *info = GetClass()->ActorInfo;

	if (info->StateList != NULL)
	{
		FStateLabel *slabel = info->StateList->FindLabel (label);
		if (slabel != NULL)
		{
			return slabel->State;
		}
	}
	return NULL;
}

//===========================================================================
//
// FindState (two name version)
//
//===========================================================================

FState *AActor::FindState (FName label, FName sublabel, bool exact) const
{
	const FActorInfo *info = GetClass()->ActorInfo;

	if (info->StateList != NULL)
	{
		FStateLabel *slabel = info->StateList->FindLabel (label);
		if (slabel != NULL)
		{
			if (slabel->Children != NULL)
			{
				FStateLabel *slabel2 = slabel->Children->FindLabel(sublabel);
				if (slabel2 != NULL)
				{
					return slabel2->State;
				}
			}
			if (!exact) return slabel->State;
		}
	}
	return NULL;
}

//===========================================================================
//
// FindState (multiple names version)
//
// Finds a state that matches as many of the supplied names as possible.
// A state with more names than those provided does not match.
// A state with fewer names can match if there are no states with the exact
// same number of names.
//
// The search proceeds like this. For the current class, keeping matching
// names until there are no more. If both the argument list and the state
// are out of names, it's an exact match, so return it. If the state still
// has names, ignore it. If the argument list still has names, remember it.
//
//===========================================================================
FState *FActorInfo::FindState (FName name) const
{
	return FindState(1, &name);
}

FState *FActorInfo::FindState (int numnames, FName *names, bool exact) const
{
	FStateLabels *labels = StateList;
	FState *best = NULL;

	if (labels != NULL)
	{
		int count = 0;
		FStateLabel *slabel = NULL;
		FName label;

		// Find the best-matching label for this class.
		while (labels != NULL && count < numnames)
		{
			label = *names++;
			slabel = labels->FindLabel (label);

			if (slabel != NULL)
			{
				count++;
				labels = slabel->Children;
				best = slabel->State;
			}
			else
			{
				break;
			}
		}
		if (count < numnames && exact) return NULL;
	}
	return best;
}

//==========================================================================
//
// Creates a list of names from a string. Dots are used as separator
//
//==========================================================================

void MakeStateNameList(const char * fname, TArray<FName> * out)
{
	FName firstpart, secondpart;
	char * c;

	// Handle the old names for the existing death states
	char * name = copystring(fname);
	firstpart = strtok(name, ".");
	switch (firstpart)
	{
	case NAME_Burn:
		firstpart = NAME_Death;
		secondpart = NAME_Fire;
		break;
	case NAME_Ice:
		firstpart = NAME_Death;
		secondpart = NAME_Ice;
		break;
	case NAME_Disintegrate:
		firstpart = NAME_Death;
		secondpart = NAME_Disintegrate;
		break;
	case NAME_XDeath:
		firstpart = NAME_Death;
		secondpart = NAME_Extreme;
		break;
	}

	out->Clear();
	out->Push(firstpart);
	if (secondpart!=NAME_None) out->Push(secondpart);

	while ((c = strtok(NULL, "."))!=NULL)
	{
		FName cc = c;
		out->Push(cc);
	}
	delete [] name;
}



//==========================================================================
//
//
//==========================================================================

FDoomEdMap DoomEdMap;

FDoomEdMap::FDoomEdEntry *FDoomEdMap::DoomEdHash[DOOMED_HASHSIZE];

FDoomEdMap::~FDoomEdMap()
{
	Empty();
}

void FDoomEdMap::AddType (int doomednum, const PClass *type)
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
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
	else
	{
		Printf (PRINT_BOLD, "Warning: %s and %s both have doomednum %d.\n",
			type->TypeName.GetChars(), entry->Type->TypeName.GetChars(), doomednum);
	}
	entry->Type = type;
}

void FDoomEdMap::DelType (int doomednum)
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
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

const PClass *FDoomEdMap::FindType (int doomednum) const
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry *entry = DoomEdHash[hash];
	while (entry && entry->DoomEdNum != doomednum)
		entry = entry->HashNext;
	return entry ? entry->Type : NULL;
}

struct EdSorting
{
	const PClass *Type;
	int DoomEdNum;
};

static int STACK_ARGS sortnums (const void *a, const void *b)
{
	return ((const EdSorting *)a)->DoomEdNum -
		((const EdSorting *)b)->DoomEdNum;
}

void FDoomEdMap::DumpMapThings ()
{
	TArray<EdSorting> infos (PClass::m_Types.Size());
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
				infos[i].DoomEdNum, infos[i].Type->TypeName.GetChars());
		}
	}
}

CCMD (dumpmapthings)
{
	FDoomEdMap::DumpMapThings ();
}

bool CheckCheatmode ();

CCMD (summon)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		const PClass *type = PClass::FindClass (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMON);
		Net_WriteString (type->TypeName.GetChars());
	}
}

CCMD (summonfriend)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		const PClass *type = PClass::FindClass (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMONFRIEND);
		Net_WriteString (type->TypeName.GetChars());
	}
}

CCMD (summonfoe)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		const PClass *type = PClass::FindClass (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMONFOE);
		Net_WriteString (type->TypeName.GetChars());
	}
}
