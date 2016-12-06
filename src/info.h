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
#if !defined(_WIN32)
#include <inttypes.h>		// for intptr_t
#else
#include <stdint.h>			// for mingw
#endif

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
};

enum EStateUseFlags
{
	SUF_ACTOR = 1,
	SUF_OVERLAY = 2,
	SUF_WEAPON = 4,
	SUF_ITEM = 8,
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
	uint8_t		DefineFlags;	// Unused byte so let's use it during state creation.
	int32_t		Misc1;			// Was changed to SBYTE, reverted to long for MBF compat
	int32_t		Misc2;			// Was changed to BYTE, reverted to long for MBF compat
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
	inline void SetFrame(BYTE frame)
	{
		Frame = frame - 'A';
	}
	void SetAction(VMFunction *func) { ActionFunc = func; }
	void ClearAction() { ActionFunc = NULL; }
	void SetAction(const char *name);
	bool CallAction(AActor *self, AActor *stateowner, FStateParamInfo *stateinfo, FState **stateret);
	static PClassActor *StaticFindStateOwner (const FState *state);
	static PClassActor *StaticFindStateOwner (const FState *state, PClassActor *info);
	static FRandom pr_statetics;
};

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

struct DmgFactors : public TMap<FName, double>
{
	int Apply(FName type, int damage);
};
typedef TMap<FName, int> PainChanceList;

struct DamageTypeDefinition
{
public:
	DamageTypeDefinition() { Clear(); }

	double DefaultFactor;
	bool ReplaceFactor;
	bool NoArmor;

	void Apply(FName type);
	void Clear()
	{
		DefaultFactor = 1.;
		ReplaceFactor = false;
		NoArmor = false;
	}

	static DamageTypeDefinition *Get(FName type);
	static bool IgnoreArmor(FName type);
	static double GetMobjDamageFactor(FName type, DmgFactors const * const factors);
	static int ApplyMobjDamageFactor(int damage, FName type, DmgFactors const * const factors);
};

class DDropItem;
class PClassPlayerPawn;

class PClassActor : public PClass
{
	DECLARE_CLASS(PClassActor, PClass);
	HAS_OBJECT_POINTERS;
protected:
public:
	static void StaticInit ();
	static void StaticSetActorNums ();
	virtual void DeriveData(PClass *newclass);

	PClassActor();
	~PClassActor();

	virtual size_t PointerSubstitution(DObject *oldclass, DObject *newclass);
	void BuildDefaults();
	void ApplyDefaults(BYTE *defaults);
	void RegisterIDs();
	void SetDamageFactor(FName type, double factor);
	void SetPainChance(FName type, int chance);
	size_t PropagateMark();
	bool SetReplacement(FName replaceName);
	void SetDropItems(DDropItem *drops);
	virtual void Finalize(FStateDefinitions &statedef);

	FState *FindState(int numnames, FName *names, bool exact=false) const;
	FState *FindStateByString(const char *name, bool exact=false);
	FState *FindState(FName name) const
	{
		return FindState(1, &name);
	}

	bool OwnsState(const FState *state)
	{
		return state >= OwnedStates && state < OwnedStates + NumOwnedStates;
	}

	PClassActor *GetReplacement(bool lookskill=true);
	PClassActor *GetReplacee(bool lookskill=true);

	FState *OwnedStates;
	PClassActor *Replacement;
	PClassActor *Replacee;
	int NumOwnedStates;
	BYTE GameFilter;
	uint8_t DefaultStateUsage; // state flag defaults for blocks without a qualifier.
	WORD SpawnID;
	WORD ConversationID;
	SWORD DoomEdNum;
	FStateLabels *StateList;
	DmgFactors *DamageFactors;
	PainChanceList *PainChances;

	TArray<PClassPlayerPawn *> VisibleToPlayerClass;

	FString Obituary;		// Player was killed by this actor
	FString HitObituary;	// Player was killed by this actor in melee
	double DeathHeight;	// Height on normal death
	double BurnHeight;		// Height on burning death
	PalEntry BloodColor;	// Colorized blood
	int GibHealth;			// Negative health below which this monster dies an extreme death
	int WoundHealth;		// Health needed to enter wound state
	double FastSpeed;		// speed in fast mode
	double RDFactor;		// Radius damage factor
	double CameraHeight;	// Height of camera when used as such
	FSoundID HowlSound;		// Sound being played when electrocuted or poisoned
	FName BloodType;		// Blood replacement type
	FName BloodType2;		// Bloopsplatter replacement type
	FName BloodType3;		// AxeBlood replacement type

	DDropItem *DropItems;
	FString SourceLumpName;
	FIntCVar *distancecheck;

	// Old Decorate compatibility stuff
	bool DontHurtShooter;
	int ExplosionRadius;
	int ExplosionDamage;
	int MeleeDamage;
	FSoundID MeleeSound;
	FName MissileName;
	double MissileHeight;

	// For those times when being able to scan every kind of actor is convenient
	static TArray<PClassActor *> AllActorClasses;
};

inline PClassActor *PClass::FindActor(FName name)
{
	 return dyn_cast<PClassActor>(FindClass(name));
}

struct FDoomEdEntry
{
	PClassActor *Type;
	short Special;
	signed char ArgsDefined;
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
