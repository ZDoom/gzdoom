/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Christoph Oelckers
** Copyright 2004-2006 Randy Heit
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
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "autosegs.h"
#include "i_system.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "a_doomglobal.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"


const PClass *QuestItemClasses[31];


extern TArray<FActorInfo *> Decorations;

// allow decal specifications in DECORATE. Decals are loaded after DECORATE so the names must be stored here.
TArray<char*> DecalNames;
// all state parameters
TArray<int> StateParameters;

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_FLAG2(symbol, name, type, variable) { symbol, #name, (int)(size_t)&((type*)1)->variable - 1 }

struct flagdef
{
	int flagbit;
	const char *name;
	int structoffset;
};


static flagdef ActorFlags[]=
{
	DEFINE_FLAG(MF, SOLID, AActor, flags),
	DEFINE_FLAG(MF, SHOOTABLE, AActor, flags),
	DEFINE_FLAG(MF, NOSECTOR, AActor, flags),
	DEFINE_FLAG(MF, NOBLOCKMAP, AActor, flags),
	DEFINE_FLAG(MF, AMBUSH, AActor, flags),
	DEFINE_FLAG(MF, JUSTHIT, AActor, flags),
	DEFINE_FLAG(MF, JUSTATTACKED, AActor, flags),
	DEFINE_FLAG(MF, SPAWNCEILING, AActor, flags),
	DEFINE_FLAG(MF, NOGRAVITY, AActor, flags),
	DEFINE_FLAG(MF, DROPOFF, AActor, flags),
	DEFINE_FLAG(MF, NOCLIP, AActor, flags),
	DEFINE_FLAG(MF, FLOAT, AActor, flags),
	DEFINE_FLAG(MF, TELEPORT, AActor, flags),
	DEFINE_FLAG(MF, MISSILE, AActor, flags),
	DEFINE_FLAG(MF, DROPPED, AActor, flags),
	DEFINE_FLAG(MF, SHADOW, AActor, flags),
	DEFINE_FLAG(MF, NOBLOOD, AActor, flags),
	DEFINE_FLAG(MF, CORPSE, AActor, flags),
	DEFINE_FLAG(MF, INFLOAT, AActor, flags),
	DEFINE_FLAG(MF, COUNTKILL, AActor, flags),
	DEFINE_FLAG(MF, COUNTITEM, AActor, flags),
	DEFINE_FLAG(MF, SKULLFLY, AActor, flags),
	DEFINE_FLAG(MF, NOTDMATCH, AActor, flags),
	DEFINE_FLAG(MF, FRIENDLY, AActor, flags),
	DEFINE_FLAG(MF, NOLIFTDROP, AActor, flags),
	DEFINE_FLAG(MF, STEALTH, AActor, flags),
	DEFINE_FLAG(MF, ICECORPSE, AActor, flags),
	DEFINE_FLAG2(MF2_LOGRAV, LOWGRAVITY, AActor, flags2),
	DEFINE_FLAG(MF2, WINDTHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, HERETICBOUNCE , AActor, flags2),
	DEFINE_FLAG(MF2, FLOORCLIP, AActor, flags2),
	DEFINE_FLAG(MF2, SPAWNFLOAT, AActor, flags2),
	DEFINE_FLAG(MF2, NOTELEPORT, AActor, flags2),
	DEFINE_FLAG2(MF2_RIP, RIPPER, AActor, flags2),
	DEFINE_FLAG(MF2, PUSHABLE, AActor, flags2),
	DEFINE_FLAG2(MF2_SLIDE, SLIDESONWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_PASSMOBJ, CANPASS, AActor, flags2),
	DEFINE_FLAG(MF2, CANNOTPUSH, AActor, flags2),
	DEFINE_FLAG(MF2, THRUGHOST, AActor, flags2),
	DEFINE_FLAG(MF2, BOSS, AActor, flags2),
	DEFINE_FLAG2(MF2_NODMGTHRUST, NODAMAGETHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, TELESTOMP, AActor, flags2),
	DEFINE_FLAG(MF2, FLOATBOB, AActor, flags2),
	DEFINE_FLAG(MF2, HEXENBOUNCE, AActor, flags2),
	DEFINE_FLAG(MF2, DOOMBOUNCE, AActor, flags2),
	DEFINE_FLAG2(MF2_IMPACT, ACTIVATEIMPACT, AActor, flags2),
	DEFINE_FLAG2(MF2_PUSHWALL, CANPUSHWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_MCROSS, ACTIVATEMCROSS, AActor, flags2),
	DEFINE_FLAG2(MF2_PCROSS, ACTIVATEPCROSS, AActor, flags2),
	DEFINE_FLAG(MF2, CANTLEAVEFLOORPIC, AActor, flags2),
	DEFINE_FLAG(MF2, NONSHOOTABLE, AActor, flags2),
	DEFINE_FLAG(MF2, INVULNERABLE, AActor, flags2),
	DEFINE_FLAG(MF2, DORMANT, AActor, flags2),
	DEFINE_FLAG(MF2, SEEKERMISSILE, AActor, flags2),
	DEFINE_FLAG(MF2, REFLECTIVE, AActor, flags2),
	DEFINE_FLAG(MF3, FLOORHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, CEILINGHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, NORADIUSDMG, AActor, flags3),
	DEFINE_FLAG(MF3, GHOST, AActor, flags3),
	DEFINE_FLAG(MF3, ALWAYSPUFF, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSPLASH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTOVERLAP, AActor, flags3),
	DEFINE_FLAG(MF3, DONTMORPH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSQUASH, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLACTIVE, AActor, flags3),
	DEFINE_FLAG(MF3, ISMONSTER, AActor, flags3),
	DEFINE_FLAG(MF3, SKYEXPLODE, AActor, flags3),
	DEFINE_FLAG(MF3, STAYMORPHED, AActor, flags3),
	DEFINE_FLAG(MF3, DONTBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, CANBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, NOTARGET, AActor, flags3),
	DEFINE_FLAG(MF3, DONTGIB, AActor, flags3),
	DEFINE_FLAG(MF3, NOBLOCKMONST, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLDEATH, AActor, flags3),
	DEFINE_FLAG(MF3, CANBOUNCEWATER, AActor, flags3),
	DEFINE_FLAG(MF3, NOWALLBOUNCESND, AActor, flags3),
	DEFINE_FLAG(MF3, FOILINVUL, AActor, flags3),
	DEFINE_FLAG(MF3, NOTELEOTHER, AActor, flags3),
	DEFINE_FLAG(MF3, BLOODLESSIMPACT, AActor, flags3),
	DEFINE_FLAG(MF3, NOEXPLODEFLOOR, AActor, flags3),
	DEFINE_FLAG(MF3, PUFFONACTORS, AActor, flags3),
	DEFINE_FLAG(MF4, QUICKTORETALIATE, AActor, flags4),
	DEFINE_FLAG(MF4, NOICEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, RANDOMIZE, AActor, flags4),
	DEFINE_FLAG(MF4, FIXMAPTHINGPOS , AActor, flags4),
	DEFINE_FLAG(MF4, ACTLIKEBRIDGE, AActor, flags4),
	DEFINE_FLAG(MF4, STRIFEDAMAGE, AActor, flags4),
	DEFINE_FLAG(MF4, LONGMELEERANGE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEMORE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEEVENMORE, AActor, flags4),
	DEFINE_FLAG(MF4, SHORTMISSILERANGE, AActor, flags4),
	DEFINE_FLAG(MF4, DONTFALL, AActor, flags4),
	DEFINE_FLAG(MF4, SEESDAGGERS, AActor, flags4),
	DEFINE_FLAG(MF4, INCOMBAT, AActor, flags4),
	DEFINE_FLAG(MF4, LOOKALLAROUND, AActor, flags4),
	DEFINE_FLAG(MF4, STANDSTILL, AActor, flags4),
	DEFINE_FLAG(MF4, SPECTRAL, AActor, flags4),
	DEFINE_FLAG(MF4, FIRERESIST, AActor, flags4),
	DEFINE_FLAG(MF4, NOSPLASHALERT, AActor, flags4),
	DEFINE_FLAG(MF4, SYNCHRONIZED, AActor, flags4),
	DEFINE_FLAG(MF4, NOTARGETSWITCH, AActor, flags4),
	DEFINE_FLAG(MF4, DONTHURTSPECIES, AActor, flags4),
	DEFINE_FLAG(MF4, SHIELDREFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, DEFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, ALLOWPARTICLES, AActor, flags4),
	DEFINE_FLAG(MF4, EXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, NOEXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, FRIGHTENED, AActor, flags4),
	DEFINE_FLAG(MF4, NOBOUNCESOUND, AActor, flags4),
	DEFINE_FLAG(MF4, NOSKIN, AActor, flags4),

	DEFINE_FLAG(MF5, FASTER, AActor, flags5),
	DEFINE_FLAG(MF5, FASTMELEE, AActor, flags5),
	DEFINE_FLAG(MF5, NODROPOFF, AActor, flags5),
	DEFINE_FLAG(MF5, BOUNCEONACTORS, AActor, flags5),
	DEFINE_FLAG(MF5, EXPLODEONWATER, AActor, flags5),
	DEFINE_FLAG(MF5, NODAMAGE, AActor, flags5),
	DEFINE_FLAG(MF5, BLOODSPLATTER, AActor, flags5),
	DEFINE_FLAG(MF5, OLDRADIUSDMG, AActor, flags5),

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
};

static flagdef InventoryFlags[] =
{
	// Inventory flags
	DEFINE_FLAG(IF, QUIET, AInventory, ItemFlags),
	DEFINE_FLAG(IF, AUTOACTIVATE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNDROPPABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INVBAR, AInventory, ItemFlags),
	DEFINE_FLAG(IF, HUBPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INTERHUBSTRIP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, PICKUPFLASH, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ALWAYSPICKUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, FANCYPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, BIGPOWERUP, AInventory, ItemFlags),
};

static flagdef WeaponFlags[] =
{
	// Weapon flags
	DEFINE_FLAG(WIF, NOAUTOFIRE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, READYSNDHALF, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, DONTBOB, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AXEBLOOD, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOALERT, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, ALT_AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, PRIMARY_USES_BOTH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, WIMPY_WEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, POWERED_UP, AWeapon, WeaponFlags),
	//DEFINE_FLAG(WIF, EXTREME_DEATH, AWeapon, WeaponFlags),	// this should be removed now!
	DEFINE_FLAG(WIF, STAFF2_KICKBACK, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, EXPLOSIVE, AWeapon, WeaponFlags),
	DEFINE_FLAG2(WIF_BOT_MELEE, MELEEWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, BFG, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, CHEATNOTWEAPON, AWeapon, WeaponFlags),
	//WIF_BOT_REACTION_SKILL_THING = 1<<31, // I don't understand this
	//DEFINE_FLAG(WIF , HITS_GHOSTS, WeaponFlags),	// I think it would be smarter to remap the THRUGHOST flag to this
};

static const struct { const PClass *Type; flagdef *Defs; int NumDefs; } FlagLists[] =
{
	{ RUNTIME_CLASS(AActor), 		ActorFlags,		sizeof(ActorFlags)/sizeof(flagdef) },
	{ RUNTIME_CLASS(AInventory), 	InventoryFlags,	sizeof(InventoryFlags)/sizeof(flagdef) },
	{ RUNTIME_CLASS(AWeapon), 		WeaponFlags,	sizeof(WeaponFlags)/sizeof(flagdef) }
};
#define NUM_FLAG_LISTS 3

//==========================================================================
//
// Find a flag by name using a binary search
//
//==========================================================================
static int STACK_ARGS flagcmp (const void * a, const void * b)
{
	return stricmp( ((flagdef*)a)->name, ((flagdef*)b)->name);
}

static flagdef *FindFlag (flagdef *flags, int numflags, const char *flag)
{
	int min = 0, max = numflags - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (flag, flags[mid].name);
		if (lexval == 0)
		{
			return &flags[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

static flagdef *FindFlag (const PClass *type, const char *part1, const char *part2)
{
	static bool flagsorted = false;
	flagdef *def;
	int i;

	if (!flagsorted) 
	{
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			qsort (FlagLists[i].Defs, FlagLists[i].NumDefs, sizeof(flagdef), flagcmp);
		}
		flagsorted = true;
	}
	if (part2 == NULL)
	{ // Search all lists
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (type->IsDescendantOf (FlagLists[i].Type))
			{
				def = FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part1);
				if (def != NULL)
				{
					return def;
				}
			}
		}
	}
	else
	{ // Search just the named list
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (stricmp (FlagLists[i].Type->TypeName.GetChars(), part1) == 0)
			{
				if (type->IsDescendantOf (FlagLists[i].Type))
				{
					return FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part2);
				}
				else
				{
					return NULL;
				}
			}
		}
	}
	return NULL;
}

int EvalExpressionI (int id, AActor *self);
//===========================================================================
//
// A_ChangeFlag
//
// This cannot be placed in thingdef_codeptr because it needs the flag table
//
//===========================================================================
void A_ChangeFlag(AActor * self)
{
	int index=CheckIndex(2);
	const char * flagname = FName((ENamedName)StateParameters[index]).GetChars();	
	int expression = EvalExpressionI (StateParameters[index+1], self);

	const char *dot = strchr (flagname, '.');
	flagdef *fd;

	if (dot != NULL)
	{
		FString part1(flagname, dot-flagname);
		fd = FindFlag (self->GetClass(), part1, dot+1);
	}
	else
	{
		fd = FindFlag (self->GetClass(), flagname, NULL);
	}

	if (fd != NULL)
	{
		int * flagp = (int*) (((char*)self) + fd->structoffset);

		if (expression) *flagp |= fd->flagbit;
		else *flagp &= ~fd->flagbit;
	}
	else
	{
		Printf("Unknown flag '%s' in '%s'\n", flagname, self->GetClass()->TypeName.GetChars());
	}
}


//==========================================================================
//
// Action functions
//
//==========================================================================


#include "thingdef_specials.h"

struct AFuncDesc
{
	const char * Name;
	actionf_p Function;
	const char * parameters;
};


// Prototype the code pointers
#define WEAPON(x)	void A_##x(AActor*);	
#define ACTOR(x)	void A_##x(AActor*);
ACTOR(MeleeAttack)
ACTOR(MissileAttack)
ACTOR(ComboAttack)
ACTOR(BulletAttack)
ACTOR(ScreamAndUnblock)
ACTOR(ActiveAndUnblock)
ACTOR(ActiveSound)
ACTOR(FastChase)
ACTOR(CentaurDefend)
ACTOR(FreezeDeath)
ACTOR(FreezeDeathChunks)
ACTOR(GenericFreezeDeath)
ACTOR(IceGuyDie)
ACTOR(M_Saw)
ACTOR(Wander)
ACTOR(Look2)
ACTOR(TossGib)
ACTOR(SentinelBob)
ACTOR(SentinelRefire)
ACTOR(Tracer2)
ACTOR(SetShadow)
ACTOR(ClearShadow)
ACTOR(GetHurt)
ACTOR(TurretLook)
ACTOR(KlaxonBlare)
ACTOR(CheckTerrain)
ACTOR(Countdown)
ACTOR(AlertMonsters)
ACTOR(ClearSoundTarget)
ACTOR(FireAssaultGun)
ACTOR(PlaySound)
ACTOR(PlayWeaponSound)
ACTOR(FLoopActiveSound)
ACTOR(LoopActiveSound)
ACTOR(StopSound)
ACTOR(SeekerMissile)
ACTOR(Jump)
ACTOR(ExplodeParms)
ACTOR(CallSpecial)
ACTOR(CustomMissile)
ACTOR(CustomBulletAttack)
ACTOR(JumpIfHealthLower)
ACTOR(JumpIfCloser)
ACTOR(JumpIfNoAmmo)
ACTOR(JumpIfInventory)
ACTOR(CustomPunch)
ACTOR(FireBullets)
ACTOR(FireCustomMissile)
ACTOR(RailAttack)
ACTOR(CustomRailgun)
ACTOR(LightInverse)
ACTOR(GiveInventory)
ACTOR(TakeInventory)
ACTOR(SpawnItem)
ACTOR(ThrowGrenade)
ACTOR(Recoil)
ACTOR(SelectWeapon)
ACTOR(Print)
ACTOR(SetTranslucent)
ACTOR(FadeIn)
ACTOR(FadeOut)
ACTOR(SpawnDebris)
ACTOR(SetSolid)
ACTOR(UnsetSolid)
ACTOR(SetFloat)
ACTOR(UnsetFloat)
ACTOR(BishopMissileWeave)
ACTOR(CStaffMissileSlither)
ACTOR(CheckSight)
ACTOR(ExtChase)
ACTOR(Jiggle)
ACTOR(DropInventory)
ACTOR(SetBlend)
ACTOR(JumpIf)
ACTOR(SetUserVar)
ACTOR(SetUserVarRandom)
ACTOR(KillMaster)
ACTOR(KillChildren)
ACTOR(DualPainAttack)
ACTOR(GiveToTarget)
ACTOR(TakeFromTarget)
ACTOR(JumpIfInTargetInventory)
ACTOR(CountdownArg)
ACTOR(CustomMeleeAttack)
ACTOR(Light)
ACTOR(Burst)
ACTOR(SkullPop)
ACTOR(CheckFloor)
ACTOR(CheckSkullDone)
ACTOR(RadiusThrust)


#include "d_dehackedactions.h"

#define FUNC(name, parm) { #name, name, parm },
// Declare the code pointer table
AFuncDesc AFTable[]=
{

	// most of the functions available in Dehacked
	FUNC(A_MonsterRail, NULL)
	FUNC(A_BFGSpray, "mxx")
	FUNC(A_Pain, NULL)
	FUNC(A_NoBlocking, NULL)
	FUNC(A_XScream, NULL)
	FUNC(A_Look, NULL)
	FUNC(A_Chase, NULL)
	FUNC(A_FaceTarget, NULL)
	FUNC(A_PosAttack, NULL)
	FUNC(A_Scream, NULL)
	FUNC(A_SPosAttack, NULL)
	FUNC(A_VileChase, NULL)
	FUNC(A_VileStart, NULL)
	FUNC(A_VileTarget, NULL)
	FUNC(A_VileAttack, NULL)
	FUNC(A_StartFire, NULL)
	FUNC(A_Fire, NULL)
	FUNC(A_FireCrackle, NULL)
	FUNC(A_Tracer, NULL)
	FUNC(A_SkelWhoosh, NULL)
	FUNC(A_SkelFist, NULL)
	FUNC(A_SkelMissile, NULL)
	FUNC(A_FatRaise, NULL)
	FUNC(A_FatAttack1, "m")
	FUNC(A_FatAttack2, "m")
	FUNC(A_FatAttack3, "m")
	FUNC(A_BossDeath, NULL)
	FUNC(A_CPosAttack, NULL)
	FUNC(A_CPosRefire, NULL)
	FUNC(A_TroopAttack, NULL)
	FUNC(A_SargAttack, NULL)
	FUNC(A_HeadAttack, NULL)
	FUNC(A_BruisAttack, NULL)
	FUNC(A_SkullAttack, NULL)
	FUNC(A_Metal, NULL)
	FUNC(A_SpidRefire, NULL)
	FUNC(A_BabyMetal, NULL)
	FUNC(A_BspiAttack, NULL)
	FUNC(A_Hoof, NULL)
	FUNC(A_CyberAttack, NULL)
	FUNC(A_PainAttack, "m")
	FUNC(A_DualPainAttack, "m")
	FUNC(A_PainDie, "m")
	FUNC(A_KeenDie, NULL)
	FUNC(A_BrainPain, NULL)
	FUNC(A_BrainScream, NULL)
	FUNC(A_BrainDie, NULL)
	FUNC(A_BrainAwake, NULL)
	FUNC(A_BrainSpit, NULL)
	FUNC(A_SpawnSound, NULL)
	FUNC(A_SpawnFly, NULL)
	FUNC(A_BrainExplode, NULL)
	FUNC(A_Die, NULL)
	FUNC(A_Detonate, NULL)
	FUNC(A_Mushroom, "mx")

	FUNC(A_SetFloorClip, NULL)
	FUNC(A_UnSetFloorClip, NULL)
	FUNC(A_HideThing, NULL)
	FUNC(A_UnHideThing, NULL)
	FUNC(A_SetInvulnerable, NULL)
	FUNC(A_UnSetInvulnerable, NULL)
	FUNC(A_SetReflective, NULL)
	FUNC(A_UnSetReflective, NULL)
	FUNC(A_SetReflectiveInvulnerable, NULL)
	FUNC(A_UnSetReflectiveInvulnerable, NULL)
	FUNC(A_SetShootable, NULL)
	FUNC(A_UnSetShootable, NULL)
	FUNC(A_NoGravity, NULL)
	FUNC(A_Gravity, NULL)
	FUNC(A_LowGravity, NULL)
	{"A_Fall", A_NoBlocking, NULL},		// Allow Doom's old name, too, for this function
	FUNC(A_SetSolid, NULL)
	FUNC(A_UnsetSolid, NULL)
	FUNC(A_SetFloat, NULL)
	FUNC(A_UnsetFloat, NULL)

	// For better chainsaw attacks
	FUNC(A_M_Saw, NULL)

	// some functions from the old DECORATE parser
	FUNC(A_BulletAttack, NULL)
	FUNC(A_ScreamAndUnblock, NULL)
	FUNC(A_ActiveAndUnblock, NULL)
	FUNC(A_ActiveSound, NULL)

	// useful functions from Hexen
	FUNC(A_FastChase, NULL)
	FUNC(A_FreezeDeath, NULL)
	FUNC(A_FreezeDeathChunks, NULL)
	FUNC(A_GenericFreezeDeath, NULL)
	FUNC(A_IceGuyDie, NULL)			// useful for bursting a monster into ice chunks without delay
	FUNC(A_CentaurDefend, NULL)
	FUNC(A_BishopMissileWeave, NULL)
	FUNC(A_CStaffMissileSlither, NULL)
	FUNC(A_PlayerScream, NULL)
	FUNC(A_SkullPop, "m")
	{"A_CheckPlayerDone", A_CheckSkullDone, NULL },

	// useful functions from Strife
	FUNC(A_Wander, NULL)
	FUNC(A_Look2, NULL)
	FUNC(A_TossGib, NULL)
	FUNC(A_SentinelBob, NULL)
	FUNC(A_SentinelRefire, NULL)
	FUNC(A_Tracer2, NULL)
	FUNC(A_SetShadow, NULL)
	FUNC(A_ClearShadow, NULL)
	FUNC(A_GetHurt, NULL)
	FUNC(A_TurretLook, NULL)
	FUNC(A_KlaxonBlare, NULL)
	FUNC(A_Countdown, NULL)
	FUNC(A_AlertMonsters, NULL)
	FUNC(A_ClearSoundTarget, NULL)
	FUNC(A_FireAssaultGun, NULL)
	FUNC(A_CheckTerrain, NULL )

	// Only selected original weapon functions will be available. 
	// All the attack pointers are somewhat tricky due to the way the flash state is handled
	FUNC(A_Light, "X")
	FUNC(A_Light0, NULL)
	FUNC(A_Light1, NULL)
	FUNC(A_Light2, NULL)
	FUNC(A_LightInverse, NULL)
	FUNC(A_WeaponReady, NULL)
	FUNC(A_Lower, NULL)
	FUNC(A_Raise, NULL)
	FUNC(A_ReFire, NULL)
	FUNC(A_Punch, NULL)
	FUNC(A_CheckReload, NULL)
	FUNC(A_GunFlash, NULL)
	FUNC(A_Saw, NULL)

	// DECORATE specific functions
	FUNC(A_BulletAttack, NULL)
	FUNC(A_PlaySound, "S" )
	FUNC(A_PlayWeaponSound, "S" )
	FUNC(A_FLoopActiveSound, NULL )
	FUNC(A_LoopActiveSound, NULL )
	FUNC(A_StopSound, NULL )
	FUNC(A_SeekerMissile, "XX" )
	FUNC(A_Jump, "XL" )
	FUNC(A_CustomMissile, "MXXxxx" )
	FUNC(A_CustomBulletAttack, "XXXXmx" )
	FUNC(A_CustomRailgun, "Xxccxxx" )
	FUNC(A_JumpIfHealthLower, "XL" )
	FUNC(A_JumpIfCloser, "XL" )
	FUNC(A_JumpIfInventory, "MXL" )
	FUNC(A_GiveInventory, "Mx" )
	FUNC(A_TakeInventory, "Mx" )
	FUNC(A_SpawnItem, "Mxxy" )
	FUNC(A_ThrowGrenade, "Mxxxy" )
	FUNC(A_SelectWeapon, "M")
	FUNC(A_Print, "T")
	FUNC(A_SetTranslucent, "Xx")
	FUNC(A_FadeIn, "x")
	FUNC(A_FadeOut, "x")
	FUNC(A_SpawnDebris, "M")
	FUNC(A_CheckSight, "L")
	FUNC(A_ExtChase, "XXyx")
	FUNC(A_Jiggle, "XX")
	FUNC(A_DropInventory, "M")
	FUNC(A_SetBlend, "CXXc")
	FUNC(A_ChangeFlag, "TX")
	FUNC(A_JumpIf, "XL")
	FUNC(A_KillMaster, NULL)
	FUNC(A_KillChildren, NULL)
	FUNC(A_CheckFloor, "L")
	{"A_BasicAttack", A_ComboAttack, "ISMF" },

	// Weapon only functions
	FUNC(A_JumpIfNoAmmo, "L" )
	FUNC(A_CustomPunch, "Xxymx" )
	FUNC(A_FireBullets, "XXXXmyx" )
	FUNC(A_FireCustomMissile, "Mxyxx" )
	FUNC(A_RailAttack, "Xxyccxx" )
	FUNC(A_Recoil, "X")
	FUNC(A_JumpIfInTargetInventory, "MXL" )
	FUNC(A_GiveToTarget, "Mx" )
	FUNC(A_TakeFromTarget, "Mx" )
	FUNC(A_CountdownArg, "X")
	FUNC(A_CustomMeleeAttack, "XXXsty" )
	FUNC(A_Burst, "M")
	FUNC(A_RadiusThrust, "xxy")
	{"A_Explode", A_ExplodeParms, "xxy" },
};

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================
static int STACK_ARGS funccmp(const void * a, const void * b)
{
	return stricmp( ((AFuncDesc*)a)->Name, ((AFuncDesc*)b)->Name);
}

static AFuncDesc * FindFunction(char * string)
{
	static bool funcsorted=false;

	if (!funcsorted) 
	{
		qsort(AFTable, countof(AFTable), sizeof(AFTable[0]), funccmp);
		funcsorted=true;
	}

	int min = 0, max = countof(AFTable)-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, AFTable[mid].Name);
		if (lexval == 0)
		{
			return &AFTable[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}


static const char *BasicAttackNames[4] =
{
	"A_MeleeAttack",
	"A_MissileAttack",
	"A_ComboAttack",
	NULL
};
static const actionf_p BasicAttacks[3] =
{
	A_MeleeAttack,
	A_MissileAttack,
	A_ComboAttack
};



//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// Translation parsing
//
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int NumUsedTranslations;
static int NumUsedBloodTranslations;
byte decorate_translations[256*256*2];
PalEntry BloodTranslations[256];

void InitDecorateTranslations()
{
	// The translation tables haven't been allocated yet so we may as easily use a static buffer instead!
	NumUsedBloodTranslations = NumUsedTranslations = 0;
	for(int i=0;i<256*256*2;i++) decorate_translations[i]=i&255;
}

static bool Check(char *& range,  char c, bool error=true)
{
	while (isspace(*range)) range++;
	if (*range==c)
	{
		range++;
		return true;
	}
	if (error)
	{
		//SC_ScriptError("Invalid syntax in translation specification: '%c' expected", c);
	}
	return false;
}


static void AddToTranslation(unsigned char * translation, char * range)
{
	int start,end;

	start=strtol(range, &range, 10);
	if (!Check(range, ':')) return;
	end=strtol(range, &range, 10);
	if (!Check(range, '=')) return;
	if (!Check(range, '[', false))
	{
		int pal1,pal2;
		fixed_t palcol, palstep;

		pal1=strtol(range, &range, 10);
		if (!Check(range, ':')) return;
		pal2=strtol(range, &range, 10);

		if (start > end)
		{
			swap (start, end);
			swap (pal1, pal2);
		}
		else if (start == end)
		{
			translation[start] = pal1;
			return;
		}
		palcol = pal1 << FRACBITS;
		palstep = ((pal2 << FRACBITS) - palcol) / (end - start);
		for (int i = start; i <= end; palcol += palstep, ++i)
		{
			translation[i] = palcol >> FRACBITS;
		}
	}
	else
	{ 
		// translation using RGB values
		int r1;
		int g1;
		int b1;
		int r2;
		int g2;
		int b2;
		int r,g,b;
		int rs,gs,bs;

		r1=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		g1=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		b1=strtol(range, &range, 10);
		if (!Check(range, ']')) return;
		if (!Check(range, ':')) return;
		if (!Check(range, '[')) return;
		r2=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		g2=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		b2=strtol(range, &range, 10);
		if (!Check(range, ']')) return;

		if (start > end)
		{
			swap (start, end);
			r = r2;
			g = g2;
			b = b2;
			rs = r1 - r2;
			gs = g1 - g2;
			bs = b1 - b2;
		}
		else
		{
			r = r1;
			g = g1;
			b = b1;
			rs = r2 - r1;
			gs = g2 - g1;
			bs = b2 - b1;
		}
		if (start == end)
		{
			translation[start] = ColorMatcher.Pick
				(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
			return;
		}
		rs /= (end - start);
		gs /= (end - start);
		bs /= (end - start);
		for (int i = start; i <= end; ++i)
		{
			translation[i] = ColorMatcher.Pick
				(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
			r += rs;
			g += gs;
			b += bs;
		}

	}
}

static int StoreTranslation(const unsigned char * translation)
{
	for (int i = 0; i < NumUsedTranslations; i++)
	{
		if (!memcmp(translation, decorate_translations + i*256, 256))
		{
			// A duplicate of this translation already exists
			return TRANSLATION(TRANSLATION_Decorate, i);
		}
	}
	if (NumUsedTranslations>=MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many translations in DECORATE");
	}
	memcpy(decorate_translations + NumUsedTranslations*256, translation, 256);
	NumUsedTranslations++;
	return TRANSLATION(TRANSLATION_Decorate, NumUsedTranslations-1);
}

static int CreateBloodTranslation(PalEntry color)
{
	int i;

	for (i = 0; i < NumUsedBloodTranslations; i++)
	{
		if (color.r == BloodTranslations[i].r &&
			color.g == BloodTranslations[i].g &&
			color.b == BloodTranslations[i].b)
		{
			// A duplicate of this translation already exists
			return i;
		}
	}
	if (NumUsedBloodTranslations>=MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many blood colors in DECORATE");
	}
	for (i = 0; i < 256; i++)
	{
		int bright = MAX(MAX(GPalette.BaseColors[i].r, GPalette.BaseColors[i].g), GPalette.BaseColors[i].b);
		int entry = ColorMatcher.Pick(color.r*bright/255, color.g*bright/255, color.b*bright/255);

		*(decorate_translations + MAX_DECORATE_TRANSLATIONS*256 + NumUsedBloodTranslations*256 + i)=entry;
	}
	BloodTranslations[NumUsedBloodTranslations]=color;
	return NumUsedBloodTranslations++;
}

//----------------------------------------------------------------------------
//
// DropItem handling
//
//----------------------------------------------------------------------------

static void FreeDropItemChain(FDropItem *chain)
{
	while (chain != NULL)
	{
		FDropItem *next = chain->Next;
		delete chain;
		chain = next;
	}
}

class FDropItemPtrArray : public TArray<FDropItem *>
{
public:
	~FDropItemPtrArray()
	{
		for (unsigned int i = 0; i < Size(); ++i)
		{
			FreeDropItemChain ((*this)[i]);
		}
	}
};

static FDropItemPtrArray DropItemList;

FDropItem *GetDropItems(AActor * actor)
{
	unsigned int index = actor->GetClass ()->Meta.GetMetaInt (ACMETA_DropItems) - 1;

	if (index >= 0 && index < DropItemList.Size())
	{
		return DropItemList[index];
	}
	return NULL;
}

//==========================================================================
//
// Extra info maintained while defining an actor. The original
// implementation stored these in a CustomActor class. They have all been
// moved into action function parameters so that no special CustomActor
// class is necessary.
//
//==========================================================================

struct FBasicAttack
{
	int MeleeDamage;
	int MeleeSound;
	FName MissileName;
	fixed_t MissileHeight;
};

struct Baggage
{
	FActorInfo *Info;
	bool DropItemSet;
	bool StateSet;
	int CurrentState;

	FDropItem *DropItemList;
	FBasicAttack BAttack;
};


//==========================================================================
//
//
//==========================================================================

static TArray<FState> StateArray;



typedef void (*ActorPropFunction) (AActor *defaults, Baggage &bag);

struct ActorProps { const char *name; ActorPropFunction Handler; const PClass * type; };

typedef ActorProps (*ActorPropHandler) (register const char *str, register unsigned int len);

static const ActorProps *is_actorprop (const char *str);

//==========================================================================
//
// Some functions which check for simple tokens
//
//==========================================================================

inline void ChkCom()
{
	SC_MustGetStringName (",");
}
	
inline void ChkBraceOpn()
{
	SC_MustGetStringName ("{");
}
	
inline bool TestBraceCls()
{
	return SC_CheckString ("}");
}

inline bool TestCom()
{
	return SC_CheckString (",");
}

//==========================================================================
//
// Find a state address
//
//==========================================================================

// These strings must be in the same order as the respective variables in the actor struct!
static const char * actor_statenames[]={"SPAWN","SEE","PAIN","MELEE","MISSILE","CRASH", "DEATH",
										"XDEATH", "BURN","ICE","DISINTEGRATE","RAISE","WOUND","HEAL",
										"CRUSH", "YES", "NO", "GREETINGS", NULL};

static const char * weapon_statenames[]={"SELECT", "DESELECT", "READY", "FIRE", "HOLD",
										 "ALTFIRE", "ALTHOLD", "FLASH", "ALTFLASH", NULL };

static const char * inventory_statenames[]={"USE", "PICKUP", "DROP", NULL };


FState ** FindState(AActor * actor, const PClass * type, const char * name)
{
	int i;

	for(i=0;actor_statenames[i];i++)
	{
		if (!stricmp(actor_statenames[i],name))
			return (&actor->SpawnState)+i;
	}
	if (type->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{
		for(i=0;weapon_statenames[i];i++)
		{
			if (!stricmp(weapon_statenames[i],name))
				return (&static_cast<AWeapon*>(actor)->UpState)+i;
		}
	}
	else if (type->IsDescendantOf (RUNTIME_CLASS(ACustomInventory)))
	{
		for(i=0;inventory_statenames[i];i++)
		{
			if (!stricmp(inventory_statenames[i],name))
				return (&static_cast<ACustomInventory*>(actor)->UseState)+i;
		}
	}
	else if (type->IsDescendantOf (RUNTIME_CLASS(ASwitchableDecoration)))
	{
		// These are the names that the code will use when custom labels are working.
		if (!stricmp(name, "ACTIVE")) return &actor->SeeState;
		if (!stricmp(name, "INACTIVE")) return &actor->MeleeState;
	} 
   return NULL;
}


//==========================================================================
//
// Sets the default values with which an actor definition starts
//
//==========================================================================

static void ResetBaggage (Baggage *bag)
{
	bag->DropItemList = NULL;
	bag->BAttack.MeleeDamage = 0;
	bag->BAttack.MissileHeight = 32*FRACUNIT;
	bag->BAttack.MeleeSound = 0;
	bag->BAttack.MissileName = NAME_None;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->StateSet = false;
}

static void ResetActor (AActor *actor, Baggage *bag)
{
	memcpy (actor, GetDefault<AActor>(), sizeof(AActor));
	if (bag->DropItemList != NULL)
	{
		FreeDropItemChain (bag->DropItemList);
	}
	ResetBaggage (bag);
}



//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo * CreateNewActor(FActorInfo ** parentc, Baggage *bag)
{
	FName typeName;

	// Get actor name
	SC_MustGetString();
	
	char * colon = strchr(sc_String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	if (PClass::FindClass (sc_String) != NULL)
	{
		SC_ScriptError ("Actor %s is already defined.", sc_String);
	}

	typeName = sc_String;

	PClass * parent = RUNTIME_CLASS(AActor);
	if (parentc)
	{
		*parentc = NULL;
		
		// Do some tweaking so that a definition like 'Actor:Parent' which is read as a single token is recognized as well
		// without having resort to C-mode (which disallows periods in actor names.)
		if (colon == NULL)
		{
			SC_MustGetString ();
			if (sc_String[0]==':')
			{
				colon = sc_String + 1;
			}
		}
		
		if (colon != NULL)
		{
			if (colon[0] == 0)
			{
				SC_MustGetString ();
				colon = sc_String;
			}
		}
			
		if (colon != NULL)
		{
			parent = const_cast<PClass *> (PClass::FindClass (colon));

			if (parent == NULL)
			{
				SC_ScriptError ("Parent type '%s' not found", colon);
			}
			else if (parent->ActorInfo == NULL)
			{
				SC_ScriptError ("Parent type '%s' is not an actor", colon);
			}
			else
			{
				*parentc = parent->ActorInfo;
			}
		}
		else SC_UnGet();
	}

	PClass * ti = parent->CreateDerivedClass (typeName, parent->Size);
	FActorInfo * info = ti->ActorInfo;

	Decorations.Push (info);
	info->NumOwnedStates = 0;
	info->OwnedStates = NULL;
	info->SpawnID = 0;

	ResetBaggage (bag);
	bag->Info = info;

	info->DoomEdNum = -1;

	// Check for "replaces"
	SC_MustGetString ();
	if (SC_Compare ("replaces"))
	{
		const PClass *replacee;

		// Get actor name
		SC_MustGetString ();
		replacee = PClass::FindClass (sc_String);

		if (replacee == NULL)
		{
			SC_ScriptError ("Replaced type '%s' not found", sc_String);
		}
		else if (replacee->ActorInfo == NULL)
		{
			SC_ScriptError ("Replaced type '%s' is not an actor", sc_String);
		}
		replacee->ActorInfo->Replacement = ti->ActorInfo;
		ti->ActorInfo->Replacee = replacee->ActorInfo;
	}
	else
	{
		SC_UnGet();
	}

	// Now, after the actor names have been parsed, it is time to switch to C-mode 
	// for the rest of the actor definition.
	SC_SetCMode (true);
	if (SC_CheckNumber()) 
	{
		if (sc_Number>=-1 && sc_Number<32768) info->DoomEdNum = sc_Number;
		else SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}
	if (parent == RUNTIME_CLASS(AWeapon))
	{
		// preinitialize kickback to the default for the game
		((AWeapon*)(info->Class->Defaults))->Kickback=gameinfo.defKickback;
	}

	return info;
}

//==========================================================================
//
// PrepareStateParameters
// creates an empty parameter list for a parameterized function call
//
//==========================================================================
int PrepareStateParameters(FState * state, int numparams)
{
	int paramindex=StateParameters.Size();
	int i, v;

	if (state->Frame&SF_BIGTIC)
	{
		SC_ScriptError("You cannot use a parameterized code pointer with a state duration larger than 254!");
	}

	v=state->Misc1;
	StateParameters.Push(v);
	v=state->Misc2;
	StateParameters.Push(v);
	v=0;
	for(i=0;i<numparams;i++) StateParameters.Push(v);
	state->SetMisc1_2(paramindex);
	state->Frame|=SF_STATEPARAM;

	return paramindex+2;	// return the index of the first actual state parameter
}

//==========================================================================
//
// Returns the index of the given line special
//
//==========================================================================
int FindLineSpecial(const char * string)
{
	const ACSspecials *spec;

	spec = is_special(string, (unsigned int)strlen(string));
	if (spec) return spec->Special;
	return 0;
}

//==========================================================================
//
// DoSpecialFunctions
// handles special functions that can't be dealt with by the generic routine
//
//==========================================================================
bool DoSpecialFunctions(FState & state, bool multistate, int * statecount, Baggage &bag)
{
	int i;
	const ACSspecials *spec;

	if ((spec = is_special (sc_String, sc_StringLen)) != NULL)
	{

		int paramindex=PrepareStateParameters(&state, 6);

		StateParameters[paramindex]=spec->Special;

		// Make this consistent with all other parameter parsing
		if (SC_CheckString("("))
		{
			for (i = 0; i < 5;)
			{
				StateParameters[paramindex+i+1]=ParseExpression (false);
				i++;
				if (!TestCom()) break;
			}
			SC_MustGetStringName (")");
		}
		else i=0;

		if (i < spec->MinArgs)
		{
			SC_ScriptError ("Too few arguments to %s", spec->name);
		}
		if (i > MAX (spec->MinArgs, spec->MaxArgs))
		{
			SC_ScriptError ("Too many arguments to %s", spec->name);
		}
		state.Action = A_CallSpecial;
		return true;
	}

	// Check for A_MeleeAttack, A_MissileAttack, or A_ComboAttack
	int batk = SC_MatchString (BasicAttackNames);
	if (batk >= 0)
	{
		int paramindex=PrepareStateParameters(&state, 4);

		StateParameters[paramindex] = bag.BAttack.MeleeDamage;
		StateParameters[paramindex+1] = bag.BAttack.MeleeSound;
		StateParameters[paramindex+2] = bag.BAttack.MissileName;
		StateParameters[paramindex+3] = bag.BAttack.MissileHeight;
		state.Action = BasicAttacks[batk];
		return true;
	}
	return false;
}

//==========================================================================
//
// RetargetState(Pointer)s
//
// These functions are used when a goto follows one or more labels.
// Because multiple labels are permitted to occur consecutively with no
// intervening states, it is not enough to remember the last label defined
// and adjust it. So these functions search for all labels that point to
// the current position in the state array and give them a copy of the
// target string instead.
//
//==========================================================================

static void RetargetStatePointers (intptr_t count, const char *target, FState **start, FState **stop)
{
	for (FState **probe = start; probe <= stop; ++probe)
	{
		if (*probe == (FState *)count)
		{
			*probe = target == NULL ? NULL : (FState *)copystring (target);
		}
	}
}

static void RetargetStates (intptr_t count, const char *target, const PClass *cls, AActor *defaults)
{
	RetargetStatePointers (count, target, &defaults->SpawnState, &defaults->GreetingsState);
	if (cls->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{
		AWeapon *weapon = (AWeapon *)defaults;
		RetargetStatePointers (count, target, &weapon->UpState, &weapon->AltFlashState);
	}
	if (cls->IsDescendantOf (RUNTIME_CLASS(ACustomInventory)))
	{
		ACustomInventory *item = (ACustomInventory *)defaults;
		RetargetStatePointers (count, target, &item->UseState, &item->DropState);
	}
}

//==========================================================================
//
// ProcessStates
// processes a state block
//
//==========================================================================

static int ProcessStates(FActorInfo * actor, AActor * defaults, Baggage &bag)
{
	char statestring[256];
	intptr_t count = 0;
	FState state;
	FState * laststate = NULL;
	intptr_t lastlabel = -1;
	FState ** stp;
	int minrequiredstate = -1;

	statestring[255] = 0;

	ChkBraceOpn();
	while (!TestBraceCls() && !sc_End)
	{
		memset(&state,0,sizeof(state));
		SC_MustGetString();
		if (SC_Compare("GOTO"))
		{
do_goto:	SC_MustGetString();
			strncpy (statestring, sc_String, 255);
			if (SC_CheckString ("."))
			{
				SC_MustGetString ();
				strcat (statestring, ".");
				strcat (statestring, sc_String);
			}
			if (SC_CheckString ("+"))
			{
				SC_MustGetNumber ();
				strcat (statestring, "+");
				strcat (statestring, sc_String);
			}
			// copy the text - this must be resolved later!
			if (laststate != NULL)
			{ // Following a state definition: Modify it.
				laststate->NextState = (FState*)copystring(statestring);	
			}
			else if (lastlabel >= 0)
			{ // Following a label: Retarget it.
				RetargetStates (count+1, statestring, actor->Class, defaults);
			}
			else
			{
				SC_ScriptError("GOTO before first state");
			}
		}
		else if (SC_Compare("STOP"))
		{
do_stop:
			if (laststate!=NULL)
			{
				laststate->NextState=(FState*)-1;
			}
			else if (lastlabel >=0)
			{
				RetargetStates (count+1, NULL, actor->Class, defaults);
			}
			else
			{
				SC_ScriptError("STOP before first state");
				continue;
			}
		}
		else if (SC_Compare("WAIT") || SC_Compare("FAIL"))
		{
			if (!laststate) 
			{
				SC_ScriptError("%s before first state", sc_String);
				continue;
			}
			laststate->NextState=(FState*)-2;
		}
		else if (SC_Compare("LOOP"))
		{
			if (!laststate) 
			{
				SC_ScriptError("LOOP before first state");
				continue;
			}
			laststate->NextState=(FState*)(lastlabel+1);
		}
		else
		{
			char * statestrp;

			strncpy(statestring, sc_String, 255);
			SC_SetEscape(false);
			SC_MustGetString();	// this can read the frame string so escape characters have to be disabled
			SC_SetEscape(true);

			if (SC_Compare (":"))
			{
				laststate = NULL;
				do
				{
					lastlabel = count;
					stp = FindState(defaults, bag.Info->Class, statestring);
					if (stp) *stp=(FState *) (count+1);
					else 
						SC_ScriptError("Unknown state label %s", statestring);
					SC_MustGetString ();
					if (SC_Compare ("Goto"))
					{
						goto do_goto;
					}
					else if (SC_Compare("Stop"))
					{
						goto do_stop;
					}
					strncpy(statestring, sc_String, 255);
					SC_MustGetString ();
				} while (SC_Compare (":"));
//				continue;
			}

			SC_UnGet ();

			if (strlen (statestring) != 4)
			{
				SC_ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			memcpy(state.sprite.name, statestring, 4);
			state.Misc1=state.Misc2=0;
			SC_SetEscape(false);
			SC_MustGetString();	// This reads the frame string so escape characters have to be disabled
			SC_SetEscape(true);
			strncpy(statestring, sc_String + 1, 255);
			statestrp = statestring;
			state.Frame=(*sc_String&223)-'A';
			if ((*sc_String&223)<'A' || (*sc_String&223)>']')
			{
				SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
				state.Frame=0;
			}

			SC_MustGetNumber();
			sc_Number++;
			state.Tics=sc_Number&255;
			state.Misc1=(sc_Number>>8)&255;
			if (state.Misc1) state.Frame|=SF_BIGTIC;

			while (SC_GetString() && !sc_Crossed)
			{
				if (SC_Compare("BRIGHT")) 
				{
					state.Frame|=SF_FULLBRIGHT;
					continue;
				}
				if (SC_Compare("OFFSET"))
				{
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use OFFSET with a state duration larger than 254!");
					}
					// specify a weapon offset
					SC_MustGetStringName("(");
					SC_MustGetNumber();
					state.Misc1=sc_Number;
					ChkCom();
					SC_MustGetNumber();
					state.Misc2=sc_Number;
					SC_MustGetStringName(")");
					continue;
				}

				// Make the action name lowercase to satisfy the gperf hashers
				strlwr (sc_String);

				int minreq=count;
				if (DoSpecialFunctions(state,strlen(statestring)>0, &minreq, bag))
				{
					if (minreq>minrequiredstate) minrequiredstate=minreq;
					goto endofstate;
				}

				AFuncDesc * afd = FindFunction(sc_String);
				if (afd != NULL)
				{
					state.Action = afd->Function;
					if (afd->parameters !=NULL)
					{
						const char * params = afd->parameters;
						int numparams = (int)strlen(params);
						int v;

						if (!islower(*params))
						{
							SC_MustGetStringName("(");
						}
						else
						{
							if (!SC_CheckString("(")) goto endofstate;
						}
						
						int paramindex = PrepareStateParameters(&state, numparams);
						while (*params)
						{
							switch(*params)
							{
							/*
							case 'A':
							case 'a':		// Angle
								SC_MustGetFloat();
								v=(int)angle_t(sc_Float*ANGLE_1);
								break;

							case 'B':
							case 'b':		// Byte
								SC_MustGetNumber();
								v=clamp<int>(sc_Number, 0, 255);
								break;

							case '9':		// 90 degree angle as integer
								SC_MustGetNumber();
								v=clamp<int>(sc_Number, 0, 90);
								break;

							case '!':		// not boolean (to simulate parameters which default to 1)
								SC_MustGetNumber();
								v=!sc_Number;
								break;

							*/

							case 'I':
							case 'i':		// Integer
								SC_MustGetNumber();
								v=sc_Number;
								break;

							case 'F':
							case 'f':		// Fixed point
								SC_MustGetFloat();
								v=fixed_t(sc_Float*FRACUNIT);
								break;


							case 'S':
							case 's':		// Sound name
								SC_MustGetString();
								v=S_FindSound(sc_String);
								break;

							case 'M':
							case 'm':		// Actor name
							case 'T':
							case 't':		// String
								SC_MustGetString();
								v = (int)(sc_String[0] ? FName(sc_String) : NAME_None);
								break;

							case 'L':
							case 'l':		// Jump label

								if (strlen(statestring)>0)
								{
									SC_ScriptError("You cannot use A_Jump commands on multistate definitions\n");
								}

								SC_MustGetNumber();
								v=sc_Number;
								if (v<1)
								{
									SC_ScriptError("Negative jump offsets are not allowed");
								}

								{
									int minreq=count+v;
									if (minreq>minrequiredstate) minrequiredstate=minreq;
								}

								break;

							case 'C':
							case 'c':
								SC_MustGetString ();
								if (SC_Compare("none"))
								{
									v = -1;
								}
								else
								{
									int c = V_GetColor (NULL, sc_String);
									// 0 needs to be the default so we have to mark the color.
									v = MAKEARGB(1, RPART(c), GPART(c), BPART(c));
								}
								break;

							case 'X':
							case 'x':
								v = ParseExpression (false);
								break;

							case 'Y':
							case 'y':
								v = ParseExpression (true);
								break;

							default:
								assert(false);
								v = -1;
								break;
							}
							StateParameters[paramindex++]=v;
							params++;
							if (*params)
							{
								if ((islower(*params) || *params=='!') && SC_CheckString(")")) goto endofstate;
								ChkCom();
							}
						}
						SC_MustGetStringName(")");
					}
					else 
					{
						SC_MustGetString();
						if (SC_Compare("("))
						{
							SC_ScriptError("You cannot pass parameters to '%s'\n",sc_String);
						}
						SC_UnGet();
					}
					goto endofstate;
				}
				SC_ScriptError("Invalid state parameter %s\n", sc_String);
			}
			SC_UnGet();
endofstate:
			StateArray.Push(state);
			while (*statestrp)
			{
				int frame=((*statestrp++)&223)-'A';

				if (frame<0 || frame>28)
				{
					SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
					frame=0;
				}

				state.Frame=(state.Frame&(SF_FULLBRIGHT|SF_BIGTIC|SF_STATEPARAM))|frame;
				StateArray.Push(state);
				count++;
			}
			laststate=&StateArray[count];
			count++;
		}
	}
	if (count<=minrequiredstate)
	{
		SC_ScriptError("A_Jump offset out of range in %s", actor->Class->TypeName.GetChars());
	}
	return count;
}

//==========================================================================
//
// ResolveGotoLabel
//
//==========================================================================

static FState *ResolveGotoLabel (AActor *actor, const PClass *type, char *name)
{
	FState **stp, *state;
	char *namestart = name;
	char *label, *offset, *pt;
	int v;

	// Check for classname
	if ((pt = strchr (name, '.')) != NULL)
	{
		const char *classname = name;
		*pt = '\0';
		name = pt + 1;

		// The classname may either be "Super" to identify this class's immediate
		// superclass, or it may be the name of any class that this one derives from.
		if (stricmp (classname, "Super") == 0)
		{
			type = type->ParentClass;
			actor = GetDefaultByType (type);
		}
		else
		{
			const PClass *stype = PClass::FindClass (classname);
			if (stype == NULL)
			{
				SC_ScriptError ("%s is an unknown class.", classname);
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				SC_ScriptError ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (type))
			{
				SC_ScriptError ("%s is not derived from %s so cannot access its states.",
					type->TypeName.GetChars(), stype->TypeName.GetChars());
			}
			if (type != stype)
			{
				type = stype;
				actor = GetDefaultByType (type);
			}
		}
	}
	label = name;
	// Check for offset
	offset = NULL;
	if ((pt = strchr (name, '+')) != NULL)
	{
		*pt = '\0';
		offset = pt + 1;
	}
	v = offset ? strtol (offset, NULL, 0) : 0;

	// Calculate the state's address.
	stp = FindState (actor, type, label);
	state = NULL;
	if (stp != NULL)
	{
		if (*stp != NULL)
		{
			state = *stp + v;
		}
		else if (v != 0)
		{
			SC_ScriptError ("Attempt to get invalid state from actor %s.", type->TypeName.GetChars());
		}
	}
	else
	{
		SC_ScriptError("Unknown state label %s", label);
	}
	delete[] namestart;		// free the allocated string buffer
	return state;
}

//==========================================================================
//
// FixStatePointers
//
// Fixes an actor's default state pointers.
//
//==========================================================================

static void FixStatePointers (FActorInfo *actor, FState **start, FState **stop)
{
	FState **stp;
	size_t v;

	for (stp = start; stp <= stop; ++stp)
	{
		v = (size_t)*stp;
		if (v >= 1 && v < 0x10000)
		{
			*stp = actor->OwnedStates + v - 1;
		}
	}
}

//==========================================================================
//
// FixStatePointersAgain
//
// Resolves an actor's state pointers that were specified as jumps.
//
//==========================================================================

static void FixStatePointersAgain (FActorInfo *actor, AActor *defaults, FState **start, FState **stop)
{
	FState **stp;

	for (stp = start; stp <= stop; ++stp)
	{
		if (*stp != NULL && FState::StaticFindStateOwner (*stp, actor) == NULL)
		{ // It's not a valid state, so it must be a label string. Resolve it.
			*stp = ResolveGotoLabel (defaults, actor->Class, (char *)*stp);
		}
	}
}

//==========================================================================
//
// FinishStates
// copies a state block and fixes all state links
//
//==========================================================================
static int FinishStates (FActorInfo *actor, AActor *defaults, Baggage &bag)
{
	int count = StateArray.Size();

	if (count > 0)
	{
		FState *realstates = new FState[count];
		int i;
		int currange;

		memcpy(realstates, &StateArray[0], count*sizeof(FState));
		actor->OwnedStates = realstates;
		actor->NumOwnedStates = count;

		// adjust the state pointers
		// In the case new states are added these must be adjusted, too!
		FixStatePointers (actor, &defaults->SpawnState, &defaults->GreetingsState);
		if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon *weapon = (AWeapon*)defaults;

			FixStatePointers (actor, &weapon->UpState, &weapon->AltFlashState);
		}
		if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ACustomInventory)))
		{
			ACustomInventory *item = (ACustomInventory*)defaults;

			FixStatePointers (actor, &item->UseState, &item->DropState);
		}

		for(i = currange = 0; i < count; i++)
		{
			// resolve labels and jumps
			switch((ptrdiff_t)realstates[i].NextState)
			{
			case 0:		// next
				realstates[i].NextState = (i < count-1 ? &realstates[i+1] : &realstates[0]);
				break;

			case -1:	// stop
				realstates[i].NextState = NULL;
				break;

			case -2:	// wait
				realstates[i].NextState = &realstates[i];
				break;

			default:	// loop
				if ((size_t)realstates[i].NextState < 0x10000)
				{
					realstates[i].NextState = &realstates[(size_t)realstates[i].NextState-1];
				}
				else	// goto
				{
					realstates[i].NextState = ResolveGotoLabel (defaults, bag.Info->Class, (char *)realstates[i].NextState);
				}
			}
		}
	}
	StateArray.Clear ();

	// Fix state pointers that are gotos
	FixStatePointersAgain (actor, defaults, &defaults->SpawnState, &defaults->GreetingsState);
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
	{
		AWeapon *weapon = (AWeapon*)defaults;

		FixStatePointersAgain (actor, defaults, &weapon->UpState, &weapon->AltFlashState);
	}
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ACustomInventory)))
	{
		ACustomInventory *item = (ACustomInventory*)defaults;

		FixStatePointersAgain (actor, defaults, &item->UseState, &item->DropState);
	}

	return count;
}

//==========================================================================
//
// For getting a state address from the parent
//
//==========================================================================
static FState *CheckState(int statenum, PClass *type)
{
	if (SC_GetString() && !sc_Crossed)
	{
		if (SC_Compare("0")) return NULL;
		else if (SC_Compare("PARENT"))
		{
			SC_MustGetString();

			FState * basestate;
			FState ** stp=FindState((AActor*)type->ParentClass->Defaults, type, sc_String);
			int v = 0;

			if (stp) basestate =*stp;
			else 
			{
				SC_ScriptError("Unknown state label %s",(const char **)&sc_String);
				return NULL;
			}

			if (SC_GetString ())
			{
				if (SC_Compare ("+"))
				{
					SC_MustGetNumber ();
					v = sc_Number;
				}
				else
				{
					SC_UnGet ();
				}
			}

			if (!basestate && !v) return NULL;
			basestate+=v;

			if (v && !basestate)
			{
				SC_ScriptError("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				return NULL;
			}
			return basestate;
		}
		else SC_ScriptError("Invalid state assignment");
	}
	return NULL;
}




//==========================================================================
//
// Handle actor properties
//
//==========================================================================
void ParseActorProperties (Baggage &bag)
{
	const PClass *info;
	const ActorProps *prop;

	ChkBraceOpn ();
	while (!TestBraceCls())
	{
		if (sc_End)
		{
			SC_ScriptError("Unexpected end of file encountered");
		}

		SC_GetString ();
		strlwr (sc_String);

		// Walk the ancestors of this type and see if any of them know
		// about the property.
		info = bag.Info->Class;

		FString propname = sc_String;

		if (sc_String[0]!='-' && sc_String[0]!='+')
		{
			if (SC_CheckString ("."))
			{
				SC_MustGetString ();
				propname += '.';
				strlwr (sc_String);
				propname += sc_String;
			}
			else
			{
				SC_UnGet ();
			}
		}
		prop = is_actorprop (propname.GetChars());

		if (prop != NULL)
		{
			if (!info->IsDescendantOf(prop->type))
			{
				SC_ScriptError("\"%s\" requires an actor of type \"%s\"\n", propname.GetChars(), prop->type->TypeName.GetChars());
			}
			else
			{
				prop->Handler ((AActor *)bag.Info->Class->Defaults, bag);
			}
		}
		else
		{
			SC_ScriptError("\"%s\" is an unknown actor property\n", propname.GetChars());
		}
	}
}

//==========================================================================
//
// Reads an actor definition
//
//==========================================================================
void ProcessActor(void (*process)(FState *, int))
{
	FActorInfo * info=NULL;
	AActor * defaults;
	Baggage bag;

	try
	{
		FActorInfo * parent;


		info=CreateNewActor(&parent, &bag);
		defaults=(AActor*)info->Class->Defaults;
		bag.StateSet = false;
		bag.DropItemSet = false;
		bag.CurrentState = 0;

		ParseActorProperties (bag);
		FinishStates (info, defaults, bag);
		process(info->OwnedStates, info->NumOwnedStates);
		if (bag.DropItemSet)
		{
			if (bag.DropItemList == NULL)
			{
				if (info->Class->Meta.GetMetaInt (ACMETA_DropItems) != 0)
				{
					info->Class->Meta.SetMetaInt (ACMETA_DropItems, 0);
				}
			}
			else
			{
				info->Class->Meta.SetMetaInt (ACMETA_DropItems,
					DropItemList.Push (bag.DropItemList) + 1);
			}
		}
		if (info->Class->IsDescendantOf (RUNTIME_CLASS(AInventory)))
		{
			defaults->flags |= MF_SPECIAL;
		}
	}

	catch(CRecoverableError & e)
	{
		throw e;
	}
	// I think this is better than a crash log.
#ifndef _DEBUG
	catch (...)
	{
		if (info)
			SC_ScriptError("Unexpected error during parsing of actor %s", info->Class->TypeName.GetChars());
		else
			SC_ScriptError("Unexpected error during parsing of actor definitions");
	}
#endif

	SC_SetCMode (false);
}

//==========================================================================
//
// StatePropertyIsDeprecated
//
// Deprecated means it will be removed in a future version.
//
//==========================================================================

static void StatePropertyIsDeprecated (const char *actorname, const char *prop)
{
	static bool warned = false;

	Printf (TEXTCOLOR_YELLOW "In actor %s, the %s property is deprecated and will be removed in 2.2.0.\n",
		actorname, prop);
	if (!warned)
	{
		warned = true;
		Printf (TEXTCOLOR_YELLOW "Instead of \"%s <state>\", add this to the actor's States block:\n"
				TEXTCOLOR_YELLOW "    %s:\n"
				TEXTCOLOR_YELLOW "        Goto <state>\n", prop, prop);
	}
}

//==========================================================================
//
// Property parsers
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void ActorSkipSuper (AActor *defaults, Baggage &bag)
{
	ResetActor(defaults, &bag);
}

//==========================================================================
//
//==========================================================================
static void ActorGame (AActor *defaults, Baggage &bag)
{
	SC_MustGetString ();
	if (SC_Compare ("Doom"))
	{
		bag.Info->GameFilter |= GAME_Doom;
	}
	else if (SC_Compare ("Heretic"))
	{
		bag.Info->GameFilter |= GAME_Heretic;
	}
	else if (SC_Compare ("Hexen"))
	{
		bag.Info->GameFilter |= GAME_Hexen;
	}
	else if (SC_Compare ("Raven"))
	{
		bag.Info->GameFilter |= GAME_Raven;
	}
	else if (SC_Compare ("Strife"))
	{
		bag.Info->GameFilter |= GAME_Strife;
	}
	else if (SC_Compare ("Any"))
	{
		bag.Info->GameFilter = GAME_Any;
	}
	else
	{
		SC_ScriptError ("Unknown game type %s", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnID (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	if (sc_Number<0 || sc_Number>255)
	{
		SC_ScriptError ("SpawnID must be in the range [0,255]");
	}
	else bag.Info->SpawnID=(BYTE)sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorConversationID (AActor *defaults, Baggage &bag)
{
	int convid;

	SC_MustGetNumber();
	convid = sc_Number;

	// Handling for Strife teaser IDs - only of meaning for the standard items
	// as PWADs cannot be loaded with the teasers.
	if (SC_CheckString(","))
	{
		SC_MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE))
			convid=sc_Number;

		SC_MustGetStringName(",");
		SC_MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE|GI_TEASER2))
			convid=sc_Number;

		if (convid==-1) return;
	}
	if (convid<0 || convid>1000)
	{
		SC_ScriptError ("ConversationID must be in the range [0,1000]");
	}
	else StrifeTypes[convid] = bag.Info->Class;
}

//==========================================================================
//
//==========================================================================
static void ActorTag (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString(AMETA_StrifeName, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->health=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorGibHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_GibHealth, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorWoundHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_WoundHealth, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorReactionTime (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->reactiontime=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorPainChance (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PainChance=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->damage=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->Speed=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorFloatSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->FloatSpeed=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->radius=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->height=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMass (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Mass=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorXScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->xscale=BYTE(sc_Float*64-1);
}

//==========================================================================
//
//==========================================================================
static void ActorYScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->yscale=BYTE(sc_Float*64-1);
}

//==========================================================================
//
//==========================================================================
static void ActorScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->xscale=defaults->yscale=BYTE(sc_Float*64-1);
}

//==========================================================================
//
//==========================================================================
static void ActorSeeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->SeeSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorAttackSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AttackSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorPainSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->PainSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DeathSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorActiveSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->ActiveSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDropItem (AActor *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	SC_MustGetString();
	di->Name=sc_String;
	di->probability=255;
	di->amount=-1;
	if (SC_CheckNumber())
	{
		di->probability=sc_Number;
		if (SC_CheckNumber())
		{
			di->amount=sc_Number;
		}
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Spawn");
	defaults->SpawnState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorSeeState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "See");
	defaults->SeeState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Melee");
	defaults->MeleeState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorMissileState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Missile");
	defaults->MissileState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorPainState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Pain");
	defaults->PainState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Death");
	defaults->DeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorXDeathState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "XDeath");
	defaults->XDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorBurnState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Burn");
	defaults->BDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorIceState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Ice");
	defaults->IDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorRaiseState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Raise");
	defaults->RaiseState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorCrashState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Crash");
	defaults->CrashState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorCrushState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Crush");
	defaults->CrushState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorWoundState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Wound");
	defaults->WoundState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorDisintegrateState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Disintegrate");
	defaults->EDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorHealState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Heal");
	defaults->HealState=CheckState (bag.CurrentState, bag.Info->Class);
}

//==========================================================================
//
//==========================================================================
static void ActorStates (AActor *defaults, Baggage &bag)
{
	if (!bag.StateSet) ProcessStates(bag.Info, defaults, bag);
	else SC_ScriptError("Multiple state declarations not allowed");
	bag.StateSet=true;
}

//==========================================================================
//
//==========================================================================
static void ActorRenderStyle (AActor *defaults, Baggage &bag)
{
	static const char * renderstyles[]={
		"NONE","NORMAL","FUZZY","SOULTRANS","OPTFUZZY","STENCIL","TRANSLUCENT", "ADD",NULL};

	static const int renderstyle_values[]={
		STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
			STYLE_Stencil, STYLE_Translucent, STYLE_Add};

	SC_MustGetString();
	defaults->RenderStyle=renderstyle_values[SC_MustMatchString(renderstyles)];
}

//==========================================================================
//
//==========================================================================
static void ActorAlpha (AActor *defaults, Baggage &bag)
{
	if (SC_CheckString("DEFAULT"))
	{
		defaults->alpha = gameinfo.gametype==GAME_Heretic? HR_SHADOW : HX_SHADOW;
	}
	else
	{
		SC_MustGetFloat();
		defaults->alpha=fixed_t(sc_Float*FRACUNIT);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_Obituary, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorHitObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_HitObituary, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDontHurtShooter (AActor *defaults, Baggage &bag)
{
	bag.Info->Class->Meta.SetMetaInt (ACMETA_DontHurtShooter, true);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionRadius, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionDamage, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// AActor::Die() uses a height of 0 to mean "cut the height to 1/4",
	// so if a height of 0 is desired, store it as -1.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorBurnHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// The note above for AMETA_DeathHeight also applies here.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_BurnHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.BAttack.MeleeDamage = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeRange (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->meleerange = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.BAttack.MeleeSound = S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorMissileType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.BAttack.MissileName = sc_String;
}

//==========================================================================
//
//==========================================================================
static void ActorMissileHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.BAttack.MissileHeight = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorTranslation (AActor *defaults, Baggage &bag)
{
	if (SC_CheckNumber())
	{
		int max = (gameinfo.gametype==GAME_Strife || (bag.Info->GameFilter&GAME_Strife)) ? 6:2;
		if (sc_Number < 0 || sc_Number > max)
		{
			SC_ScriptError ("Translation must be in the range [0,%d]", max);
		}
		defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc_Number);
	}
	else
	{
		unsigned char translation[256];
		for(int i=0;i<256;i++) translation[i]=i;
		do
		{
			SC_GetString();
			AddToTranslation(translation,sc_String);
		}
		while (SC_CheckString(","));
		defaults->Translation = StoreTranslation (translation);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorBloodColor (AActor *defaults, Baggage &bag)
{
	int r,g,b;

	if (SC_CheckNumber())
	{
		SC_MustGetNumber();
		r=clamp<int>(sc_Number, 0, 255);
		SC_MustGetNumber();
		g=clamp<int>(sc_Number, 0, 255);
		SC_MustGetNumber();
		b=clamp<int>(sc_Number, 0, 255);
	}
	else
	{
		SC_MustGetString();
		int c = V_GetColor(NULL, sc_String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	PalEntry pe = MAKERGB(r,g,b);
	pe.a = CreateBloodTranslation(pe);
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodColor, pe);
}


//==========================================================================
//
//==========================================================================
static void ActorBounceFactor (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->bouncefactor = clamp<fixed_t>(fixed_t(sc_Float * FRACUNIT), 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorBounceCount (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->bouncecount = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorMinMissileChance (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MinMissileChance=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamageType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString ();   
	if (SC_Compare("Normal")) defaults->DamageType=MOD_UNKNOWN;
	else if (SC_Compare("Fire")) defaults->DamageType=MOD_FIRE;
	else if (SC_Compare("Ice")) defaults->DamageType=MOD_ICE;
	else if (SC_Compare("Disintegrate")) defaults->DamageType=MOD_DISINTEGRATE;
	else SC_ScriptError("Unknown damage type '%s'\n", sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDecal (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DecalGenerator = (FDecalBase *) ((size_t)DecalNames.Push(copystring(sc_String))+1);
}

//==========================================================================
//
//==========================================================================
static void ActorMaxStepHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxStepHeight=sc_Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorMaxDropoffHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxDropOffHeight=sc_Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorPoisonDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_PoisonDamage, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorFastSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_FastSpeed, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorRadiusDamageFactor (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_RDFactor, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorClearFlags (AActor *defaults, Baggage &bag)
{
	defaults->flags=defaults->flags2=defaults->flags3=defaults->flags4=0;
}

//==========================================================================
//
//==========================================================================
static void ActorMonster (AActor *defaults, Baggage &bag)
{
	// sets the standard flag for a monster
	defaults->flags|=MF_SHOOTABLE|MF_COUNTKILL|MF_SOLID; 
	defaults->flags2|=MF2_PUSHWALL|MF2_MCROSS|MF2_PASSMOBJ;
	defaults->flags3|=MF3_ISMONSTER;
}

//==========================================================================
//
//==========================================================================
static void ActorProjectile (AActor *defaults, Baggage &bag)
{
	// sets the standard flags for a projectile
	defaults->flags|=MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE; 
	defaults->flags2|=MF2_IMPACT|MF2_PCROSS|MF2_NOTELEPORT;
	if (gameinfo.gametype&GAME_Raven) defaults->flags5|=MF5_BLOODSPLATTER;
}

//==========================================================================
//
//==========================================================================
static void ActorFlagSetOrReset (AActor *defaults, Baggage &bag)
{
	char mod = sc_String[0];
	flagdef *fd;

	SC_MustGetString ();

	// Fire and ice damage were once flags but now are not.
	if (SC_Compare ("FIREDAMAGE"))
	{
		if (mod == '+') defaults->DamageType = MOD_FIRE;
		else defaults->DamageType = MOD_UNKNOWN;
	}
	else if (SC_Compare ("ICEDAMAGE"))
	{
		if (mod == '+') defaults->DamageType = MOD_ICE;
		else defaults->DamageType = MOD_UNKNOWN;
	}
	else
	{
		FString part1 = sc_String;
		const char *part2 = NULL;
		if (SC_CheckString ("."))
		{
			SC_MustGetString ();
			part2 = sc_String;
		}
		if ( (fd = FindFlag (bag.Info->Class, part1.GetChars(), part2)) )
		{
			DWORD * flagvar = (DWORD*) ((char*)defaults + fd->structoffset);
			if (mod == '+')
			{
				*flagvar |= fd->flagbit;
			}
			else
			{
				*flagvar &= ~fd->flagbit;
			}
		}
		else
		{
			if (part2 == NULL)
			{
				SC_ScriptError("\"%s\" is an unknown flag\n", part1.GetChars());
			}
			else
			{
				SC_ScriptError("\"%s.%s\" is an unknown flag\n", part1.GetChars(), part2);
			}
		}
	}
}

//==========================================================================
//
// Special inventory properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void AmmoBackpackAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BackpackAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoBackpackMaxAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BackpackMaxAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoDropAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AIMETA_DropAmount, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxSaveAmount (ABasicArmorBonus *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->MaxSaveAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorSaveAmount (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SaveAmount=sc_Number;
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SaveAmount=sc_Number;
	}
	else
	{
		SC_ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void ArmorSavePercent (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	if (sc_Float<0.0f) sc_Float=0.0f;
	if (sc_Float>100.0f) sc_Float=100.0f;
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SavePercent=fixed_t(sc_Float*FRACUNIT/100.0f);
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SavePercent=fixed_t(sc_Float*FRACUNIT/100.0f);
	}
	else
	{
		SC_ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryAmount (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Amount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryIcon (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->Icon = TexMan.AddPatch (sc_String);
	if (defaults->Icon <= 0)
	{
		defaults->Icon = TexMan.AddPatch (sc_String, ns_sprites);
		if (defaults->Icon<=0)
		{
			if (bag.Info->GameFilter == GAME_Any || bag.Info->GameFilter & gameinfo.gametype)
				Printf("Icon '%s' for '%s' not found\n", sc_String, bag.Info->Class->TypeName.GetChars());
		}
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryMaxAmount (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->MaxAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryDefMaxAmount (AInventory *defaults, Baggage &bag)
{
	defaults->MaxAmount = gameinfo.gametype == GAME_Heretic ? 16 : 25;
}


//==========================================================================
//
//==========================================================================
static void InventoryPickupmsg (AInventory *defaults, Baggage &bag)
{
	// allow game specific pickup messages
	const char * games[] = {"Doom", "Heretic", "Hexen", "Raven", "Strife", NULL};
	int gamemode[]={GAME_Doom, GAME_Heretic, GAME_Hexen, GAME_Raven, GAME_Strife};

	SC_MustGetString();
	int game = SC_MatchString(games);

	if (game!=-1)
	{
		SC_MustGetString();
		if (!(gameinfo.gametype&gamemode[game])) return;
	}
	bag.Info->Class->Meta.SetMetaString(AIMETA_PickupMessage, sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryPickupsound (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->PickupSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryRespawntics (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->RespawnTics=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryUsesound (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->UseSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryGiveQuest (APuzzleItem *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_GiveQuest, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void HealthLowMessage (AHealth *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_LowHealth, sc_Number);
	SC_MustGetStringName(",");
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString(AIMETA_LowHealthMessage, sc_String);
}

//==========================================================================
//
//==========================================================================
static void PuzzleitemNumber (APuzzleItem *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PuzzleItemNumber=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoGive1=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoGive2=sc_Number;
}

//==========================================================================
//
// Passing these parameters is really tricky to allow proper inheritance
// and forward declarations. Here only a name is
// stored which must be resolved after everything has been declared
//
//==========================================================================

static void WeaponAmmoType1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AmmoType1 = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoType2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AmmoType2 = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoUse1=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoUse2=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponKickback (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Kickback=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponReadySound (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->ReadySound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponSelectionOrder (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->SelectionOrder=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponSisterWeapon (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->SisterWeaponType=fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponUpSound (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->UpSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponYAdjust (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->YAdjust=fixed_t(sc_Float * FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void WPieceValue (AWeaponPiece *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PieceValue = 1 << (sc_Number-1);
}

//==========================================================================
//
//==========================================================================
static void WPieceWeapon (AWeaponPiece *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->WeaponClass = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void PowerupColor (APowerupGiver *defaults, Baggage &bag)
{
	int r;
	int g;
	int b;
	int alpha;

	if (SC_CheckNumber())
	{
		r=clamp<int>(sc_Number, 0, 255);
		SC_MustGetNumber();
		g=clamp<int>(sc_Number, 0, 255);
		SC_MustGetNumber();
		b=clamp<int>(sc_Number, 0, 255);
	}
	else
	{
		SC_MustGetString();

		if (SC_Compare("INVERSEMAP"))
		{
			defaults->BlendColor = INVERSECOLOR;
			return;
		}
		else if (SC_Compare("GOLDMAP"))
		{
			defaults->BlendColor = GOLDCOLOR;
			return;
		}

		int c = V_GetColor(NULL, sc_String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	SC_MustGetFloat();
	alpha=int(sc_Float*255);
	alpha=clamp<int>(alpha, 0, 255);
	if (alpha!=0) defaults->BlendColor = MAKEARGB(alpha, r, g, b);
	else defaults->BlendColor = 0;
}

//==========================================================================
//
//==========================================================================
static void PowerupDuration (APowerupGiver *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->EffectTics = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PowerupType (APowerupGiver *defaults, Baggage &bag)
{
	FString typestr;

	SC_MustGetString();
	typestr.Format ("Power%s", sc_String);
	const PClass * powertype=PClass::FindClass(typestr);
	if (!powertype)
	{
		SC_ScriptError("Unknown powerup type '%s' in '%s'\n", sc_String, bag.Info->Class->TypeName.GetChars());
	}
	else if (!powertype->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		SC_ScriptError("Invalid powerup type '%s' in '%s'\n", sc_String, bag.Info->Class->TypeName.GetChars());
	}
	else
	{
		defaults->PowerupType=powertype;
	}
}

//==========================================================================
//
// [GRB] Special player properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void PlayerDisplayName (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	bag.Info->Class->Meta.SetMetaString (APMETA_DisplayName, sc_String);
}

//==========================================================================
//
//==========================================================================
static void PlayerSoundClass (APlayerPawn *defaults, Baggage &bag)
{
	FString tmp;

	SC_MustGetString ();
	tmp = sc_String;
	tmp.ReplaceChars (' ', '_');
	bag.Info->Class->Meta.SetMetaString (APMETA_SoundClass, tmp);
}

//==========================================================================
//
//==========================================================================
static void PlayerColorRange (APlayerPawn *defaults, Baggage &bag)
{
	int start, end;

	SC_MustGetNumber ();
	start = sc_Number;
	SC_MustGetNumber ();
	end = sc_Number;

	if (start > end)
		swap (start, end);

	bag.Info->Class->Meta.SetMetaInt (APMETA_ColorRange, (start & 255) | ((end & 255) << 8));
}

//==========================================================================
//
//==========================================================================
static void PlayerJumpZ (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->JumpZ = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSpawnClass (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	if (SC_Compare ("Any"))
		defaults->SpawnMask = 0;
	else if (SC_Compare ("Fighter"))
		defaults->SpawnMask |= MTF_FIGHTER;
	else if (SC_Compare ("Cleric"))
		defaults->SpawnMask |= MTF_CLERIC;
	else if (SC_Compare ("Mage"))
		defaults->SpawnMask |= MTF_MAGE;
	else if (IsNum(sc_String))
	{
		int val = strtol(sc_String, NULL, 0);
		defaults->SpawnMask = (val*MTF_FIGHTER) & (MTF_FIGHTER|MTF_CLERIC|MTF_MAGE);
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerViewHeight (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->ViewHeight = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerForwardMove (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->ForwardMove1 = defaults->ForwardMove2 = FLOAT2FIXED (sc_Float);
	if (SC_CheckFloat ())
		defaults->ForwardMove2 = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSideMove (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->SideMove1 = defaults->SideMove2 = FLOAT2FIXED (sc_Float);
	if (SC_CheckFloat ())
		defaults->SideMove2 = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerMaxHealth (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxHealth = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PlayerScoreIcon (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	defaults->ScoreIcon = TexMan.AddPatch (sc_String);
	if (defaults->ScoreIcon <= 0)
	{
		defaults->ScoreIcon = TexMan.AddPatch (sc_String, ns_sprites);
		if (defaults->ScoreIcon <= 0)
		{
			Printf("Icon '%s' for '%s' not found\n", sc_String, bag.Info->Class->TypeName.GetChars ());
		}
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerCrouchSprite (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	for (int i = 0; i < sc_StringLen; i++) sc_String[i] = toupper (sc_String[i]);
	defaults->crouchsprite = GetSpriteIndex (sc_String);
}

//==========================================================================
//
// [GRB] Store start items in drop item list
//
//==========================================================================
static void PlayerStartItem (APlayerPawn *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	SC_MustGetString();
	di->Name = sc_String;
	di->probability=255;
	di->amount=0;
	if (SC_CheckNumber())
	{
		di->amount=sc_Number;
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static const ActorProps *APropSearch (const char *str, const ActorProps *props, int numprops)
{
	int min = 0, max = numprops - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = strcmp (str, props[mid].name);
		if (lexval == 0)
		{
			return &props[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//==========================================================================
//
// all actor properties
//
//==========================================================================
#define apf ActorPropFunction
static const ActorProps props[] =
{
	{ "+",							ActorFlagSetOrReset,		RUNTIME_CLASS(AActor) },
	{ "-",							ActorFlagSetOrReset,		RUNTIME_CLASS(AActor) },
	{ "activesound",				ActorActiveSound,			RUNTIME_CLASS(AActor) },
	{ "alpha",						ActorAlpha,					RUNTIME_CLASS(AActor) },
	{ "ammo.backpackamount",		(apf)AmmoBackpackAmount,	RUNTIME_CLASS(AAmmo) },
	{ "ammo.backpackmaxamount",		(apf)AmmoBackpackMaxAmount,	RUNTIME_CLASS(AAmmo) },
	{ "ammo.dropamount",			(apf)AmmoDropAmount,		RUNTIME_CLASS(AAmmo) },
	{ "armor.maxsaveamount",		(apf)ArmorMaxSaveAmount,	RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.saveamount",			(apf)ArmorSaveAmount,		RUNTIME_CLASS(AActor) },
	{ "armor.savepercent",			(apf)ArmorSavePercent,		RUNTIME_CLASS(AActor) },
	{ "attacksound",				ActorAttackSound,			RUNTIME_CLASS(AActor) },
	{ "bloodcolor",					ActorBloodColor,			RUNTIME_CLASS(AActor) },
	{ "bouncecount",				ActorBounceCount,			RUNTIME_CLASS(AActor) },
	{ "bouncefactor",				ActorBounceFactor,			RUNTIME_CLASS(AActor) },
	{ "burn",						ActorBurnState,				RUNTIME_CLASS(AActor) },
	{ "burnheight",					ActorBurnHeight,			RUNTIME_CLASS(AActor) },
	{ "clearflags",					ActorClearFlags,			RUNTIME_CLASS(AActor) },
	{ "conversationid",				ActorConversationID,		RUNTIME_CLASS(AActor) },
	{ "crash",						ActorCrashState,			RUNTIME_CLASS(AActor) },
	{ "crush",						ActorCrushState,			RUNTIME_CLASS(AActor) },
	{ "damage",						ActorDamage,				RUNTIME_CLASS(AActor) },
	{ "damagetype",					ActorDamageType,			RUNTIME_CLASS(AActor) },
	{ "death",						ActorDeathState,			RUNTIME_CLASS(AActor) },
	{ "deathheight",				ActorDeathHeight,			RUNTIME_CLASS(AActor) },
	{ "deathsound",					ActorDeathSound,			RUNTIME_CLASS(AActor) },
	{ "decal",						ActorDecal,					RUNTIME_CLASS(AActor) },
	{ "disintegrate",				ActorDisintegrateState,		RUNTIME_CLASS(AActor) },
	{ "donthurtshooter",			ActorDontHurtShooter,		RUNTIME_CLASS(AActor) },
	{ "dropitem",					ActorDropItem,				RUNTIME_CLASS(AActor) },
	{ "explosiondamage",			ActorExplosionDamage,		RUNTIME_CLASS(AActor) },
	{ "explosionradius",			ActorExplosionRadius,		RUNTIME_CLASS(AActor) },
	{ "fastspeed",					ActorFastSpeed,				RUNTIME_CLASS(AActor) },
	{ "floatspeed",					ActorFloatSpeed,			RUNTIME_CLASS(AActor) },
	{ "game",						ActorGame,					RUNTIME_CLASS(AActor) },
	{ "gibhealth",					ActorGibHealth,				RUNTIME_CLASS(AActor) },
	{ "heal",						ActorHealState,				RUNTIME_CLASS(AActor) },
	{ "health",						ActorHealth,				RUNTIME_CLASS(AActor) },
	{ "health.lowmessage",			(apf)HealthLowMessage,		RUNTIME_CLASS(AHealth) },
	{ "height",						ActorHeight,				RUNTIME_CLASS(AActor) },
	{ "hitobituary",				ActorHitObituary,			RUNTIME_CLASS(AActor) },
	{ "ice",						ActorIceState,				RUNTIME_CLASS(AActor) },
	{ "inventory.amount",			(apf)InventoryAmount,		RUNTIME_CLASS(AInventory) },
	{ "inventory.defmaxamount",		(apf)InventoryDefMaxAmount,	RUNTIME_CLASS(AInventory) },
	{ "inventory.givequest",		(apf)InventoryGiveQuest,	RUNTIME_CLASS(AInventory) },
	{ "inventory.icon",				(apf)InventoryIcon,			RUNTIME_CLASS(AInventory) },
	{ "inventory.maxamount",		(apf)InventoryMaxAmount,	RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupmessage",	(apf)InventoryPickupmsg,	RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupsound",		(apf)InventoryPickupsound,	RUNTIME_CLASS(AInventory) },
	{ "inventory.respawntics",		(apf)InventoryRespawntics,	RUNTIME_CLASS(AInventory) },
	{ "inventory.usesound",			(apf)InventoryUsesound,		RUNTIME_CLASS(AInventory) },
	{ "mass",						ActorMass,					RUNTIME_CLASS(AActor) },
	{ "maxdropoffheight",			ActorMaxDropoffHeight,		RUNTIME_CLASS(AActor) },
	{ "maxstepheight",				ActorMaxStepHeight,			RUNTIME_CLASS(AActor) },
	{ "melee",						ActorMeleeState,			RUNTIME_CLASS(AActor) },
	{ "meleedamage",				ActorMeleeDamage,			RUNTIME_CLASS(AActor) },
	{ "meleerange",					ActorMeleeRange,			RUNTIME_CLASS(AActor) },
	{ "meleesound",					ActorMeleeSound,			RUNTIME_CLASS(AActor) },
	{ "minmissilechance",			ActorMinMissileChance,		RUNTIME_CLASS(AActor) },
	{ "missile",					ActorMissileState,			RUNTIME_CLASS(AActor) },
	{ "missileheight",				ActorMissileHeight,			RUNTIME_CLASS(AActor) },
	{ "missiletype",				ActorMissileType,			RUNTIME_CLASS(AActor) },
	{ "monster",					ActorMonster,				RUNTIME_CLASS(AActor) },
	{ "obituary",					ActorObituary,				RUNTIME_CLASS(AActor) },
	{ "pain",						ActorPainState,				RUNTIME_CLASS(AActor) },
	{ "painchance",					ActorPainChance,			RUNTIME_CLASS(AActor) },
	{ "painsound",					ActorPainSound,				RUNTIME_CLASS(AActor) },
	{ "player.colorrange",			(apf)PlayerColorRange,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.crouchsprite",		(apf)PlayerCrouchSprite,	RUNTIME_CLASS(APlayerPawn) },
	{ "player.displayname",			(apf)PlayerDisplayName,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.forwardmove",			(apf)PlayerForwardMove,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.jumpz",				(apf)PlayerJumpZ,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.maxhealth",			(apf)PlayerMaxHealth,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.scoreicon",			(apf)PlayerScoreIcon,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.sidemove",			(apf)PlayerSideMove,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.soundclass",			(apf)PlayerSoundClass,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.spawnclass",			(apf)PlayerSpawnClass,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.startitem",			(apf)PlayerStartItem,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.viewheight",			(apf)PlayerViewHeight,		RUNTIME_CLASS(APlayerPawn) },
	{ "poisondamage",				ActorPoisonDamage,			RUNTIME_CLASS(AActor) },
	{ "powerup.color",				(apf)PowerupColor,			RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.duration",			(apf)PowerupDuration,		RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.type",				(apf)PowerupType,			RUNTIME_CLASS(APowerupGiver) },
	{ "projectile",					ActorProjectile,			RUNTIME_CLASS(AActor) },
	{ "puzzleitem.number",			(apf)PuzzleitemNumber,		RUNTIME_CLASS(APuzzleItem) },
	{ "radius",						ActorRadius,				RUNTIME_CLASS(AActor) },
	{ "radiusdamagefactor",			ActorRadiusDamageFactor,	RUNTIME_CLASS(AActor) },
	{ "raise",						ActorRaiseState,			RUNTIME_CLASS(AActor) },
	{ "reactiontime",				ActorReactionTime,			RUNTIME_CLASS(AActor) },
	{ "renderstyle",				ActorRenderStyle,			RUNTIME_CLASS(AActor) },
	{ "scale",						ActorScale,					RUNTIME_CLASS(AActor) },
	{ "see",						ActorSeeState,				RUNTIME_CLASS(AActor) },
	{ "seesound",					ActorSeeSound,				RUNTIME_CLASS(AActor) },
	{ "skip_super",					ActorSkipSuper,				RUNTIME_CLASS(AActor) },
	{ "spawn",						ActorSpawnState,			RUNTIME_CLASS(AActor) },
	{ "spawnid",					ActorSpawnID,				RUNTIME_CLASS(AActor) },
	{ "speed",						ActorSpeed,					RUNTIME_CLASS(AActor) },
	{ "states",						ActorStates,				RUNTIME_CLASS(AActor) },
	{ "tag",						ActorTag,					RUNTIME_CLASS(AActor) },
	{ "translation",				ActorTranslation,			RUNTIME_CLASS(AActor) },
	{ "weapon.ammogive",			(apf)WeaponAmmoGive1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive1",			(apf)WeaponAmmoGive1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive2",			(apf)WeaponAmmoGive2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype",			(apf)WeaponAmmoType1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype1",			(apf)WeaponAmmoType1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype2",			(apf)WeaponAmmoType2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse",				(apf)WeaponAmmoUse1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse1",			(apf)WeaponAmmoUse1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse2",			(apf)WeaponAmmoUse2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.kickback",			(apf)WeaponKickback,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.readysound",			(apf)WeaponReadySound,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.selectionorder",		(apf)WeaponSelectionOrder,	RUNTIME_CLASS(AWeapon) },
	{ "weapon.sisterweapon",		(apf)WeaponSisterWeapon,	RUNTIME_CLASS(AWeapon) },
	{ "weapon.upsound",				(apf)WeaponUpSound,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.yadjust",				(apf)WeaponYAdjust,			RUNTIME_CLASS(AWeapon) },
	{ "weaponpiece.number",			(apf)WPieceValue,			RUNTIME_CLASS(AWeaponPiece) },
	{ "weaponpiece.weapon",			(apf)WPieceWeapon,			RUNTIME_CLASS(AWeaponPiece) },
	{ "wound",						ActorWoundState,			RUNTIME_CLASS(AActor) },
	{ "woundhealth",				ActorWoundHealth,			RUNTIME_CLASS(AActor) },
	{ "xdeath",						ActorXDeathState,			RUNTIME_CLASS(AActor) },
	{ "xscale",						ActorXScale,				RUNTIME_CLASS(AActor) },
	{ "yscale",						ActorYScale,				RUNTIME_CLASS(AActor) },
	// AWeapon:MinAmmo1 and 2 are never used so there is no point in adding them here!
};
static const ActorProps *is_actorprop (const char *str)
{
	return APropSearch (str, props, sizeof(props)/sizeof(ActorProps));
}

//==========================================================================
//
// Do some postprocessing after everything has been defined
// This also processes all the internal actors to adjust the type
// fields in the weapons
//
//==========================================================================
void FinishThingdef()
{
	unsigned int i;
	bool isRuntimeActor=false;

	for (i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		// Everything coming after the first runtime actor is also a runtime actor
		// so this check is sufficient
		if (ti == PClass::m_RuntimeActors[0])
		{
			isRuntimeActor=true;
		}

		// Friendlies never count as kills!
		if (GetDefaultByType(ti)->flags & MF_FRIENDLY)
		{
			GetDefaultByType(ti)->flags &=~MF_COUNTKILL;
		}

		// the typeinfo properties of weapons have to be fixed here after all actors have been declared
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon * defaults=(AWeapon *)ti->Defaults;
			fuglyname v;

			v = defaults->AmmoType1;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType1 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType1)
					{
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType1->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			v = defaults->AmmoType2;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType2 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType2)
					{
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType2->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			v = defaults->SisterWeaponType;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->SisterWeaponType = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->SisterWeaponType)
					{
						I_Error("Unknown sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (!defaults->SisterWeaponType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
					{
						I_Error("Invalid sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			if (isRuntimeActor)
			{
				// Do some consistency checks. If these states are undefined the weapon cannot work!
				if (!defaults->ReadyState) I_Error("Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
				if (!defaults->UpState) I_Error("Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
				if (!defaults->DownState) I_Error("Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
				if (!defaults->AtkState) I_Error("Weapon %s doesn't define an attack state.\n", ti->TypeName.GetChars());

				// If the weapon doesn't define a hold state use the attack state instead.
				if (!defaults->HoldAtkState) defaults->HoldAtkState=defaults->AtkState;
				if (!defaults->AltHoldAtkState) defaults->AltHoldAtkState=defaults->AltAtkState;
			}

		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)ti->Defaults;
			fuglyname v;

			v = defaults->WeaponClass;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->WeaponClass = PClass::FindClass(v);
				if (!defaults->WeaponClass)
				{
					I_Error("Unknown weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
				else if (!defaults->WeaponClass->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
				{
					I_Error("Invalid weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
			}
		}
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		sprintf(fmt, "QuestItem%d", i+1);
		QuestItemClasses[i]=PClass::FindClass(fmt);
	}
}
