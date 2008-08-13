/*
** thingdef_states.cpp
**
** Actor definitions - the state parser
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Christoph Oelckers
** Copyright 2004-2007 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "templates.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "thingdef.h"
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "i_system.h"
#include "autosegs.h"

TArray<int> StateParameters;
TArray<FName> JumpParameters;
static TArray<AFuncDesc> AFTable;
static TArray<FState> StateArray;

//==========================================================================
//
// Action functions
//
//==========================================================================

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================
static int STACK_ARGS funccmp(const void * a, const void * b)
{
	return stricmp( ((AFuncDesc*)a)->Name, ((AFuncDesc*)b)->Name);
}

AFuncDesc * FindFunction(const char * string)
{
	static bool funcsorted=false;

	if (!funcsorted) 
	{
		TAutoSegIterator<AFuncDesc *, &ARegHead, &ARegTail> probe;

		while (++probe != NULL)
		{
			AFTable.Push(*probe);
		}
		AFTable.ShrinkToFit();
		qsort(&AFTable[0], AFTable.Size(), sizeof(AFTable[0]), funccmp);
		funcsorted=true;
	}

	int min = 0, max = AFTable.Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, AFTable[mid].Name);
		if (lexval == 0)
		{
			return &AFTable[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}


//==========================================================================
//
// Find an action function in AActor's table
//
//==========================================================================

PSymbolActionFunction *FindGlobalActionFunction(const char *name)
{
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol(name, false);
	if (sym != NULL && sym->SymbolType == SYM_ActionFunction)
		return static_cast<PSymbolActionFunction*>(sym);
	else
		return NULL;
}

//==========================================================================
//
// Find a state address
//
//==========================================================================

struct FStateDefine
{
	FName Label;
	TArray<FStateDefine> Children;
	FState *State;
};

static TArray<FStateDefine> StateLabels;

void ClearStateLabels()
{
	StateLabels.Clear();
}

//==========================================================================
//
// Search one list of state definitions for the given name
//
//==========================================================================

static FStateDefine * FindStateLabelInList(TArray<FStateDefine> & list, FName name, bool create)
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
		return &list[list.Push(def)];
	}
	return NULL;
}

//==========================================================================
//
// Finds the address of a state given by name. 
// Adds the state if it doesn't exist
//
//==========================================================================

static FStateDefine * FindStateAddress(const char * name)
{
	static TArray<FName> namelist(3);
	FStateDefine * statedef=NULL;

	MakeStateNameList(name, &namelist);

	TArray<FStateDefine> * statelist = &StateLabels;
	for(unsigned i=0;i<namelist.Size();i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], true);
		statelist = &statedef->Children;
	}
	return statedef;
}

void AddState (const char * statename, FState * state)
{
	FStateDefine * std = FindStateAddress(statename);
	std->State = state;
}

//==========================================================================
//
// Finds the state associated with the given name
//
//==========================================================================

FState * FindState(AActor * actor, const PClass * type, const char * name)
{
	static TArray<FName> namelist(3);
	FStateDefine * statedef=NULL;

	MakeStateNameList(name, &namelist);

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
// Finds the state associated with the given name
//
//==========================================================================

FState * FindStateInClass(AActor * actor, const PClass * type, const char * name)
{
	static TArray<FName> namelist(3);

	MakeStateNameList(name, &namelist);
	FActorInfo * info = type->ActorInfo;
	if (info) return info->FindState(namelist.Size(), &namelist[0], true);
	return NULL;
}

//==========================================================================
//
// Checks if a state list is empty
// A list is empty if it doesn't contain any states and no children
// that contain any states
//
//==========================================================================

static bool IsStateListEmpty(TArray<FStateDefine> & statelist)
{
	for(unsigned i=0;i<statelist.Size();i++)
	{
		if (statelist[i].State!=NULL || !IsStateListEmpty(statelist[i].Children)) return false;
	}
	return true;
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

static FStateLabels * CreateStateLabelList(TArray<FStateDefine> & statelist)
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

void InstallStates(FActorInfo *info, AActor *defaults)
{
	// First ensure we have a valid spawn state.
	FState * state = FindState(defaults, info->Class, "Spawn");

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

static void MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest)
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

void MakeStateDefines(const FStateLabels *list)
{
	MakeStateList(list, StateLabels);
}

void AddStateDefines(const FStateLabels *list)
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
// PrepareStateParameters
// creates an empty parameter list for a parameterized function call
//
//==========================================================================
int PrepareStateParameters(FState * state, int numparams)
{
	int paramindex=StateParameters.Size();
	int i, v;

	v=0;
	for(i=0;i<numparams;i++) StateParameters.Push(v);
	state->ParameterIndex = paramindex+1;
	return paramindex;
}

//==========================================================================
//
// DoActionSpecials
// handles action specials as code pointers
//
//==========================================================================
bool DoActionSpecials(FScanner &sc, FState & state, bool multistate, int * statecount, Baggage &bag)
{
	int i;
	int min_args, max_args;
	FString specname = sc.String;

	int special = P_FindLineSpecial(sc.String, &min_args, &max_args);

	if (special > 0 && min_args >= 0)
	{

		int paramindex=PrepareStateParameters(&state, 6);

		StateParameters[paramindex]=special;

		// Make this consistent with all other parameter parsing
		if (sc.CheckToken('('))
		{
			for (i = 0; i < 5;)
			{
				StateParameters[paramindex+i+1] = ParseExpression (sc, false, bag.Info->Class);
				i++;
				if (!sc.CheckToken (',')) break;
			}
			sc.MustGetToken (')');
		}
		else i=0;

		if (i < min_args)
		{
			sc.ScriptError ("Too few arguments to %s", specname.GetChars());
		}
		if (i > max_args)
		{
			sc.ScriptError ("Too many arguments to %s", specname.GetChars());
		}

		state.SetAction(FindGlobalActionFunction("A_CallSpecial"), false);
		return true;
	}
	return false;
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

static void RetargetStatePointers (intptr_t count, const char *target, TArray<FStateDefine> & statelist)
{
	for(unsigned i = 0;i<statelist.Size(); i++)
	{
		if (statelist[i].State == (FState*)count)
		{
			statelist[i].State = target == NULL ? NULL : (FState *)copystring (target);
		}
		if (statelist[i].Children.Size() > 0)
		{
			RetargetStatePointers(count, target, statelist[i].Children);
		}
	}
}

static void RetargetStates (intptr_t count, const char *target)
{
	RetargetStatePointers(count, target, StateLabels);
}

//==========================================================================
//
// Reads a state label that may contain '.'s.
// processes a state block
//
//==========================================================================
static FString ParseStateString(FScanner &sc)
{
	FString statestring;

	sc.MustGetString();
	statestring = sc.String;
	if (sc.CheckString("::"))
	{
		sc.MustGetString ();
		statestring << "::" << sc.String;
	}
	while (sc.CheckString ("."))
	{
		sc.MustGetString ();
		statestring << "." << sc.String;
	}
	return statestring;
}

//==========================================================================
//
// ParseStates
// parses a state block
//
//==========================================================================
int ParseStates(FScanner &sc, FActorInfo * actor, AActor * defaults, Baggage &bag)
{
	FString statestring;
	intptr_t count = 0;
	FState state;
	FState * laststate = NULL;
	intptr_t lastlabel = -1;
	int minrequiredstate = -1;
	int spriteindex = 0;
	char lastsprite[5]="";

	sc.MustGetStringName ("{");
	sc.SetEscape(false);	// disable escape sequences in the state parser
	while (!sc.CheckString ("}") && !sc.End)
	{
		memset(&state,0,sizeof(state));
		statestring = ParseStateString(sc);
		if (!statestring.CompareNoCase("GOTO"))
		{
do_goto:	
			statestring = ParseStateString(sc);
			if (sc.CheckString ("+"))
			{
				sc.MustGetNumber ();
				statestring += '+';
				statestring += sc.String;
			}
			// copy the text - this must be resolved later!
			if (laststate != NULL)
			{ // Following a state definition: Modify it.
				laststate->NextState = (FState*)copystring(statestring);	
			}
			else if (lastlabel >= 0)
			{ // Following a label: Retarget it.
				RetargetStates (count+1, statestring);
			}
			else
			{
				sc.ScriptError("GOTO before first state");
			}
		}
		else if (!statestring.CompareNoCase("STOP"))
		{
do_stop:
			if (laststate!=NULL)
			{
				laststate->NextState=(FState*)-1;
			}
			else if (lastlabel >=0)
			{
				RetargetStates (count+1, NULL);
			}
			else
			{
				sc.ScriptError("STOP before first state");
				continue;
			}
		}
		else if (!statestring.CompareNoCase("WAIT") || !statestring.CompareNoCase("FAIL"))
		{
			if (!laststate) 
			{
				sc.ScriptError("%s before first state", sc.String);
				continue;
			}
			laststate->NextState=(FState*)-2;
		}
		else if (!statestring.CompareNoCase("LOOP"))
		{
			if (!laststate) 
			{
				sc.ScriptError("LOOP before first state");
				continue;
			}
			laststate->NextState=(FState*)(lastlabel+1);
		}
		else
		{
			const char * statestrp;

			sc.MustGetString();
			if (sc.Compare (":"))
			{
				laststate = NULL;
				do
				{
					lastlabel = count;
					AddState(statestring, (FState *) (count+1));
					statestring = ParseStateString(sc);
					if (!statestring.CompareNoCase("GOTO"))
					{
						goto do_goto;
					}
					else if (!statestring.CompareNoCase("STOP"))
					{
						goto do_stop;
					}
					sc.MustGetString ();
				} while (sc.Compare (":"));
//				continue;
			}

			sc.UnGet ();

			if (statestring.Len() != 4)
			{
				sc.ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			statestring.ToUpper();
			if (strcmp(statestring, lastsprite))
			{
				strcpy(lastsprite, statestring);
				spriteindex = GetSpriteIndex(lastsprite);
			}

			state.sprite = spriteindex;
			state.Misc1 = state.Misc2 = 0;
			state.ParameterIndex = 0;
			sc.MustGetString();
			statestring = (sc.String+1);
			statestrp = statestring;
			state.Frame = (*sc.String & 223)-'A';
			if ((*sc.String & 223)<'A' || (*sc.String & 223)>']')
			{
				sc.ScriptError ("Frames must be A-Z, [, \\, or ]");
				state.Frame=0;
			}

			sc.MustGetNumber();
			state.Tics = clamp<int>(sc.Number, -1, 32767);

			while (sc.GetString() && !sc.Crossed)
			{
				if (sc.Compare("BRIGHT")) 
				{
					state.Frame |= SF_FULLBRIGHT;
					continue;
				}
				if (sc.Compare("OFFSET"))
				{
					// specify a weapon offset
					sc.MustGetStringName("(");
					sc.MustGetNumber();
					state.Misc1 = sc.Number;
					sc.MustGetStringName (",");
					sc.MustGetNumber();
					state.Misc2 = sc.Number;
					sc.MustGetStringName(")");
					continue;
				}

				// Make the action name lowercase to satisfy the gperf hashers
				strlwr (sc.String);

				int minreq = count;
				if (DoActionSpecials(sc, state, !statestring.IsEmpty(), &minreq, bag))
				{
					if (minreq>minrequiredstate) minrequiredstate=minreq;
					goto endofstate;
				}

				PSymbol *sym = bag.Info->Class->Symbols.FindSymbol (FName(sc.String, true), true);
				if (sym != NULL && sym->SymbolType == SYM_ActionFunction)
				{
					PSymbolActionFunction *afd = static_cast<PSymbolActionFunction *>(sym);
					state.SetAction(afd, false);
					if (!afd->Arguments.IsEmpty())
					{
						const char *params = afd->Arguments.GetChars();
						int numparams = (int)afd->Arguments.Len();
				
						int v;

						if (!islower(*params))
						{
							sc.MustGetStringName("(");
						}
						else
						{
							if (!sc.CheckString("(")) 
							{
								state.ParameterIndex = afd->defaultparameterindex+1;
								goto endofstate;
							}
						}
						
						int paramindex = PrepareStateParameters(&state, numparams);
						int paramstart = paramindex;
						bool varargs = params[numparams - 1] == '+';


						if (varargs)
						{
							StateParameters[paramindex++] = 0;
						}
						else if (afd->defaultparameterindex > -1)
						{
							memcpy(&StateParameters[paramindex], &StateParameters[afd->defaultparameterindex],
								afd->Arguments.Len() * sizeof (StateParameters[0]));
						}

						while (*params)
						{
							if ((*params == 'l' || *params == 'L') && sc.CheckNumber())
							{
								// Special case: State label as an offset
								if (sc.Number > 0 && strlen(statestring)>0)
								{
									sc.ScriptError("You cannot use state jumps commands with a jump offset on multistate definitions\n");
								}

								v=sc.Number;
								if (v<0)
								{
									sc.ScriptError("Negative jump offsets are not allowed");
								}

								int minreq=count+v;
								if (minreq>minrequiredstate) minrequiredstate=minreq;
							}
							else
							{
								// Use the generic parameter parser for everything else
								v = ParseParameter(sc, bag.Info->Class, *params, false);
							}
							StateParameters[paramindex++] = v;
							params++;
							if (varargs)
							{
								StateParameters[paramstart]++;
							}
							if (*params)
							{
								if (*params == '+')
								{
									if (sc.CheckString(")"))
									{
										goto endofstate;
									}
									params--;
									v = 0;
									StateParameters.Push(v);
								}
								else if ((islower(*params) || *params=='!') && sc.CheckString(")"))
								{
									goto endofstate;
								}
								sc.MustGetStringName (",");
							}
						}
						sc.MustGetStringName(")");
					}
					else 
					{
						sc.MustGetString();
						if (sc.Compare("("))
						{
							sc.ScriptError("You cannot pass parameters to '%s'\n",sc.String);
						}
						sc.UnGet();
					}
					goto endofstate;
				}
				sc.ScriptError("Invalid state parameter %s\n", sc.String);
			}
			sc.UnGet();
endofstate:
			StateArray.Push(state);
			while (*statestrp)
			{
				int frame=((*statestrp++)&223)-'A';

				if (frame<0 || frame>28)
				{
					sc.ScriptError ("Frames must be A-Z, [, \\, or ]");
					frame=0;
				}

				state.Frame=(state.Frame&(SF_FULLBRIGHT))|frame;
				StateArray.Push(state);
				count++;
			}
			laststate=&StateArray[count];
			count++;
		}
	}
	if (count<=minrequiredstate)
	{
		sc.ScriptError("A_Jump offset out of range in %s", actor->Class->TypeName.GetChars());
	}
	sc.SetEscape(true);	// re-enable escape sequences
	return count;
}

//==========================================================================
//
// ResolveGotoLabel
//
//==========================================================================

static FState *ResolveGotoLabel (FScanner &sc, AActor *actor, const PClass *mytype, char *name)
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
				sc.ScriptError ("%s is an unknown class.", classname);
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				sc.ScriptError ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (type))
			{
				sc.ScriptError ("%s is not derived from %s so cannot access its states.",
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
	if (type==mytype) state = FindState (actor, type, label);
	else state = FindStateInClass (actor, type, label);

	if (state != NULL)
	{
		state += v;
	}
	else if (v != 0)
	{
		sc.ScriptError ("Attempt to get invalid state %s from actor %s.", label, type->TypeName.GetChars());
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

static void FixStatePointers (FActorInfo *actor, TArray<FStateDefine> & list)
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
// FixStatePointersAgain
//
// Resolves an actor's state pointers that were specified as jumps.
//
//==========================================================================

static void FixStatePointersAgain (FScanner &sc, FActorInfo *actor, AActor *defaults, TArray<FStateDefine> & list)
{
	for(unsigned i=0;i<list.Size(); i++)
	{
		if (list[i].State != NULL && FState::StaticFindStateOwner (list[i].State, actor) == NULL)
		{ // It's not a valid state, so it must be a label string. Resolve it.
			list[i].State = ResolveGotoLabel (sc, defaults, actor->Class, (char *)list[i].State);
		}
		if (list[i].Children.Size() > 0) FixStatePointersAgain(sc, actor, defaults, list[i].Children);
	}
}


//==========================================================================
//
// FinishStates
// copies a state block and fixes all state links
//
//==========================================================================

int FinishStates (FScanner &sc, FActorInfo *actor, AActor *defaults, Baggage &bag)
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
			switch((ptrdiff_t)realstates[i].NextState)
			{
			case 0:		// next
				realstates[i].NextState = (i < count-1 ? &realstates[i+1] : &realstates[0]);
				break;

			case -1:	// stop
				realstates[i].NextState = NULL;
				break;

			case -2:	// wait
				realstates[i].NextState = &realstates[i];
				break;

			default:	// loop
				if ((size_t)realstates[i].NextState < 0x10000)
				{
					realstates[i].NextState = &realstates[(size_t)realstates[i].NextState-1];
				}
				else	// goto
				{
					realstates[i].NextState = ResolveGotoLabel (sc, defaults, bag.Info->Class, (char *)realstates[i].NextState);
				}
			}
		}
	}
	StateArray.Clear ();

	// Fix state pointers that are gotos
	FixStatePointersAgain (sc, actor, defaults, StateLabels);

	return count;
}

//==========================================================================
//
// For getting a state address from the parent
// No attempts have been made to add new functionality here
// This is strictly for keeping compatibility with old WADs!
//
//==========================================================================
FState *CheckState(FScanner &sc, PClass *type)
{
	int v=0;

	if (sc.GetString() && !sc.Crossed)
	{
		if (sc.Compare("0")) return NULL;
		else if (sc.Compare("PARENT"))
		{
			FState * state = NULL;
			sc.MustGetString();

			FActorInfo * info = type->ParentClass->ActorInfo;

			if (info != NULL)
			{
				state = info->FindState(FName(sc.String));
			}

			if (sc.GetString ())
			{
				if (sc.Compare ("+"))
				{
					sc.MustGetNumber ();
					v = sc.Number;
				}
				else
				{
					sc.UnGet ();
				}
			}

			if (state == NULL && v==0) return NULL;

			if (v!=0 && state==NULL)
			{
				sc.ScriptError("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				return NULL;
			}
			state+=v;
			return state;
		}
		else sc.ScriptError("Invalid state assignment");
	}
	return NULL;
}

