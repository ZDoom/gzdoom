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
#include "vm.h"
#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"
#include "templates.h"
#include "cmdlib.h"
#include "g_level.h"

extern void LoadActors ();

bool FState::CallAction(AActor *self, AActor *stateowner, StateCallData *statecall)
{
	if (ActionFunc != NULL)
	{
		//ActionFunc(self, stateowner, this, ParameterIndex-1, statecall);
		VMFrameStack stack;
		VMValue params[4] = { self, stateowner, VMValue(this, ATAG_STATE), statecall };
		stack.Call(ActionFunc, params, countof(params), NULL, 0, NULL);
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
//
//==========================================================================

int GetSpriteIndex(const char * spritename)
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
	spritedef_t temp;
	strcpy (temp.name, upper);
	temp.numframes = 0;
	temp.spriteframes = 0;
	return (lastindex = (int)sprites.Push (temp));
}

IMPLEMENT_POINTY_CLASS(PClassActor)
 DECLARE_POINTER(DropItems)
END_POINTERS

//==========================================================================
//
// PClassActor :: StaticInit										STATIC
//
//==========================================================================

void PClassActor::StaticInit()
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

	Printf ("LoadActors: Load actor definitions.\n");
	LoadActors ();
}

//==========================================================================
//
// PClassActor :: StaticSetActorNums								STATIC
//
// Called after Dehacked patches are applied
//
//==========================================================================

void PClassActor::StaticSetActorNums()
{
	memset(SpawnableThings, 0, sizeof(SpawnableThings));
	DoomEdMap.Empty();

	for (unsigned int i = 0; i < PClass::m_RuntimeActors.Size(); ++i)
	{
		static_cast<PClassActor *>(PClass::m_RuntimeActors[i])->RegisterIDs();
	}
}

//==========================================================================
//
// PClassActor Constructor
//
//==========================================================================

PClassActor::PClassActor()
{
	GameFilter = GAME_Any;
	SpawnID = 0;
	DoomEdNum = -1;
	OwnedStates = NULL;
	NumOwnedStates = 0;
	Replacement = NULL;
	Replacee = NULL;
	StateList = NULL;
	DamageFactors = NULL;
	PainChances = NULL;

	DeathHeight = -1;
	BurnHeight = -1;
	GibHealth = INT_MIN;
	WoundHealth = 6;
	PoisonDamage = 0;
	FastSpeed = FIXED_MIN;
	RDFactor = FRACUNIT;
	CameraHeight = FIXED_MIN;
	BloodType = NAME_Blood;
	BloodType2 = NAME_BloodSplatter;
	BloodType3 = NAME_AxeBlood;

	DropItems = NULL;

	DontHurtShooter = false;
	ExplosionDamage = 128;
	ExplosionRadius = -1;
	MissileHeight = 32*FRACUNIT;
	MeleeDamage = 0;
}

//==========================================================================
//
// PClassActor Destructor
//
//==========================================================================

PClassActor::~PClassActor()
{
	if (OwnedStates != NULL)
	{
		delete[] OwnedStates;
	}
	if (DamageFactors != NULL)
	{
		delete DamageFactors;
	}
	if (PainChances != NULL)
	{
		delete PainChances;
	}
	if (StateList != NULL)
	{
		StateList->Destroy();
		M_Free(StateList);
	}
}

//==========================================================================
//
// PClassActor :: Derive
//
//==========================================================================

void PClassActor::Derive(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassActor)));
	Super::Derive(newclass);
	PClassActor *newa = static_cast<PClassActor *>(newclass);

	newa->Obituary = Obituary;
	newa->HitObituary = HitObituary;
	newa->DeathHeight = DeathHeight;
	newa->BurnHeight = BurnHeight;
	newa->BloodColor = BloodColor;
	newa->GibHealth = GibHealth;
	newa->WoundHealth = WoundHealth;
	newa->PoisonDamage = PoisonDamage;
	newa->FastSpeed = FastSpeed;
	newa->RDFactor = RDFactor;
	newa->CameraHeight = CameraHeight;
	newa->HowlSound = HowlSound;
	newa->BloodType = BloodType;
	newa->BloodType2 = BloodType2;
	newa->BloodType3 = BloodType3;

	newa->DropItems = DropItems;

	newa->DontHurtShooter = DontHurtShooter;
	newa->ExplosionRadius = ExplosionRadius;
	newa->ExplosionDamage = ExplosionDamage;
	newa->MeleeDamage = MeleeDamage;
	newa->MeleeSound = MeleeSound;
	newa->MissileName = MissileName;
	newa->MissileHeight = MissileHeight;
}

//==========================================================================
//
// PClassActor :: PropagateMark
//
//==========================================================================

size_t PClassActor::PropagateMark()
{
	// Mark state functions
	for (int i = 0; i < NumOwnedStates; ++i)
	{
		if (OwnedStates[i].ActionFunc != NULL)
		{
			GC::Mark(OwnedStates[i].ActionFunc);
		}
	}
//	marked += ActorInfo->NumOwnedStates * sizeof(FState);
	return Super::PropagateMark();
}

//==========================================================================
//
// PClassActor :: InitializeNativeDefaults
//
// This is used by DECORATE to assign ActorInfos to internal classes
//
//==========================================================================

void PClassActor::InitializeNativeDefaults()
{
	Symbols.SetParentTable(&ParentClass->Symbols);
	assert(Defaults == NULL);
	Defaults = new BYTE[Size];
	if (ParentClass->Defaults != NULL) 
	{
		memcpy(Defaults, ParentClass->Defaults, ParentClass->Size);
		if (Size > ParentClass->Size)
		{
			memset(Defaults + ParentClass->Size, 0, Size - ParentClass->Size);
		}
	}
	else
	{
		memset (Defaults, 0, Size);
	}
	m_RuntimeActors.Push(this);
}

//==========================================================================
//
// PClassActor :: RegisterIDs
//
// Registers this class's SpawnID and DoomEdNum in the appropriate tables.
//
//==========================================================================

void PClassActor::RegisterIDs()
{
	PClassActor *cls = PClass::FindActor(TypeName);
	bool set = false;

	if (cls == NULL)
	{
		Printf(TEXTCOLOR_RED"The actor '%s' has been hidden by a non-actor of the same name\n", TypeName.GetChars());
		return;
	}

	if (GameFilter == GAME_Any || (GameFilter & gameinfo.gametype))
	{
		if (SpawnID != 0)
		{
			SpawnableThings[SpawnID] = cls;
			if (cls != this) 
			{
				Printf(TEXTCOLOR_RED"Spawn ID %d refers to hidden class type '%s'\n", SpawnID, cls->TypeName.GetChars());
			}
		}
		if (DoomEdNum != -1)
		{
			DoomEdMap.AddType(DoomEdNum, cls);
			if (cls != this) 
			{
				Printf(TEXTCOLOR_RED"Editor number %d refers to hidden class type '%s'\n", DoomEdNum, cls->TypeName.GetChars());
			}
		}
	}
	// Fill out the list for Chex Quest with Doom's actors
	if (gameinfo.gametype == GAME_Chex && DoomEdMap.FindType(DoomEdNum) == NULL && (GameFilter & GAME_Doom))
	{
		DoomEdMap.AddType(DoomEdNum, this, true);
		if (cls != this) 
		{
			Printf(TEXTCOLOR_RED"Editor number %d refers to hidden class type '%s'\n", DoomEdNum, cls->TypeName.GetChars());
		}
	}
}

//==========================================================================
//
// PClassActor :: GetReplacement
//
//==========================================================================

PClassActor *PClassActor::GetReplacement(bool lookskill)
{
	FName skillrepname;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacement(TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
		{
			Printf("Warning: incorrect actor name in definition of skill %s: \n"
				   "class %s is replaced by non-existent class %s\n"
				   "Skill replacement will be ignored for this actor.\n", 
				   AllSkills[gameskill].Name.GetChars(), 
				   TypeName.GetChars(), skillrepname.GetChars());
			AllSkills[gameskill].SetReplacement(TypeName, NAME_None);
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
	PClassActor *savedrep = Replacement;
	Replacement = NULL;
	PClassActor *rep = savedrep;
	// Handle skill-based replacement here. It has precedence on DECORATE replacement
	// in that the skill replacement is applied first, followed by DECORATE replacement
	// on the actor indicated by the skill replacement.
	if (lookskill && (skillrepname != NAME_None))
	{
		rep = PClass::FindActor(skillrepname);
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
// PClassActor :: GetReplacee
//
//==========================================================================

PClassActor *PClassActor::GetReplacee(bool lookskill)
{
	FName skillrepname;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacedBy(TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
		{
			Printf("Warning: incorrect actor name in definition of skill %s: \n"
				   "non-existent class %s is replaced by class %s\n"
				   "Skill replacement will be ignored for this actor.\n", 
				   AllSkills[gameskill].Name.GetChars(), 
				   skillrepname.GetChars(), TypeName.GetChars());
			AllSkills[gameskill].SetReplacedBy(TypeName, NAME_None);
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
	PClassActor *savedrep = Replacee;
	Replacee = NULL;
	PClassActor *rep = savedrep;
	if (lookskill && (skillrepname != NAME_None) && (PClass::FindClass(skillrepname) != NULL))
	{
		rep = PClass::FindActor(skillrepname);
	}
	rep = rep->GetReplacee(false);
	Replacee = savedrep;
	return rep;
}

//==========================================================================
//
// PClassActor :: SetDamageFactor
//
//==========================================================================

void PClassActor::SetDamageFactor(FName type, fixed_t factor)
{
	if (DamageFactors == NULL)
	{
		DamageFactors = new DmgFactors;
	}
	DamageFactors->Insert(type, factor);
}

//==========================================================================
//
// PClassActor :: SetPainChance
//
//==========================================================================

void PClassActor::SetPainChance(FName type, int chance)
{
	if (chance >= 0) 
	{
		if (PainChances == NULL)
		{
			PainChances = new PainChanceList;
		}
		PainChances->Insert(type, MIN(chance, 256));
	}
	else if (PainChances != NULL)
	{
		PainChances->Remove(type);
	}
}

//==========================================================================
//
//
//==========================================================================

void PClassActor::SetColorSet(int index, const FPlayerColorSet *set)
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
//
//==========================================================================

FDoomEdMap DoomEdMap;

FDoomEdMap::FDoomEdEntry *FDoomEdMap::DoomEdHash[DOOMED_HASHSIZE];

FDoomEdMap::~FDoomEdMap()
{
	Empty();
}

void FDoomEdMap::AddType (int doomednum, PClassActor *type, bool temporary)
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
	else if (!entry->temp)
	{
		Printf (PRINT_BOLD, "Warning: %s and %s both have doomednum %d.\n",
			type->TypeName.GetChars(), entry->Type->TypeName.GetChars(), doomednum);
	}
	entry->temp = temporary;
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

PClassActor *FDoomEdMap::FindType (int doomednum) const
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