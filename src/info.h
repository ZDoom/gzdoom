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
** Important restrictions because of the way FState is structured:
**
** The range of Frame is [0,63]. Since sprite naming conventions
** are even more restrictive than this, this isn't something to
** really worry about.
**
** The range of Tics is [-1,65534]. If Misc1 is important, then
** the range of Tics is reduced to [-1,254], because Misc1 also
** doubles as the high byte of the tic.
**
** The range of Misc1 is [-128,127] and Misc2's range is [0,255].
**
** When compiled with Visual C++, this struct is 16 bytes. With
** any other compiler (assuming a 32-bit architecture), it is 20 bytes.
** This is because with VC++, I can use the charizing operator to
** initialize the name array to exactly 4 chars. If GCC would
** compile something like char t = "PLYR"[0]; as char t = 'P'; then GCC
** could also use the 16-byte version. Unfortunately, GCC compiles it
** more like:
**
** char t;
** void initializer () {
**     static const char str[]="PLYR";
**     t = str[0];
** }
**
** While this does allow the use of a 16-byte FState, the additional
** code amounts to more than 4 bytes.
**
** If C++ would allow char name[4] = "PLYR"; without an error (as C does),
** I could just initialize the name as a regular string and be done with it.
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

const BYTE SF_FULLBRIGHT = 0x40;

struct Baggage;
class FScanner;
struct FActorInfo;
class FArchive;

struct FState
{
	WORD		sprite;
	SWORD		Tics;
	SBYTE		Misc1;
	BYTE		Misc2;
	BYTE		Frame;
	BYTE		DefineFlags;	// Unused byte so let's use it during state creation.
	FState		*NextState;
	actionf_p	ActionFunc;
	int			ParameterIndex;

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
	inline bool CallAction(AActor *self, StateCallData *statecall = NULL)
	{
		if (ActionFunc != NULL)
		{
			ActionFunc(self, this, ParameterIndex-1, statecall);
			return true;
		}
		else
		{
			return false;
		}
	}
	static const PClass *StaticFindStateOwner (const FState *state);
	static const PClass *StaticFindStateOwner (const FState *state, const FActorInfo *info);
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

#ifndef EGAMETYPE
#define EGAMETYPE
enum EGameType
{
	GAME_Any	 = 0,
	GAME_Doom	 = 1,
	GAME_Heretic = 2,
	GAME_Hexen	 = 4,
	GAME_Strife	 = 8,
	GAME_Chex	 = 16,

	GAME_Raven			= GAME_Heretic|GAME_Hexen,
	GAME_DoomStrife		= GAME_Doom|GAME_Strife,
	GAME_DoomChex		= GAME_Doom|GAME_Chex,
	GAME_DoomStrifeChex	= GAME_Doom|GAME_Strife|GAME_Chex
};
#endif

typedef TMap<FName, fixed_t> DmgFactors;
typedef TMap<FName, BYTE> PainChanceList;

struct FActorInfo
{
	static void StaticInit ();
	static void StaticSetActorNums ();

	void BuildDefaults ();
	void ApplyDefaults (BYTE *defaults);
	void RegisterIDs ();
	void SetDamageFactor(FName type, fixed_t factor);
	void SetPainChance(FName type, int chance);

	FState *FindState (int numnames, FName *names, bool exact=false) const;
	FState *FindStateByString(const char *name, bool exact=false);
	FState *FindState (FName name) const
	{
		return FindState(1, &name);
	}

	FActorInfo *GetReplacement ();
	FActorInfo *GetReplacee ();

	PClass *Class;
	FState *OwnedStates;
	FActorInfo *Replacement;
	FActorInfo *Replacee;
	int NumOwnedStates;
	BYTE GameFilter;
	BYTE SpawnID;
	SWORD DoomEdNum;
	FStateLabels * StateList;
	DmgFactors *DamageFactors;
	PainChanceList * PainChances;
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

int GetSpriteIndex(const char * spritename);
TArray<FName> &MakeStateNameList(const char * fname);

#endif	// __INFO_H__
