/*
** info.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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
*/

#ifndef __INFO_H__
#define __INFO_H__

#include <stddef.h>
#include <stdint.h>

#include "dobject.h"
#include "doomdef.h"
#include "s_sound.h"

#include "m_fixed.h"
#include "m_random.h"

struct Baggage;
class FScanner;
struct FActorInfo;
class FIntCVar;
class FStateDefinitions;
class FInternalLightAssociation;
struct FState;

enum EStateDefineFlags
{
	SDF_NEXT = 0,
	SDF_STATE = 1,
	SDF_STOP = 2,
	SDF_WAIT = 3,
	SDF_LABEL = 4,
	SDF_INDEX = 5,
	SDF_MASK = 7,
};

enum EStateFlags
{
	STF_SLOW = 1,		// State duration is extended when slow monsters is on.
	STF_FAST = 2,		// State duration is shortened when fast monsters is on.
	STF_FULLBRIGHT = 4,	// State is fullbright
	STF_NODELAY = 8,	// Spawn states executes its action normally
	STF_SAMEFRAME = 16,	// Ignore Frame (except when spawning actor)
	STF_CANRAISE = 32,	// Allows a monster to be resurrected without waiting for an infinate frame
	STF_DEHACKED = 64,	// Modified by Dehacked
	STF_CONSUMEAMMO = 128,	// Needed by the Dehacked parser.
};

enum EStateType : int // this must ensure proper alignment.
{
	STATE_Actor,
	STATE_Psprite,
	STATE_StateChain,
};

struct FStateParamInfo
{
	FState *mCallingState;
	EStateType mStateType;
	int mPSPIndex;
};


// Sprites that are fixed in position because they can have special meanings.
enum
{
	SPR_TNT1,		// The empty sprite
	SPR_FIXED,		// Do not change sprite or frame
	SPR_NOCHANGE,	// Do not change sprite (frame change is okay)
};

struct FState
{
	FState		*NextState;
	VMFunction	*ActionFunc;
	int32_t		sprite;
	int16_t		Tics;
	uint16_t	TicRange;
	int16_t		Light;
	uint16_t	StateFlags;
	uint8_t		Frame;
	uint8_t		UseFlags;		
	uint8_t		DefineFlags;
	int32_t		Misc1;			// Was changed to int8_t, reverted to long for MBF compat
	int32_t		Misc2;			// Was changed to uint8_t, reverted to long for MBF compat
	int32_t		DehIndex;		// we need this to resolve offsets in P_SetSafeFlash.
public:
	inline int GetFrame() const
	{
		return Frame;
	}
	inline bool GetSameFrame() const
	{
		return !!(StateFlags & STF_SAMEFRAME);
	}
	inline int GetFullbright() const
	{
		return (StateFlags & STF_FULLBRIGHT)? 0x10 /*RF_FULLBRIGHT*/ : 0;
	}
	inline bool GetFast() const
	{
		return !!(StateFlags & STF_FAST);
	}
	inline bool GetSlow() const
	{
		return !!(StateFlags & STF_SLOW);
	}
	inline bool GetNoDelay() const
	{
		return !!(StateFlags & STF_NODELAY);
	}
	inline bool GetCanRaise() const
	{
		return !!(StateFlags & STF_CANRAISE);
	}
	inline int GetTics() const
	{
		if (TicRange == 0)
		{
			return Tics;
		}
		return Tics + pr_statetics.GenRand32() % (TicRange + 1);
	}
	inline int GetMisc1() const
	{
		return Misc1;
	}
	inline int GetMisc2() const
	{
		return Misc2;
	}
	inline FState *GetNextState() const
	{
		return NextState;
	}
	inline void SetFrame(uint8_t frame)
	{
		Frame = frame - 'A';
	}
	void SetAction(VMFunction *func) { ActionFunc = func; }
	void ClearAction() { ActionFunc = NULL; }
	void SetAction(const char *name);
	bool CallAction(AActor *self, AActor *stateowner, FStateParamInfo *stateinfo, FState **stateret);
    void CheckCallerType(AActor *self, AActor *stateowner);

	static PClassActor *StaticFindStateOwner (const FState *state);
	static PClassActor *StaticFindStateOwner (const FState *state, PClassActor *info);
	static FString StaticGetStateName(const FState *state, PClassActor *info = nullptr);
	static FRandom pr_statetics;

};

extern TMap<int, FState*> dehExtStates;

struct FStateLabels;
struct FStateLabel
{
	FName Label;
	FState *State;
	FStateLabels *Children;
};

struct FStateLabels
{
	int NumLabels;
	FStateLabel Labels[1];

	FStateLabel *FindLabel (FName label);

	void Destroy();	// intentionally not a destructor!
};

#include "gametype.h"

struct DmgFactors : public TArray<std::pair<FName, double>>
{
	int Apply(FName type, int damage);
};
typedef TArray<std::pair<FName, int>> PainChanceList;

struct DamageTypeDefinition
{
public:
	DamageTypeDefinition() { Clear(); }

	FString Obituary;
	double DefaultFactor;
	bool ReplaceFactor;
	bool NoArmor;

	void Apply(FName type);
	void Clear()
	{
		Obituary = "";
		DefaultFactor = 1.;
		ReplaceFactor = false;
		NoArmor = false;
	}

	static bool IgnoreArmor(FName type);
	static int ApplyMobjDamageFactor(int damage, FName type, DmgFactors const * const factors);
	static FString GetObituary(FName type);

private:
	static double GetMobjDamageFactor(FName type, DmgFactors const * const factors);
	static DamageTypeDefinition *Get(FName type);
};

struct FDropItem;

struct FActorInfo
{
	TArray<FInternalLightAssociation *> LightAssociations;
	PClassActor *Replacement = nullptr;
	PClassActor *Replacee = nullptr;
	FState *OwnedStates = nullptr;
	int NumOwnedStates = 0;
	bool SkipSuperSet = false;
	uint8_t GameFilter = GAME_Any;
	uint16_t SpawnID = 0;
	uint16_t ConversationID = 0;
	int16_t DoomEdNum = -1;
	int infighting_group = 0;
	int projectile_group = 0;
	int splash_group = 0;

	FStateLabels *StateList = nullptr;
	DmgFactors DamageFactors;
	PainChanceList PainChances;

	TArray<PClassActor *> VisibleToPlayerClass;

	FDropItem *DropItems;
	FIntCVar *distancecheck;

	// This is from PClassPlayerPawn
	FString DisplayName;

	uint8_t DefaultStateUsage = 0; // state flag defaults for blocks without a qualifier.

	FActorInfo() = default;
	FActorInfo(const FActorInfo & other)
	{
		// only copy the fields that get inherited
		DefaultStateUsage = other.DefaultStateUsage;
		DamageFactors = other.DamageFactors;
		PainChances = other.PainChances;
		VisibleToPlayerClass = other.VisibleToPlayerClass;
		DropItems = other.DropItems;
		distancecheck = other.distancecheck;
		DisplayName = other.DisplayName;
	}

	~FActorInfo()
	{
		if (StateList != NULL)
		{
			StateList->Destroy();
			M_Free(StateList);
		}
	}
};

// This is now merely a wrapper that adds actor-specific functionality to PClass.
// No objects of this type will be created ever - its only use is to static_cast
// PClass to it.
class PClassActor : public PClass
{
protected:
public:
	static void StaticInit ();
	static void StaticSetActorNums ();

	void BuildDefaults();
	void ApplyDefaults(uint8_t *defaults);
	void RegisterIDs();
	void SetDamageFactor(FName type, double factor);
	void SetPainChance(FName type, int chance);
	bool SetReplacement(FName replaceName);
	void InitializeDefaults();

	FActorInfo *ActorInfo() const
	{
		return (FActorInfo*)Meta;
	}

	void SetDropItems(FDropItem *drops)
	{
		ActorInfo()->DropItems = drops;
	}

	const FString &GetDisplayName() const
	{
		return ActorInfo()->DisplayName;
	}

	FState *GetStates() const
	{
		return ActorInfo()->OwnedStates;
	}

	unsigned GetStateCount() const
	{
		return ActorInfo()->NumOwnedStates;
	}

	FStateLabels *GetStateLabels() const
	{
		return ActorInfo()->StateList;
	}

	FState *FindState(int numnames, FName *names, bool exact=false) const;
	FState *FindStateByString(const char *name, bool exact=false);
	FState *FindState(FName name) const
	{
		return FindState(1, &name);
	}

	bool OwnsState(const FState *state)
	{
		auto i = ActorInfo();
		return i != nullptr && state >= i->OwnedStates && state < i->OwnedStates + i->NumOwnedStates;
	}

	PClassActor *GetReplacement(FLevelLocals *Level, bool lookskill=true);
	PClassActor *GetReplacee(FLevelLocals *Level, bool lookskill=true);

	// For those times when being able to scan every kind of actor is convenient
	static TArray<PClassActor *> AllActorClasses;
};

struct FDoomEdEntry
{
	PClassActor *Type;
	short Special;
	signed char ArgsDefined;
	bool NoSkillFlags;
	int Args[5];
};

struct FStateLabelStorage
{
	TArray<uint8_t> Storage;

	int AddPointer(FState *ptr)
	{
		if (ptr != nullptr)
		{
			int pos = Storage.Reserve(sizeof(ptr) + sizeof(int));
			memset(&Storage[pos], 0, sizeof(int));
			memcpy(&Storage[pos + sizeof(int)], &ptr, sizeof(ptr));
			return pos / 4 + 1;
		}
		else return 0;
	}

	int AddNames(TArray<FName> &names)
	{
		int siz = names.Size();
		if (siz > 1)
		{
			int pos = Storage.Reserve(sizeof(int) + sizeof(FName) * names.Size());
			memcpy(&Storage[pos], &siz, sizeof(int));
			memcpy(&Storage[pos + sizeof(int)], &names[0], sizeof(FName) * names.Size());
			return pos / 4 + 1;
		}
		else
		{
			// don't store single name states in the array.
			return names[0].GetIndex() + 0x10000000;
		}
	}

	FState *GetState(int pos, PClassActor *cls, bool exact = false);
};

extern FStateLabelStorage StateLabels;

enum ESpecialMapthings
{
	SMT_Player1Start = 1,
	SMT_Player2Start,
	SMT_Player3Start,
	SMT_Player4Start,
	SMT_Player5Start,
	SMT_Player6Start,
	SMT_Player7Start,
	SMT_Player8Start,
	SMT_DeathmatchStart,
	SMT_SSeqOverride,
	SMT_PolyAnchor,
	SMT_PolySpawn,
	SMT_PolySpawnCrush,
	SMT_PolySpawnHurt,
	SMT_SlopeFloorPointLine,
	SMT_SlopeCeilingPointLine,
	SMT_SetFloorSlope,
	SMT_SetCeilingSlope,
	SMT_VavoomFloor,
	SMT_VavoomCeiling,
	SMT_CopyFloorPlane,
	SMT_CopyCeilingPlane,
	SMT_VertexFloorZ,
	SMT_VertexCeilingZ,
	SMT_EDThing,

};


typedef TMap<int, FDoomEdEntry> FDoomEdMap;

extern FDoomEdMap DoomEdMap;

void InitActorNumsFromMapinfo();


int GetSpriteIndex(const char * spritename, bool add = true);
TArray<FName> &MakeStateNameList(const char * fname);
void AddStateLight(FState *state, const char *lname);

#endif	// __INFO_H__
