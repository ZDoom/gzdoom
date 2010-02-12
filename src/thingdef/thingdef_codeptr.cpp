/*
** thingdef.cpp
**
** Code pointers for Actor definitions
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
#include "g_level.h"
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
#include "i_system.h"
#include "p_local.h"
#include "c_console.h"
#include "doomerrors.h"
#include "a_sharedglobal.h"
#include "thingdef/thingdef.h"
#include "v_video.h"
#include "v_font.h"
#include "doomstat.h"
#include "v_palette.h"


static FRandom pr_camissile ("CustomActorfire");
static FRandom pr_camelee ("CustomMelee");
static FRandom pr_cabullet ("CustomBullet");
static FRandom pr_cajump ("CustomJump");
static FRandom pr_cwbullet ("CustomWpBullet");
static FRandom pr_cwjump ("CustomWpJump");
static FRandom pr_cwpunch ("CustomWpPunch");
static FRandom pr_grenade ("ThrowGrenade");
static FRandom pr_crailgun ("CustomRailgun");
static FRandom pr_spawndebris ("SpawnDebris");
static FRandom pr_spawnitemex ("SpawnItemEx");
static FRandom pr_burst ("Burst");
static FRandom pr_monsterrefire ("MonsterRefire");


//==========================================================================
//
// ACustomInventory :: CallStateChain
//
// Executes the code pointers in a chain of states
// until there is no next state
//
//==========================================================================

bool ACustomInventory::CallStateChain (AActor *actor, FState * State)
{
	StateCallData StateCall;
	bool result = false;
	int counter = 0;

	StateCall.Item = this;
	while (State != NULL)
	{
		// Assume success. The code pointer will set this to false if necessary
		StateCall.State = State;
		StateCall.Result = true;
		if (State->CallAction(actor, this, &StateCall))
		{
			// collect all the results. Even one successful call signifies overall success.
			result |= StateCall.Result;
		}


		// Since there are no delays it is a good idea to check for infinite loops here!
		counter++;
		if (counter >= 10000)	break;

		if (StateCall.State == State) 
		{
			// Abort immediately if the state jumps to itself!
			if (State == State->GetNextState()) 
			{
				return false;
			}
			
			// If both variables are still the same there was no jump
			// so we must advance to the next state.
			State = State->GetNextState();
		}
		else 
		{
			State = StateCall.State;
		}
	}
	return result;
}

//==========================================================================
//
// Simple flag changers
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetSolid)
{
	PARAM_ACTION_PROLOGUE;
	self->flags |= MF_SOLID;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetSolid)
{
	PARAM_ACTION_PROLOGUE;
	self->flags &= ~MF_SOLID;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetFloat)
{
	PARAM_ACTION_PROLOGUE;
	self->flags |= MF_FLOAT;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetFloat)
{
	PARAM_ACTION_PROLOGUE;
	self->flags &= ~(MF_FLOAT|MF_INFLOAT);
	return 0;
}

//==========================================================================
//
// Customizable attack functions which use actor parameters.
//
//==========================================================================
static void DoAttack (AActor *self, bool domelee, bool domissile,
					  int MeleeDamage, FSoundID MeleeSound, const PClass *MissileType,fixed_t MissileHeight)
{
	if (self->target == NULL) return;

	A_FaceTarget (self);
	if (domelee && MeleeDamage>0 && self->CheckMeleeRange ())
	{
		int damage = pr_camelee.HitDice(MeleeDamage);
		if (MeleeSound) S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
	}
	else if (domissile && MissileType != NULL)
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z+=MissileHeight-32*FRACUNIT;
		AActor * missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, MissileType, false);
		self->z-=MissileHeight-32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
			// set the health value so that the missile works properly
			if (missile->flags4&MF4_SPECTRAL)
			{
				missile->health=-2;
			}
			P_CheckMissileSpawn(missile);
		}
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_MeleeAttack)
{
	PARAM_ACTION_PROLOGUE;
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt(ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound = self->GetClass()->Meta.GetMetaInt(ACMETA_MeleeSound, 0);
	DoAttack(self, true, false, MeleeDamage, MeleeSound, NULL, 0);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_MissileAttack)
{
	PARAM_ACTION_PROLOGUE;
	const PClass *MissileType = PClass::FindClass(ENamedName(self->GetClass()->Meta.GetMetaInt(ACMETA_MissileName, NAME_None)));
	fixed_t MissileHeight = self->GetClass()->Meta.GetMetaFixed(ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, false, true, 0, 0, MissileType, MissileHeight);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ComboAttack)
{
	PARAM_ACTION_PROLOGUE;
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt(ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound =  self->GetClass()->Meta.GetMetaInt(ACMETA_MeleeSound, 0);
	const PClass *MissileType = PClass::FindClass(ENamedName(self->GetClass()->Meta.GetMetaInt(ACMETA_MissileName, NAME_None)));
	fixed_t MissileHeight = self->GetClass()->Meta.GetMetaFixed(ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BasicAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT	(melee_damage);
	PARAM_SOUND	(melee_sound);
	PARAM_CLASS	(missile_type, AActor);
	PARAM_FIXED	(missile_height);

	if (missile_type != NULL)
	{
		DoAttack(self, true, true, melee_damage, melee_sound, missile_type, missile_height);
	}
	return 0;
}

//==========================================================================
//
// Custom sound functions. 
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySound)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND_OPT	(soundid)		{ soundid = "weapons/pistol"; }
	PARAM_INT_OPT	(channel)		{ channel = CHAN_BODY; }
	PARAM_FLOAT_OPT	(volume)		{ volume = 1; }
	PARAM_BOOL_OPT	(looping)		{ looping = false; }
	PARAM_FLOAT_OPT	(attenuation)	{ attenuation = ATTN_NORM; }

	if (!looping)
	{
		S_Sound (self, channel, soundid, volume, attenuation);
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, channel&7, soundid))
		{
			S_Sound (self, channel | CHAN_LOOP, soundid, volume, attenuation);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSound)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT(slot) { slot = CHAN_VOICE; }

	S_StopSound(self, slot);
	return 0;
}

//==========================================================================
//
// These come from a time when DECORATE constants did not exist yet and
// the sound interface was less flexible. As a result the parameters are
// not optimal and these functions have been deprecated in favor of extending
// A_PlaySound and A_StopSound.
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlayWeaponSound)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND(soundid);

	S_Sound(self, CHAN_WEAPON, soundid, 1, ATTN_NORM);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySoundEx)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND		(soundid);
	PARAM_NAME		(channel);
	PARAM_BOOL_OPT	(looping)		  { looping = false; }
	PARAM_INT_OPT	(attenuation_raw) { attenuation_raw = 0; }

	float attenuation;
	switch (attenuation_raw)
	{
		case -1: attenuation = ATTN_STATIC;	break; // drop off rapidly
		default:
		case  0: attenuation = ATTN_NORM;	break; // normal
		case  1:
		case  2: attenuation = ATTN_NONE;	break; // full volume
	}

	if (channel < NAME_Auto || channel > NAME_SoundSlot7)
	{
		channel = NAME_Auto;
	}

	if (!looping)
	{
		S_Sound (self, int(channel) - NAME_Auto, soundid, 1, attenuation);
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, int(channel) - NAME_Auto, soundid))
		{
			S_Sound (self, (int(channel) - NAME_Auto) | CHAN_LOOP, soundid, 1, attenuation);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSoundEx)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME(channel);

	if (channel > NAME_Auto && channel <= NAME_SoundSlot7)
	{
		S_StopSound (self, int(channel) - NAME_Auto);
	}
	return 0;
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SeekerMissile)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(ang1);
	PARAM_INT(ang2);

	P_SeekerMissile(self, clamp<int>(ang1, 0, 90) * ANGLE_1, clamp<int>(ang2, 0, 90) * ANGLE_1);
	return 0;
}

//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_BulletAttack)
{
	PARAM_ACTION_PROLOGUE;

	int i;
	int bangle;
	int slope;
		
	if (!self->target) return 0;

	A_FaceTarget (self);
	bangle = self->angle;

	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	for (i = self->GetMissileDamage (0, 1); i > 0; --i)
    {
		int angle = bangle + (pr_cabullet.Random2() << 20);
		int damage = ((pr_cabullet()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage,
			NAME_None, NAME_BulletPuff);
    }
	return 0;
}


//==========================================================================
//
// Do the state jump
//
//==========================================================================
static void DoJump(AActor *self, FState *callingstate, FState *jumpto, StateCallData *statecall)
{
	if (jumpto == NULL) return;

	if (statecall != NULL)
	{
		statecall->State = jumpto;
	}
	else if (self->player != NULL && callingstate == self->player->psprites[ps_weapon].state)
	{
		P_SetPsprite(self->player, ps_weapon, jumpto);
	}
	else if (self->player != NULL && callingstate == self->player->psprites[ps_flash].state)
	{
		P_SetPsprite(self->player, ps_flash, jumpto);
	}
	else if (callingstate == self->state)
	{
		self->SetState(jumpto);
	}
	else
	{
		// something went very wrong. This should never happen.
		assert(false);
	}
}

// This is just to avoid having to directly reference the internally defined
// CallingState and statecall parameters in the code below.
#define ACTION_JUMP(offset) DoJump(self, callingstate, offset, statecall)

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Jump)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT(maxchance) { maxchance = 256; }

	int count = numparam - pnum;
	if (count > 0 && (maxchance >= 256 || pr_cajump() < maxchance))
	{
		int jumpnum = (count == 1 ? 0 : (pr_cajump() % count));
		PARAM_STATE_AT(pnum + jumpnum, jumpto);
		ACTION_JUMP(jumpto);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfHealthLower)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT	(health);
	PARAM_STATE	(jump);

	if (self->health < health)
	{
		ACTION_JUMP(jump);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfCloser)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED	(dist);
	PARAM_STATE	(jump);

	AActor *target;

	if (self->player == NULL)
	{
		target = self->target;
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (target == NULL)
		return 0;

	if (P_AproxDistance(self->x-target->x, self->y-target->y) < dist &&
		( (self->z > target->z && self->z - (target->z + target->height) < dist) || 
		  (self->z <=target->z && target->z - (self->z + self->height) < dist) 
		)
	   )
	{
		ACTION_JUMP(jump);
	}
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfInventory(AActor *owner, AActor *self, StateCallData *statecall, FState *callingstate, VMValue *param, int numparam)
{
	int pnum = NAP;
	PARAM_CLASS	(itemtype, AInventory);
	PARAM_INT	(itemamount);
	PARAM_STATE	(label);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (itemtype == NULL || owner == NULL)
		return;

	AInventory *item = owner->FindInventory(itemtype);

	if (item)
	{
		if (itemamount > 0)
		{
			if (item->Amount >= itemamount)
				ACTION_JUMP(label);
		}
		else if (item->Amount >= item->MaxAmount)
		{
			ACTION_JUMP(label);
		}
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInventory)
{
	PARAM_ACTION_PROLOGUE;
	DoJumpIfInventory(self, self, statecall, callingstate, param, numparam);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetInventory)
{
	PARAM_ACTION_PROLOGUE;
	DoJumpIfInventory(self->target, self, statecall, callingstate, param, numparam);
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfArmorType)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME	 (type);
	PARAM_STATE	 (label);
	PARAM_INT_OPT(amount) { amount = 1; }

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	ABasicArmor *armor = (ABasicArmor *)self->FindInventory(NAME_BasicArmor);

	if (armor && armor->ArmorType == type && armor->Amount >= amount)
		ACTION_JUMP(label);
	return 0;
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Explode)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(damage)		  { damage = -1; }
	PARAM_INT_OPT	(distance)		  { distance = -1; }
	PARAM_BOOL_OPT	(hurtSource)	  { hurtSource = true; }
	PARAM_BOOL_OPT	(alert)			  { alert = false; }
	PARAM_INT_OPT	(fulldmgdistance) { fulldmgdistance = 0; }
	PARAM_INT_OPT	(nails)			  { nails = 0; }
	PARAM_INT_OPT	(naildamage)	  { naildamage = 10; }

	if (damage < 0)	// get parameters from metadata
	{
		damage = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionDamage, 128);
		distance = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionRadius, damage);
		hurtSource = !self->GetClass()->Meta.GetMetaInt (ACMETA_DontHurtShooter);
		alert = false;
	}
	else
	{
		if (distance <= 0) distance = damage;
	}
	// NailBomb effect, from SMMU but not from its source code: instead it was implemented and
	// generalized from the documentation at http://www.doomworld.com/eternity/engine/codeptrs.html

	if (nails)
	{
		angle_t ang;
		for (int i = 0; i < nails; i++)
		{
			ang = i*(ANGLE_MAX/nails);
			// Comparing the results of a test wad with Eternity, it seems A_NailBomb does not aim
			P_LineAttack (self, ang, MISSILERANGE, 0,
				//P_AimLineAttack (self, ang, MISSILERANGE), 
				naildamage, NAME_None, NAME_BulletPuff);
		}
	}

	P_RadiusAttack (self, self->target, damage, distance, self->DamageType, hurtSource, true, fulldmgdistance);
	if (self->z <= self->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (self);
	}
	if (alert && self->target != NULL && self->target->player != NULL)
	{
		validcount++;
		P_RecursiveSound (self->Sector, self->target, false, 0);
	}
	return 0;
}

//==========================================================================
//
// A_RadiusThrust
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusThrust)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(force)			{ force = 128; }
	PARAM_INT_OPT	(distance)		{ distance = 1; }
	PARAM_BOOL_OPT	(affectSource)	{ affectSource = true; }

	if (force <= 0) force = 128;
	if (distance <= 0) distance = force;

	P_RadiusAttack (self, self->target, force, distance, self->DamageType, affectSource, false);
	if (self->z <= self->floorz + (distance << FRACBITS))
	{
		P_HitFloor (self);
	}
	return 0;
}

//==========================================================================
//
// Execute a line special / script
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CallSpecial)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(special);
	PARAM_INT_OPT	(arg1) { arg1 = 0; }
	PARAM_INT_OPT	(arg2) { arg2 = 0; }
	PARAM_INT_OPT	(arg3) { arg3 = 0; }
	PARAM_INT_OPT	(arg4) { arg4 = 0; }
	PARAM_INT_OPT	(arg5) { arg5 = 0; }

	bool res = !!LineSpecials[special](NULL, self, false, arg1, arg2, arg3, arg4, arg5);

	ACTION_SET_RESULT(res);
	return 0;
}

//==========================================================================
//
// Checks whether this actor is a missile
// Unfortunately this was buggy in older versions of the code and many
// released DECORATE monsters rely on this bug so it can only be fixed
// with an optional flag
//
//==========================================================================
inline static bool isMissile(AActor *self, bool precise=true)
{
	return (self->flags & MF_MISSILE) || (precise && (self->GetDefault()->flags & MF_MISSILE));
}

//==========================================================================
//
// The ultimate code pointer: Fully customizable missiles!
//
//==========================================================================
enum CM_Flags
{
	CMF_AIMMODE = 3,
	CMF_TRACKOWNER = 4,
	CMF_CHECKTARGETDEAD = 8,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomMissile)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(ti, AActor);
	PARAM_FIXED_OPT	(spawnheight) { spawnheight = 32*FRACUNIT; }
	PARAM_INT_OPT	(spawnofs_xy) { spawnofs_xy = 0; }
	PARAM_ANGLE_OPT	(angle)		  { angle = 0; }
	PARAM_INT_OPT	(flags)		  { flags = 0; }
	PARAM_INT_OPT	(pitch)		  { pitch = 0; }

	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile;

	if (self->target != NULL || aimmode == 2)
	{
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = spawnofs_xy * finecosine[ang];
			fixed_t y = spawnofs_xy * finesine[ang];
			fixed_t z = spawnheight - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

			switch (aimmode)
			{
			case 0:
			default:
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->x += x;
				self->y += y;
				self->z += z;
				missile = P_SpawnMissileXYZ(self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
				self->x -= x;
				self->y -= y;
				self->z -= z;
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+spawnheight, self, self->target, ti, false);
				break;

			case 2:
				self->x += x;
				self->y += y;
				missile = P_SpawnMissileAngleZSpeed(self, self->z+spawnheight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
				self->x -= x;
				self->y -= y;

				// It is not necessary to use the correct angle here.
				// The only important thing is that the horizontal velocity is correct.
				// Therefore use 0 as the missile's angle and simplify the calculations accordingly.
				// The actual velocity vector is set below.
				if (missile != NULL)
				{
					fixed_t vx = finecosine[pitch>>ANGLETOFINESHIFT];
					fixed_t vz = finesine[pitch>>ANGLETOFINESHIFT];

					missile->velx = FixedMul(vx, missile->Speed);
					missile->vely = 0;
					missile->velz = FixedMul(vz, missile->Speed);
				}

				break;
			}

			if (missile != NULL)
			{
				// Use the actual velocity instead of the missile's Speed property
				// so that this can handle missiles with a high vertical velocity 
				// component properly.
				FVector2 velocity (missile->velx, missile->vely);

				fixed_t missilespeed = (fixed_t)velocity.Length();

				missile->angle += angle;
				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->velx = FixedMul(missilespeed, finecosine[ang]);
				missile->vely = FixedMul(missilespeed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (isMissile(self, !!(flags & CMF_TRACKOWNER)))
                {
                	AActor *owner = self;//->target;
                	while (isMissile(owner, !!(flags & CMF_TRACKOWNER)) && owner->target)
						owner = owner->target;
                	targ = owner;
                	missile->target = owner;
					// automatic handling of seeker missiles
					if (self->flags & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer = self->tracer;
					}
                }
				else if (missile->flags2 & MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer = self->target;
				}
				// set the health value so that the missile works properly
				if (missile->flags4 & MF4_SPECTRAL)
				{
					missile->health = -2;
				}
				P_CheckMissileSpawn(missile);
			}
		}
	}
	else if (flags & CMF_CHECKTARGETDEAD)
	{
		// Target is dead and the attack shall be aborted.
		if (self->SeeState != NULL)
			self->SetState(self->SeeState);
	}
	return 0;
}

//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomBulletAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_OPT	(pufftype, AActor) { pufftype = PClass::FindClass(NAME_BulletPuff); }
	PARAM_FIXED_OPT	(range)			   { range = MISSILERANGE; }
	PARAM_BOOL_OPT	(aimfacing)		   { aimfacing = false; }

	if (range == 0)
		range = MISSILERANGE;

	int i;
	int bangle;
	int bslope;

	if (self->target || aimfacing)
	{
		if (!aimfacing) A_FaceTarget (self);
		bangle = self->angle;

		bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i = 0; i < numbullets; i++)
		{
			int angle = bangle + pr_cabullet.Random2() * (spread_xy / 255);
			int slope = bslope + pr_cabullet.Random2() * (spread_z / 255);
			int damage = ((pr_cabullet()%3)+1) * damageperbullet;
			P_LineAttack(self, angle, range, slope, damage, GetDefaultByType(pufftype)->DamageType, pufftype);
		}
    }
	return 0;
}

//==========================================================================
//
// A fully customizable melee attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomMeleeAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(damage)	 { damage = 0; }
	PARAM_SOUND_OPT	(meleesound) { meleesound = 0; }
	PARAM_SOUND_OPT	(misssound)	 { misssound = 0; }
	PARAM_NAME_OPT	(damagetype) { damagetype = NAME_None; }
	PARAM_BOOL_OPT	(bleed)		 { bleed = true; }

	if (damagetype == NAME_None)
		damagetype = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (damage, self->target, self);
	}
	else
	{
		if (misssound)
			S_Sound (self, CHAN_WEAPON, misssound, 1, ATTN_NORM);
	}
	return 0;
}

//==========================================================================
//
// A fully customizable combo attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomComboAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(ti, AActor);
	PARAM_FIXED		(spawnheight);
	PARAM_INT		(damage);
	PARAM_SOUND_OPT	(meleesound)	{ meleesound = 0; }
	PARAM_NAME_OPT	(damagetype)	{ damagetype = NAME_Melee; }
	PARAM_BOOL_OPT	(bleed)			{ bleed = true; }

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange())
	{
		if (damagetype == NAME_None)
			damagetype = NAME_Melee;	// Melee is the default type
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z += spawnheight - 32*FRACUNIT;
		AActor *missile = P_SpawnMissileXYZ(self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
		self->z -= spawnheight - 32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2 & MF2_SEEKERMISSILE)
			{
				missile->tracer = self->target;
			}
			// set the health value so that the missile works properly
			if (missile->flags4 & MF4_SPECTRAL)
			{
				missile->health = -2;
			}
			P_CheckMissileSpawn(missile);
		}
	}
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfNoAmmo)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (!ACTION_CALL_FROM_WEAPON())
		return 0;

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
	{
		ACTION_JUMP(jump);
	}
	return 0;
}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireBullets)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }
	PARAM_BOOL_OPT	(useammo)			{ useammo = true; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }

	if (!self->player) return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;

	int i;
	int bangle;
	int bslope;

	if (useammo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}
	
	if (range == 0)
		range = PLAYERMISSILERANGE;

	static_cast<APlayerPawn *>(self)->PlayAttacking2();

	bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (pufftype == NULL)
		pufftype = PClass::FindClass(NAME_BulletPuff);

	S_Sound(self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

	if ((numbullets == 1 && !player->refire) || numbullets == 0)
	{
		int damage = ((pr_cwbullet()%3)+1) * damageperbullet;
		P_LineAttack(self, bangle, range, bslope, damage, GetDefaultByType(pufftype)->DamageType, pufftype);
	}
	else 
	{
		if (numbullets < 0)
			numbullets = 1;
		for (i = 0; i < numbullets; i++)
		{
			int angle = bangle + pr_cwbullet.Random2() * (spread_xy / 255);
			int slope = bslope + pr_cwbullet.Random2() * (spread_z / 255);
			int damage = ((pr_cwbullet()%3)+1) * damageperbullet;
			P_LineAttack(self, angle, range, slope, damage, GetDefaultByType(pufftype)->DamageType, pufftype);
		}
	}
	return 0;
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireCustomMissile)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(ti, AActor);
	PARAM_ANGLE_OPT	(angle)			{ angle = 0; }
	PARAM_BOOL_OPT	(useammo)		{ useammo = true; }
	PARAM_INT_OPT	(spawnofs_xy)	{ spawnofs_xy = 0; }
	PARAM_FIXED_OPT	(spawnheight)	{ spawnheight = 0; }
	PARAM_BOOL_OPT	(aimatangle)	{ aimatangle = false; }
	PARAM_ANGLE_OPT	(pitch)			{ pitch = 0; }

	if (!self->player)
		return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;
	AActor *linetarget;

	if (useammo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	if (ti) 
	{
		angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = spawnofs_xy * finecosine[ang];
		fixed_t y = spawnofs_xy * finesine[ang];
		fixed_t z = spawnheight;
		fixed_t shootangle = self->angle;

		if (aimatangle) shootangle += angle;

		// Temporarily adjusts the pitch
		fixed_t saved_player_pitch = self->pitch;
		self->pitch -= pitch;
		AActor *misl = P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget);
		self->pitch = saved_player_pitch;
		// automatic handling of seeker missiles
		if (misl)
		{
			if (linetarget && (misl->flags2 & MF2_SEEKERMISSILE))
				misl->tracer = linetarget;
			if (!aimatangle)
			{
				// This original implementation is to aim straight ahead and then offset
				// the angle from the resulting direction. 
				FVector3 velocity(misl->velx, misl->vely, 0);
				fixed_t missilespeed = (fixed_t)velocity.Length();
				misl->angle += angle;
				angle_t an = misl->angle >> ANGLETOFINESHIFT;
				misl->velx = FixedMul (missilespeed, finecosine[an]);
				misl->vely = FixedMul (missilespeed, finesine[an]);
			}
		}
	}
	return 0;
}


//==========================================================================
//
// A_CustomPunch
//
// Berserk is not handled here. That can be done with A_CheckIfInventory
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomPunch)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(damage);
	PARAM_BOOL_OPT	(norandom)			{ norandom = false; }
	PARAM_BOOL_OPT	(useammo)			{ useammo = true; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }

	if (!self->player)
		return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;
	AActor *	linetarget;

	if (!norandom)
		damage *= pr_cwpunch() % 8 + 1;

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (range == 0)
		range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, range, &linetarget);

	// only use ammo when actually hitting something!
	if (useammo && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	if (pufftype == NULL)
		pufftype = PClass::FindClass(NAME_BulletPuff);

	P_LineAttack (self, angle, range, pitch, damage, GetDefaultByType(pufftype)->DamageType, pufftype, true);

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

		self->angle = R_PointToAngle2 (self->x, self->y, linetarget->x, linetarget->y);
	}
	return 0;
}


enum
{	
	RAF_SILENT = 1,
	RAF_NOPIERCE = 2
};

//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RailAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(damage);
	PARAM_INT_OPT	(spawnofs_xy)		{ spawnofs_xy = 0; }
	PARAM_BOOL_OPT	(useammo)			{ useammo = true; }
	PARAM_COLOR_OPT	(color1)			{ color1 = 0; }
	PARAM_COLOR_OPT	(color2)			{ color2 = 0; }
	PARAM_INT_OPT	(flags)				{ flags = 0; }
	PARAM_FLOAT_OPT	(maxdiff)			{ maxdiff = 0; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = PClass::FindClass(NAME_BulletPuff); }

	if (!self->player)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (useammo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	P_RailAttack (self, damage, spawnofs_xy, color1, color2, float(maxdiff), (flags & RAF_SILENT), pufftype, (!(flags & RAF_NOPIERCE)));
	return 0;
}

//==========================================================================
//
// also for monsters
//
//==========================================================================
enum
{
	CRF_DONTAIM = 0,
	CRF_AIMPARALLEL = 1,
	CRF_AIMDIRECT = 2
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomRailgun)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(damage);
	PARAM_INT_OPT	(spawnofs_xy)		{ spawnofs_xy = 0; }
	PARAM_COLOR_OPT	(color1)			{ color1 = 0; }
	PARAM_COLOR_OPT	(color2)			{ color2 = 0; }
	PARAM_INT_OPT	(flags)				{ flags = 0; }
	PARAM_INT_OPT	(aim)				{ aim = CRF_DONTAIM; }
	PARAM_FLOAT_OPT	(maxdiff)			{ maxdiff = 0; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = PClass::FindClass(NAME_BulletPuff); }

	fixed_t saved_x = self->x;
	fixed_t saved_y = self->y;
	angle_t saved_angle = self->angle;

	if (aim && self->target == NULL)
	{
		return 0;
	}
	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	self->flags &= ~MF_AMBUSH;


	if (aim)
	{
		self->angle = R_PointToAngle2 (self->x,
										self->y,
										self->target->x,
										self->target->y);
	}

	self->pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);

	// Let the aim trail behind the player
	if (aim)
	{
		saved_angle = self->angle = R_PointToAngle2 (self->x, self->y,
										self->target->x - self->target->velx * 3,
										self->target->y - self->target->vely * 3);

		if (aim == CRF_AIMDIRECT)
		{
			// Tricky: We must offset to the angle of the current position
			// but then change the angle again to ensure proper aim.
			self->x += spawnofs_xy * finecosine[self->angle];
			self->y += spawnofs_xy * finesine[self->angle];
			spawnofs_xy = 0;
			self->angle = R_PointToAngle2 (self->x, self->y,
											self->target->x - self->target->velx * 3,
											self->target->y - self->target->vely * 3);
		}

		if (self->target->flags & MF_SHADOW)
		{
			angle_t rnd = pr_crailgun.Random2() << 21;
			self->angle += rnd;
			saved_angle = rnd;
		}
	}

	angle_t angle = (self->angle - ANG90) >> ANGLETOFINESHIFT;

	P_RailAttack (self, damage, spawnofs_xy, color1, color2, float(maxdiff), flags & RAF_SILENT, pufftype, !(flags & RAF_NOPIERCE));

	self->x = saved_x;
	self->y = saved_y;
	self->angle = saved_angle;
	return 0;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor *receiver, StateCallData *statecall, VM_ARGS)
{
	int pnum = NAP;
	PARAM_CLASS		(mi, AInventory);
	PARAM_INT_OPT	(amount)			{ amount = 1; }

	bool res = true;
	if (receiver == NULL) return;

	if (amount <= 0)
	{
		amount = 1;
	}
	if (mi) 
	{
		AInventory *item = static_cast<AInventory *>(Spawn(mi, 0, 0, 0, NO_REPLACE));
		if (item->IsKindOf(RUNTIME_CLASS(AHealth)))
		{
			item->Amount *= amount;
		}
		else
		{
			item->Amount = amount;
		}
		item->flags |= MF_DROPPED;
		if (item->flags & MF_COUNTITEM)
		{
			item->flags &= ~MF_COUNTITEM;
			level.total_items--;
		}
		if (!item->CallTryPickup(receiver))
		{
			item->Destroy();
			res = false;
		}
		else
		{
			res = true;
		}
	}
	else
	{
		res = false;
	}
	ACTION_SET_RESULT(res);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveInventory)
{
	PARAM_ACTION_PROLOGUE;
	DoGiveInventory(self, statecall, VM_ARGS_NAMES);
	return 0;
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToTarget)
{
	PARAM_ACTION_PROLOGUE;
	DoGiveInventory(self->target, statecall, VM_ARGS_NAMES);
	return 0;
}

//===========================================================================
//
// A_TakeInventory
//
//===========================================================================

void DoTakeInventory(AActor *receiver, StateCallData *statecall, VM_ARGS)
{
	int pnum = NAP;
	PARAM_CLASS		(itemtype, AInventory);
	PARAM_INT_OPT	(amount)		{ amount = 0; }
	
	if (itemtype == NULL || receiver == NULL) return;

	bool res = false;

	AInventory *inv = receiver->FindInventory(itemtype);

	if (inv && !inv->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
	{
		if (inv->Amount > 0)
		{
			res = true;
		}
		if (amount == 0 || amount >= inv->Amount) 
		{
			if (inv->ItemFlags & IF_KEEPDEPLETED)
				inv->Amount = 0;
			else 
				inv->Destroy();
		}
		else
		{
			inv->Amount -= amount;
		}
	}
	ACTION_SET_RESULT(res);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeInventory)
{
	PARAM_ACTION_PROLOGUE;
	DoTakeInventory(self, statecall, VM_ARGS_NAMES);
	return 0;
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromTarget)
{
	PARAM_ACTION_PROLOGUE;
	DoTakeInventory(self->target, statecall, VM_ARGS_NAMES);
	return 0;
}	

//===========================================================================
//
// Common code for A_SpawnItem and A_SpawnItemEx
//
//===========================================================================
enum SIX_Flags
{
	SIXF_TRANSFERTRANSLATION=1,
	SIXF_ABSOLUTEPOSITION=2,
	SIXF_ABSOLUTEANGLE=4,
	SIXF_ABSOLUTEVELOCITY=8,
	SIXF_SETMASTER=16,
	SIXF_NOCHECKPOSITION=32,
	SIXF_TELEFRAG=64,
	// 128 is used by Skulltag!
	SIXF_TRANSFERAMBUSHFLAG=256,
	SIXF_TRANSFERPITCH=512,
	SIXF_TRANSFERPOINTERS=1024,
};


static bool InitSpawnedItem(AActor *self, AActor *mo, int flags)
{
	if (mo)
	{
		AActor * originator = self;

		if ((flags & SIXF_TRANSFERTRANSLATION) && !(mo->flags2 & MF2_DONTTRANSLATE))
		{
			mo->Translation = self->Translation;
		}
		if (flags & SIXF_TRANSFERPOINTERS)
		{
			mo->target = self->target;
			mo->master = self->master; // This will be overridden later if SIXF_SETMASTER is set
			mo->tracer = self->tracer;
		}

		mo->angle=self->angle;
		if (flags & SIXF_TRANSFERPITCH) mo->pitch = self->pitch;
		while (originator && isMissile(originator)) originator = originator->target;

		if (flags & SIXF_TELEFRAG) 
		{
			P_TeleportMove(mo, mo->x, mo->y, mo->z, true);
			// This is needed to ensure consistent behavior.
			// Otherwise it will only spawn if nothing gets telefragged
			flags |= SIXF_NOCHECKPOSITION;	
		}
		if (mo->flags3&MF3_ISMONSTER)
		{
			if (!(flags&SIXF_NOCHECKPOSITION) && !P_TestMobjLocation(mo))
			{
				// The monster is blocked so don't spawn it at all!
				if (mo->CountsAsKill()) level.total_monsters--;
				mo->Destroy();
				return false;
			}
			else if (originator)
			{
				if (originator->flags3&MF3_ISMONSTER)
				{
					// If this is a monster transfer all friendliness information
					mo->CopyFriendliness(originator, true);
					if (flags&SIXF_SETMASTER) mo->master = originator;	// don't let it attack you (optional)!
				}
				else if (originator->player)
				{
					// A player always spawns a monster friendly to him
					mo->flags|=MF_FRIENDLY;
					mo->FriendPlayer = int(originator->player-players+1);

					AActor * attacker=originator->player->attacker;
					if (attacker)
					{
						if (!(attacker->flags&MF_FRIENDLY) || 
							(deathmatch && attacker->FriendPlayer!=0 && attacker->FriendPlayer!=mo->FriendPlayer))
						{
							// Target the monster which last attacked the player
							mo->LastHeard = mo->target = attacker;
						}
					}
				}
			}
		}
		else if (!(flags & SIXF_TRANSFERPOINTERS))
		{
			// If this is a missile or something else set the target to the originator
			mo->target=originator? originator : self;
		}
	}
	return true;
}

//===========================================================================
//
// A_SpawnItem
//
// Spawns an item in front of the caller like Heretic's time bomb
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnItem)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT	(missile, AActor)		{ missile = PClass::FindClass("Unknown"); }
	PARAM_FIXED_OPT	(distance)				{ distance = 0; }
	PARAM_FIXED_OPT	(zheight)				{ zheight = 0; }
	PARAM_BOOL_OPT	(useammo)				{ useammo = true; }
	PARAM_BOOL_OPT	(transfer_translation)	{ transfer_translation = false; }

	if (missile == NULL)
	{
		ACTION_SET_RESULT(false);
		return 0;
	}

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
		return 0;

	if (distance == 0) 
	{
		// use the minimum distance that does not result in an overlap
		distance = (self->radius + GetDefaultByType(missile)->radius) >> FRACBITS;
	}

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (weapon == NULL)
			return 0;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
			return 0;
	}

	AActor *mo = Spawn(missile, 
					self->x + FixedMul(distance, finecosine[self->angle>>ANGLETOFINESHIFT]), 
					self->y + FixedMul(distance, finesine[self->angle>>ANGLETOFINESHIFT]), 
					self->z - self->floorclip + zheight, ALLOW_REPLACE);

	int flags = (transfer_translation ? SIXF_TRANSFERTRANSLATION : 0) + (useammo ? SIXF_SETMASTER : 0);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	return 0;
}

//===========================================================================
//
// A_SpawnItemEx
//
// Enhanced spawning function
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnItemEx)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(missile, AActor);
	PARAM_FIXED_OPT	(xofs)		{ xofs = 0; }
	PARAM_FIXED_OPT	(yofs)		{ yofs = 0; }
	PARAM_FIXED_OPT	(zofs)		{ zofs = 0; }
	PARAM_FIXED_OPT	(xvel)		{ xvel = 0; }
	PARAM_FIXED_OPT	(yvel)		{ yvel = 0; }
	PARAM_FIXED_OPT	(zvel)		{ zvel = 0; }
	PARAM_ANGLE_OPT	(angle)		{ angle = 0; }
	PARAM_INT_OPT	(flags)		{ flags = 0; }
	PARAM_INT_OPT	(chance)	{ chance = 0; }

	if (missile == NULL) 
	{
		ACTION_SET_RESULT(false);
		return 0;
	}

	if (chance > 0 && pr_spawnitemex() < chance)
		return 0;

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
		return 0;

	fixed_t x, y;

	if (!(flags & SIXF_ABSOLUTEANGLE))
	{
		angle += self->angle;
	}

	angle_t ang = angle >> ANGLETOFINESHIFT;

	if (flags & SIXF_ABSOLUTEPOSITION)
	{
		x = self->x + xofs;
		y = self->y + yofs;
	}
	else
	{
		// in relative mode negative y values mean 'left' and positive ones mean 'right'
		// This is the inverse orientation of the absolute mode!
		x = self->x + FixedMul(xofs, finecosine[ang]) + FixedMul(yofs, finesine[ang]);
		y = self->y + FixedMul(xofs, finesine[ang]) - FixedMul(yofs, finecosine[ang]);
	}

	if (!(flags & SIXF_ABSOLUTEVELOCITY))
	{
		// Same orientation issue here!
		fixed_t newxvel = FixedMul(xvel, finecosine[ang]) + FixedMul(yvel, finesine[ang]);
		yvel = FixedMul(xvel, finesine[ang]) - FixedMul(yvel, finecosine[ang]);
		xvel = newxvel;
	}

	AActor *mo = Spawn(missile, x, y, self->z - self->floorclip + zofs, ALLOW_REPLACE);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	if (mo)
	{
		mo->velx = xvel;
		mo->vely = yvel;
		mo->velz = zvel;
		mo->angle = angle;
		if (flags & SIXF_TRANSFERAMBUSHFLAG)
			mo->flags = (mo->flags & ~MF_AMBUSH) | (self->flags & MF_AMBUSH);
	}
	return 0;
}

//===========================================================================
//
// A_ThrowGrenade
//
// Throws a grenade (like Hexen's fighter flechette)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ThrowGrenade)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(missile, AActor);
	PARAM_FIXED_OPT	(zheight)		{ zheight = 0; }
	PARAM_FIXED_OPT	(xyvel)			{ xyvel = 0; }
	PARAM_FIXED_OPT	(zvel)			{ zvel = 0; }
	PARAM_BOOL_OPT	(useammo)		{ useammo = true; }

	if (missile == NULL)
		return 0;

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (weapon == NULL)
			return 0;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
			return 0;
	}

	AActor *bo;

	bo = Spawn(missile, self->x, self->y, 
			self->z - self->floorclip + zheight + 35*FRACUNIT + (self->player ? self->player->crouchoffset : 0),
			ALLOW_REPLACE);
	if (bo)
	{
		int pitch = self->pitch;

		P_PlaySpawnSound(bo, self);
		if (xyvel)
			bo->Speed = xyvel;
		bo->angle = self->angle + (((pr_grenade()&7) - 4) << 24);
		bo->velz = zvel + 2*finesine[pitch>>ANGLETOFINESHIFT];
		bo->z += 2 * finesine[pitch>>ANGLETOFINESHIFT];
		P_ThrustMobj(bo, bo->angle, bo->Speed);
		bo->velx += self->velx >> 1;
		bo->vely += self->vely >> 1;
		bo->target= self;
		P_CheckMissileSpawn (bo);
	} 
	else
	{
		ACTION_SET_RESULT(false);
	}
	return 0;
}


//===========================================================================
//
// A_Recoil
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Recoil)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED(xyvel);

	angle_t angle = self->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	self->velx += FixedMul(xyvel, finecosine[angle]);
	self->vely += FixedMul(xyvel, finesine[angle]);
	return 0;
}


//===========================================================================
//
// A_SelectWeapon
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SelectWeapon)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS(cls, AWeapon);

	if (cls == NULL || self->player == NULL) 
	{
		ACTION_SET_RESULT(false);
		return 0;
	}

	AWeapon *weaponitem = static_cast<AWeapon*>(self->FindInventory(cls));

	if (weaponitem != NULL && weaponitem->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{
		if (self->player->ReadyWeapon != weaponitem)
		{
			self->player->PendingWeapon = weaponitem;
		}
	}
	else
	{
		ACTION_SET_RESULT(false);
	}
	return 0;
}


//===========================================================================
//
// A_Print
//
//===========================================================================
EXTERN_CVAR(Float, con_midtime)

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Print)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STRING	(text);
	PARAM_FLOAT_OPT	(time)		{ time = 0; }
	PARAM_NAME_OPT	(fontname)	{ fontname = NAME_None; }

	if (self->CheckLocalView (consoleplayer) ||
		(self->target != NULL && self->target->CheckLocalView (consoleplayer)))
	{
		float saved = con_midtime;
		FFont *font = NULL;
		
		if (fontname != NAME_None)
		{
			font = V_GetFont(fontname);
		}
		if (time > 0)
		{
			con_midtime = float(time);
		}
		C_MidPrint(font != NULL ? font : SmallFont, text);
		con_midtime = saved;
	}
	return 0;
}

//===========================================================================
//
// A_PrintBold
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PrintBold)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STRING	(text);
	PARAM_FLOAT_OPT	(time)		{ time = 0; }
	PARAM_NAME_OPT	(fontname)	{ fontname = NAME_None; }

	float saved = con_midtime;
	FFont *font = NULL;
	
	if (fontname != NAME_None)
	{
		font = V_GetFont(fontname);
	}
	if (time > 0)
	{
		con_midtime = float(time);
	}
	C_MidPrintBold(font != NULL ? font : SmallFont, text);
	con_midtime = saved;
	return 0;
}

//===========================================================================
//
// A_Log
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Log)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STRING(text);
	Printf("%s\n", text);
	return 0;
}

//===========================================================================
//
// A_SetTranslucent
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTranslucent)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED		(alpha);
	PARAM_INT_OPT	(mode)	{ mode = 0; }

	mode = mode == 0 ? STYLE_Translucent : mode == 2 ? STYLE_Fuzzy : STYLE_Add;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha = clamp<fixed_t>(alpha, 0, FRACUNIT);
	self->RenderStyle = ERenderStyle(mode);
	return 0;
}

//===========================================================================
//
// A_FadeIn
//
// Fades the actor in
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeIn)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT(reduce)	{ reduce = FRACUNIT/10; }

	if (reduce == 0)
		reduce = FRACUNIT/10;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;
	return 0;
}

//===========================================================================
//
// A_FadeOut
//
// fades the actor out and destroys it when done
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeOut)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT(reduce)	{ reduce = FRACUNIT/10; }
	PARAM_BOOL_OPT(remove)	{ remove = true; }

	if (reduce == 0)
		reduce = FRACUNIT/10;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;
	if (self->alpha<=0 && remove)
		self->Destroy();
	return 0;
}

//===========================================================================
//
// A_SpawnDebris
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnDebris)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(debris, AActor);
	PARAM_BOOL_OPT	(transfer_translation)	{ transfer_translation = false; }
	PARAM_FIXED_OPT	(mult_h)				{ mult_h = FRACUNIT; }
	PARAM_FIXED_OPT	(mult_v)				{ mult_v = FRACUNIT; }
	int i;
	AActor *mo;

	if (debris == NULL)
		return 0;

	// only positive values make sense here
	if (mult_v <= 0)
		mult_v = FRACUNIT;
	if (mult_h <= 0)
		mult_h = FRACUNIT;
	
	for (i = 0; i < GetDefaultByType(debris)->health; i++)
	{
		mo = Spawn(debris, self->x+((pr_spawndebris()-128)<<12),
			self->y+((pr_spawndebris()-128)<<12), 
			self->z+(pr_spawndebris()*self->height/256), ALLOW_REPLACE);
		if (mo && transfer_translation)
		{
			mo->Translation = self->Translation;
		}
		if (mo && i < mo->GetClass()->ActorInfo->NumOwnedStates)
		{
			mo->SetState (mo->GetClass()->ActorInfo->OwnedStates + i);
			mo->velz = FixedMul(mult_v, ((pr_spawndebris()&7)+5)*FRACUNIT);
			mo->velx = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
			mo->vely = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
		}
	}
	return 0;
}


//===========================================================================
//
// A_CheckSight
// jumps if no player can see this actor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSight)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	for (int i = 0; i < MAXPLAYERS; i++) 
	{
		if (playeringame[i] && P_CheckSight(players[i].camera, self, true))
			return 0;
	}

	ACTION_JUMP(jump);
	return 0;
}


//===========================================================================
//
// Inventory drop
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropInventory)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS(drop, AInventory);

	if (drop)
	{
		AInventory *inv = self->FindInventory(drop);
		if (inv)
		{
			self->DropInventory(inv);
		}
	}
	return 0;
}


//===========================================================================
//
// A_SetBlend
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetBlend)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_COLOR		(color);
	PARAM_FLOAT		(alpha);
	PARAM_INT		(tics);
	PARAM_COLOR_OPT	(color2)	{ color2 = 0; }

	if (color == MAKEARGB(255,255,255,255))
		color = 0;
	if (color2 == MAKEARGB(255,255,255,255))
		color2 = 0;
	if (color2.a == 0)
		color2 = color;

	new DFlashFader(color.r/255.f, color.g/255.f, color.b/255.f, float(alpha),
					color2.r/255.f, color2.g/255.f, color2.b/255.f, 0,
					float(tics)/TICRATE, self);
	return 0;
}


//===========================================================================
//
// A_JumpIf
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIf)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL	(condition);
	PARAM_STATE	(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (condition)
		ACTION_JUMP(jump);
	return 0;
}

//===========================================================================
//
// A_KillMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillMaster)
{
	PARAM_ACTION_PROLOGUE;
	if (self->master != NULL)
	{
		P_DamageMobj(self->master, self, self, self->master->health, NAME_None, DMG_NO_ARMOR);
	}
	return 0;
}

//===========================================================================
//
// A_KillChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillChildren)
{
	PARAM_ACTION_PROLOGUE;
	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			P_DamageMobj(mo, self, self, mo->health, NAME_None, DMG_NO_ARMOR);
		}
	}
	return 0;
}

//===========================================================================
//
// A_KillSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillSiblings)
{
	PARAM_ACTION_PROLOGUE;
	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			P_DamageMobj(mo, self, self, mo->health, NAME_None, DMG_NO_ARMOR);
		}
	}
	return 0;
}

//===========================================================================
//
// A_CountdownArg
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CountdownArg)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(argnum);

	if (argnum > 0 && argnum < countof(self->args))
	{
		if (!self->args[argnum]--)
		{
			if (self->flags & MF_MISSILE)
			{
				P_ExplodeMissile(self, NULL, NULL);
			}
			else if (self->flags & MF_SHOOTABLE)
			{
				P_DamageMobj(self, NULL, NULL, self->health, NAME_None, DMG_FORCED);
			}
			else
			{
				self->SetState(self->FindState(NAME_Death));
			}
		}
	}
	return 0;
}

//============================================================================
//
// A_Burst
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Burst)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS(chunk, AActor);

   int i, numChunks;
   AActor *mo;

   if (chunk == NULL)
	   return 0;

   self->velx = self->vely = self->velz = 0;
   self->height = self->GetDefault()->height;

   // [RH] In Hexen, this creates a random number of shards (range [24,56])
   // with no relation to the size of the self shattering. I think it should
   // base the number of shards on the size of the dead thing, so bigger
   // things break up into more shards than smaller things.
   // An self with radius 20 and height 64 creates ~40 chunks.
   numChunks = MAX<int> (4, (self->radius>>FRACBITS)*(self->height>>FRACBITS)/32);
   i = (pr_burst.Random2()) % (numChunks/4);
   for (i = MAX (24, numChunks + i); i >= 0; i--)
   {
      mo = Spawn(chunk,
         self->x + (((pr_burst()-128)*self->radius)>>7),
         self->y + (((pr_burst()-128)*self->radius)>>7),
         self->z + (pr_burst()*self->height/255), ALLOW_REPLACE);

	  if (mo)
      {
         mo->velz = FixedDiv(mo->z - self->z, self->height)<<2;
         mo->velx = pr_burst.Random2 () << (FRACBITS-7);
         mo->vely = pr_burst.Random2 () << (FRACBITS-7);
         mo->RenderStyle = self->RenderStyle;
         mo->alpha = self->alpha;
		 mo->CopyFriendliness(self, true);
      }
   }

   // [RH] Do some stuff to make this more useful outside Hexen
   if (self->flags4 & MF4_BOSSDEATH)
   {
		CALL_ACTION(A_BossDeath, self);
   }
   CALL_ACTION(A_NoBlocking, self);

   self->Destroy();
   return 0;
}

//===========================================================================
//
// A_CheckFloor
// [GRB] Jumps if actor is standing on floor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckFloor)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->z <= self->floorz)
	{
		ACTION_JUMP(jump);
	}
	return 0;
}

//===========================================================================
//
// A_CheckCeiling
// [GZ] Totally copied from A_CheckFloor, jumps if actor touches ceiling
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckCeiling)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);
	if (self->z + self->height >= self->ceilingz) // Height needs to be counted
	{
		ACTION_JUMP(jump);
	}
	return 0;
}

//===========================================================================
//
// A_Stop
// resets all velocity of the actor to 0
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Stop)
{
	PARAM_ACTION_PROLOGUE;
	self->velx = self->vely = self->velz = 0;
	if (self->player && self->player->mo == self && !(self->player->cheats & CF_PREDICTING))
	{
		self->player->mo->PlayIdle();
		self->player->velx = self->player->vely = 0;
	}
	return 0;
}

static void CheckStopped(AActor *self)
{
	if (self->player != NULL &&
		self->player->mo == self &&
		!(self->player->cheats & CF_PREDICTING) &&
		!(self->velx | self->vely | self->velz))
	{
		self->player->mo->PlayIdle();
		self->player->velx = self->player->vely = 0;
	}
}

//===========================================================================
//
// A_Respawn
//
//===========================================================================

enum RS_Flags
{
	RSF_FOG=1,
	RSF_KEEPTARGET=2,
	RSF_TELEFRAG=4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Respawn)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT(flags) { flags = RSF_FOG; }

	fixed_t x = self->SpawnPoint[0];
	fixed_t y = self->SpawnPoint[1];
	bool oktorespawn = false;
	sector_t *sec;

	self->flags |= MF_SOLID;
	sec = P_PointInSector (x, y);
	self->height = self->GetDefault()->height;

	if (flags & RSF_TELEFRAG)
	{
		// [KS] DIE DIE DIE DIE erm *ahem* =)
		if (P_TeleportMove(self, x, y, sec->floorplane.ZatPoint (x, y), true))
			oktorespawn = true;
	}
	else
	{
		self->SetOrigin(x, y, sec->floorplane.ZatPoint (x, y));
		if (P_TestMobjLocation(self))
			oktorespawn = true;
	}

	if (oktorespawn)
	{
		AActor *defs = self->GetDefault();
		self->health = defs->health;

		// [KS] Don't keep target, because it could be self if the monster committed suicide
		//      ...Actually it's better off an option, so you have better control over monster behavior.
		if (!(flags & RSF_KEEPTARGET))
		{
			self->target = NULL;
			self->LastHeard = NULL;
			self->lastenemy = NULL;
		}
		else
		{
			// Don't attack yourself (Re: "Marine targets itself after suicide")
			if (self->target == self)
				self->target = NULL;
			if (self->lastenemy == self)
				self->lastenemy = NULL;
		}

		self->flags  = (defs->flags & ~MF_FRIENDLY) | (self->flags & MF_FRIENDLY);
		self->flags2 = defs->flags2;
		self->flags3 = (defs->flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (self->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
		self->flags4 = (defs->flags4 & ~MF4_NOHATEPLAYERS) | (self->flags4 & MF4_NOHATEPLAYERS);
		self->flags5 = defs->flags5;
		self->SetState (self->SpawnState);
		self->renderflags &= ~RF_INVISIBLE;

		if (flags & RSF_FOG)
		{
			Spawn<ATeleportFog>(x, y, self->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		}
		if (self->CountsAsKill())
			level.total_monsters++;
	}
	else
	{
		self->flags &= ~MF_SOLID;
	}
	return 0;
}


//==========================================================================
//
// A_PlayerSkinCheck
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlayerSkinCheck)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->player != NULL &&
		skins[self->player->userinfo.skin].othergame)
	{
		ACTION_JUMP(jump);
	}
	return 0;
}

//===========================================================================
//
// A_SetGravity
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetGravity)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED(gravity);
	
	self->gravity = clamp<fixed_t>(gravity, 0, FRACUNIT*10); 
	return 0;
}


// [KS] *** Start of my modifications ***

//===========================================================================
//
// A_ClearTarget
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClearTarget)
{
	PARAM_ACTION_PROLOGUE;
	self->target = NULL;
	self->LastHeard = NULL;
	self->lastenemy = NULL;
	return 0;
}

//==========================================================================
//
// A_JumpIfTargetInLOS (state label, optional fixed fov, optional bool
// projectiletarget)
//
// Jumps if the actor can see its target, or if the player has a linetarget.
// ProjectileTarget affects how projectiles are treated. If set, it will use
// the target of the projectile for seekers, and ignore the target for
// normal projectiles. If not set, it will use the missile's owner instead
// (the default).
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetInLOS)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE		(jump);
	PARAM_ANGLE_OPT	(fov)		{ fov = 0; }
	PARAM_BOOL_OPT	(projtarg)	{ projtarg = false; }

	angle_t an;
	AActor *target;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (!self->player)
	{
		if (self->flags & MF_MISSILE && projtarg)
		{
			if (self->flags2 & MF2_SEEKERMISSILE)
				target = self->tracer;
			else
				target = NULL;
		}
		else
		{
			target = self->target;
		}

		if (target == NULL)
			return 0; // [KS] Let's not call P_CheckSight unnecessarily in this case.

		if (!P_CheckSight (self, target, 1))
			return 0;

		if (fov && (fov < ANGLE_MAX))
		{
			an = R_PointToAngle2(self->x, self->y, target->x, target->y) - self->angle;

			if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
			{
				return 0; // [KS] Outside of FOV - return
			}

		}
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}

	if (target == NULL)
		return 0;

	ACTION_JUMP(jump);
	return 0;
}


//==========================================================================
//
// A_JumpIfInTargetLOS (state label, optional fixed fov, optional bool
// projectiletarget)
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetLOS)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE		(jump);
	PARAM_ANGLE_OPT	(fov)		{ fov = 0; }
	PARAM_BOOL_OPT	(projtarg)	{ projtarg = false; }

	angle_t an;
	AActor *target;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (self->flags & MF_MISSILE && projtarg)
	{
		if (self->flags2 & MF2_SEEKERMISSILE)
			target = self->tracer;
		else
			target = NULL;
	}
	else
	{
		target = self->target;
	}

	if (target == NULL)
		return 0; // [KS] Let's not call P_CheckSight unnecessarily in this case.

	if (!P_CheckSight (target, self, 1))
		return 0;

	if (fov && (fov < ANGLE_MAX))
	{
		an = R_PointToAngle2(self->x, self->y, target->x, target->y) - self->angle;

		if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
		{
			return 0; // [KS] Outside of FOV - return
		}
	}
	ACTION_JUMP(jump);
	return 0;
}


//===========================================================================
//
// A_DamageMaster (int amount)
// Damages the master of this child by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageMaster)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(amount);
	PARAM_NAME_OPT	(damagetype)	{ damagetype = NAME_None; }

	if (self->master != NULL)
	{
		if (amount > 0)
		{
			P_DamageMobj(self->master, self, self, amount, damagetype, DMG_NO_ARMOR);
		}
		else if (amount < 0)
		{
			amount = -amount;
			P_GiveBody(self->master, amount);
		}
	}
	return 0;
}

//===========================================================================
//
// A_DamageChildren (amount)
// Damages the children of this master by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageChildren)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(amount);
	PARAM_NAME_OPT	(damagetype)	{ damagetype = NAME_None; }

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, damagetype, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
	return 0;
}

// [KS] *** End of my modifications ***

//===========================================================================
//
// A_DamageSiblings (amount)
// Damages the siblings of this master by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageSiblings)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(amount);
	PARAM_NAME_OPT	(damagetype)	{ damagetype = NAME_None; }

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, damagetype, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
	return 0;
}


//===========================================================================
//
// Modified code pointer from Skulltag
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckForReload)
{
	PARAM_ACTION_PROLOGUE;

	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
		return 0;

	PARAM_INT		(count);
	PARAM_STATE		(jump);
	PARAM_BOOL_OPT	(dontincrement)		{ dontincrement = false; }

	if (count <= 0)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;

	int ReloadCounter = weapon->ReloadCounter;
	if (!dontincrement || ReloadCounter != 0)
		ReloadCounter = (weapon->ReloadCounter+1) % count;
	else // 0 % 1 = 1?  So how do we check if the weapon was never fired?  We should only do this when we're not incrementing the counter though.
		ReloadCounter = 1;

	// If we have not made our last shot...
	if (ReloadCounter != 0)
	{
		// Go back to the refire frames, instead of continuing on to the reload frames.
		ACTION_JUMP(jump);
	}
	else
	{
		// We need to reload. However, don't reload if we're out of ammo.
		weapon->CheckAmmo(false, false);
	}

	if (!dontincrement)
		weapon->ReloadCounter = ReloadCounter;
	return 0;
}

//===========================================================================
//
// Resets the counter for the above function
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ResetReloadCounter)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player == NULL || self->player->ReadyWeapon == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;
	weapon->ReloadCounter = 0;
	return 0;
}

//===========================================================================
//
// A_ChangeFlag
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeFlag)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STRING	(flagname);
	PARAM_BOOL		(value);

	const char *dot = strchr(flagname, '.');
	FFlagDef *fd;
	const PClass *cls = self->GetClass();

	if (dot != NULL)
	{
		FString part1(flagname.GetChars(), dot - flagname);
		fd = FindFlag(cls, part1, dot + 1);
	}
	else
	{
		fd = FindFlag(cls, flagname, NULL);
	}

	if (fd != NULL)
	{
		bool kill_before, kill_after;
		INTBOOL item_before, item_after;

		kill_before = self->CountsAsKill();
		item_before = self->flags & MF_COUNTITEM;

		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags(self, cls->ActorInfo, value, fd->flagbit);
		}
		else
		{
			DWORD *flagp = (DWORD*) (((char*)self) + fd->structoffset);

			// If these 2 flags get changed we need to update the blockmap and sector links.
			bool linkchange = flagp == &self->flags && (fd->flagbit == MF_NOBLOCKMAP || fd->flagbit == MF_NOSECTOR);

			if (linkchange) self->UnlinkFromWorld();
			if (value)
			{
				*flagp |= fd->flagbit;
			}
			else
			{
				*flagp &= ~fd->flagbit;
			}
			if (linkchange) self->LinkToWorld();
		}
		kill_after = self->CountsAsKill();
		item_after = self->flags & MF_COUNTITEM;
		// Was this monster previously worth a kill but no longer is?
		// Or vice versa?
		if (kill_before != kill_after)
		{
			if (kill_after)
			{ // It counts as a kill now.
				level.total_monsters++;
			}
			else
			{ // It no longer counts as a kill.
				level.total_monsters--;
			}
		}
		// same for items
		if (item_before != item_after)
		{
			if (item_after)
			{ // It counts as an item now.
				level.total_items++;
			}
			else
			{ // It no longer counts as an item
				level.total_items--;
			}
		}
	}
	else
	{
		Printf("Unknown flag '%s' in '%s'\n", flagname, cls->TypeName.GetChars());
	}
	return 0;
}

//===========================================================================
//
// A_RemoveMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveMaster)
{
	PARAM_ACTION_PROLOGUE;
	if (self->master != NULL)
	{
		P_RemoveThing(self->master);
	}
	return 0;
}

//===========================================================================
//
// A_RemoveChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveChildren)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL_OPT(removeall) { removeall = false; }

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if ( ( mo->master == self ) && ( ( mo->health <= 0 ) || removeall) )
		{
			P_RemoveThing(mo);
		}
	}
	return 0;
}

//===========================================================================
//
// A_RemoveSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveSiblings)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL_OPT(removeall) { removeall = false; }

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if ( ( mo->master == self->master ) && ( mo != self ) && ( ( mo->health <= 0 ) || removeall) )
		{
			P_RemoveThing(mo);
		}
	}
	return 0;
}

//===========================================================================
//
// A_RaiseMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseMaster)
{
	PARAM_ACTION_PROLOGUE;
	if (self->master != NULL)
	{
		P_Thing_Raise(self->master);
	}
	return 0;
}

//===========================================================================
//
// A_RaiseChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseChildren)
{
	PARAM_ACTION_PROLOGUE;

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ((mo = it.Next()))
	{
		if ( mo->master == self )
		{
			P_Thing_Raise(mo);
		}
	}
	return 0;
}

//===========================================================================
//
// A_RaiseSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseSiblings)
{
	PARAM_ACTION_PROLOGUE;

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if ( ( mo->master == self->master ) && ( mo != self ) )
		{
			P_Thing_Raise(mo);
		}
	}
	return 0;
}

//===========================================================================
//
// A_MonsterRefire
//
// Keep firing unless target got out of sight
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_MonsterRefire)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT	(prob);
	PARAM_STATE	(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	A_FaceTarget(self);

	if (pr_monsterrefire() < prob)
		return 0;

	if (self->target == NULL
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight(self, self->target, 0) )
	{
		ACTION_JUMP(jump);
	}
	return 0;
}

//===========================================================================
//
// A_SetAngle
//
// Set actor's angle (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetAngle)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE_OPT(angle)	{ angle = 0; }
	self->angle = angle;
	return 0;
}

//===========================================================================
//
// A_SetPitch
//
// Set actor's pitch (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetPitch)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE(pitch);
	self->pitch = pitch;
	return 0;
}

//===========================================================================
//
// A_ScaleVelocity
//
// Scale actor's velocity.
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ScaleVelocity)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED(scale);

	INTBOOL was_moving = self->velx | self->vely | self->velz;

	self->velx = FixedMul(self->velx, scale);
	self->vely = FixedMul(self->vely, scale);
	self->velz = FixedMul(self->velz, scale);

	// If the actor was previously moving but now is not, and is a player,
	// update its player variables. (See A_Stop.)
	if (was_moving)
	{
		CheckStopped(self);
	}
	return 0;
}

//===========================================================================
//
// A_ChangeVelocity
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeVelocity)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT	(x)		{ x = 0; }
	PARAM_FIXED_OPT	(y)		{ y = 0; }
	PARAM_FIXED_OPT	(z)		{ z = 0; }
	PARAM_INT_OPT	(flags)	{ flags = 0; }

	INTBOOL was_moving = self->velx | self->vely | self->velz;

	fixed_t vx = x, vy = y, vz = z;
	fixed_t sina = finesine[self->angle >> ANGLETOFINESHIFT];
	fixed_t cosa = finecosine[self->angle >> ANGLETOFINESHIFT];

	if (flags & 1)	// relative axes - make x, y relative to actor's current angle
	{
		vx = DMulScale16(x, cosa, -y, sina);
		vy = DMulScale16(x, sina,  y, cosa);
	}
	if (flags & 2)	// discard old velocity - replace old velocity with new velocity
	{
		self->velx = vx;
		self->vely = vy;
		self->velz = vz;
	}
	else	// add new velocity to old velocity
	{
		self->velx += vx;
		self->vely += vy;
		self->velz += vz;
	}

	if (was_moving)
	{
		CheckStopped(self);
	}
	return 0;
}

//===========================================================================
//
// A_SetArg
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetArg)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(pos);
	PARAM_INT(value);

	// Set the value of the specified arg
	if ((size_t)pos < countof(self->args))
	{
		self->args[pos] = value;
	}
	return 0;
}

//===========================================================================
//
// A_SetSpecial
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetSpecial)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(spec);
	PARAM_INT_OPT	(arg0)	{ arg0 = 0; }
	PARAM_INT_OPT	(arg1)	{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)	{ arg2 = 0; }
	PARAM_INT_OPT	(arg3)	{ arg3 = 0; }
	PARAM_INT_OPT	(arg4)	{ arg4 = 0; }
	
	self->special = spec;
	self->args[0] = arg0;
	self->args[1] = arg1;
	self->args[2] = arg2;
	self->args[3] = arg3;
	self->args[4] = arg4;
	return 0;
}

//===========================================================================
//
// A_SetVar
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserVar)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(pos);
	PARAM_INT(value);

	if (pos < 0 || pos >= countof(self->uservar))
		return 0;
	
	// Set the value of the specified arg
	self->uservar[pos] = value;
	return 0;
}

//===========================================================================
//
// A_Turn
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Turn)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE_OPT(angle);
	self->angle += angle;
	return 0;
}

//===========================================================================
//
// A_LineEffect
//
// This allows linedef effects to be activated inside deh frames.
//
//===========================================================================

void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LineEffect)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT(special)	{ special = 0; }
	PARAM_INT_OPT(tag)		{ tag = 0; }

	line_t junk;
	maplinedef_t oldjunk;
	bool res = false;
	if (!(self->flags6 & MF6_LINEDONE))						// Unless already used up
	{
		if ((oldjunk.special = special))					// Linedef type
		{
			oldjunk.tag = tag;								// Sector tag for linedef
			P_TranslateLineDef(&junk, &oldjunk);			// Turn into native type
			res = !!LineSpecials[junk.special](NULL, self, false, junk.args[0], 
				junk.args[1], junk.args[2], junk.args[3], junk.args[4]); 
			if (res && !(junk.flags & ML_REPEAT_SPECIAL))	// If only once,
				self->flags6 |= MF6_LINEDONE;				// no more for this thing
		}
	}
	ACTION_SET_RESULT(res);
	return 0;
}
