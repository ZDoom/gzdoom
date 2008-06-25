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
#include "dthinker.h"
#include "farchive.h"
#include "doomdef.h"
#include "name.h"
#include "tarray.h"

const BYTE SF_FULLBRIGHT = 0x40;
const BYTE SF_BIGTIC	 = 0x80;

struct FState
{
	union
	{
#if _MSC_VER
		char name[4];
#else
		char name[8];	// 4 for name, 1 for '\0', 3 for pad
#endif
		int index;
	} sprite;
	BYTE		Tics;
	SBYTE		Misc1;
	BYTE		Misc2;
	BYTE		Frame;
	actionf_p	Action;
	FState		*NextState;
	int			ParameterIndex;

	inline int GetFrame() const
	{
		return Frame & ~(SF_FULLBRIGHT|SF_BIGTIC);
	}
	inline int GetFullbright() const
	{
		return Frame & SF_FULLBRIGHT ? 0x10 /*RF_FULLBRIGHT*/ : 0;
	}
	inline int GetTics() const
	{
		int tics;
#ifdef WORDS_BIGENDIAN
		tics = Frame & SF_BIGTIC ? (Tics|((BYTE)Misc1<<8))-1 : Tics-1;
#else
		// Use some trickery to help the compiler create this without
		// using any jumps.
		tics = ((*(SDWORD *)&Tics) & ((*(SDWORD *)&Tics) < 0 ? 0xffff : 0xff)) - 1;
#endif
#if TICRATE == 35
		return tics;
#else
		return tics > 0 ? tics * TICRATE / 35 : tics;
#endif
	}
	inline int GetMisc1() const
	{
		return Frame & SF_BIGTIC ? 0 : Misc1;
	}
	inline int GetMisc2() const
	{
		return Misc2;
	}
	inline FState *GetNextState() const
	{
		return NextState;
	}
	inline actionf_p GetAction() const
	{
		return Action;
	}
	inline void SetFrame(BYTE frame)
	{
		Frame = (Frame & (SF_FULLBRIGHT|SF_BIGTIC)) | (frame-'A');
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

#if _MSC_VER
#define _S__SPRITE_(spr) \
	{ {{(char)(#@spr>>24),(char)((#@spr>>16)&255),(char)((#@spr>>8)&255),(char)(#@spr&255)}}
#else
#define _S__SPRITE_(spr) \
	{ {{#spr}}
#endif

#define _S__FR_TIC_(spr,frm,tic,m1,m2,cmd,next) \
	_S__SPRITE_(spr), (tic+1)&255, (m1)|((tic+1)>>8), m2, (tic>254)?(SF_BIGTIC|(frm)):(frm), cmd, next }

#define S_NORMAL2(spr,frm,tic,cmd,next,m1,m2) \
	_S__FR_TIC_(spr, (frm) - 'A', tic, m1, m2, cmd, next)

#define S_BRIGHT2(spr,frm,tic,cmd,next,m1,m2) \
	_S__FR_TIC_(spr, ((frm) - 'A') | SF_FULLBRIGHT, tic, m1, m2, cmd, next)

/* <winbase.h> #defines its own, completely unrelated S_NORMAL.
 * Since winbase.h will only be included in Win32-specific files that
 * don't define any actors, we can safely avoid defining it here.
 */

#ifndef S_NORMAL
#define S_NORMAL(spr,frm,tic,cmd,next)	S_NORMAL2(spr,frm,tic,cmd,next,0,0)
#endif
#define S_BRIGHT(spr,frm,tic,cmd,next)	S_BRIGHT2(spr,frm,tic,cmd,next,0,0)


#ifndef EGAMETYPE
#define EGAMETYPE
enum EGameType
{
	GAME_Any	 = 0,
	GAME_Doom	 = 1,
	GAME_Heretic = 2,
	GAME_Hexen	 = 4,
	GAME_Strife	 = 8,

	GAME_Raven		= GAME_Heretic|GAME_Hexen,
	GAME_DoomStrife	= GAME_Doom|GAME_Strife
};
#endif

enum
{
	ADEFTYPE_Byte		= 0x0000,
	ADEFTYPE_FixedMul	= 0x4000,		// one byte, multiplied by FRACUNIT
	ADEFTYPE_Word		= 0x8000,
	ADEFTYPE_Long		= 0xC000,
	ADEFTYPE_MASK		= 0xC000,

	// These first properties are always strings
	ADEF_SeeSound = 1,
	ADEF_AttackSound,
	ADEF_PainSound,
	ADEF_DeathSound,
	ADEF_ActiveSound,
	ADEF_UseSound,
	ADEF_Weapon_UpSound,
	ADEF_Weapon_ReadySound,
	ADEF_Inventory_PickupSound,
	ADEF_Tag,			// Used by Strife to name items
	ADEF_Weapon_AmmoType1,
	ADEF_Weapon_AmmoType2,
	ADEF_Weapon_SisterType,
	ADEF_Weapon_ProjectileType,
	ADEF_PowerupGiver_Powerup,
	ADEF_Inventory_Icon,
	ADEF_Obituary,
	ADEF_HitObituary,
	ADEF_Inventory_PickupMsg,
	// [GRB] Player class properties
	ADEF_PlayerPawn_CrouchSprite,
	ADEF_PlayerPawn_DisplayName,
	ADEF_PlayerPawn_SoundClass,
	ADEF_PlayerPawn_Face,
	ADEF_PlayerPawn_ScoreIcon,
	ADEF_PlayerPawn_MorphWeapon,
	ADEF_LastString = ADEF_PlayerPawn_MorphWeapon,

	// The rest of the properties use their type field (upper 2 bits)
	ADEF_XScale,
	ADEF_YScale,
	ADEF_SpawnHealth,
	ADEF_ReactionTime,
	ADEF_PainChance,
	ADEF_Speed,
	ADEF_Radius,
	ADEF_Height,
	ADEF_Mass,
	ADEF_Damage,
	ADEF_DamageType,
	ADEF_Flags,			// Use these flags exactly
	ADEF_Flags2,		// "
	ADEF_Flags3,		// "
	ADEF_Flags4,		// "
	ADEF_Flags5,		// "
	ADEF_FlagsSet,		// Or these flags with previous
	ADEF_Flags2Set,		// "
	ADEF_Flags3Set,		// "
	ADEF_Flags4Set,		// "
	ADEF_Flags5Set,		// "
	ADEF_FlagsClear,	// Clear these flags from previous
	ADEF_Flags2Clear,	// "
	ADEF_Flags3Clear,	// "
	ADEF_Flags4Clear,	// "
	ADEF_Flags5Clear,	// "
	ADEF_Alpha,
	ADEF_RenderStyle,
	ADEF_RenderFlags,
	ADEF_Translation,
	ADEF_MinMissileChance,
	ADEF_MeleeRange,
	ADEF_MaxDropOffHeight,
	ADEF_MaxStepHeight,
	ADEF_BounceFactor,
	ADEF_WallBounceFactor,
	ADEF_BounceCount,
	ADEF_FloatSpeed,
	ADEF_RDFactor,
	ADEF_FXFlags,
	ADEF_Gravity,

	ADEF_SpawnState,
	ADEF_SeeState,
	ADEF_PainState,
	ADEF_MeleeState,
	ADEF_MissileState,
	ADEF_CrashState,
	ADEF_DeathState,
	ADEF_XDeathState,
	ADEF_BDeathState,
	ADEF_IDeathState,
	ADEF_EDeathState,
	ADEF_RaiseState,
	ADEF_WoundState,

	ADEF_StrifeType,	// Not really a property. Used to init StrifeTypes[] in p_conversation.h.
	ADEF_StrifeTeaserType,
	ADEF_StrifeTeaserType2,

	ADEF_Inventory_Amount,
	ADEF_Inventory_MaxAmount,
	ADEF_Inventory_DefMaxAmount,
	ADEF_Inventory_RespawnTics,
	ADEF_Inventory_FlagsSet,
	ADEF_Inventory_FlagsClear,
	ADEF_Inventory_PickupFlash,

	ADEF_PuzzleItem_Number,	// Identifies the puzzle item for UsePuzzleItem

	ADEF_BasicArmorPickup_SavePercent,
	ADEF_BasicArmorPickup_SaveAmount,
	ADEF_BasicArmorBonus_SavePercent,
	ADEF_BasicArmorBonus_SaveAmount,
	ADEF_BasicArmorBonus_MaxSaveAmount,
	ADEF_HexenArmor_ArmorAmount,

	ADEF_Powerup_EffectTics,
	ADEF_Powerup_Color,
	ADEF_PowerupGiver_EffectTics,

	ADEF_Ammo_BackpackAmount,
	ADEF_Ammo_BackpackMaxAmount,
	ADEF_Ammo_DropAmount,

	ADEF_Weapon_Flags,
	ADEF_Weapon_FlagsSet,
	ADEF_Weapon_AmmoGive1,
	ADEF_Weapon_AmmoGive2,
	ADEF_Weapon_AmmoUse1,
	ADEF_Weapon_AmmoUse2,
	ADEF_Weapon_Kickback,
	ADEF_Weapon_YAdjust,
	ADEF_Weapon_SelectionOrder,
	ADEF_Weapon_MoveCombatDist,
	ADEF_Weapon_UpState,
	ADEF_Weapon_DownState,
	ADEF_Weapon_ReadyState,
	ADEF_Weapon_AtkState,
	ADEF_Weapon_HoldAtkState,
	ADEF_Weapon_FlashState,
	ADEF_Sigil_NumPieces,

	// [GRB] Player class properties
	ADEF_PlayerPawn_JumpZ,
	ADEF_PlayerPawn_ViewHeight,
	ADEF_PlayerPawn_ForwardMove1,
	ADEF_PlayerPawn_ForwardMove2,
	ADEF_PlayerPawn_SideMove1,
	ADEF_PlayerPawn_SideMove2,
	ADEF_PlayerPawn_ColorRange,
	ADEF_PlayerPawn_SpawnMask,
	ADEF_PlayerPawn_AttackZOffset,

	// The following are not properties but affect how the list is parsed
	ADEF_FirstCommand,
	ADEF_SkipSuper = ADEF_FirstCommand,	// Take defaults from AActor instead of superclass(es)

	ADEF_EOL = 0xED5E		// End Of List
};

#if _MSC_VER
// nonstandard extension used : zero-sized array in struct/union
#pragma warning(disable:4200)
#endif

typedef TMap<FName, fixed_t> DmgFactors;
typedef TMap<FName, BYTE> PainChanceList;

struct FActorInfo
{
	static void StaticInit ();
	static void StaticGameSet ();
	static void StaticSetActorNums ();
	static void StaticSpeedSet ();

	void BuildDefaults ();
	void ApplyDefaults (BYTE *defaults);
	void RegisterIDs ();

	FState *FindState (FName name) const;
	FState *FindState (int numnames, FName *names, bool exact=false) const;

	void ChangeState (FName label, FState * newstate) const;


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

#if _MSC_VER
	// A 0-terminated list of default properties
	BYTE DefaultList[];
#else
	// A function to initialize the defaults for this actor
	void (*DefaultsConstructor)(); 
#endif
};

#if _MSC_VER
#pragma warning(default:4200)
#endif

class FDoomEdMap
{
public:
	~FDoomEdMap();

	const PClass *FindType (int doomednum) const;
	void AddType (int doomednum, const PClass *type);
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
	};

	static FDoomEdEntry *DoomEdHash[DOOMED_HASHSIZE];
};

extern FDoomEdMap DoomEdMap;

int GetSpriteIndex(const char * spritename);
void MakeStateNameList(const char * fname, TArray<FName> * out);
void ProcessStates (FState *states, int numstates);

#include "infomacros.h"

#endif	// __INFO_H__
