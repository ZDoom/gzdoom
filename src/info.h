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

#include "m_fixed.h"
#include "m_random.h"

struct Baggage;
class FScanner;
struct FActorInfo;
class FArchive;

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
	actionf_p	ActionFunc;
	WORD		sprite;
	SWORD		Tics;
	WORD		TicRange;
	BYTE		Frame;
	BYTE		DefineFlags;	// Unused byte so let's use it during state creation.
	int			Misc1;			// Was changed to SBYTE, reverted to long for MBF compat
	int			Misc2;			// Was changed to BYTE, reverted to long for MBF compat
	short		Light;
	BYTE		Fullbright:1;	// State is fullbright
	BYTE		SameFrame:1;	// Ignore Frame (except when spawning actor)
	BYTE		Fast:1;
	BYTE		NoDelay:1;		// Spawn states executes its action normally
	BYTE		CanRaise:1;		// Allows a monster to be resurrected without waiting for an infinate frame
	BYTE		Slow:1;			// Inverse of fast
	int			ParameterIndex;

	inline int GetFrame() const
	{
		return Frame;
	}
	inline bool GetSameFrame() const
	{
		return SameFrame;
	}
	inline int GetFullbright() const
	{
		return Fullbright ? 0x10 /*RF_FULLBRIGHT*/ : 0;
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
	inline bool GetNoDelay() const
	{
		return NoDelay;
	}
	inline bool GetCanRaise() const
	{
		return CanRaise;
	}
	inline void SetFrame(BYTE frame)
	{
		Frame = frame - 'A';
	}
	void SetAction(PSymbolActionFunction *func, bool setdefaultparams = true)
	{
		if (func != NULL)
		{
			ActionFunc = func->Function;
			if (setdefaultparams) ParameterIndex = func->defaultparameterindex+1;
		}
		else 
		{
			ActionFunc = NULL;
			if (setdefaultparams) ParameterIndex = 0;
		}
	}
	inline bool CallAction(AActor *self, AActor *stateowner, StateCallData *statecall = NULL)
	{
		if (ActionFunc != NULL)
		{
			ActionFunc(self, stateowner, this, ParameterIndex-1, statecall);
			return true;
		}
		else
		{
			return false;
		}
	}
	static const PClass *StaticFindStateOwner (const FState *state);
	static const PClass *StaticFindStateOwner (const FState *state, const FActorInfo *info);
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



FArchive &operator<< (FArchive &arc, FState *&state);

#include "gametype.h"

// Standard pre-defined skin colors
struct FPlayerColorSet
{
	struct ExtraRange
	{
		BYTE RangeStart, RangeEnd;	// colors to remap
		BYTE FirstColor, LastColor;	// colors to map to
	};

	FName Name;			// Name of this color

	int Lump;			// Lump to read the translation from, otherwise use next 2 fields
	BYTE FirstColor, LastColor;		// Describes the range of colors to use for the translation

	BYTE RepresentativeColor;		// A palette entry representative of this translation,
									// for map arrows and status bar backgrounds and such
	BYTE NumExtraRanges;
	ExtraRange Extra[6];
};

struct DmgFactors : public TMap<FName, fixed_t>
{
	fixed_t *CheckFactor(FName type);
};
typedef TMap<FName, int> PainChanceList;
typedef TMap<FName, PalEntry> PainFlashList;
typedef TMap<int, FPlayerColorSet> FPlayerColorSetMap;



struct DamageTypeDefinition
{
public:
	DamageTypeDefinition() { Clear(); }

	fixed_t DefaultFactor;
	bool ReplaceFactor;
	bool NoArmor;

	void Apply(FName const type);
	void Clear()
	{
		DefaultFactor = FRACUNIT;
		ReplaceFactor = false;
		NoArmor = false;
	}

	static DamageTypeDefinition *Get(FName const type);
	static bool IgnoreArmor(FName const type);
	static int ApplyMobjDamageFactor(int damage, FName const type, DmgFactors const * const factors);
};


struct FActorInfo
{
	static void StaticInit ();
	static void StaticSetActorNums ();

	void BuildDefaults ();
	void ApplyDefaults (BYTE *defaults);
	void RegisterIDs ();
	void SetDamageFactor(FName type, fixed_t factor);
	void SetPainChance(FName type, int chance);
	void SetPainFlash(FName type, PalEntry color);
	bool GetPainFlash(FName type, PalEntry *color) const;
	void SetColorSet(int index, const FPlayerColorSet *set);

	FState *FindState (int numnames, FName *names, bool exact=false) const;
	FState *FindStateByString(const char *name, bool exact=false);
	FState *FindState (FName name) const
	{
		return FindState(1, &name);
	}

	bool OwnsState(const FState *state)
	{
		return state >= OwnedStates && state < OwnedStates + NumOwnedStates;
	}

	FActorInfo *GetReplacement (bool lookskill=true);
	FActorInfo *GetReplacee (bool lookskill=true);

	PClass *Class;
	FState *OwnedStates;
	FActorInfo *Replacement;
	FActorInfo *Replacee;
	int NumOwnedStates;
	BYTE GameFilter;
	BYTE SpawnID;
	SWORD DoomEdNum;
	FStateLabels *StateList;
	DmgFactors *DamageFactors;
	PainChanceList *PainChances;
	PainFlashList *PainFlashes;
	FPlayerColorSetMap *ColorSets;
	TArray<const PClass *> VisibleToPlayerClass;
	TArray<const PClass *> RestrictedToPlayerClass;
	TArray<const PClass *> ForbiddenToPlayerClass;
};

class FDoomEdMap
{
public:
	~FDoomEdMap();

	const PClass *FindType (int doomednum) const;
	void AddType (int doomednum, const PClass *type, bool temporary = false);
	void DelType (int doomednum);
	void Empty ();

	static void DumpMapThings ();

private:
	enum { DOOMED_HASHSIZE = 256 };

	struct FDoomEdEntry
	{
		FDoomEdEntry *HashNext;
		const PClass *Type;
		int DoomEdNum;
		bool temp;
	};

	static FDoomEdEntry *DoomEdHash[DOOMED_HASHSIZE];
};

extern FDoomEdMap DoomEdMap;

int GetSpriteIndex(const char * spritename, bool add = true);
TArray<FName> &MakeStateNameList(const char * fname);
void AddStateLight(FState *state, const char *lname);

#endif	// __INFO_H__
