/*
** p_states.cpp
** state management
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2006-2008 Christoph Oelckers
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
**
*/
#include "actor.h"
#include "farchive.h"
#include "templates.h"
#include "cmdlib.h"
#include "i_system.h"
#include "thingdef/thingdef.h"

// Each state is owned by an actor. Actors can own any number of
// states, but a single state cannot be owned by more than one
// actor. States are archived by recording the actor they belong
// to and the index into that actor's list of states.

// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

TArray<FName> JumpParameters;

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

//==========================================================================
//
// Resolves a label parameter
//
//==========================================================================

FState *P_GetState(AActor *self, FState *CallingState, int offset)
{
	if (offset == 0 || offset == INT_MIN)
	{
		return NULL;	// 0 means 'no state'
	}
	else if (offset>0)
	{
		if (CallingState == NULL) return NULL;
		return CallingState + offset;
	}
	else if (self != NULL)
	{
		FName *params = &JumpParameters[-offset];

		FName classname = params[0];
		const PClass *cls;
		cls = classname==NAME_None?  RUNTIME_TYPE(self) : PClass::FindClass(classname);
		if (cls==NULL || cls->ActorInfo==NULL) return NULL;	// shouldn't happen

		int numnames = (int)params[1];
		FState *jumpto = cls->ActorInfo->FindState(numnames, &params[2]);
		if (jumpto == NULL)
		{
			const char *dot="";
			Printf("Jump target '");
			if (classname != NAME_None) Printf("%s::", classname.GetChars());
			for (int i=0;i<numnames;i++)
			{
				Printf("%s%s", dot, params[2+i].GetChars());
				dot = ".";
			}
			Printf("' not found in %s\n", self->GetClass()->TypeName.GetChars());
		}
		return jumpto;
	}
	else return NULL;
}

//==========================================================================
//
// Creates a list of names from a string. Dots are used as separator
//
//==========================================================================

TArray<FName> &MakeStateNameList(const char * fname)
{
	static TArray<FName> namelist(3);
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

	namelist.Clear();
	namelist.Push(firstpart);
	if (secondpart!=NAME_None) namelist.Push(secondpart);

	while ((c = strtok(NULL, "."))!=NULL)
	{
		FName cc = c;
		namelist.Push(cc);
	}
	delete [] name;
	return namelist;
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
// Finds the state associated with the given string
//
//==========================================================================

FState *FActorInfo::FindStateByString(const char *name, bool exact)
{
	TArray<FName> &namelist = MakeStateNameList(name);
	return FindState(namelist.Size(), &namelist[0], exact);
}




//==========================================================================
//
// Search one list of state definitions for the given name
//
//==========================================================================

FStateDefine *FStateDefinitions::FindStateLabelInList(TArray<FStateDefine> & list, FName name, bool create)
{
	for(unsigned i = 0; i<list.Size(); i++)
	{
		if (list[i].Label == name) return &list[i];
	}
	if (create)
	{
		FStateDefine def;
		def.Label=name;
		def.State=NULL;
		def.DefineFlags = SDF_NEXT;
		return &list[list.Push(def)];
	}
	return NULL;
}

//==========================================================================
//
// Finds the address of a state label given by name. 
// Adds the state label if it doesn't exist
//
//==========================================================================

FStateDefine * FStateDefinitions::FindStateAddress(const char *name)
{
	FStateDefine * statedef=NULL;

	TArray<FName> &namelist = MakeStateNameList(name);

	TArray<FStateDefine> * statelist = &StateLabels;
	for(unsigned i=0;i<namelist.Size();i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], true);
		statelist = &statedef->Children;
	}
	return statedef;
}

//==========================================================================
//
// Adds a new state tp the curremt list
//
//==========================================================================

void FStateDefinitions::AddState (const char *statename, FState *state, BYTE defflags)
{
	FStateDefine *std = FindStateAddress(statename);
	std->State = state;
	std->DefineFlags = defflags;
}

//==========================================================================
//
// Finds the state associated with the given name
// returns NULL if none found
//
//==========================================================================

FState * FStateDefinitions::FindState(const char * name)
{
	FStateDefine * statedef=NULL;

	TArray<FName> &namelist = MakeStateNameList(name);

	TArray<FStateDefine> * statelist = &StateLabels;
	for(unsigned i=0;i<namelist.Size();i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], false);
		if (statedef == NULL) return NULL;
		statelist = &statedef->Children;
	}
	return statedef? statedef->State : NULL;
}

//==========================================================================
//
// Creates the final list of states from the state definitions
//
//==========================================================================

static int STACK_ARGS labelcmp(const void * a, const void * b)
{
	FStateLabel * A = (FStateLabel *)a;
	FStateLabel * B = (FStateLabel *)b;
	return ((int)A->Label - (int)B->Label);
}

FStateLabels * FStateDefinitions::CreateStateLabelList(TArray<FStateDefine> & statelist)
{
	// First delete all empty labels from the list
	for (int i=statelist.Size()-1;i>=0;i--)
	{
		if (statelist[i].Label == NAME_None || (statelist[i].State == NULL && statelist[i].Children.Size() == 0))
		{
			statelist.Delete(i);
		}
	}

	int count=statelist.Size();

	if (count == 0) return NULL;

	FStateLabels * list = (FStateLabels*)M_Malloc(sizeof(FStateLabels)+(count-1)*sizeof(FStateLabel));
	list->NumLabels = count;

	for (int i=0;i<count;i++)
	{
		list->Labels[i].Label = statelist[i].Label;
		list->Labels[i].State = statelist[i].State;
		list->Labels[i].Children = CreateStateLabelList(statelist[i].Children);
	}
	qsort(list->Labels, count, sizeof(FStateLabel), labelcmp);
	return list;
}

//===========================================================================
//
// InstallStates
//
// Creates the actor's state list from the current definition
//
//===========================================================================

void FStateDefinitions::InstallStates(FActorInfo *info, AActor *defaults)
{
	// First ensure we have a valid spawn state.
	FState *state = FindState("Spawn");

	if (state == NULL)
	{
		// A NULL spawn state will crash the engine so set it to something valid.
		AddState("Spawn", GetDefault<AActor>()->SpawnState);
	}

	if (info->StateList != NULL) 
	{
		info->StateList->Destroy();
		M_Free(info->StateList);
	}
	info->StateList = CreateStateLabelList(StateLabels);

	// Cache these states as member veriables.
	defaults->SpawnState = info->FindState(NAME_Spawn);
	defaults->SeeState = info->FindState(NAME_See);
	// Melee and Missile states are manipulated by the scripted marines so they
	// have to be stored locally
	defaults->MeleeState = info->FindState(NAME_Melee);
	defaults->MissileState = info->FindState(NAME_Missile);
}

//===========================================================================
//
// MakeStateDefines
//
// Creates a list of state definitions from an existing actor
// Used by Dehacked to modify an actor's state list
//
//===========================================================================

void FStateDefinitions::MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest)
{
	dest.Clear();
	if (list != NULL) for(int i=0;i<list->NumLabels;i++)
	{
		FStateDefine def;

		def.Label = list->Labels[i].Label;
		def.State = list->Labels[i].State;
		dest.Push(def);
		if (list->Labels[i].Children != NULL)
		{
			MakeStateList(list->Labels[i].Children, dest[dest.Size()-1].Children);
		}
	}
}

void FStateDefinitions::MakeStateDefines(const PClass *cls)
{
	if (cls->ActorInfo && cls->ActorInfo->StateList)
	{
		MakeStateList(cls->ActorInfo->StateList, StateLabels);
	}
	else
	{
		ClearStateLabels();
	}
}

//===========================================================================
//
// AddStateDefines
//
// Adds a list of states to the current definitions
//
//===========================================================================

void FStateDefinitions::AddStateDefines(const FStateLabels *list)
{
	if (list != NULL) for(int i=0;i<list->NumLabels;i++)
	{
		if (list->Labels[i].Children == NULL)
		{
			if (!FindStateLabelInList(StateLabels, list->Labels[i].Label, false))
			{
				FStateDefine def;

				def.Label = list->Labels[i].Label;
				def.State = list->Labels[i].State;
				StateLabels.Push(def);
			}
		}
	}
}

//==========================================================================
//
// RetargetState(Pointer)s
//
// These functions are used when a goto follows one or more labels.
// Because multiple labels are permitted to occur consecutively with no
// intervening states, it is not enough to remember the last label defined
// and adjust it. So these functions search for all labels that point to
// the current position in the state array and give them a copy of the
// target string instead.
//
//==========================================================================

void FStateDefinitions::RetargetStatePointers (intptr_t count, const char *target, TArray<FStateDefine> & statelist)
{
	for(unsigned i = 0;i<statelist.Size(); i++)
	{
		if (statelist[i].State == (FState*)count)
		{
			if (target == NULL)
			{
				statelist[i].State = NULL;
				statelist[i].DefineFlags = SDF_STOP;
			}
			else
			{
				statelist[i].State = (FState *)copystring (target);
				statelist[i].DefineFlags = SDF_LABEL;
			}
		}
		if (statelist[i].Children.Size() > 0)
		{
			RetargetStatePointers(count, target, statelist[i].Children);
		}
	}
}

void FStateDefinitions::RetargetStates (intptr_t count, const char *target)
{
	RetargetStatePointers(count, target, StateLabels);
}


//==========================================================================
//
// ResolveGotoLabel
//
// Resolves any strings being stored in a state's NextState field
//
//==========================================================================

FState *FStateDefinitions::ResolveGotoLabel (AActor *actor, const PClass *mytype, char *name)
{
	const PClass *type=mytype;
	FState *state;
	char *namestart = name;
	char *label, *offset, *pt;
	int v;

	// Check for classname
	if ((pt = strstr (name, "::")) != NULL)
	{
		const char *classname = name;
		*pt = '\0';
		name = pt + 2;

		// The classname may either be "Super" to identify this class's immediate
		// superclass, or it may be the name of any class that this one derives from.
		if (stricmp (classname, "Super") == 0)
		{
			type = type->ParentClass;
			actor = GetDefaultByType (type);
		}
		else
		{
			// first check whether a state of the desired name exists
			const PClass *stype = PClass::FindClass (classname);
			if (stype == NULL)
			{
				I_Error ("%s is an unknown class.", classname);
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				I_Error ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (type))
			{
				I_Error ("%s is not derived from %s so cannot access its states.",
					type->TypeName.GetChars(), stype->TypeName.GetChars());
			}
			if (type != stype)
			{
				type = stype;
				actor = GetDefaultByType (type);
			}
		}
	}
	label = name;
	// Check for offset
	offset = NULL;
	if ((pt = strchr (name, '+')) != NULL)
	{
		*pt = '\0';
		offset = pt + 1;
	}
	v = offset ? strtol (offset, NULL, 0) : 0;

	// Get the state's address.
	if (type==mytype) state = FindState (label);
	else state = type->ActorInfo->FindStateByString(label, true);

	if (state != NULL)
	{
		state += v;
	}
	else if (v != 0)
	{
		I_Error ("Attempt to get invalid state %s from actor %s.", label, type->TypeName.GetChars());
	}
	delete[] namestart;		// free the allocated string buffer
	return state;
}

//==========================================================================
//
// FixStatePointers
//
// Fixes an actor's default state pointers.
//
//==========================================================================

void FStateDefinitions::FixStatePointers (FActorInfo *actor, TArray<FStateDefine> & list)
{
	for(unsigned i=0;i<list.Size(); i++)
	{
		size_t v=(size_t)list[i].State;
		if (v >= 1 && v < 0x10000)
		{
			list[i].State = actor->OwnedStates + v - 1;
		}
		if (list[i].Children.Size() > 0) FixStatePointers(actor, list[i].Children);
	}
}

//==========================================================================
//
// ResolveGotoLabels
//
// Resolves an actor's state pointers that were specified as jumps.
//
//==========================================================================

void FStateDefinitions::ResolveGotoLabels (FActorInfo *actor, AActor *defaults, TArray<FStateDefine> & list)
{
	for(unsigned i=0;i<list.Size(); i++)
	{
		if (list[i].State != NULL && list[i].DefineFlags == SDF_LABEL)
		{ // It's not a valid state, so it must be a label string. Resolve it.
			list[i].State = ResolveGotoLabel (defaults, actor->Class, (char *)list[i].State);
			list[i].DefineFlags = SDF_STATE;
		}
		if (list[i].Children.Size() > 0) ResolveGotoLabels(actor, defaults, list[i].Children);
	}
}


//==========================================================================
//
// FinishStates
// copies a state block and fixes all state links using the current list of labels
//
//==========================================================================

int FStateDefinitions::FinishStates (FActorInfo *actor, AActor *defaults, TArray<FState> &StateArray)
{
	static int c=0;
	int count = StateArray.Size();

	if (count > 0)
	{
		FState *realstates = new FState[count];
		int i;
		int currange;

		memcpy(realstates, &StateArray[0], count*sizeof(FState));
		actor->OwnedStates = realstates;
		actor->NumOwnedStates = count;

		// adjust the state pointers
		// In the case new states are added these must be adjusted, too!
		FixStatePointers (actor, StateLabels);

		for(i = currange = 0; i < count; i++)
		{
			// resolve labels and jumps
			switch(realstates[i].DefineFlags)
			{
			case SDF_STOP:	// stop
				realstates[i].NextState = NULL;
				break;

			case SDF_WAIT:	// wait
				realstates[i].NextState = &realstates[i];
				break;

			case SDF_NEXT:		// next
				realstates[i].NextState = (i < count-1 ? &realstates[i+1] : &realstates[0]);
				break;

			case SDF_INDEX:		// loop
				realstates[i].NextState = &realstates[(size_t)realstates[i].NextState-1];
				break;

			case SDF_LABEL:
				realstates[i].NextState = ResolveGotoLabel (defaults, actor->Class, (char *)realstates[i].NextState);
				break;
			}
		}
	}

	// Fix state pointers that are gotos
	ResolveGotoLabels (actor, defaults, StateLabels);

	return count;
}

