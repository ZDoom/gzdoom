/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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

#include "gi.h"
#include "actor.h"
#include "r_defs.h"
#include "a_pickups.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "decallib.h"
#include "p_local.h"
#include "a_weapons.h"
#include "p_conversation.h"
#include "v_text.h"
#include "codegen.h"
#include "stats.h"
#include "info.h"
#include "thingdef.h"
#include "zcc_parser.h"
#include "zcc_compile_doom.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void InitThingdef();

// STATIC FUNCTION PROTOTYPES --------------------------------------------

static TMap<FState *, FScriptPosition> StateSourceLines;
static FScriptPosition unknownstatesource("unknown file", 0);

EXTERN_CVAR(Bool, strictdecorate);
EXTERN_CVAR(Bool, warningstoerrors);

//==========================================================================
//
// PClassActor :: Finalize
//
// Installs the parsed states and does some sanity checking
//
//==========================================================================

void FinalizeClass(PClass *ccls, FStateDefinitions &statedef)
{
	if (!ccls->IsDescendantOf(NAME_Actor)) return;
	auto cls = static_cast<PClassActor*>(ccls);
	try
	{
		statedef.FinishStates(cls);
	}
	catch (CRecoverableError &)
	{
		statedef.MakeStateDefines(nullptr);
		throw;
	}
	auto def = GetDefaultByType(cls);
	statedef.InstallStates(cls, def);
	statedef.MakeStateDefines(nullptr);

	if (cls->IsDescendantOf(NAME_Inventory))
	{
		def->flags |= MF_SPECIAL;
	}

	if (cls->IsDescendantOf(NAME_Weapon) && !cls->bAbstract)
	{
		FState *ready = def->FindState(NAME_Ready);
		FState *select = def->FindState(NAME_Select);
		FState *deselect = def->FindState(NAME_Deselect);
		FState *fire = def->FindState(NAME_Fire);
		auto TypeName = cls->TypeName;

		// Consider any weapon without any valid state abstract and don't output a warning
		// This is for creating base classes for weapon groups that only set up some properties.
		if (ready || select || deselect || fire)
		{
			if (!ready)
			{
				I_Error("Weapon %s doesn't define a ready state.", TypeName.GetChars());
			}
			if (!select)
			{
				I_Error("Weapon %s doesn't define a select state.", TypeName.GetChars());
			}
			if (!deselect)
			{
				I_Error("Weapon %s doesn't define a deselect state.", TypeName.GetChars());
			}
			if (!fire)
			{
				I_Error("Weapon %s doesn't define a fire state.", TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
// Saves the state's source lines for error messages during postprocessing
//
//==========================================================================

void SaveStateSourceLines(FState *firststate, TArray<FScriptPosition> &positions)
{
	for (unsigned i = 0; i < positions.Size(); i++)
	{
		StateSourceLines[firststate + i] = positions[i];
	}
}

FScriptPosition & GetStateSource(FState *state)
{
	auto check = StateSourceLines.CheckKey(state);
	return check ? *check : unknownstatesource;
}

//==========================================================================
//
// SetImplicitArgs
//
// Adds the parameters implied by the function flags.
//
//==========================================================================

void SetImplicitArgs(TArray<PType *> *args, TArray<uint32_t> *argflags, TArray<FName> *argnames, PContainerType *cls, uint32_t funcflags, int useflags)
{
	// Must be called before adding any other arguments.
	assert(args == nullptr || args->Size() == 0);
	assert(argflags == nullptr || argflags->Size() == 0);

	if (funcflags & VARF_Method)
	{
		// implied self pointer
		if (args != nullptr)		args->Push(NewPointer(cls, !!(funcflags & VARF_ReadOnly))); 
		if (argflags != nullptr)	argflags->Push(VARF_Implicit | VARF_ReadOnly);
		if (argnames != nullptr)	argnames->Push(NAME_self);
	}
	if (funcflags & VARF_Action)
	{
		assert(!(funcflags & VARF_ReadOnly));
		// implied caller and callingstate pointers
		if (args != nullptr)
		{
			// Special treatment for weapons and CustomInventory flagged functions: 'self' is not the defining class but the actual user of the item, so this pointer must be of type 'Actor'
			if (useflags & (SUF_WEAPON|SUF_ITEM))
			{
				args->Insert(0, NewPointer(RUNTIME_CLASS(AActor)));	// this must go in before the real pointer to the containing class.
			}
			else
			{
				args->Push(NewPointer(cls));
			}
			args->Push(NewPointer(NewStruct("FStateParamInfo", nullptr)));
		}
		if (argflags != nullptr)
		{
			argflags->Push(VARF_Implicit | VARF_ReadOnly);
			argflags->Push(VARF_Implicit | VARF_ReadOnly);
		}
		if (argnames != nullptr)
		{
			argnames->Push(NAME_invoker);
			argnames->Push(NAME_stateinfo);
		}
	}
}

//==========================================================================
//
// CreateAnonymousFunction
//
// Creates a function symbol for an anonymous function
// This contains actual info about the implied variables which is needed
// during code generation.
//
//==========================================================================

PFunction *CreateAnonymousFunction(PContainerType *containingclass, PType *returntype, int flags)
{
	TArray<PType *> rets;
	TArray<PType *> args;
	TArray<uint32_t> argflags;
	TArray<FName> argnames;

	// Functions that only get flagged for actors do not need the additional two context parameters.
	int fflags = (flags& (SUF_OVERLAY | SUF_WEAPON | SUF_ITEM)) ? VARF_Action | VARF_Method : VARF_Method;

	// [ZZ] give anonymous functions the scope of their class 
	//      (just give them VARF_Play, whatever)
	fflags |= VARF_Play;

	if (returntype) rets.Push(returntype);
	SetImplicitArgs(&args, &argflags, &argnames, containingclass, fflags, flags);

	PFunction *sym = Create<PFunction>(containingclass, NAME_None);	// anonymous functions do not have names.
	sym->AddVariant(NewPrototype(rets, args), argflags, argnames, nullptr, fflags, flags);
	return sym;
}

//==========================================================================
//
// CreateDamageFunction
//
// creates a damage function from the given expression
//
//==========================================================================

void CreateDamageFunction(PNamespace *OutNamespace, const VersionInfo &ver, PClassActor *info, AActor *defaults, FxExpression *id, bool fromDecorate, int lumpnum)
{
	if (id == nullptr)
	{
		defaults->DamageFunc = nullptr;
	}
	else
	{
		auto dmg = new FxReturnStatement(new FxIntCast(id, true), id->ScriptPosition);
		auto funcsym = CreateAnonymousFunction(info->VMType, TypeSInt32, 0);
		defaults->DamageFunc = FunctionBuildList.AddFunction(OutNamespace, ver, funcsym, dmg, FStringf("%s.DamageFunction", info->TypeName.GetChars()), fromDecorate, -1, 0, lumpnum);
	}
}

//==========================================================================
//
// CheckForUnsafeStates
//
// Performs a quick analysis to find potentially bad states.
// This is not perfect because it cannot track jumps by function.
// For such cases a runtime check in the relevant places is also present.
//
//==========================================================================
static void CheckForUnsafeStates(PClassActor *obj)
{
	static ENamedName weaponstates[] = { NAME_Ready, NAME_Deselect, NAME_Select, NAME_Fire, NAME_AltFire, NAME_Hold, NAME_AltHold, NAME_Flash, NAME_AltFlash, NAME_None };
	static ENamedName pickupstates[] = { NAME_Pickup, NAME_Drop, NAME_Use, NAME_None };
	TMap<FState *, bool> checked;
	ENamedName *test;

	auto cwtype = PClass::FindActor(NAME_Weapon);
	if (obj->IsDescendantOf(cwtype))
	{
		if (obj->Size == cwtype->Size) return;	// This class cannot have user variables.
		test = weaponstates;
	}
	else
	{
		auto citype = PClass::FindActor(NAME_CustomInventory);
		if (obj->IsDescendantOf(citype))
		{
			if (obj->Size == citype->Size) return;	// This class cannot have user variables.
			test = pickupstates;
		}
		else return;	// something else derived from StateProvider. We do not know what this may be.
	}

	for (; *test != NAME_None; test++)
	{
		FState *state = obj->FindState(*test);
		while (state != nullptr && checked.CheckKey(state) == nullptr)	// have we checked this state already. If yes, we can stop checking the current chain.
		{
			checked[state] = true;
			if (state->ActionFunc && state->ActionFunc->Unsafe)
			{
				// If an unsafe function (i.e. one that accesses user variables) is being detected, print a warning once and remove the bogus function. We may not call it because that would inevitably crash.
				GetStateSource(state).Message(MSG_ERROR, TEXTCOLOR_RED "Unsafe state call in state %s which accesses user variables, reached by %s.%s.\n",
					FState::StaticGetStateName(state).GetChars(), obj->TypeName.GetChars(), FName(*test).GetChars());
			}
			state = state->NextState;
		}
	}
}

//==========================================================================
//
// CheckStates
//
// Checks if states link to ones with proper restrictions
// Checks that all base labels refer a string with proper restrictions.
// For these cases a runtime check in the relevant places is also present.
//
//==========================================================================

static void CheckLabel(PClassActor *obj, FStateLabel *slb, int useflag, FName statename, const char *descript)
{
	auto state = slb->State;
	if (state != nullptr)
	{
		if (uintptr_t(state) <= 0xffff)
		{
			// can't do much here aside from printing a message and aborting.
			I_Error("Bad state label %s in actor %s", slb->Label.GetChars(), obj->TypeName.GetChars());
		}

		if (!(state->UseFlags & useflag))
		{
			GetStateSource(state).Message(MSG_ERROR, TEXTCOLOR_RED "%s references state %s as %s state, but this state is not flagged for use as %s.\n",
				obj->TypeName.GetChars(), FState::StaticGetStateName(state, obj).GetChars(), statename.GetChars(), descript);
		}
	}
	if (slb->Children != nullptr)
	{
		for (int i = 0; i < slb->Children->NumLabels; i++)
		{
			auto state = slb->Children->Labels[i].State;
			CheckLabel(obj, &slb->Children->Labels[i], useflag, statename, descript);
		}
	}
}

static void CheckStateLabels(PClassActor *obj, ENamedName *test, int useflag,  const char *descript)
{
	FStateLabels *labels = obj->GetStateLabels();

	for (; *test != NAME_None; test++)
	{
		auto label = labels->FindLabel(*test);
		if (label != nullptr)
		{
			CheckLabel(obj, label, useflag, *test, descript);
		}
	}
}


static void CheckStates(PClassActor *obj)
{
	static ENamedName actorstates[] = { NAME_Spawn, NAME_See, NAME_Melee, NAME_Missile, NAME_Pain, NAME_Death, NAME_Wound, NAME_Raise, NAME_Yes, NAME_No, NAME_Greetings, NAME_None };
	static ENamedName weaponstates[] = { NAME_Ready, NAME_Deselect, NAME_Select, NAME_Fire, NAME_AltFire, NAME_Hold, NAME_AltHold, NAME_Flash, NAME_AltFlash, NAME_None };
	static ENamedName pickupstates[] = { NAME_Pickup, NAME_Drop, NAME_Use, NAME_None };
	TMap<FState *, bool> checked;

	CheckStateLabels(obj, actorstates, SUF_ACTOR, "actor sprites");

	if (obj->IsDescendantOf(NAME_Weapon))
	{
		CheckStateLabels(obj, weaponstates, SUF_WEAPON, "weapon sprites");
	}
	else if (obj->IsDescendantOf(NAME_CustomInventory))
	{
		CheckStateLabels(obj, pickupstates, SUF_ITEM, "CustomInventory state chain");
	}
	for (unsigned i = 0; i < obj->GetStateCount(); i++)
	{
		auto state = obj->GetStates() + i;
		if (state->NextState && (state->UseFlags & state->NextState->UseFlags) != state->UseFlags)
		{
			GetStateSource(state).Message(MSG_ERROR, TEXTCOLOR_RED "State %s links to a state with incompatible restrictions.\n",
				FState::StaticGetStateName(state, obj).GetChars());
		}
	}
}

void CheckDropItems(const PClassActor *const obj)
{
	const FDropItem *dropItem = obj->ActorInfo()->DropItems;

	while (dropItem != nullptr)
	{
		if (dropItem->Name != NAME_None)
		{
			const char *const dropItemName = dropItem->Name.GetChars();

			if (dropItemName[0] != '\0' && PClass::FindClass(dropItem->Name) == nullptr)
			{
				Printf(TEXTCOLOR_ORANGE "Undefined drop item class %s referenced from actor %s\n", dropItemName, obj->TypeName.GetChars());
				FScriptPosition::WarnCounter++;
			}
		}

		dropItem = dropItem->Next;
	}
}

//==========================================================================
//
// LoadActors
//
// Called from FActor::StaticInit()
//
//==========================================================================
void ParseScripts();
void ParseAllDecorate();
void SynthesizeFlagFields();
void SetDoomCompileEnvironment();

void ParseScripts()
{
	int lump, lastlump = 0;
	FScriptPosition::ResetErrorCounter();

	while ((lump = fileSystem.FindLump("ZSCRIPT", &lastlump)) != -1)
	{
		ZCCParseState state;
		auto newns = ParseOneScript(lump, state);
		PSymbolTable symtable;

		ZCCDoomCompiler cc(state, NULL, symtable, newns, lump, state.ParseVersion);
		cc.Compile();

		if (FScriptPosition::ErrorCounter > 0)
		{
			// Abort if the compiler produced any errors. Also do not compile further lumps, because they very likely miss some stuff.
			I_Error("%d errors, %d warnings while compiling %s", FScriptPosition::ErrorCounter, FScriptPosition::WarnCounter, fileSystem.GetFileFullPath(lump).c_str());
		}
		else if (FScriptPosition::WarnCounter > 0)
		{
			// If we got warnings, but no errors, print the information but continue.
			Printf(TEXTCOLOR_ORANGE "%d warnings while compiling %s\n", FScriptPosition::WarnCounter, fileSystem.GetFileFullPath(lump).c_str());
		}

	}
}

void LoadActors()
{
	cycle_t timer;

	timer.Reset(); timer.Clock();
	FScriptPosition::ResetErrorCounter();

	SetDoomCompileEnvironment();
	InitThingdef();
	FScriptPosition::StrictErrors = true;
	ParseScripts();

	FScriptPosition::StrictErrors = strictdecorate;
	ParseAllDecorate();
	SynthesizeFlagFields();

	FunctionBuildList.Build();

	if (FScriptPosition::ErrorCounter > 0)
	{
		if (FScriptPosition::WarnCounter > 0)
		{
			I_Error("%d errors, %d warnings while parsing scripts", FScriptPosition::ErrorCounter, FScriptPosition::WarnCounter);
		}
		else
		{
			I_Error("%d errors while parsing scripts", FScriptPosition::ErrorCounter);
		}
	}
	else if (FScriptPosition::WarnCounter > 0)
	{
		if(warningstoerrors)
		{
			I_Error("%d warnings while parsing scripts\n", FScriptPosition::WarnCounter);
		}
		else
		{
			Printf(TEXTCOLOR_ORANGE "%d warnings while parsing scripts\n", FScriptPosition::WarnCounter);
		}
	}

	FScriptPosition::ResetErrorCounter();
	// AllActorClasses hasn'T been set up yet.
	for (int i = PClass::AllClasses.Size() - 1; i >= 0; i--)
	{
		auto ti = (PClassActor*)PClass::AllClasses[i];
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;
		if (ti->Size == TentativeClass)
		{
			if (ti->bOptional)
			{
				Printf(TEXTCOLOR_ORANGE "Class %s referenced but not defined\n", ti->TypeName.GetChars());
				FScriptPosition::WarnCounter++;
				// the class must be rendered harmless so that it won't cause problems.
				ti->ParentClass = RUNTIME_CLASS(AActor);
				ti->Size = sizeof(AActor);
			}
			else
			{
				Printf(TEXTCOLOR_RED "Class %s referenced but not defined\n", ti->TypeName.GetChars());
				FScriptPosition::ErrorCounter++;
			}
			continue;
		}

		if (GetDefaultByType(ti) == nullptr)
		{
			Printf(TEXTCOLOR_RED "No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			FScriptPosition::ErrorCounter++;
			continue;
		}


		CheckStates(ti);

		if (ti->bDecorateClass && ti->IsDescendantOf(NAME_StateProvider))
		{
			// either a DECORATE based weapon or CustomInventory. 
			// These are subject to relaxed rules for user variables in states.
			// Although there is a runtime check for bogus states, let's do a quick analysis if any of the known entry points
			// hits an unsafe state. If we can find something here it can be handled wuth a compile error rather than a runtime error.
			CheckForUnsafeStates(ti);
		}

		// ensure that all actor bouncers have PASSMOBJ set.
		auto defaults = GetDefaultByType(ti);
		if (defaults->BounceFlags & (BOUNCE_Actors | BOUNCE_AllActors))
		{
			// PASSMOBJ is irrelevant for normal missiles, but not for bouncers.
			defaults->flags2 |= MF2_PASSMOBJ;
		}

		CheckDropItems(ti);
	}
	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors during actor postprocessing", FScriptPosition::ErrorCounter);
	}

	timer.Unclock();
	if (!batchrun) Printf("script parsing took %.2f ms\n", timer.TimeMS());

	// Now we may call the scripted OnDestroy method.
	PClass::bVMOperational = true;
	StateSourceLines.Clear();
}
