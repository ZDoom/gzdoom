/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2004 Christoph Oelckers
** Copyright 2004 Randy Heit
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
#include "doomerrors.h"
#include "g_doom/a_doomglobal.h"
#include "p_conversation.h"

static FRandom pr_camissile ("CustomActorfire");
static FRandom pr_camelee ("CustomMelee");
static FRandom pr_cabullet ("CustomBullet");
static FRandom pr_cajump ("CustomJump");
static FRandom pr_cwbullet ("CustomWpBullet");
static FRandom pr_cwjump ("CustomWpJump");
static FRandom pr_cwpunch ("CustomWpPunch");

static const char * flagnames[]={
	"*",			// Pickup items require special handling - don't set the flag explicitly
	"SOLID",
	"SHOOTABLE",
	"NOSECTOR",
	"NOBLOCKMAP",
	"AMBUSH",
	"JUSTHIT",
	"JUSTATTACKED",
	"SPAWNCEILING",
	"NOGRAVITY",
	"DROPOFF",
	"*",			// MF_PICKUP does not make much sense
	"NOCLIP",
	"*",			// MF_SLIDE is completly unused
	"FLOAT",
	"TELEPORT",
	"MISSILE",
	"DROPPED",
	"SHADOW",
	"NOBLOOD",
	"CORPSE",
	"INFLOAT",
	"COUNTKILL",
	"COUNTITEM",
	"SKULLFLY",
	"NOTDMATCH",
	"*",			// MF_TRANSLATION1 - better use ACS for this!
	"*",			// MF_TRANSLATION2 - better use ACS for this!
	"*",			// MF_UNMORPHED - not to be used in an actor definition.
	"NOLIFTDROP",
	"STEALTH",
	"ICECORPSE",
	NULL
};
	
static const char * flag2names[]={
	"LOWGRAVITY",
	"WINDTHRUST",
	"HERETICBOUNCE",
	"*",				// BLASTED
	"*",				// MF2_FLY is for players only
	"FLOORCLIP",
	"SPAWNFLOAT",
	"NOTELEPORT",
	"RIPPER",
	"PUSHABLE",
	"SLIDESONWALLS",
	"*",				// MF2_ONMOBJ is only used internally
	"CANPASS",
	"CANNOTPUSH",
	"THRUGHOST",
	"BOSS",
	"FIREDAMAGE",
	"NODAMAGETHRUST",
	"TELESTOMP",
	"FLOATBOB",
	"HEXENBOUNCE",
	"ACTIVATEIMPACT",
	"CANPUSHWALLS",
	"ACTIVATEMCROSS",
	"ACTIVATEPCROSS",
	"CANTLEAVEFLOORPIC",
	"NONSHOOTABLE",
	"INVULNERABLE",
	"DORMANT",
	"ICEDAMAGE",
	"SEEKERMISSILE",
	"REFLECTIVE",
	NULL
};

static const char * flag3names[]=
{
	"FLOORHUGGER",
	"CEILINGHUGGER",
	"NORADIUSDMG",
	"GHOST",
	"*",
	"*",
	"DONTSPLASH",
	"*",					// NOSIGHTCHECK
	"DONTOVERLAP",
	"DONTMORPH",
	"DONTSQUASH",
	"*",					// EXPLOCOUNT
	"FULLVOLACTIVE",
	"ISMONSTER",
	"SKYEXPLODE",
	"STAYMORPHED",
	"DONTBLAST",
	"CANBLAST",
	"NOTARGET",
	"DONTGIB",
	"NOBLOCKMONST",
	"*",
	"FULLVOLDEATH",
	"CANBOUNCEWATER",
	"NOWALLBOUNCESND",
	"FOILINVUL",
	"NOTELEOTHER",
	"*",
	"*",
	"*",					// WARNBOT
	"*",					// PUFFONACTORS
	"*",					// HUNTPLAYERS
	NULL
};

static const char * flag4names[]=
{
	"*",					// NOHATEPLAYERS
	"QUICKTORETALIATE",
	"NOICEDEATH",
	"*",					// BOSSDEATH
	"RANDOMIZE",
	"*",					// NOSKIN
	"FIXMAPTHINGPOS",
	"ACTLIKEBRIDGE",
	"STRIFEDAMAGE",
	"LONGMELEERANGE",		// new stuff
	"MISSILEMORE",
	"MISSILEEVENMORE",
	"SHORTMISSILERANGE",
	"DONTFALL",
	"SEESDAGGERS",
	"INCOMBAT",
	"LOOKALLAROUND",
	"STANDSTILL",
	"SPECTRAL",
	"FIRERESIST",
	"NOSPLASHALERT",
	NULL
};

static const char *weaponflagnames[] =
{
	"noautofire",
	"readysoundhalf",
	"dontbob",
	"*",
	"firedamage",
	"noalert",
	"ammonotneeded"
};

#include "thingdef_specials.h"

struct AFuncDesc
{
	const char * Name;
	actionf_p Function;
};


// Prototype the code pointers
#define WEAPON(x)	void A_##x(AActor*);	
#define ACTOR(x)	void A_##x(AActor*);
WEAPON(FireProjectile)
ACTOR(MeleeAttack)
ACTOR(MissileAttack)
ACTOR(ComboAttack)
ACTOR(BulletAttack)
ACTOR(ScreamAndUnblock)
ACTOR(ActiveAndUnblock)
ACTOR(ActiveSound)
ACTOR(FastChase)
ACTOR(FreezeDeath)
ACTOR(FreezeDeathChunks)
ACTOR(GenericFreezeDeath)
ACTOR(IceGuyDie)
ACTOR(M_Saw)
#include "d_dehackedactions.h"

// Declare the code pointer table
AFuncDesc AFTable[]=
{
#define WEAPON(x)	
#define ACTOR(x)	{"A_"#x, A_##x},
ACTOR(BulletAttack)
ACTOR(ScreamAndUnblock)
ACTOR(ActiveAndUnblock)
ACTOR(ActiveSound)
ACTOR(FastChase)
ACTOR(FreezeDeath)
ACTOR(FreezeDeathChunks)
ACTOR(GenericFreezeDeath)
ACTOR(IceGuyDie)			// useful for bursting a monster into ice chunks without delay
ACTOR(M_Saw)
#include "d_dehackedactions.h"
	{"A_Fall", A_NoBlocking},	// Allow Doom's old name, too, for this function
	{NULL,NULL}
};

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

#define WEAPON(x)	{"A_"#x, A_##x},

AFuncDesc WFTable[]=
{
	WEAPON(Light0)
	WEAPON(Light1)
	WEAPON(Light2)
	WEAPON(WeaponReady)
	WEAPON(Lower)
	WEAPON(Raise)
	WEAPON(ReFire)
	WEAPON(Punch)
	WEAPON(CheckReload)
	WEAPON(GunFlash)
	WEAPON(Saw)
	WEAPON(FireProjectile)
	{NULL,NULL}
};

extern TArray<FActorInfo *> Decorations;


struct FDropItem 
{
	const char * Name;
	int probability;
	int amount;
	FDropItem * Next;
};
TArray<FDropItem *> DropItemList;

struct FBasicAttack
{
	int MeleeDamage;
	int MeleeSound;
	const char *MissileName;
	fixed_t MissileHeight;
};
TArray<FBasicAttack> BasicAttackList;

struct FMissileAttack
{
	const char *MissileName;
	fixed_t SpawnHeight;
	fixed_t Spawnofs_XY;
	angle_t Angle;
	bool aimparallel;
};

struct FBulletAttack
{
	angle_t Spread_XY;
	angle_t Spread_Z;
	int NumBullets;
	int DamagePerBullet;
	const char * PuffType;
};

struct FJump
{
	int compare;
	int offset;
};

struct FLnSpec
{
	int special;
	int args[5];
};

struct FWeaponSingleParam : public FWeaponParam
{
	int param;
};

struct FWeaponJump : public FWeaponParam
{
	int compare;
	int offset;
};

struct FWeaponMissileAttack : public FWeaponParam
{
	const char * MissileName;
	fixed_t SpawnHeight;
	fixed_t Spawnofs_XY;
	angle_t Angle;
	bool dontuseammo;
};

struct FWeaponBulletAttack : public FWeaponParam
{
	angle_t Spread_XY;
	angle_t Spread_Z;
	int NumBullets;
	int DamagePerBullet;
};

struct FRailAttack : public FWeaponParam
{
	int damage;
	int Spawnofs_XY;
};

struct FExplodeParms
{
	bool HurtShooter;
	int ExplosionRadius, ExplosionDamage;
};

static TArray<FMissileAttack> AttackList;
static TArray<FBulletAttack> BulletAttackList;
static TArray<FJump> JumpList;
static TArray<FLnSpec> LineSpecialList;
static TArray<FExplodeParms> ExplodeList;
TArray<FWeaponParam *> WeaponParams;

static TArray<FState> StateArray;
static int StateRangeEnd[3];
static int StateRangeType[3];
enum
{
	SRT_Actor,
	SRT_WeaponLevel1,
	SRT_WeaponLevel2
};
int NumRanges;

//==========================================================================
//
// Extra info maintained while defining an actor. The original
// implementation stored these in a CustomActor class. They have all been
// moved into action function parameters so that no special CustomActor
// class is necessary.
//
//==========================================================================

struct Baggage
{
	FActorInfo *Info;
	bool DropItemSet;
	bool StateSet;
	int CurrentState;

	FExplodeParms EParms;
	FDropItem *DropItemList;
	FBasicAttack BAttack;

	const char *PuffType;
	const char *HitPuffType;
	int AttackSound;
	int AmmoGive;

	int WeapNum;
};

enum
{
	ACMETA_BASE				= 0x83000,
	ACMETA_DropItems,		// Int (index into DropItemList)
};

struct ActorProps { const char *name; void (*Handler)(AActor *defaults, Baggage &bag); };

typedef ActorProps (*ActorPropHandler) (register const char *str, register unsigned int len);

static const ActorProps *is_actorprop (const char *str);
static const ActorProps *is_weaponprop (const char *str);
static const ActorProps *is_weapondefprop (const char *str);

//==========================================================================
//
// Customizable attack functions which use actor parameters.
// I think this is among the most requested stuff ever ;-)
//
//==========================================================================
static void DoAttack (AActor *self, bool domelee, bool domissile)
{
	if (self->target == NULL)
		return;

	size_t index = self->state->GetMisc1_2();
	if (index >= BasicAttackList.Size())
		return;

	const FBasicAttack *cust = &BasicAttackList[index];
				
	A_FaceTarget (self);
	if (domelee && self->CheckMeleeRange ())
	{
		int damage = pr_camelee.HitDice(cust->MeleeDamage);
		if (cust->MeleeSound) S_SoundID (self, CHAN_WEAPON, cust->MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
	}
	else if (domissile)
	{
		const TypeInfo * ti=TypeInfo::FindType(cust->MissileName);
		if (ti) 
		{
#if 1
			// Hmm, the result of this hack (taken from A_SkelMissile) is
			// significantly more desirable than using P_SpawnMissileZ!
			self->z+=cust->MissileHeight-32*FRACUNIT;
			AActor * missile = P_SpawnMissile (self, self->target, ti);
			self->z-=cust->MissileHeight-32*FRACUNIT;
#else
			// ZDoom has a P_SpawnMissile Z to do this.
			// Edit: why is the aiming of this so much worse than the hack I used previously???
			AActor * missile = P_SpawnMissileZ(self, self->z+cust->MissileHeight, self->target, ti);
#endif

			// automatic handling of seeker missiles
			if (missile && missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
		}
	}
}

void A_MeleeAttack(AActor * self)
{
	DoAttack(self, true, false);
}

void A_MissileAttack(AActor * self)
{
	DoAttack(self, false, true);
}

void A_ComboAttack(AActor * self)
{
	DoAttack(self, true, true);
}

//==========================================================================
//
// Custom sound functions. These use misc1 and misc2 in the state structure
//
//==========================================================================
void A_PlaySound(AActor * self)
{
	int soundid = self->state->GetMisc1_2();
	S_SoundID (self, CHAN_BODY, soundid, 1, ATTN_NORM);
}

void A_PlayWeaponSound(AActor * self)
{
	int soundid = self->state->GetMisc1_2();
	S_SoundID (self, CHAN_WEAPON, soundid, 1, ATTN_NORM);
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
void A_SeekerMissile(AActor * self)
{
	P_SeekerMissile(self, self->state->GetMisc1()*ANGLE_1, self->state->GetMisc2()*ANGLE_1);
}

//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
void A_BulletAttack (AActor *self)
{
	int i;
	int bangle;
	int slope;
		
	if (!self->target)
		return;

	A_FaceTarget (self);
	bangle = self->angle;

	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	for (i=0 ; i<self->damage ; i++)
    {
		int angle = bangle + (pr_cabullet.Random2() << 20);
		int damage = ((pr_cabullet()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage, RUNTIME_CLASS(ABulletPuff));
    }
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_Jump(AActor * self)
{
	if (pr_cajump() < self->state->GetMisc2())
		self->SetState (self->state + self->state->GetMisc1());
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_JumpIfHealthLower(AActor * self)
{
	int index=self->state->GetMisc1_2();
	FJump * jmp=&JumpList[index];

	if (self->health<jmp->compare)
		self->SetState(self->state+jmp->offset);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_JumpIfCloser(AActor * self)
{
	if (!self->target) return;

	int index=self->state->GetMisc1_2();
	FJump * jmp=&JumpList[index];

	if (P_AproxDistance(self->x-self->target->x, self->y-self->target->y)<jmp->compare<<FRACBITS)
		self->SetState(self->state+jmp->offset);
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

void A_ExplodeParms (AActor *self)
{
	int damage = 128;
	int distance = 128;
	bool hurtSource = true;
	size_t index = self->state->GetMisc1_2();

	if (index < AttackList.Size())
	{
		FExplodeParms *parms = &ExplodeList[index];

		if (parms->ExplosionDamage != 0)
		{
			damage = parms->ExplosionDamage;
		}
		if (parms->ExplosionRadius != 0)
		{
			distance = parms->ExplosionRadius;
		}
		hurtSource = parms->HurtShooter;
	}

	P_RadiusAttack (self, self->target, damage, distance, hurtSource, self->GetMOD ());
	if (self->z <= self->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (self);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_NoBlocking
//
// (moved here so that it has access to FDropItemList's definition)
//
//----------------------------------------------------------------------------

void A_NoBlocking (AActor *actor)
{
	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->alpha = OPAQUE;
		actor->visdir = 0;
	}

	actor->flags &= ~MF_SOLID;

	size_t index = actor->GetClass()->Meta.GetMetaInt (ACMETA_DropItems) - 1;

	// If the actor has a conversation that sets an item to drop, drop that.
	if (actor->Conversation != NULL && actor->Conversation->DropType != NULL)
	{
		P_DropItem (actor, actor->Conversation->DropType, -1, 256);
		actor->Conversation = NULL;
		return;
	}

	actor->Conversation = NULL;

	// If the actor has attached metadata for items to drop, drop those.
	// Otherwise, call NoBlockingSet() and let it decide what to drop.
	if (index >= 0 && index < DropItemList.Size())
	{
		FDropItem *di = DropItemList[index];

		while (di != NULL)
		{
			const TypeInfo *ti = TypeInfo::FindType(di->Name);
			if (ti) P_DropItem (actor, ti, di->amount, di->probability);
			di = di->Next;
		}
	}
	else
	{
		actor->NoBlockingSet ();
	}
}

//==========================================================================
//
// Execute a line special / script
//
//==========================================================================
void A_CallSpecial(AActor * self)
{
	int index=self->state->GetMisc1_2();
	FLnSpec * lspc=&LineSpecialList[index];

	LineSpecials[lspc->special](NULL, self, false, lspc->args[0], lspc->args[1], 
								lspc->args[2], lspc->args[3], lspc->args[4]);
}

//==========================================================================
//
// The ultimate code pointer: Fully customizable missiles!
//
//==========================================================================
void A_CustomMissile(AActor * self)
{
	size_t index = self->state->GetMisc1_2();
	AActor * targ;
	AActor * missile;

	if (self->target != NULL && index>=0 && index<AttackList.Size())
	{
		FMissileAttack * att=&AttackList[index];

		const TypeInfo * ti=TypeInfo::FindType(att->MissileName);
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = att->Spawnofs_XY * finecosine[ang];
			fixed_t y = att->Spawnofs_XY * finesine[ang];
			fixed_t z = att->SpawnHeight-32*FRACUNIT;

			if (!att->aimparallel)
			{
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->x+=x;
				self->y+=y;
				self->z+=z;
				missile = P_SpawnMissile(self, self->target, ti);
				self->x-=x;
				self->y-=y;
				self->z-=z;
			}
			else
			{
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+att->SpawnHeight, self, self->target, ti);
			}

			if (missile)
			{
				missile->angle += att->Angle;
				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->momx = FixedMul (missile->Speed, finecosine[ang]);
				missile->momy = FixedMul (missile->Speed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (self->flags&MF_MISSILE)
                {
                	AActor * owner=self->target;
                	while (owner->flags&MF_MISSILE && owner->target) owner=owner->target;
                	targ=owner;
                	missile->target=owner;
					// automatic handling of seeker missiles
					if (self->flags & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer=self->tracer;
					}
                }
				else if (missile->flags2&MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer=self->target;
				}
			}
		}
	}
}

//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
void A_CustomBulletAttack (AActor *self)
{
	int i;
	int bangle;
	int bslope;
	const TypeInfo *pufftype;

	size_t index=self->state->GetMisc1_2();

	if (self->target && index>=0 && index<BulletAttackList.Size())
	{
		FBulletAttack * att=&BulletAttackList[index];

		if (!self->target) return;

		A_FaceTarget (self);
		bangle = self->angle;

		pufftype = TypeInfo::FindType(att->PuffType);
		if (!pufftype) pufftype=RUNTIME_CLASS(ABulletPuff);

		bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i=0 ; i<att->NumBullets ; i++)
		{
			int angle = bangle + pr_cabullet.Random2() * att->Spread_XY * (ANGLE_1/255);
			int slope = bslope + pr_cabullet.Random2() * att->Spread_Z * (ANGLE_1/255);
			int damage = ((pr_cabullet()%3)+1)*att->DamagePerBullet;
			P_LineAttack(self, angle, MISSILERANGE, slope, damage, pufftype);
		}
    }
}

//==========================================================================
//
// Weapon functions
//
//==========================================================================
void A_WeaponPlaySound(AActor * self)
{
	/*
	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	FWeaponSingleParam * p=static_cast<FWeaponSingleParam *>(WeaponParams[index]);
	S_SoundID (self, CHAN_WEAPON, p->param, 1, ATTN_NORM);
	*/
}

//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
void A_FireProjectile (AActor *actor)
{
	/*
	player_t *player;
	ACustomWeapon *info;

	if (NULL == (player = actor->player))
	{
		return;
	}
	int index=!!(player->powers[pw_weaponlevel2] && !deathmatch);

	player->UseAmmo (true);

	info = (ACustomWeapon*)(wpnlev1info[player->readyweapon]->type->ActorInfo->Defaults);

	if (info->w[index].missiletype)
	{
		const TypeInfo * ti=TypeInfo::FindType(info->w[index].missiletype);
		if (ti) 
		{
			AActor * misl=P_SpawnPlayerMissile (actor, ti);
			// automatic handling of seeker missiles
			if (misl && linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
		}
	}
	*/
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_WeaponJump(AActor * self)
{
	/*
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	FWeaponJump * p=static_cast<FWeaponJump *>(WeaponParams[index]);

	if (pr_cwjump()<p->compare)
		P_SetPsprite(player, psp-player->psprites, psp->state + p->offset);
	*/
}


//==========================================================================
//
// State jump function
//
//==========================================================================
void A_WeaponJumpIfCloser(AActor * self)
{
	/*
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	P_BulletSlope(self);
	if (!linetarget) return;

	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;
	FWeaponJump * jmp=static_cast<FWeaponJump *>(WeaponParams[index]);

	if (P_AproxDistance(self->x-linetarget->x, self->y-linetarget->y)<jmp->compare<<FRACBITS)
		P_SetPsprite(player, psp-player->psprites, psp->state + jmp->offset);
	*/
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_WeaponJumpIfNoAmmo(AActor * self)
{
	/*
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	FWeaponSingleParam * p=static_cast<FWeaponSingleParam *>(WeaponParams[index]);


	if (!CheckAmmo(player))
		P_SetPsprite(player, psp-player->psprites, psp->state + p->param);
	*/
}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
void A_FireBullets (AActor *self)
{
	/*
	int i;
	int bangle;
	int bslope;

	player_t *player;
	ACustomWeapon * info;
	FWeaponInfo ** wpinfo;
	ACustomWeapon::CWInfo * cwi;


	if (NULL == (player = self->player))
	{
		return;
	}
	info = (ACustomWeapon*)(wpnlev1info[player->readyweapon]->type->ActorInfo->Defaults);

	if (player->powers[pw_weaponlevel2] && !deathmatch)
	{
		wpinfo = wpnlev2info;
		cwi=&info->w[1];
	}
	else
	{
		wpinfo = wpnlev1info;
		cwi=&info->w[0];
	}


	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	FWeaponBulletAttack * att=static_cast<FWeaponBulletAttack *>(WeaponParams[index]);

	S_SoundID (self, CHAN_WEAPON, cwi->attacksound, 1, ATTN_NORM);

	player->UseAmmo (true);

	static_cast<APlayerPawn *>(self)->PlayAttacking2 ();
	P_BulletSlope(self);

	bangle = self->angle;
	bslope = bulletpitch;

	PuffType=NULL;
	if (cwi->pufftype) PuffType = TypeInfo::FindType(cwi->pufftype);
	if (!PuffType) PuffType=RUNTIME_CLASS(ABulletPuff);

	HitPuffType=NULL;
	if (cwi->hitpufftype) HitPuffType = TypeInfo::FindType(cwi->hitpufftype);

	if (att->NumBullets==1 && !player->refire)
	{
		int damage = ((pr_cwbullet()%3)+1)*att->DamagePerBullet;
		P_LineAttack(self, bangle, MISSILERANGE, bslope, damage);
	}
	else for (i=0 ; i<att->NumBullets ; i++)
	{
		int angle = bangle + pr_cwbullet.Random2() * att->Spread_XY * (ANGLE_1/255);
		int slope = bslope + pr_cwbullet.Random2() * att->Spread_Z * (ANGLE_1/255);
		int damage = ((pr_cwbullet()%3)+1)*att->DamagePerBullet;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage);
	}
	HitPuffType=NULL;
	*/
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
void A_FireCustomMissile (AActor *actor)
{
	/*
	player_t *player;
	ACustomWeapon *info;

	if (NULL == (player = actor->player))
	{
		return;
	}

	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;
	FWeaponMissileAttack * att=static_cast<FWeaponMissileAttack *>(WeaponParams[index]);

	if (!att->dontuseammo) player->UseAmmo (true);

	info = (ACustomWeapon*)(wpnlev1info[player->readyweapon]->type->ActorInfo->Defaults);

	const TypeInfo * ti=TypeInfo::FindType(att->MissileName);
	if (ti) 
	{
		angle_t ang = (actor->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = att->Spawnofs_XY * finecosine[ang];
		fixed_t y = att->Spawnofs_XY * finesine[ang];
		fixed_t z = att->SpawnHeight;

		AActor * misl=P_SpawnPlayerMissile (actor, actor->x+x, actor->y+y, actor->z+z, ti, actor->angle);
		// automatic handling of seeker missiles
		if (misl)
		{
			if (linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
			misl->angle += att->Angle;
			angle_t an = misl->angle >> ANGLETOFINESHIFT;
			misl->momx = FixedMul (misl->Speed, finecosine[an]);
			misl->momy = FixedMul (misl->Speed, finesine[an]);
		}
	}
	*/
}


//==========================================================================
//
// A_Punch
//
//==========================================================================
void A_CustomPunch (AActor *actor)
{
	/*
	player_t * player;
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	FWeaponInfo ** wpinfo;
	ACustomWeapon::CWInfo * cwi;
	ACustomWeapon * info;


	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	if (NULL == (player = actor->player))
	{
		return;
	}

	FWeaponSingleParam * p=static_cast<FWeaponSingleParam *>(WeaponParams[index]);

	info = (ACustomWeapon*)(wpnlev1info[player->readyweapon]->type->ActorInfo->Defaults);
	if (player->powers[pw_weaponlevel2] && !deathmatch)
	{
		wpinfo = wpnlev2info;
		cwi=&info->w[1];
	}
	else
	{
		wpinfo = wpnlev1info;
		cwi=&info->w[0];
	}

	damage = p->param * (pr_cwpunch()%8+1);

	if (player->powers[pw_strength])	damage *= 10;

	angle = actor->angle + (pr_cwpunch.Random2() << 18);
	pitch = P_AimLineAttack (actor, angle, MELEERANGE);

	// only use ammo when actually hitting something!
	if (linetarget)	player->UseAmmo (true);

	PuffType=NULL;
	if (cwi->pufftype) PuffType = TypeInfo::FindType(cwi->pufftype);
	if (!PuffType) PuffType=RUNTIME_CLASS(ABulletPuff);

	HitPuffType=NULL;
	if (cwi->hitpufftype) HitPuffType = TypeInfo::FindType(cwi->hitpufftype);

	P_LineAttack (actor, angle, MELEERANGE, pitch, damage);

	HitPuffType=NULL;

	// turn to face target
	if (linetarget)
	{
		ACustomWeapon * info = (ACustomWeapon*)(wpinfo[player->readyweapon]->type->ActorInfo->Defaults);
		S_SoundID (actor, CHAN_WEAPON, cwi->attacksound, 1, ATTN_NORM);

		actor->angle = R_PointToAngle2 (actor->x,
										actor->y,
										linetarget->x,
										linetarget->y);
	}
	*/
}


//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
void A_RailAttack (AActor *actor)
{
	/*
	player_t *player;

	unsigned index=psp->state->GetMisc1_2();
	if (index>=WeaponParams.Size()) return;

	if (NULL == (player = actor->player))
	{
		return;
	}

	FRailAttack * p=static_cast<FRailAttack *>(WeaponParams[index]);
	player->UseAmmo ();
	P_RailAttack (actor, p->damage, p->Spawnofs_XY);
	*/
}

//==========================================================================
//
// SC_CheckNumber
// similar to SC_GetNumber but ungets the token if it isn't a number 
// and does not print an error
//
//==========================================================================

BOOL SC_CheckNumber (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		if (strcmp (sc_String, "MAXINT") == 0)
		{
			sc_Number = INT_MAX;
		}
		else
		{
			sc_Number = strtol (sc_String, &stopper, 0);
			if (*stopper != 0)
			{
				SC_UnGet();
				return false;
			}
		}
		sc_Float = (float)sc_Number;
		return true;
	}
	else
	{
		return false;
	}
}


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
static const char * mobj_statenames[]={"SPAWN","SEE","PAIN","MELEE","MISSILE","CRASH",
							"DEATH","XDEATH", "BURN","ICE","DISINTEGRATE","RAISE","WOUND",
							NULL};

static const char * weapon_statenames[]={"up","down","ready","attack","holdattack",
	"altattack","altholdattack","flash",NULL};


FState ** FindState(AActor * actor, const char * name)
{
	int i;

	for(i=0;mobj_statenames[i];i++)
	{
		if (!stricmp(mobj_statenames[i],name))
			return (&actor->SpawnState)+i;
	}
	if (actor->IsKindOf (RUNTIME_CLASS(AWeapon)))
	{
		for(i=0;weapon_statenames[i];i++)
		{
			if (!stricmp(weapon_statenames[i],name))
				return (&static_cast<AWeapon*>(actor)->UpState)+i;
		}
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
	bag->EParms.ExplosionDamage = bag->EParms.ExplosionRadius = 128;
	bag->EParms.HurtShooter = true;
	bag->DropItemList = NULL;
	bag->BAttack.MissileHeight = 32*FRACUNIT;
	bag->BAttack.MeleeSound = 0;
	bag->BAttack.MissileName = NULL;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->StateSet = false;
}

static void ResetActor (AActor *actor, Baggage *bag)
{
	memcpy (actor, GetDefault<AActor>(), sizeof(AActor));
	ResetBaggage (bag);
}



//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo * CreateNewActor(FActorInfo ** parentc, Baggage *bag)
{
	char * typeName;

	SC_MustGetString();

	if (TypeInfo::IFindType (sc_String) != NULL)
	{
		SC_ScriptError ("Actor %s is already defined.", sc_String);
	}

	typeName = new char[strlen(sc_String)+2];
	sprintf(typeName, "A%s", sc_String);


	TypeInfo * parent = RUNTIME_CLASS(AActor);
	if (parentc)
	{
		*parentc = NULL;
		SC_MustGetString();
		if (SC_Compare(":"))
		{
			SC_MustGetString();
			parent = const_cast<TypeInfo *> (TypeInfo::IFindType (sc_String));

			if (parent == NULL)
			{
				SC_ScriptError ("Parent type '%s' not found", sc_String);
			}
			else if (parent->ActorInfo == NULL)
			{
				SC_ScriptError ("Parent type '%s' is not an actor", sc_String);
			}
			else
			{
				*parentc = parent->ActorInfo;
			}
		}
		else SC_UnGet();
	}

	TypeInfo * ti = parent->CreateDerivedClass (typeName, parent->SizeOf);
	FActorInfo * info = ti->ActorInfo;

	Decorations.Push (info);
	info->NumOwnedStates=0;
	info->OwnedStates=NULL;
	info->SpawnID=0;

	ResetBaggage (bag);
	bag->Info = info;

	info->DoomEdNum=-1;
	if (SC_CheckNumber()) 
	{
		if (sc_Number>=-1 && sc_Number<32768) info->DoomEdNum=sc_Number;
		else SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}

	return info;
}

//==========================================================================
//
// State processing
//
//==========================================================================

bool DoSpecialFunctions(FState & state, bool multistate, int * statecount, Baggage &bag)
{
	int i;
	const ACSspecials *spec;

	// These functions are special because they require parameters

	if ((spec = is_special (sc_String, sc_StringLen)) != NULL)
	{
		FLnSpec ls = { spec->Special };

		memset(&ls,0,sizeof(ls));
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use a special with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		for (i = 0; i < 5; )
		{
			if (SC_CheckNumber ())
			{
				ls.args[i++] = sc_Number;
			}
			else if (!TestCom ())
			{
				SC_UnGet ();
				break;
			}
		}
		SC_MustGetStringName (")");
		if (i < spec->MinArgs)
		{
			SC_ScriptError ("Too few arguments to %s", spec->name);
		}
		if (i > MAX (spec->MinArgs, spec->MaxArgs))
		{
			SC_ScriptError ("Too many arguments to %s", spec->name);
		}
		state.SetMisc1_2 (LineSpecialList.Push(ls));
		state.Action = A_CallSpecial;
		return true;
	}

	// Override the normal A_Explode with a parameterized version
	if (SC_Compare ("A_Explode"))
	{
		FExplodeParms local_eparms = bag.EParms;

		if (state.Frame & SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_Explode with a state duration larger than 254!");
		}
		SC_MustGetString ();
		if (SC_Compare ("("))	// Parameters are optional
		{
			SC_MustGetNumber ();
			local_eparms.ExplosionDamage = sc_Number;
			SC_MustGetStringName (",");
			SC_MustGetNumber ();
			local_eparms.ExplosionRadius = sc_Number;
			SC_MustGetStringName (",");
			SC_MustGetNumber ();
			local_eparms.HurtShooter = !!sc_Number;
			SC_MustGetStringName (")");
			state.SetMisc1_2 (ExplodeList.Push (local_eparms));
		}
		else
		{
			SC_UnGet ();
		}
		state.SetMisc1_2 (ExplodeList.Push (local_eparms));
		state.Action = A_ExplodeParms;
		return true;
	}

	// Check for A_MeleeAttack, A_MissileAttack, or A_ComboAttack
	int batk = SC_MatchString (BasicAttackNames);
	if (batk >= 0)
	{
		if (state.Frame & SF_BIGTIC)
		{
			SC_ScriptError ("You cannot use %s with a state duration larger than 254!", sc_String);
		}
		state.SetMisc1_2 (BasicAttackList.Push (bag.BAttack));
		state.Action = BasicAttacks[batk];
		return true;
	}

	if (SC_Compare("A_PlaySound"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_PlaySound with a state duration larger than 254!");
		}
		SC_MustGetStringName ("(");
		SC_MustGetString();
		state.SetMisc1_2 (S_FindSound(sc_String));
		SC_MustGetStringName (")");
		state.Action = A_PlaySound;
		return true;
	}
	if (SC_Compare("A_PlayWeaponSound"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_PlayWeaponSound with a state duration larger than 254!");
		}
		SC_MustGetStringName ("(");
		SC_MustGetString();
		state.SetMisc1_2 (S_FindSound(sc_String));
		SC_MustGetStringName (")");
		state.Action = A_PlayWeaponSound;
		return true;
	}
	if (SC_Compare("A_SeekerMissile"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_SeekerMissile with a state duration larger than 254!");
		}
		SC_MustGetStringName ("(");
		SC_MustGetNumber();
		if (sc_Number<0 || sc_Number>90)
		{
			SC_ScriptError("A_SeekerMissile parameters must be in the range [0..90]");
		}
		state.Misc1=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<0 || sc_Number>90)
		{
			SC_ScriptError("A_SeekerMissile parameters must be in the range [0..90]");
		}
		state.Misc2=sc_Number;
		SC_MustGetStringName (")");
		state.Action = A_SeekerMissile;
		return true;
	}
	if (SC_Compare("A_Jump"))
	{
		if (multistate)
		{
			SC_ScriptError("A_Jump may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_Jump with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		SC_MustGetNumber();
		state.Misc2=sc_Number;
		if (sc_Number<0 || sc_Number>255)
		{
			SC_ScriptError("The first parameter of A_Jump must be in the range [0..255]");
		}
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<=0)
		{
			SC_ScriptError("The second parameter of A_Jump must be greater than 0");
		}
		state.Misc1=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=state.Misc1;
		state.Action = A_Jump;
		return true;
	}
	if (SC_Compare("A_CustomMissile"))
	{
		FMissileAttack att;

		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_CustomMissile with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		SC_MustGetString();
		att.MissileName=strdup(sc_String);
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.SpawnHeight=sc_Number*FRACUNIT;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.Spawnofs_XY=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.Angle=sc_Number*ANGLE_1;

		if (TestCom()) 
		{
			SC_MustGetNumber();
			att.aimparallel=!!sc_Number;
		}
		else att.aimparallel=false;


		SC_MustGetStringName (")");

		state.SetMisc1_2 (AttackList.Push(att));
		state.Action = A_CustomMissile;

		return true;
	}
	if (SC_Compare("A_CustomBulletAttack"))
	{
		FBulletAttack att;

		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_CustomBulletAttack with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		SC_MustGetNumber();
		att.Spread_XY=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.Spread_Z=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.NumBullets=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att.DamagePerBullet=sc_Number;
		if (TestCom())
		{
			SC_MustGetString();
			att.PuffType=strdup(sc_String);
		}
		else att.PuffType="BulletPuff";
		SC_MustGetStringName (")");

		state.SetMisc1_2 (BulletAttackList.Push(att));
		state.Action = A_CustomBulletAttack;
		return true;
	}
	if (SC_Compare("A_JumpIfHealthLower"))
	{
		FJump jmp;

		if (multistate)
		{
			SC_ScriptError("A_JumpIfHealthLower may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_JumpIfHealthLower with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		SC_MustGetNumber();
		jmp.compare=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<=0)
		{
			SC_ScriptError("The second parameter of A_JumpIfHealthLower must be greater than 0");
		}
		jmp.offset=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=jmp.offset;
		state.SetMisc1_2 (JumpList.Push(jmp));
		state.Action = A_JumpIfHealthLower;
		return true;
	}
	if (SC_Compare("A_JumpIfCloser"))
	{
		FJump jmp;

		if (multistate)
		{
			SC_ScriptError("A_JumpIfCloser may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_JumpIfCloser with a state duration larger than 254!");
		}
		SC_MustGetStringName("(");
		SC_MustGetNumber();
		jmp.compare=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<=0)
		{
			SC_ScriptError("The second parameter of A_JumpIfCloser must be greater than 0");
		}
		jmp.offset=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=jmp.offset;
		state.SetMisc1_2 (JumpList.Push(jmp));
		state.Action = A_JumpIfCloser;
		return true;
	}
	return false;
}

bool DoSpecialWeaponFunctions(FState & state, bool multistate, int * statecount)
{
	// These functions are special because they require a parameter
	if (SC_Compare("A_PlaySound"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_PlaySound with a state duration larger than 254!");
		}
		FWeaponSingleParam * parm=new FWeaponSingleParam;
		parm->wp_xoffset=state.Misc1;
		parm->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName ("(");
		SC_MustGetString();
		parm->param=S_FindSound(sc_String);
		SC_MustGetStringName (")");

		int v=WeaponParams.Push(parm);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_WeaponPlaySound;
		return true;
	}
	if (SC_Compare("A_Jump"))
	{
		if (multistate)
		{
			SC_ScriptError("A_Jump may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_Jump with a state duration larger than 254!");
		}
		FWeaponJump * parm=new FWeaponJump;
		parm->wp_xoffset=state.Misc1;
		parm->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetNumber();
		parm->compare=sc_Number;
		if (sc_Number<0 || sc_Number>255)
		{
			SC_ScriptError("The first parameter of A_Jump must be in the range [0..255]");
		}
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<=0)
		{
			SC_ScriptError("The second parameter of A_Jump must be greater than 0");
		}
		parm->offset=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=parm->offset;

		int v=WeaponParams.Push(parm);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_WeaponJump;
		return true;
	}
	if (SC_Compare("A_JumpIfCloser"))
	{
		if (multistate)
		{
			SC_ScriptError("A_JumpIfCloser may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_JumpIfCloser with a state duration larger than 254!");
		}
		FWeaponJump * parm=new FWeaponJump;
		parm->wp_xoffset=state.Misc1;
		parm->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetNumber();
		parm->compare=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		if (sc_Number<=0)
		{
			SC_ScriptError("The second parameter of A_JumpIfCloser must be greater than 0");
		}
		parm->offset=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=parm->offset;

		int v=WeaponParams.Push(parm);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_WeaponJumpIfCloser;
		return true;
	}
	if (SC_Compare("A_JumpIfNoAmmo"))
	{
		if (multistate)
		{
			SC_ScriptError("A_JumpIfCloser may not be used on a multiple state definition!");
		}
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_JumpIfCloser with a state duration larger than 254!");
		}
		FWeaponSingleParam * parm=new FWeaponSingleParam;
		parm->wp_xoffset=state.Misc1;
		parm->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetNumber();
		parm->param=sc_Number;
		SC_MustGetStringName (")");
		*statecount+=parm->param;

		int v=WeaponParams.Push(parm);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_WeaponJumpIfNoAmmo;
		return true;
	}
	if (SC_Compare("A_CustomPunch"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_CustomPunch with a state duration larger than 254!");
		}
		FWeaponSingleParam * parm=new FWeaponSingleParam;
		parm->wp_xoffset=state.Misc1;
		parm->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName ("(");
		SC_MustGetNumber();
		parm->param=sc_Number;
		SC_MustGetStringName (")");

		int v=WeaponParams.Push(parm);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_CustomPunch;
		return true;
	}
	if (SC_Compare("A_FireBullets"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_FireBullets with a state duration larger than 254!");
		}
		FWeaponBulletAttack * att=new FWeaponBulletAttack;
		att->wp_xoffset=state.Misc1;
		att->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetNumber();
		att->Spread_XY=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att->Spread_Z=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att->NumBullets=sc_Number;
		SC_MustGetStringName (",");
		SC_MustGetNumber();
		att->DamagePerBullet=sc_Number;
		SC_MustGetStringName (")");

		int v=WeaponParams.Push(att);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_FireBullets;
		return true;
	}
	if (SC_Compare("A_FireCustomMissile"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_FireCustomMissile with a state duration larger than 254!");
		}
		FWeaponMissileAttack * att=new FWeaponMissileAttack;
		memset(att,0,sizeof(FWeaponMissileAttack));
		att->wp_xoffset=state.Misc1;
		att->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetString();
		att->MissileName=strdup(sc_String);
		if (TestCom())
		{
			SC_MustGetNumber();
			att->Angle=sc_Number*ANGLE_1;
			if (TestCom())
			{
				SC_MustGetNumber();
				att->dontuseammo=!!sc_Number;
				if (TestCom())
				{
					SC_MustGetNumber();
					att->Spawnofs_XY=sc_Number;
					if (TestCom())
					{
						SC_MustGetNumber();
						att->SpawnHeight=sc_Number*FRACUNIT;
					}
				}
			}
		}

		SC_MustGetStringName (")");

		int v=WeaponParams.Push(att);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_FireCustomMissile;
		return true;
	}
	if (SC_Compare("A_RailAttack"))
	{
		if (state.Frame&SF_BIGTIC)
		{
			SC_ScriptError("You cannot use A_RailAttack with a state duration larger than 254!");
		}
		FRailAttack * att=new FRailAttack;
		att->wp_xoffset=state.Misc1;
		att->wp_yoffset=state.Misc2;
		state.Frame|=SF_WEAPONPARAM;

		SC_MustGetStringName("(");
		SC_MustGetNumber();
		att->damage=sc_Number;
		if (TestCom())
		{
			SC_MustGetNumber();
			att->Spawnofs_XY=sc_Number;
		}
		else att->Spawnofs_XY=0;
		SC_MustGetStringName (")");
		int v=WeaponParams.Push(att);
		state.Misc2=v&255;
		state.Misc1=v>>8;
		state.Action = A_RailAttack;
		return true;
	}
	return false;
}

static int ProcessStates(FActorInfo * actor, AActor * defaults, Baggage &bag, int rangetype)
{
	char statestring[256];
	int count = StateArray.Size();
	FState state;
	FState * laststate=NULL;
	int lastlabel;
	FState ** stp;
	int minrequiredstate=0;

	statestring[255] = 0;

	StateRangeType[NumRanges] = rangetype;

	ChkBraceOpn();
	while (!TestBraceCls() && !sc_End)
	{
		memset(&state,0,sizeof(state));
		SC_MustGetString();
		if (SC_Compare("GOTO"))
		{
			SC_MustGetString();
			if (!laststate) 
			{
				SC_ScriptError("GOTO before first state");
				continue;
			}
			strncpy (statestring, sc_String, 255);
			if (SC_CheckString ("+"))
			{
				SC_MustGetNumber ();
				strcat (statestring, "+");
				strcat (statestring, sc_String);
			}
			// copy the text - this must be resolved later!
			laststate->NextState=(FState*)strdup(statestring);	
		}
		else if (SC_Compare("STOP"))
		{
			if (!laststate) 
			{
				SC_ScriptError("STOP before first state");
				continue;
			}
			laststate->NextState=(FState*)-1;
		}
		else if (SC_Compare("WAIT"))
		{
			if (!laststate) 
			{
				SC_ScriptError("WAIT before first state");
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
			SC_MustGetString();

			while (SC_Compare (":"))
			{
				lastlabel = count;
				stp = FindState(defaults, statestring);
				if (stp) *stp=(FState *) (count+1);
				else 
					SC_ScriptError("Unknown state label %s", statestring);
				SC_MustGetString ();
				strncpy(statestring, sc_String, 255);
				SC_MustGetString ();
			}

			SC_UnGet ();

			if (strlen (statestring) != 4)
			{
				SC_ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			memcpy(state.sprite.name, statestring, 4);
			SC_MustGetString();
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

				// Make the action name lowercase to satisfy the gperf hashers
				strlwr (sc_String);

				int minreq=count;
				if (DoSpecialFunctions(state,strlen(statestring)>0, &minreq, bag))
				{
					if (minreq>minrequiredstate) minrequiredstate=minreq;
					goto endofstate;
				}

				for(int i=0;AFTable[i].Name;i++) if (SC_Compare(AFTable[i].Name))
				{
					state.Action = AFTable[i].Function;
					goto endofstate;
				}
				SC_ScriptError("Invalid state parameter");
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

				state.Frame=(state.Frame&(SF_FULLBRIGHT|SF_BIGTIC))|frame;
				StateArray.Push(state);
				count++;
			}
			laststate=&StateArray[count];
			count++;
		}
	}
	if (count<=minrequiredstate)
	{
		SC_ScriptError("A_Jump offset out of range in %s", actor->Class->Name);
	}
	StateRangeEnd[NumRanges++] = StateArray.Size();
	return count;
}

static int FinishStates (FActorInfo *actor, AActor *defaults, Baggage &bag)
{
	FState **stp;
	int count = StateArray.Size();
	FState * realstates=new FState[count];
	int i;
	int currange;

	memcpy(realstates,&StateArray[0],count*sizeof(FState));
	actor->OwnedStates=realstates;
	actor->NumOwnedStates=count;

	// adjust the state pointers
	for(stp=&defaults->SpawnState;stp<=&defaults->RaiseState;stp++)
	{
		int v=(int)*stp;
		if (v>=1 && v<0x10000) *stp=actor->OwnedStates+v-1;
	}

	for(i = currange = 0; i < count; i++)
	{
		// resolve labels and jumps
		switch((int)realstates[i].NextState)
		{
		case 0:		// next
			realstates[i].NextState=(i<count-1? &realstates[i+1]:&realstates[0]);
			break;

		case -1:	// stop
			realstates[i].NextState=NULL;
			break;

		case -2:	// wait
			realstates[i].NextState=&realstates[i];
			break;

		default:	// loop
			if (((int)realstates[i].NextState)<0x10000)
			{
				realstates[i].NextState=&realstates[(int)realstates[i].NextState-1];
			}
			else	// goto
			{
				char *label = strtok ((char*)realstates[i].NextState, "+");
				char *labelpt = label;
				char *offset = strtok (NULL, "+");
				int v = offset ? strtol (offset, NULL, 0) : 0;

				stp = FindState (defaults, label);
				if (stp)
				{
					if (*stp) realstates[i].NextState=*stp+v;
					else 
					{
						realstates[i].NextState=NULL;
						if (v)
						{
							SC_ScriptError("Attempt to get invalid state from actor %s\n", actor->Class->Name);
							return 0;
						}
					}
				}
				else 
					SC_ScriptError("Unknown state label %s", label);
				free(labelpt);		// free the allocated string buffer
			}
		}
		if (i == StateRangeEnd[currange])
		{
			currange++;
		}
	}
	StateArray.Clear ();
	NumRanges = 0;
	return count;
}

//==========================================================================
//
// For getting a state address from the parent
//
//==========================================================================
static FState *CheckState(int statenum, TypeInfo *type)
{
	if (SC_GetString() && !sc_Crossed)
	{
		if (SC_Compare("0")) return NULL;
		else if (SC_Compare("PARENT"))
		{
			SC_MustGetString();

			FState * basestate;
			FState ** stp=FindState((AActor*)type->ParentType->ActorInfo->Defaults, sc_String);
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
				SC_ScriptError("Attempt to get invalid state from actor %s\n", type->ParentType->Name+1);
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
// Reads an actor definition
//
//==========================================================================
inline char * MS_Strdup(const char * s)
{
	return *s? strdup(s):NULL;
}

void ParseActorProperties (Baggage &bag)
{
	const TypeInfo *info;
	const ActorProps *prop;

	ChkBraceOpn ();
	while (!TestBraceCls() && !sc_End)
	{
		SC_GetString ();
		strlwr (sc_String);

		// Walk the ancestors of this type and see if any of them know
		// about the property.
		info = bag.Info->Class;
		prop = NULL;
		do
		{
			info = info->ParentType;
			if (info == RUNTIME_CLASS(AWeapon))
			{
				//prop = is_weaponprop (sc_String);
				prop = NULL;
			}
			else if (info == RUNTIME_CLASS(AActor))
			{
				prop = is_actorprop (sc_String);
			}
		} while (info != RUNTIME_CLASS(AActor) && prop == NULL);
		if (prop != NULL)
		{
			prop->Handler ((AActor *)bag.Info->Defaults, bag);
		}
		else
		{
			SC_ScriptError("\"%s\" is an unknown actor property\n", sc_String);
		}
	}
}

void ProcessActor(void (*process)(FState *, int))
{
	FActorInfo * info=NULL;
	AActor * defaults;
	Baggage bag;

	try
	{
		FActorInfo * parent;

		info=CreateNewActor(&parent, &bag);
		defaults=(AActor*)info->Defaults;
		bag.StateSet = false;
		bag.DropItemSet = false;
		bag.CurrentState = 0;

		SC_SetCMode (true);

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
			SC_ScriptError("Unexpected error during parsing of actor %s", info->Class->Name+1);
		else
			SC_ScriptError("Unexpected error during parsing of actor definitions");
	}
#endif

	SC_SetCMode (false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void ActorSkipSuper (AActor *defaults, Baggage &bag)
{
	ResetActor(defaults, &bag);
}

static void ActorSpawnID (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	if (sc_Number<0 || sc_Number>255)
	{
		SC_ScriptError ("SpawnNum must be in the range [0,255]");
	}
	else bag.Info->SpawnID=(BYTE)sc_Number;
}

static void ActorHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->health=sc_Number;
}

static void ActorReactionTime (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->reactiontime=sc_Number;
}

static void ActorPainChance (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PainChance=sc_Number;
}

static void ActorDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->damage=sc_Number;
}

static void ActorSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->Speed=fixed_t(sc_Float*FRACUNIT);
}

static void ActorRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->radius=fixed_t(sc_Float*FRACUNIT);
}

static void ActorHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->height=fixed_t(sc_Float*FRACUNIT);
}

static void ActorMass (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Mass=sc_Number;
}

static void ActorXScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->xscale=BYTE(sc_Float*64)-1;
}

static void ActorYScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->yscale=BYTE(sc_Float*64)-1;
}

static void ActorScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->xscale=defaults->yscale=BYTE(sc_Float*64)-1;
}

static void ActorSeeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->SeeSound=S_FindSound(sc_String);
}

static void ActorAttackSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AttackSound=S_FindSound(sc_String);
}

static void ActorPainSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->PainSound=S_FindSound(sc_String);
}

static void ActorDeathSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DeathSound=S_FindSound(sc_String);
}

static void ActorActiveSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->ActiveSound=S_FindSound(sc_String);
}

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
	di->Name=strdup(sc_String);
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

static void ActorSpawnState (AActor *defaults, Baggage &bag)
{
	defaults->SpawnState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorSeeState (AActor *defaults, Baggage &bag)
{
	defaults->SeeState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorMeleeState (AActor *defaults, Baggage &bag)
{
	defaults->MeleeState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorMissileState (AActor *defaults, Baggage &bag)
{
	defaults->MissileState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorPainState (AActor *defaults, Baggage &bag)
{
	defaults->PainState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorDeathState (AActor *defaults, Baggage &bag)
{
	defaults->DeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorXDeathState (AActor *defaults, Baggage &bag)
{
	defaults->XDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorBurnState (AActor *defaults, Baggage &bag)
{
	defaults->BDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorIceState (AActor *defaults, Baggage &bag)
{
	defaults->IDeathState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorRaiseState (AActor *defaults, Baggage &bag)
{
	defaults->RaiseState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorCrashState (AActor *defaults, Baggage &bag)
{
	defaults->CrashState=CheckState (bag.CurrentState, bag.Info->Class);
}

static void ActorStates (AActor *defaults, Baggage &bag)
{
	if (!bag.StateSet) ProcessStates(bag.Info, defaults, bag, SRT_Actor);
	else SC_ScriptError("Multiple state declarations not allowed");
	bag.StateSet=true;
}

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

static void ActorAlpha (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->alpha=fixed_t(sc_Float*FRACUNIT);
}

static void ActorObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_Obituary, sc_String);
}

static void ActorHitObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_HitObituary, sc_String);
}

static void ActorDontHurtShooter (AActor *defaults, Baggage &bag)
{
	bag.EParms.HurtShooter=false;
}

static void ActorExplosionRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.EParms.ExplosionRadius=sc_Number;
}

static void ActorExplosionDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.EParms.ExplosionDamage=sc_Number;
}

static void ActorDeathHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// AActor::Die() uses a height of 0 to mean "cut the height to 1/4",
	// so if a height of 0 is desired, store it as -1.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, h <= 0 ? -1 : h);
}

static void ActorBurnHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// The note above for AMETA_DeathHeight also applies here.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_BurnHeight, h <= 0 ? -1 : h);
}

static void ActorMeleeDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.BAttack.MeleeDamage = sc_Number;
}

static void ActorMeleeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.BAttack.MeleeSound = S_FindSound(sc_String);
}

static void ActorMissileType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.BAttack.MissileName = MS_Strdup(sc_String);
}

static void ActorMissileHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.BAttack.MissileHeight = fixed_t(sc_Float*FRACUNIT);
}

static void ActorTranslation (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	if (sc_Number < 0 || sc_Number > 2)
	{
		SC_ScriptError ("Translation must be in the range [0,2]");
	}
	defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc_Number);
}

static void ActorClearFlags (AActor *defaults, Baggage &bag)
{
	defaults->flags=defaults->flags2=defaults->flags3=defaults->flags4=0;
}

static void ActorMonster (AActor *defaults, Baggage &bag)
{
	// sets the standard flag for a monster
	defaults->flags|=MF_SHOOTABLE|MF_COUNTKILL|MF_SOLID; 
	defaults->flags2|=MF2_PUSHWALL|MF2_MCROSS|MF2_PASSMOBJ;
	defaults->flags3|=MF3_ISMONSTER;
}

static void ActorProjectile (AActor *defaults, Baggage &bag)
{
	// sets the standard flags for a projectile
	defaults->flags|=MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE; 
	defaults->flags2|=MF2_IMPACT|MF2_PCROSS|MF2_NOTELEPORT;
}

static void ActorFlagSetOrReset (AActor *defaults, Baggage &bag)
{
	char mod = sc_String[0];
	int v;

	SC_MustGetString ();
	// This isn't a real flag but should be usable as one!
	if (SC_Compare ("DOOMBOUNCE"))
	{
		if (mod=='+') defaults->flags2|=MF2_DOOMBOUNCE;
		else defaults->flags2&=~MF2_DOOMBOUNCE;
	}
	else if ((v=SC_MatchString(flagnames))!=-1)
	{
		unsigned fmod=1<<v;
		if (mod=='+') defaults->flags|=fmod;
		else defaults->flags&=~fmod;
	}
	else if ((v=SC_MatchString(flag2names))!=-1)
	{
		unsigned long fmod=1<<v;
		if (mod=='+') defaults->flags2|=fmod;
		else defaults->flags2&=~fmod;
	}
	else if ((v=SC_MatchString(flag3names))!=-1)
	{
		unsigned long fmod=1<<v;
		if (mod=='+') defaults->flags3|=fmod;
		else defaults->flags3&=~fmod;
	}
	else if ((v=SC_MatchString(flag4names))!=-1)
	{
		unsigned long fmod=1<<v;
		if (mod=='+') defaults->flags4|=fmod;
		else defaults->flags4&=~fmod;
	}
	else SC_ScriptError("\"%s\" is an unknown flag\n", sc_String);
}

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

static const ActorProps *is_actorprop (const char *str)
{
	static const ActorProps props[] =
	{
		{ "+",					ActorFlagSetOrReset },
		{ "-",					ActorFlagSetOrReset },
		{ "activesound",		ActorActiveSound },
		{ "alpha",				ActorAlpha },
		{ "attacksound",		ActorAttackSound },
		{ "burn",				ActorBurnState },
		{ "burnheight",			ActorBurnHeight },
		{ "clearflags",			ActorClearFlags },
		{ "crash",				ActorCrashState },
		{ "damage",				ActorDamage },
		{ "death",				ActorDeathState },
		{ "deathheight",		ActorDeathHeight },
		{ "deathsound",			ActorDeathSound },
		{ "donthurtshooter",	ActorDontHurtShooter },
		{ "dropitem",			ActorDropItem },
		{ "explosiondamage",	ActorExplosionDamage },
		{ "explosionradius",	ActorExplosionRadius },
		{ "health",				ActorHealth },
		{ "height",				ActorHeight },
		{ "hitobituary",		ActorHitObituary },
		{ "ice",				ActorIceState },
		{ "mass",				ActorMass },
		{ "melee",				ActorMeleeState },
		{ "meleedamage",		ActorMeleeDamage },
		{ "meleesound",			ActorMeleeSound },
		{ "missile",			ActorMissileState },
		{ "missileheight",		ActorMissileHeight },
		{ "missiletype",		ActorMissileType },
		{ "monster",			ActorMonster },
		{ "obituary",			ActorObituary },
		{ "pain",				ActorPainState },
		{ "painchance",			ActorPainChance },
		{ "painsound",			ActorPainSound },
		{ "projectile",			ActorProjectile },
		{ "radius",				ActorRadius },
		{ "raise",				ActorRaiseState },
		{ "reactiontime",		ActorReactionTime },
		{ "renderstyle",		ActorRenderStyle },
		{ "scale",				ActorScale },
		{ "see",				ActorSeeState },
		{ "seesound",			ActorSeeSound },
		{ "skip_super",			ActorSkipSuper },
		{ "spawn",				ActorSpawnState },
		{ "spawnid",			ActorSpawnID },
		{ "speed",				ActorSpeed },
		{ "states",				ActorStates },
		{ "translation",		ActorTranslation },
		{ "xdeath",				ActorXDeathState },
		{ "xscale",				ActorXScale },
		{ "yscale",				ActorYScale },
	};
	return APropSearch (str, props, sizeof(props)/sizeof(ActorProps));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FinishThingdef()
{
}


