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
#elif !defined(_MSC_VER)
#include <stdint.h>			// for mingw
#endif

#include "dobject.h"
#include "doomdef.h"
#include "vm.h"
#include "s_sound.h"

const BYTE SF_FULLBRIGHT = 0x40;

struct Baggage;
class FScanner;
struct FActorInfo;
class FArchive;

struct FState
{
	WORD		sprite;
	SWORD		Tics;
	long		Misc1;			// Was changed to SBYTE, reverted to long for MBF compat
	long		Misc2;			// Was changed to BYTE, reverted to long for MBF compat
	BYTE		Frame;
	BYTE		DefineFlags;	// Unused byte so let's use it during state creation.
	short		Light;
	FState		*NextState;
	VMFunction	*ActionFunc;

	inline int GetFrame() const
	{
		return Frame & ~(SF_FULLBRIGHT);
	}
	inline int GetFullbright() const
	{
		return Frame & SF_FULLBRIGHT ? 0x10 /*RF_FULLBRIGHT*/ : 0;
	}
	inline int GetTics() const
	{
		return Tics;
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
		Frame = (Frame & SF_FULLBRIGHT) | (frame-'A');
	}
	void SetAction(VMFunction *func) { ActionFunc = func; }
	bool CallAction(AActor *self, AActor *stateowner, StateCallData *statecall = NULL);
	static PClassActor *StaticFindStateOwner (const FState *state);
	static PClassActor *StaticFindStateOwner (const FState *state, PClassActor *info);
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



FArchive &operator<< (FArchive &arc, FState *&state);

#include "gametype.h"

typedef TMap<FName, fixed_t> DmgFactors;
typedef TMap<FName, int> PainChanceList;
class DDropItem;

class PClassActor : public PClass
{
	DECLARE_CLASS(PClassActor, PClass);
	HAS_OBJECT_POINTERS;
protected:
	virtual void Derive(PClass *newclass);
public:
	static void StaticInit ();
	static void StaticSetActorNums ();

	PClassActor();
	~PClassActor();

	void BuildDefaults();
	void ApplyDefaults(BYTE *defaults);
	void RegisterIDs();
	void SetDamageFactor(FName type, fixed_t factor);
	void SetPainChance(FName type, int chance);
	size_t PropagateMark();
	void InitializeNativeDefaults();

	FState *FindState(int numnames, FName *names, bool exact=false) const;
	FState *FindStateByString(const char *name, bool exact=false);
	FState *FindState(FName name) const
	{
		return FindState(1, &name);
	}

	PClassActor *GetReplacement(bool lookskill=true);
	PClassActor *GetReplacee(bool lookskill=true);

	FState *OwnedStates;
	PClassActor *Replacement;
	PClassActor *Replacee;
	int NumOwnedStates;
	BYTE GameFilter;
	BYTE SpawnID;
	SWORD DoomEdNum;
	FStateLabels *StateList;
	DmgFactors *DamageFactors;
	PainChanceList *PainChances;
	FString Obituary;		// Player was killed by this actor
	FString HitObituary;	// Player was killed by this actor in melee
	fixed_t DeathHeight;	// Height on normal death
	fixed_t BurnHeight;		// Height on burning death
	PalEntry BloodColor;	// Colorized blood
	int GibHealth;			// Negative health below which this monster dies an extreme death
	int WoundHealth;		// Health needed to enter wound state
	int PoisonDamage;		// Amount of poison damage
	fixed_t FastSpeed;		// Speed in fast mode
	fixed_t RDFactor;		// Radius damage factor
	fixed_t CameraHeight;	// Height of camera when used as such
	FSoundID HowlSound;		// Sound being played when electrocuted or poisoned
	FName BloodType;		// Blood replacement type
	FName BloodType2;		// Bloopsplatter replacement type
	FName BloodType3;		// AxeBlood replacement type

	DDropItem *DropItems;

	// Old Decorate compatibility stuff
	bool DontHurtShooter;
	int ExplosionRadius;
	int ExplosionDamage;
	int MeleeDamage;
	FSoundID MeleeSound;
	FName MissileName;
	fixed_t MissileHeight;

	// For those times when being able to scan every kind of actor is convenient
	static TArray<PClassActor *> AllActorClasses;
};

inline PClassActor *PClass::FindActor(FName name)
{
	 return dyn_cast<PClassActor>(FindClass(name));
}

class FDoomEdMap
{
public:
	~FDoomEdMap();

	PClassActor *FindType (int doomednum) const;
	void AddType (int doomednum, PClassActor *type, bool temporary = false);
	void DelType (int doomednum);
	void Empty ();

	static void DumpMapThings ();

private:
	enum { DOOMED_HASHSIZE = 256 };

	struct FDoomEdEntry
	{
		FDoomEdEntry *HashNext;
		PClassActor *Type;
		int DoomEdNum;
		bool temp;
	};

	static FDoomEdEntry *DoomEdHash[DOOMED_HASHSIZE];
};

extern FDoomEdMap DoomEdMap;

int GetSpriteIndex(const char * spritename);
TArray<FName> &MakeStateNameList(const char * fname);
void AddStateLight(FState *state, const char *lname);

// Standard parameters for all action functons
//   self         - Actor this action is to operate on (player if a weapon)
//   stateowner   - Actor this action really belongs to (may be a weapon)
//   callingstate - State this action was called from
//   statecall    - CustomInventory stuff
#define PARAM_ACTION_PROLOGUE_TYPE(type) \
	PARAM_PROLOGUE; \
	PARAM_OBJECT	 (self, type); \
	PARAM_OBJECT_OPT (stateowner, AActor) { stateowner = self; } \
	PARAM_STATE_OPT  (callingstate) { callingstate = NULL; } \
	PARAM_POINTER_OPT(statecall, StateCallData) { statecall = NULL; }

#define PARAM_ACTION_PROLOGUE	PARAM_ACTION_PROLOGUE_TYPE(AActor)

// Number of action paramaters
#define NAP 4

#endif	// __INFO_H__
