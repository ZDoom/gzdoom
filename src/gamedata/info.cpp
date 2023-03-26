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


#include "doomstat.h"
#include "info.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "v_text.h"

#include "gi.h"
#include "actor.h"
#include "r_state.h"
#include "p_local.h"
#include "stats.h"
#include "thingdef.h"
#include "d_player.h"
#include "events.h"
#include "types.h"
#include "filesystem.h"
#include "g_levellocals.h"
#include "texturemanager.h"
#include "d_main.h"
#include "maps.h"

extern void LoadActors ();
extern void InitBotStuff();
extern void ClearStrifeTypes();

TArray<PClassActor *> PClassActor::AllActorClasses;
FRandom FState::pr_statetics("StateTics");

cycle_t ActionCycles;

void InitServices();

//==========================================================================
//
// special type for the native ActorInfo. This allows to let this struct
// be handled by the generic object constructors for the VM.
//
//==========================================================================

class PActorInfo : public PCompoundType
{
public:
	PActorInfo()
		:PCompoundType(sizeof(FActorInfo), alignof(FActorInfo))
	{
	}

	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) override
	{
		if (base != nullptr) new((uint8_t *)base + offset) FActorInfo;
		if (special != nullptr)
		{
			special->Push(std::make_pair(this, offset));
		}
	}

	void InitializeValue(void *addr, const void *def) const override
	{
		if (def == nullptr)
		{
			new(addr) FActorInfo;
		}
		else
		{
			new(addr) FActorInfo(*(const FActorInfo*)def);
		}
	}

	void DestroyValue(void *addr) const override
	{
		FActorInfo *self = (FActorInfo*)addr;
		self->~FActorInfo();
	}

};

void AddActorInfo(PClass *cls)
{
	auto type = new PActorInfo;
	TypeTable.AddType(type, NAME_Actor);
	cls->AddField("*", type, VARF_Meta);
}


void FState::SetAction(const char *name)
{
	ActionFunc = FindVMFunction(RUNTIME_CLASS(AActor), name);
}


void FState::CheckCallerType(AActor *self, AActor *stateowner)
{
	auto CheckType = [=](AActor *check, PType *requiredType)
	{
		// This should really never happen. Any valid action function must have actor pointers here.
		if (!requiredType->isObjectPointer())
		{
			ThrowAbortException(X_OTHER, "Bad function prototype in function call to %s", ActionFunc->PrintableName.GetChars());
		}
		auto cls = static_cast<PObjectPointer*>(requiredType)->PointedClass();
		if (check == nullptr)
		{
			ThrowAbortException(X_OTHER, "%s called without valid caller. %s expected", ActionFunc->PrintableName.GetChars(), cls->TypeName.GetChars());
		}
		if (!(StateFlags & STF_DEHACKED) && !check->IsKindOf(cls))
		{
			ThrowAbortException(X_OTHER, "Invalid class %s in function call to %s. %s expected", check->GetClass()->TypeName.GetChars(), ActionFunc->PrintableName.GetChars(), cls->TypeName.GetChars());
		}
	};
	
	if (ActionFunc->ImplicitArgs >= 1)
	{
		auto argtypes = ActionFunc->Proto->ArgumentTypes;
		
		CheckType(self, argtypes[0]);
		
		if (ActionFunc->ImplicitArgs >= 2)
		{
			CheckType(stateowner, argtypes[1]);
		}
	}
}

TArray<VMValue> actionParams;

bool FState::CallAction(AActor *self, AActor *stateowner, FStateParamInfo *info, FState **stateret)
{
	if (ActionFunc != nullptr)
	{
		ActionCycles.Clock();

		// If the function returns a state, store it at *stateret.
		// If it doesn't return a state but stateret is non-nullptr, we need
		// to set *stateret to nullptr.
		if (stateret != nullptr)
		{
			*stateret = nullptr;
			if (ActionFunc->Proto == nullptr ||
				ActionFunc->Proto->ReturnTypes.Size() == 0 ||
				ActionFunc->Proto->ReturnTypes[0] != TypeState)
			{
				stateret = nullptr;
			}
		}

		VMReturn ret;
		ret.PointerAt((void **)stateret);
		try
		{
			CheckCallerType(self, stateowner);

			// Build the parameter array. Action functions have never any explicit parameters but need to pass the defaults
			// and fill in the implicit arguments of the called function.

			if (ActionFunc->DefaultArgs.Size() > 0)
			{
				auto defs = ActionFunc->DefaultArgs;
				auto index = actionParams.Reserve(defs.Size());
				for (unsigned i = 0; i < defs.Size(); i++)
				{
					actionParams[i + index] = defs[i];
				}

				if (ActionFunc->ImplicitArgs >= 1)
				{
					actionParams[index] = self;
				}
				if (ActionFunc->ImplicitArgs == 3)
				{
					actionParams[index + 1] = stateowner;
					actionParams[index + 2] = VMValue(info);
				}

				VMCallAction(ActionFunc, &actionParams[index], ActionFunc->DefaultArgs.Size(), &ret, stateret != nullptr);
				actionParams.Clamp(index);
			}
			else
			{
				VMValue params[3] = { self, stateowner, VMValue(info) };
				VMCallAction(ActionFunc, params, ActionFunc->ImplicitArgs, &ret, stateret != nullptr);
			}
		}
		catch (CVMAbortException &err)
		{
			err.MaybePrintMessage();

			if (stateowner != nullptr)
			{
				const char *callinfo = "";
				if (info != nullptr && info->mStateType == STATE_Psprite)
				{
					if (stateowner->IsKindOf(NAME_Weapon) && stateowner != self) callinfo = "weapon ";
					else callinfo = "overlay ";
				}
				err.stacktrace.AppendFormat("Called from %sstate %s in %s\n", callinfo, FState::StaticGetStateName(this).GetChars(), stateowner->GetClass()->TypeName.GetChars());
			}
			else
			{
				err.stacktrace.AppendFormat("Called from state %s\n", FState::StaticGetStateName(this).GetChars());
			}

			throw;
		}

		ActionCycles.Unclock();
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
// Load alt HUD icons. This is meant to be an override of the item's own settings.
//
//==========================================================================

static void LoadAltHudStuff()
{
	// Now read custom icon overrides
	int lump, lastlump = 0;

	switch (gameinfo.gametype)
	{
	case GAME_Heretic:
	case GAME_Hexen:
		gameinfo.healthpic = TexMan.CheckForTexture("ARTIPTN2", ETextureType::MiscPatch).GetIndex();
		gameinfo.berserkpic = -1;
		break;

	case GAME_Strife:
		gameinfo.healthpic = TexMan.CheckForTexture("I_MDKT", ETextureType::MiscPatch).GetIndex();
		gameinfo.berserkpic = -1;
		break;

	default:
		gameinfo.healthpic = TexMan.CheckForTexture("MEDIA0", ETextureType::Sprite).GetIndex();
		gameinfo.berserkpic = TexMan.CheckForTexture("PSTRA0", ETextureType::Sprite).GetIndex();
		break;
	}

	while ((lump = fileSystem.FindLump("ALTHUDCF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			if (sc.Compare("Health"))
			{
				sc.MustGetString();
				FTextureID tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				if (tex.isValid()) gameinfo.healthpic = tex.GetIndex();
			}
			else if (sc.Compare("Berserk"))
			{
				sc.MustGetString();
				FTextureID tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				if (tex.isValid()) gameinfo.berserkpic = tex.GetIndex();
			}
			else
			{
				PClass *ti = PClass::FindClass(sc.String);
				if (!ti)
				{
					Printf("Unknown item class '%s' in ALTHUDCF\n", sc.String);
				}
				else if (!ti->IsDescendantOf(NAME_Inventory))
				{
					Printf("Invalid item class '%s' in ALTHUDCF\n", sc.String);
					ti = NULL;
				}
				sc.MustGetString();
				FTextureID tex;

				if (!sc.Compare("0") && !sc.Compare("NULL") && !sc.Compare(""))
				{
					tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				}
				else tex.SetInvalid();

				if (ti) GetDefaultByType(ti)->TextureIDVar(NAME_AltHUDIcon) = tex;
			}
		}
	}
}

//==========================================================================
//
// PClassActor :: StaticInit										STATIC
//
//==========================================================================
void InitServices();

void PClassActor::StaticInit()
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

	if (!batchrun) Printf ("LoadActors: Load actor definitions.\n");
	ClearStrifeTypes();
	LoadActors ();
	InitServices();


	for (auto cls : AllClasses)
	{
		if (cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			AllActorClasses.Push(static_cast<PClassActor*>(cls));
		}
	}

	LoadAltHudStuff();
	InitBotStuff();

	// reinit GLOBAL static stuff from gameinfo, once classes are loaded.
	staticEventManager.InitStaticHandlers(primaryLevel, false);
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
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor::AllActorClasses[i]->RegisterIDs();
	}
}

//==========================================================================
//
// PClassActor :: SetReplacement
//
// Sets as a replacement class for another class.
//
//==========================================================================

bool PClassActor::SetReplacement(FName replaceName)
{
	// Check for "replaces"
	if (replaceName != NAME_None)
	{
		// Get actor name
		PClassActor *replacee = PClass::FindActor(replaceName);

		if (replacee == nullptr)
		{
			return false;
		}
		if (replacee != nullptr)
		{
			replacee->ActorInfo()->Replacement = this;
			ActorInfo()->Replacee = replacee;
		}
	}
	return true;
}

//==========================================================================
//
// PClassActor :: InitializeNativeDefaults
//
//==========================================================================

void PClassActor::InitializeDefaults()
{
	if (IsDescendantOf(RUNTIME_CLASS(AActor)))
	{
		assert(Defaults == nullptr);
		Defaults = (uint8_t*)M_Malloc(Size);

		ConstructNative(Defaults);
		// We must unlink the defaults from the class list because it's just a static block of data to the engine.
		DObject* optr = (DObject*)Defaults;
		GC::Root = optr->ObjNext;
		optr->ObjNext = nullptr;
		optr->SetClass(this);

		// Copy the defaults from the parent but leave the DObject part alone because it contains important data.
		if (ParentClass->Defaults != nullptr)
		{
			memcpy(Defaults + sizeof(DObject), ParentClass->Defaults + sizeof(DObject), ParentClass->Size - sizeof(DObject));
			if (Size > ParentClass->Size)
			{
				memset(Defaults + ParentClass->Size, 0, Size - ParentClass->Size);
			}
		}
		else
		{
			memset(Defaults + sizeof(DObject), 0, Size - sizeof(DObject));
		}

		assert(MetaSize >= ParentClass->MetaSize);
		if (MetaSize != 0)
		{
			Meta = (uint8_t*)M_Malloc(MetaSize);

			// Copy the defaults from the parent but leave the DObject part alone because it contains important data.
			if (ParentClass->Meta != nullptr)
			{
				memcpy(Meta, ParentClass->Meta, ParentClass->MetaSize);
				if (MetaSize > ParentClass->MetaSize)
				{
					memset(Meta + ParentClass->MetaSize, 0, MetaSize - ParentClass->MetaSize);
				}
			}
			else
			{
				memset(Meta, 0, MetaSize);
			}

			if (MetaSize > 0) memcpy(Meta, ParentClass->Meta, ParentClass->MetaSize);
			else memset(Meta, 0, MetaSize);
		}
	}
	PClass::InitializeDefaults();
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

	if (cls == nullptr)
	{
		Printf(TEXTCOLOR_RED"The actor '%s' has been hidden by a non-actor of the same name\n", TypeName.GetChars());
		return;
	}

	FActorInfo *actorInfo = ActorInfo();

	if (nullptr == actorInfo)
	{
		// Undefined class, exiting
		return;
	}

	// Conversation IDs have never been filtered by game so we cannot start doing that.
	auto ConversationID = actorInfo->ConversationID;
	if (ConversationID > 0)
	{
		StrifeTypes[ConversationID] = cls;
		if (cls != this) 
		{
			Printf(TEXTCOLOR_RED"Conversation ID %d refers to hidden class type '%s'\n", ConversationID, cls->TypeName.GetChars());
		}
	}
	if (actorInfo->GameFilter == GAME_Any || (ActorInfo()->GameFilter & gameinfo.gametype))
	{
		auto SpawnID = actorInfo->SpawnID;
		if (SpawnID > 0)
		{
			SpawnableThings[SpawnID] = cls;
			if (cls != this) 
			{
				Printf(TEXTCOLOR_RED"Spawn ID %d refers to hidden class type '%s'\n", SpawnID, cls->TypeName.GetChars());
			}
		}
		auto DoomEdNum = actorInfo->DoomEdNum;
		if (DoomEdNum != -1)
		{
			FDoomEdEntry *oldent = DoomEdMap.CheckKey(DoomEdNum);
			if (oldent != nullptr && oldent->Special == -2)
			{
				Printf(TEXTCOLOR_RED"Editor number %d defined twice for classes '%s' and '%s'\n", DoomEdNum, cls->TypeName.GetChars(), oldent->Type->TypeName.GetChars());
			}
			FDoomEdEntry ent;
			memset(&ent, 0, sizeof(ent));
			ent.Type = cls;
			ent.Special = -2;	// use -2 instead of -1 so that we can recognize DECORATE defined entries and print a warning message if duplicates occur.
			DoomEdMap.Insert(DoomEdNum, ent);
			if (cls != this) 
			{
				Printf(TEXTCOLOR_RED"Editor number %d refers to hidden class type '%s'\n", DoomEdNum, cls->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
// PClassActor :: GetReplacement
//
//==========================================================================

PClassActor *PClassActor::GetReplacement(FLevelLocals *Level, bool lookskill)
{
	FName skillrepname = NAME_None;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacement(TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == nullptr)
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
	// [MK] ZScript replacement through Event Handlers, has priority over others.
	PClassActor *Replacement = ActorInfo()->Replacement;
	// Level can be null when initializing dynamic lights.
	// Since they only want to check for DehackedPickups this code should be skipped for them.
	if (Level && Level->localEventManager->CheckReplacement(this,&Replacement) )
	{
		// [MK] the replacement is final, so don't continue with the chain
		return Replacement ? Replacement : this;
	}
	if (Replacement == nullptr && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacement field is temporarily NULLed to prevent
	// potential infinite recursion.
	PClassActor *oldrep = ActorInfo()->Replacement;
	ActorInfo()->Replacement = nullptr;
	PClassActor *rep = Replacement;
	// Handle skill-based replacement here. It has precedence on DECORATE replacement
	// in that the skill replacement is applied first, followed by DECORATE replacement
	// on the actor indicated by the skill replacement.
	if (lookskill && (skillrepname != NAME_None))
	{
		rep = PClass::FindActor(skillrepname);
	}
	// Now handle DECORATE replacement chain
	// Skill replacements are not recursive, contrarily to DECORATE replacements
	rep = rep->GetReplacement(Level, false);
	// Reset the temporarily NULLed field
	ActorInfo()->Replacement = oldrep;
	return rep;
}

//==========================================================================
//
// PClassActor :: GetReplacee
//
//==========================================================================

PClassActor *PClassActor::GetReplacee(FLevelLocals *Level, bool lookskill)
{
	FName skillrepname = NAME_None;
	
	if (lookskill && AllSkills.Size() > (unsigned)gameskill)
	{
		skillrepname = AllSkills[gameskill].GetReplacedBy(TypeName);
		if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == nullptr)
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
	PClassActor *savedrep = ActorInfo()->Replacee;
	// [MC] Same code as CheckReplacement but turned around so modders can indicate
	// what monsters spawn from which entity. I.e. instead of a randomspawner
	// showing up, one can assign an Arachnotron as the one being replaced
	// so functions like CheckReplacee and A_BossDeath can actually work, given
	// modders set it up that way.
	if (Level->localEventManager->CheckReplacee(&savedrep, this))
	{
		// [MK] the replacement is final, so don't continue with the chain
		return savedrep ? savedrep : this;
	}
	if (savedrep == nullptr && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacee field is temporarily NULLed to prevent
	// potential infinite recursion.
	ActorInfo()->Replacee = nullptr;
	PClassActor *rep = savedrep;
	if (lookskill && (skillrepname != NAME_None) && (PClass::FindClass(skillrepname) != nullptr))
	{
		rep = PClass::FindActor(skillrepname);
	}
	rep = rep->GetReplacee(Level, false);
	ActorInfo()->Replacee = savedrep;
	return rep;
}

//==========================================================================
//
// PClassActor :: SetDamageFactor
//
//==========================================================================

void PClassActor::SetDamageFactor(FName type, double factor)
{
	for (auto & p : ActorInfo()->DamageFactors)
	{
		if (p.first == type)
		{
			p.second = factor;
			return;
		}
	}
	ActorInfo()->DamageFactors.Push({ type, factor });
}

//==========================================================================
//
// PClassActor :: SetPainChance
//
//==========================================================================

void PClassActor::SetPainChance(FName type, int chance)
{
	for (auto & p : ActorInfo()->PainChances)
	{
		if (p.first == type)
		{
			p.second = chance;
			return;
		}
	}

	if (chance >= 0) 
	{
		ActorInfo()->PainChances.Push({ type, min(chance, 256) });
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

int DmgFactors::Apply(FName type, int damage)
{
	double factor = -1.;
	for (auto & p : *this)
	{
		if (p.first == type)
		{
			factor = p.second;
			break;
		}
		if (p.first == NAME_None)
		{
			factor = p.second;
		}
	}

	if (factor < 0.) return damage;
	return int(damage * factor);
}


static void SummonActor (int command, int command2, FCommandLine argv)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		PClassActor *type = PClass::FindActor(argv[1]);
		if (type == nullptr)
		{
			Printf ("Unknown actor '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (argv.argc() > 2 ? command2 : command);
		Net_WriteString (type->TypeName.GetChars());

		if (argv.argc () > 2)
		{
			Net_WriteWord (atoi (argv[2])); // angle
			Net_WriteWord ((argv.argc() > 3) ? atoi(argv[3]) : 0); // TID
			Net_WriteByte ((argv.argc() > 4) ? atoi(argv[4]) : 0); // special
			for (int i = 5; i < 10; i++)
			{ // args[5]
				Net_WriteLong((i < argv.argc()) ? atoi(argv[i]) : 0);
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

DEFINE_ACTION_FUNCTION(_DamageTypeDefinition, IgnoreArmor)
{
	PARAM_PROLOGUE;
	PARAM_NAME(type);
	ACTION_RETURN_BOOL(DamageTypeDefinition::IgnoreArmor(type));
}

FString DamageTypeDefinition::GetObituary(FName type)
{
	DamageTypeDefinition *dtd = Get(type);
	if (dtd) return dtd->Obituary;
	return "";
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

double DamageTypeDefinition::GetMobjDamageFactor(FName type, DmgFactors const * const factors)
{
	double defaultfac = -1.;
	if (factors)
	{
		// If the actor has named damage factors, look for a specific factor
		for (auto & p : *factors)
		{
			if (p.first == type) return p.second; // type specific damage type
			if (p.first == NAME_None) defaultfac = p.second;
		}
		// If this was nonspecific damage, don't fall back to nonspecific search
		if (type == NAME_None) return 1.;
	}
	
	// If this was nonspecific damage, don't fall back to nonspecific search
	else if (type == NAME_None) 
	{ 
		return 1.;
	}
	else
	{
		// Normal is unsupplied / 1.0, so there's no difference between modifying and overriding
		DamageTypeDefinition *dtd = Get(type);
		return dtd ? dtd->DefaultFactor : 1.;
	}
	
	{
		DamageTypeDefinition *dtd = Get(type);
		// Here we are looking for modifications to untyped damage
		// If the calling actor defines untyped damage factor, that is contained in "pdf".
		if (defaultfac >= 0.) // normal damage available
		{
			if (dtd)
			{
				if (dtd->ReplaceFactor) return dtd->DefaultFactor; // use default instead of untyped factor
				return defaultfac * dtd->DefaultFactor; // use default as modification of untyped factor
			}
			return defaultfac; // there was no default, so actor default is used
		}
		else if (dtd)
		{
			return dtd->DefaultFactor; // implicit untyped factor 1.0 does not need to be applied/replaced explicitly
		}
	}
	return 1.;
}

int DamageTypeDefinition::ApplyMobjDamageFactor(int damage, FName type, DmgFactors const * const factors)
{
	double factor = GetMobjDamageFactor(type, factors);
	return int(damage * factor);
}

//==========================================================================
//
// Reads a damage definition
//
//==========================================================================

void FMapInfoParser::ParseDamageDefinition()
{
	sc.MustGetString();
	FName damageType = sc.String;

	DamageTypeDefinition dtd;

	ParseOpenBrace();
	while (sc.MustGetAnyToken(), sc.TokenType != '}')
	{
		if (sc.Compare("FACTOR"))
		{
			sc.MustGetStringName("=");
			sc.MustGetFloat();
			dtd.DefaultFactor = sc.Float;
			if (dtd.DefaultFactor == 0) dtd.ReplaceFactor = true;
		}
		else if (sc.Compare("OBITUARY"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			dtd.Obituary = sc.String;
		}
		else if (sc.Compare("REPLACEFACTOR"))
		{
			dtd.ReplaceFactor = true;
		}
		else if (sc.Compare("NOARMOR"))
		{
			dtd.NoArmor = true;
		}
		else
		{
			sc.ScriptError("Unexpected data (%s) in damagetype definition.", sc.String);
		}
	}

	dtd.Apply(damageType);
}
