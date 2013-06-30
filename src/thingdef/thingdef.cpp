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
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "p_local.h"
#include "doomerrors.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "thingdef_exp.h"
#include "a_sharedglobal.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void InitThingdef();
void ParseDecorate (FScanner &sc);

// STATIC FUNCTION PROTOTYPES --------------------------------------------
const PClass *QuestItemClasses[31];
PSymbolTable		 GlobalSymbols;

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
FActorInfo *CreateNewActor(const FScriptPosition &sc, FName typeName, FName parentName, bool native)
{
	const PClass *replacee = NULL;
	PClass *ti = NULL;
	FActorInfo *info = NULL;

	PClass *parent = RUNTIME_CLASS(AActor);

	if (parentName != NAME_None)
	{
		parent = const_cast<PClass *> (PClass::FindClass (parentName));
		
		const PClass *p = parent;
		while (p != NULL)
		{
			if (p->TypeName == typeName)
			{
				sc.Message(MSG_ERROR, "'%s' inherits from a class with the same name", typeName.GetChars());
				break;
			}
			p = p->ParentClass;
		}

		if (parent == NULL)
		{
			sc.Message(MSG_ERROR, "Parent type '%s' not found in %s", parentName.GetChars(), typeName.GetChars());
			parent = RUNTIME_CLASS(AActor);
		}
		else if (!parent->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			sc.Message(MSG_ERROR, "Parent type '%s' is not an actor in %s", parentName.GetChars(), typeName.GetChars());
			parent = RUNTIME_CLASS(AActor);
		}
		else if (parent->ActorInfo == NULL)
		{
			sc.Message(MSG_ERROR, "uninitialized parent type '%s' in %s", parentName.GetChars(), typeName.GetChars());
			parent = RUNTIME_CLASS(AActor);
		}
	}

	if (native)
	{
		ti = (PClass*)PClass::FindClass(typeName);
		if (ti == NULL)
		{
			sc.Message(MSG_ERROR, "Unknown native class '%s'", typeName.GetChars());
			goto create;
		}
		else if (ti != RUNTIME_CLASS(AActor) && ti->ParentClass->NativeClass() != parent->NativeClass())
		{
			sc.Message(MSG_ERROR, "Native class '%s' does not inherit from '%s'", typeName.GetChars(), parentName.GetChars());
			parent = RUNTIME_CLASS(AActor);
			goto create;
		}
		else if (ti->ActorInfo != NULL)
		{
			sc.Message(MSG_ERROR, "Redefinition of internal class '%s'", typeName.GetChars());
			goto create;
		}
		ti->InitializeActorInfo();
		info = ti->ActorInfo;
	}
	else
	{
	create:
		ti = parent->CreateDerivedClass (typeName, parent->Size);
		info = ti->ActorInfo;
	}

	// Copy class lists from parent
	info->ForbiddenToPlayerClass = parent->ActorInfo->ForbiddenToPlayerClass;
	info->RestrictedToPlayerClass = parent->ActorInfo->RestrictedToPlayerClass;
	info->VisibleToPlayerClass = parent->ActorInfo->VisibleToPlayerClass;

	if (parent->ActorInfo->DamageFactors != NULL)
	{
		// copy damage factors from parent
		info->DamageFactors = new DmgFactors;
		*info->DamageFactors = *parent->ActorInfo->DamageFactors;
	}
	if (parent->ActorInfo->PainChances != NULL)
	{
		// copy pain chances from parent
		info->PainChances = new PainChanceList;
		*info->PainChances = *parent->ActorInfo->PainChances;
	}
	if (parent->ActorInfo->ColorSets != NULL)
	{
		// copy color sets from parent
		info->ColorSets = new FPlayerColorSetMap;
		*info->ColorSets = *parent->ActorInfo->ColorSets;
	}
	info->Replacee = info->Replacement = NULL;
	info->DoomEdNum = -1;
	return info;
}

//==========================================================================
//
// 
//
//==========================================================================

void SetReplacement(FScanner &sc, FActorInfo *info, FName replaceName)
{
	// Check for "replaces"
	if (replaceName != NAME_None)
	{
		// Get actor name
		const PClass *replacee = PClass::FindClass (replaceName);

		if (replacee == NULL)
		{
			sc.ScriptMessage("Replaced type '%s' not found for %s", replaceName.GetChars(), info->Class->TypeName.GetChars());
			return;
		}
		else if (replacee->ActorInfo == NULL)
		{
			sc.ScriptMessage("Replaced type '%s' for %s is not an actor", replaceName.GetChars(), info->Class->TypeName.GetChars());
			return;
		}
		if (replacee != NULL)
		{
			replacee->ActorInfo->Replacement = info;
			info->Replacee = replacee->ActorInfo;
		}
	}

}

//==========================================================================
//
// Finalizes an actor definition
//
//==========================================================================

void FinishActor(const FScriptPosition &sc, FActorInfo *info, Baggage &bag)
{
	PClass *ti = info->Class;
	AActor *defaults = (AActor*)ti->Defaults;

	try
	{
		bag.statedef.FinishStates (info, defaults);
	}
	catch (CRecoverableError &err)
	{
		sc.Message(MSG_ERROR, "%s", err.GetMessage());
		bag.statedef.MakeStateDefines(NULL);
		return;
	}
	bag.statedef.InstallStates (info, defaults);
	bag.statedef.MakeStateDefines(NULL);
	if (bag.DropItemSet)
	{
		if (bag.DropItemList == NULL)
		{
			if (ti->Meta.GetMetaInt (ACMETA_DropItems) != 0)
			{
				ti->Meta.SetMetaInt (ACMETA_DropItems, 0);
			}
		}
		else
		{
			ti->Meta.SetMetaInt (ACMETA_DropItems,
				StoreDropItemChain(bag.DropItemList));
		}
	}
	if (ti->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		defaults->flags |= MF_SPECIAL;
	}

	// Weapons must be checked for all relevant states. They may crash the game otherwise.
	if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
	{
		FState * ready = ti->ActorInfo->FindState(NAME_Ready);
		FState * select = ti->ActorInfo->FindState(NAME_Select);
		FState * deselect = ti->ActorInfo->FindState(NAME_Deselect);
		FState * fire = ti->ActorInfo->FindState(NAME_Fire);

		// Consider any weapon without any valid state abstract and don't output a warning
		// This is for creating base classes for weapon groups that only set up some properties.
		if (ready || select || deselect || fire)
		{
			if (!ready)
			{
				sc.Message(MSG_ERROR, "Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
			}
			if (!select) 
			{
				sc.Message(MSG_ERROR, "Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
			}
			if (!deselect) 
			{
				sc.Message(MSG_ERROR, "Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
			}
			if (!fire) 
			{
				sc.Message(MSG_ERROR, "Weapon %s doesn't define a fire state.\n", ti->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
// Do some postprocessing after everything has been defined
//
//==========================================================================

static void FinishThingdef()
{
	int errorcount = StateParams.ResolveAll();

	for (unsigned i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		if (ti->Size == (unsigned)-1)
		{
			Printf("Class %s referenced but not defined\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		AActor *def = GetDefaultByType(ti);

		if (!def)
		{
			Printf("No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}
	}
	if (errorcount > 0)
	{
		I_Error("%d errors during actor postprocessing", errorcount);
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		mysnprintf(fmt, countof(fmt), "QuestItem%d", i+1);
		QuestItemClasses[i] = PClass::FindClass(fmt);
	}
}



//==========================================================================
//
// LoadActors
//
// Called from FActor::StaticInit()
//
//==========================================================================

void LoadActors ()
{
	int lastlump, lump;

	StateParams.Clear();
	GlobalSymbols.ReleaseSymbols();
	DropItemList.Clear();
	FScriptPosition::ResetErrorCounter();
	InitThingdef();
	lastlump = 0;
	while ((lump = Wads.FindLump ("DECORATE", &lastlump)) != -1)
	{
		FScanner sc(lump);
		ParseDecorate (sc);
	}
	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors while parsing DECORATE scripts", FScriptPosition::ErrorCounter);
	}
	FinishThingdef();
}

