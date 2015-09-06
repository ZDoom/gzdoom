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
#include "d_net.h"
#include "v_text.h"

#include "gi.h"

#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"
#include "templates.h"
#include "cmdlib.h"
#include "g_level.h"

extern void LoadActors ();
extern void InitBotStuff();
extern void ClearStrifeTypes();

FRandom FState::pr_statetics("StateTics");

//==========================================================================
//
//
//==========================================================================

int GetSpriteIndex(const char * spritename, bool add)
{
	static char lastsprite[5];
	static int lastindex;

	// Make sure that the string is upper case and 4 characters long
	char upper[5]={0,0,0,0,0};
	for (int i = 0; spritename[i] != 0 && i < 4; i++)
	{
		upper[i] = toupper (spritename[i]);
	}

	// cache the name so if the next one is the same the function doesn't have to perform a search.
	if (!strcmp(upper, lastsprite))
	{
		return lastindex;
	}
	strcpy(lastsprite, upper);

	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strcmp (sprites[i].name, upper) == 0)
		{
			return (lastindex = (int)i);
		}
	}
	if (!add)
	{
		return (lastindex = -1);
	}
	spritedef_t temp;
	strcpy (temp.name, upper);
	temp.numframes = 0;
	temp.spriteframes = 0;
	return (lastindex = (int)sprites.Push (temp));
}


//==========================================================================
//
//
//==========================================================================

void FActorInfo::StaticInit ()
{
	sprites.Clear();
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

		// Sprite 2 is always ####
		memcpy (temp.name, "####", 5);
		sprites.Push (temp);
	}

	Printf ("LoadActors: Load actor definitions.\n");
	ClearStrifeTypes();
	LoadActors ();
	InitBotStuff();
}

//==========================================================================
//
// Called after Dehacked patches are applied
//
//==========================================================================

void FActorInfo::StaticSetActorNums ()
{
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
	const PClass *cls = PClass::FindClass(Class->TypeName);

	// Conversation IDs have never been filtered by game so we cannot start doing that.
	if (ConversationID > 0)
	{
		StrifeTypes[ConversationID] = cls;
		if (cls != Class) 
		{
			Printf(TEXTCOLOR_RED"Conversation ID %d refers to hidden class type '%s'\n", SpawnID, cls->TypeName.GetChars());
		}
	}
	if (GameFilter == GAME_Any || (GameFilter & gameinfo.gametype))
	{
		if (SpawnID > 0)
		{
			SpawnableThings[SpawnID] = cls;
			if (cls != Class) 
			{
				Printf(TEXTCOLOR_RED"Spawn ID %d refers to hidden class type '%s'\n", SpawnID, cls->TypeName.GetChars());
			}
		}
		if (DoomEdNum != -1)
		{
			FDoomEdEntry *oldent = DoomEdMap.CheckKey(DoomEdNum);
			if (oldent != NULL && oldent->Special == -2)
			{
				Printf(TEXTCOLOR_RED"Editor number %d defined twice for classes '%s' and '%s'\n", DoomEdNum, cls->TypeName.GetChars(), oldent->Type->TypeName.GetChars());
			}
			FDoomEdEntry ent;
			memset(&ent, 0, sizeof(ent));
			ent.Type = cls;
			ent.Special = -2;	// use -2 instead of -1 so that we can recognize DECORATE defined entries and print a warning message if duplicates occur.
			DoomEdMap.Insert(DoomEdNum, ent);
			if (cls != Class) 
			{
				Printf(TEXTCOLOR_RED"Editor number %d refers to hidden class type '%s'\n", DoomEdNum, cls->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacement (bool lookskill)
{
	FName skillrepname;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacement(this->Class->TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
		{
			Printf("Warning: incorrect actor name in definition of skill %s: \n"
				   "class %s is replaced by non-existent class %s\n"
				   "Skill replacement will be ignored for this actor.\n", 
				   AllSkills[gameskill].Name.GetChars(), 
				   this->Class->TypeName.GetChars(), skillrepname.GetChars());
			AllSkills[gameskill].SetReplacement(this->Class->TypeName, NAME_None);
			AllSkills[gameskill].SetReplacedBy(skillrepname, NAME_None);
			lookskill = false; skillrepname = NAME_None;
		}
	}
	if (Replacement == NULL && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacement field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacement;
	Replacement = NULL;
	FActorInfo *rep = savedrep;
	// Handle skill-based replacement here. It has precedence on DECORATE replacement
	// in that the skill replacement is applied first, followed by DECORATE replacement
	// on the actor indicated by the skill replacement.
	if (lookskill && (skillrepname != NAME_None))
	{
		rep = PClass::FindClass(skillrepname)->ActorInfo;
	}
	// Now handle DECORATE replacement chain
	// Skill replacements are not recursive, contrarily to DECORATE replacements
	rep = rep->GetReplacement(false);
	// Reset the temporarily NULLed field
	Replacement = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacee (bool lookskill)
{
	FName skillrepname;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacedBy(this->Class->TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
		{
			Printf("Warning: incorrect actor name in definition of skill %s: \n"
				   "non-existent class %s is replaced by class %s\n"
				   "Skill replacement will be ignored for this actor.\n", 
				   AllSkills[gameskill].Name.GetChars(), 
				   skillrepname.GetChars(), this->Class->TypeName.GetChars());
			AllSkills[gameskill].SetReplacedBy(this->Class->TypeName, NAME_None);
			AllSkills[gameskill].SetReplacement(skillrepname, NAME_None);
			lookskill = false; 
		}
	}
	if (Replacee == NULL && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacee field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacee;
	Replacee = NULL;
	FActorInfo *rep = savedrep;
	if (lookskill && (skillrepname != NAME_None) && (PClass::FindClass(skillrepname) != NULL))
	{
		rep = PClass::FindClass(skillrepname)->ActorInfo;
	}
	rep = rep->GetReplacee (false);	Replacee = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetDamageFactor(FName type, fixed_t factor)
{
	if (DamageFactors == NULL)
	{
		DamageFactors = new DmgFactors;
	}
	DamageFactors->Insert(type, factor);
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
		PainChances->Insert(type, MIN(chance, 256));
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

void FActorInfo::SetPainFlash(FName type, PalEntry color)
{
	if (PainFlashes == NULL)
		PainFlashes = new PainFlashList;

	PainFlashes->Insert(type, color);
}

//==========================================================================
//
//
//==========================================================================

bool FActorInfo::GetPainFlash(FName type, PalEntry *color) const
{
	const FActorInfo *info = this;

	while (info != NULL)
	{
		if (info->PainFlashes != NULL)
		{
			PalEntry *flash = info->PainFlashes->CheckKey(type);
			if (flash != NULL)
			{
				*color = *flash;
				return true;
			}
		}
		// Try parent class
		info = info->Class->ParentClass->ActorInfo;
	}
	return false;
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetColorSet(int index, const FPlayerColorSet *set)
{
	if (set != NULL) 
	{
		if (ColorSets == NULL) ColorSets = new FPlayerColorSetMap;
		ColorSets->Insert(index, *set);
	}
	else 
	{
		if (ColorSets != NULL) 
			ColorSets->Remove(index);
	}
}

//==========================================================================
//
// DmgFactors :: CheckFactor
//
// Checks for the existance of a certain damage type. If that type does not
// exist, the damage factor for type 'None' will be returned, if present.
//
//==========================================================================

fixed_t *DmgFactors::CheckFactor(FName type)
{
	fixed_t *pdf = CheckKey(type);
	if (pdf == NULL && type != NAME_None)
	{
		pdf = CheckKey(NAME_None);
	}
	return pdf;
}

static void SummonActor (int command, int command2, FCommandLine argv)
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
		Net_WriteByte (argv.argc() > 2 ? command2 : command);
		Net_WriteString (type->TypeName.GetChars());

		if (argv.argc () > 2) {
			Net_WriteWord (atoi (argv[2])); // angle
			if (argv.argc () > 3) Net_WriteWord (atoi (argv[3])); // TID
			else Net_WriteWord (0);
			if (argv.argc () > 4) Net_WriteByte (atoi (argv[4])); // special
			else Net_WriteByte (0);
			for(int i = 5; i < 10; i++) { // args[5]
				if(i < argv.argc()) Net_WriteLong (atoi (argv[i]));
				else Net_WriteLong (0);
			}
		}
	}
}

CCMD (summon)
{
	SummonActor (DEM_SUMMON, DEM_SUMMON2, argv);
}

CCMD (summonfriend)
{
	SummonActor (DEM_SUMMONFRIEND, DEM_SUMMONFRIEND2, argv);
}

CCMD (summonmbf)
{
	SummonActor (DEM_SUMMONMBF, DEM_SUMMONFRIEND2, argv);
}

CCMD (summonfoe)
{
	SummonActor (DEM_SUMMONFOE, DEM_SUMMONFOE2, argv);
}


// Damage type defaults / global settings

TMap<FName, DamageTypeDefinition> GlobalDamageDefinitions;

void DamageTypeDefinition::Apply(FName type) 
{ 
	GlobalDamageDefinitions[type] = *this; 
}

DamageTypeDefinition *DamageTypeDefinition::Get(FName type) 
{ 
	return GlobalDamageDefinitions.CheckKey(type); 
}

bool DamageTypeDefinition::IgnoreArmor(FName type)
{ 
	DamageTypeDefinition *dtd = Get(type);
	if (dtd) return dtd->NoArmor;
	return false;
}

//==========================================================================
//
// DamageTypeDefinition :: ApplyMobjDamageFactor
//
// Calculates mobj damage based on original damage, defined damage factors
// and damage type.
//
// If the specific damage type is not defined, the damage factor for
// type 'None' will be used (with 1.0 as a default value).
//
// Globally declared damage types may override or multiply the damage
// factor when 'None' is used as a fallback in this function.
//
//==========================================================================

int DamageTypeDefinition::ApplyMobjDamageFactor(int damage, FName type, DmgFactors const * const factors)
{
	if (factors)
	{
		// If the actor has named damage factors, look for a specific factor
		fixed_t const *pdf = factors->CheckKey(type);
		if (pdf) return FixedMul(damage, *pdf); // type specific damage type
		
		// If this was nonspecific damage, don't fall back to nonspecific search
		if (type == NAME_None) return damage;
	}
	
	// If this was nonspecific damage, don't fall back to nonspecific search
	else if (type == NAME_None) 
	{ 
		return damage; 
	}
	else
	{
		// Normal is unsupplied / 1.0, so there's no difference between modifying and overriding
		DamageTypeDefinition *dtd = Get(type);
		return dtd ? FixedMul(damage, dtd->DefaultFactor) : damage;
	}
	
	{
		fixed_t const *pdf  = factors->CheckKey(NAME_None);
		DamageTypeDefinition *dtd = Get(type);
		// Here we are looking for modifications to untyped damage
		// If the calling actor defines untyped damage factor, that is contained in "pdf".
		if (pdf) // normal damage available
		{
			if (dtd)
			{
				if (dtd->ReplaceFactor) return FixedMul(damage, dtd->DefaultFactor); // use default instead of untyped factor
				return FixedMul(damage, FixedMul(*pdf, dtd->DefaultFactor)); // use default as modification of untyped factor
			}
			return FixedMul(damage, *pdf); // there was no default, so actor default is used
		}
		else if (dtd)
		{
			return FixedMul(damage, dtd->DefaultFactor); // implicit untyped factor 1.0 does not need to be applied/replaced explicitly
		}
	}
	return damage;
}
