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
	self->flags |= MF_SOLID;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetSolid)
{
	self->flags &= ~MF_SOLID;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetFloat)
{
	self->flags |= MF_FLOAT;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetFloat)
{
	self->flags &= ~(MF_FLOAT|MF_INFLOAT);
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
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound =  self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeSound, 0);
	DoAttack(self, true, false, MeleeDamage, MeleeSound, NULL, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_MissileAttack)
{
	const PClass *MissileType=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	fixed_t MissileHeight= self->GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, false, true, 0, 0, MissileType, MissileHeight);
}

DEFINE_ACTION_FUNCTION(AActor, A_ComboAttack)
{
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound =  self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeSound, 0);
	const PClass *MissileType=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	fixed_t MissileHeight= self->GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BasicAttack)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(MeleeDamage, 0);
	ACTION_PARAM_SOUND(MeleeSound, 1);
	ACTION_PARAM_CLASS(MissileType, 2);
	ACTION_PARAM_FIXED(MissileHeight, 3);

	if (MissileType == NULL) return;
	DoAttack(self, true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
}

//==========================================================================
//
// Custom sound functions. 
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySound)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_SOUND(soundid, 0);
	ACTION_PARAM_INT(channel, 1);
	ACTION_PARAM_FLOAT(volume, 2);
	ACTION_PARAM_BOOL(looping, 3);
	ACTION_PARAM_FLOAT(attenuation, 4);

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
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSound)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(slot, 0);

	S_StopSound(self, slot);
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
	ACTION_PARAM_START(1);
	ACTION_PARAM_SOUND(soundid, 0);

	S_Sound (self, CHAN_WEAPON, soundid, 1, ATTN_NORM);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySoundEx)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_SOUND(soundid, 0);
	ACTION_PARAM_NAME(channel, 1);
	ACTION_PARAM_BOOL(looping, 2);
	ACTION_PARAM_INT(attenuation_raw, 3);

	int attenuation;
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
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSoundEx)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(channel, 0);

	if (channel > NAME_Auto && channel <= NAME_SoundSlot7)
	{
		S_StopSound (self, int(channel) - NAME_Auto);
	}
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SeekerMissile)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(ang1, 0);
	ACTION_PARAM_INT(ang2, 1);

	P_SeekerMissile(self, clamp<int>(ang1, 0, 90) * ANGLE_1, clamp<int>(ang2, 0, 90) * ANGLE_1);
}

//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_BulletAttack)
{
	int i;
	int bangle;
	int slope;
		
	if (!self->target) return;

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
}


//==========================================================================
//
// Do the state jump
//
//==========================================================================
static void DoJump(AActor * self, FState * CallingState, FState *jumpto, StateCallData *statecall)
{
	if (jumpto == NULL) return;

	if (statecall != NULL)
	{
		statecall->State = jumpto;
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_weapon].state)
	{
		P_SetPsprite(self->player, ps_weapon, jumpto);
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_flash].state)
	{
		P_SetPsprite(self->player, ps_flash, jumpto);
	}
	else if (CallingState == self->state)
	{
		self->SetState (jumpto);
	}
	else
	{
		// something went very wrong. This should never happen.
		assert(false);
	}
}

// This is just to avoid having to directly reference the internally defined
// CallingState and statecall parameters in the code below.
#define ACTION_JUMP(offset) DoJump(self, CallingState, offset, statecall)

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Jump)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(count, 0);
	ACTION_PARAM_INT(maxchance, 1);

	if (count >= 2 && (maxchance >= 256 || pr_cajump() < maxchance))
	{
		int jumps = 2 + (count == 2? 0 : (pr_cajump() % (count - 1)));
		ACTION_PARAM_STATE(jumpto, jumps);
		ACTION_JUMP(jumpto);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfHealthLower)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(health, 0);
	ACTION_PARAM_STATE(jump, 1);

	if (self->health < health) ACTION_JUMP(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfCloser)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(dist, 0);
	ACTION_PARAM_STATE(jump, 1);

	AActor *target;

	if (!self->player)
	{
		target=self->target;
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (target==NULL) return;

	if (P_AproxDistance(self->x-target->x, self->y-target->y) < dist &&
		( (self->z > target->z && self->z - (target->z + target->height) < dist) || 
		  (self->z <=target->z && target->z - (self->z + self->height) < dist) 
		)
	   )
	{
		ACTION_JUMP(jump);
	}
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfInventory(AActor * owner, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(Type, 0);
	ACTION_PARAM_INT(ItemAmount, 1);
	ACTION_PARAM_STATE(JumpOffset, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (!Type || owner == NULL) return;

	AInventory * Item=owner->FindInventory(Type);

	if (Item)
	{
		if (ItemAmount>0 && Item->Amount>=ItemAmount) ACTION_JUMP(JumpOffset);
		else if (Item->Amount>=Item->MaxAmount) ACTION_JUMP(JumpOffset);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInventory)
{
	DoJumpIfInventory(self, PUSH_PARAMINFO);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetInventory)
{
	DoJumpIfInventory(self->target, PUSH_PARAMINFO);
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Explode)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_INT(distance, 1);
	ACTION_PARAM_BOOL(hurtSource, 2);
	ACTION_PARAM_BOOL(alert, 3);

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

	P_RadiusAttack (self, self->target, damage, distance, self->DamageType, hurtSource);
	if (self->z <= self->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (self);
	}
	if (alert && self->target != NULL && self->target->player != NULL)
	{
		validcount++;
		P_RecursiveSound (self->Sector, self->target, false, 0);
	}
}

//==========================================================================
//
// A_RadiusThrust
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusThrust)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(force, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_BOOL(affectSource, 2);

	if (force <= 0) force = 128;
	if (distance <= 0) distance = force;

	P_RadiusAttack (self, self->target, force, distance, self->DamageType, affectSource, false);
	if (self->z <= self->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (self);
	}
}

//==========================================================================
//
// Execute a line special / script
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CallSpecial)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_INT(special, 0);
	ACTION_PARAM_INT(arg1, 1);
	ACTION_PARAM_INT(arg2, 2);
	ACTION_PARAM_INT(arg3, 3);
	ACTION_PARAM_INT(arg4, 4);
	ACTION_PARAM_INT(arg5, 5);

	bool res = !!LineSpecials[special](NULL, self, false, arg1, arg2, arg3, arg4, arg5);

	ACTION_SET_RESULT(res);
}

//==========================================================================
//
// Checks whether this actor is a missile
// Unfortunately this was buggy in older versions of the code and many
// released DECORATE monsters rely on this bug so it can only be fixed
// with an optional flag
//
//==========================================================================
inline static bool isMissile(AActor * self, bool precise=true)
{
	return self->flags&MF_MISSILE || (precise && self->GetDefault()->flags&MF_MISSILE);
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
	ACTION_PARAM_START(6);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_FIXED(SpawnHeight, 1);
	ACTION_PARAM_INT(Spawnofs_XY, 2);
	ACTION_PARAM_ANGLE(Angle, 3);
	ACTION_PARAM_INT(flags, 4);
	ACTION_PARAM_ANGLE(pitch, 5);

	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile;

	if (self->target != NULL || aimmode==2)
	{
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = Spawnofs_XY * finecosine[ang];
			fixed_t y = Spawnofs_XY * finesine[ang];
			fixed_t z = SpawnHeight - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

			switch (aimmode)
			{
			case 0:
			default:
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->x+=x;
				self->y+=y;
				self->z+=z;
				missile = P_SpawnMissileXYZ(self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
				self->x-=x;
				self->y-=y;
				self->z-=z;
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+SpawnHeight, self, self->target, ti, false);
				break;

			case 2:
				self->x+=x;
				self->y+=y;
				missile = P_SpawnMissileAngleZSpeed(self, self->z+SpawnHeight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
				self->x-=x;
				self->y-=y;

				// It is not necessary to use the correct angle here.
				// The only important thing is that the horizontal momentum is correct.
				// Therefore use 0 as the missile's angle and simplify the calculations accordingly.
				// The actual momentum vector is set below.
				if (missile)
				{
					fixed_t vx = finecosine[pitch>>ANGLETOFINESHIFT];
					fixed_t vz = finesine[pitch>>ANGLETOFINESHIFT];

					missile->momx = FixedMul (vx, missile->Speed);
					missile->momy = 0;
					missile->momz = FixedMul (vz, missile->Speed);
				}

				break;
			}

			if (missile)
			{
				// Use the actual momentum instead of the missile's Speed property
				// so that this can handle missiles with a high vertical velocity 
				// component properly.
				FVector3 velocity (missile->momx, missile->momy, 0);

				fixed_t missilespeed = (fixed_t)velocity.Length();

				missile->angle += Angle;
				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->momx = FixedMul (missilespeed, finecosine[ang]);
				missile->momy = FixedMul (missilespeed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (isMissile(self, !!(flags & CMF_TRACKOWNER)))
                {
                	AActor * owner=self ;//->target;
                	while (isMissile(owner, !!(flags & CMF_TRACKOWNER)) && owner->target) owner=owner->target;
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
				// set the health value so that the missile works properly
				if (missile->flags4&MF4_SPECTRAL)
				{
					missile->health=-2;
				}
				P_CheckMissileSpawn(missile);
			}
		}
	}
	else if (flags & CMF_CHECKTARGETDEAD)
	{
		// Target is dead and the attack shall be aborted.
		if (self->SeeState != NULL) self->SetState(self->SeeState);
	}
}

//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomBulletAttack)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(pufftype, 4);
	ACTION_PARAM_FIXED(Range, 5);
	ACTION_PARAM_BOOL(AimFacing, 6);

	if(Range==0) Range=MISSILERANGE;

	int i;
	int bangle;
	int bslope;

	if (self->target || AimFacing)
	{
		if (!AimFacing) A_FaceTarget (self);
		bangle = self->angle;

		if (!pufftype) pufftype = PClass::FindClass(NAME_BulletPuff);

		bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i=0 ; i<NumBullets ; i++)
		{
			int angle = bangle + pr_cabullet.Random2() * (Spread_XY / 255);
			int slope = bslope + pr_cabullet.Random2() * (Spread_Z / 255);
			int damage = ((pr_cabullet()%3)+1) * DamagePerBullet;
			P_LineAttack(self, angle, Range, slope, damage, GetDefaultByType(pufftype)->DamageType, pufftype);
		}
    }
}

//==========================================================================
//
// A fully customizable melee attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomMeleeAttack)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_SOUND(MeleeSound, 1);
	ACTION_PARAM_SOUND(MissSound, 2);
	ACTION_PARAM_NAME(DamageType, 3);
	ACTION_PARAM_BOOL(bleed, 4);

	if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (MeleeSound) S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else
	{
		if (MissSound) S_Sound (self, CHAN_WEAPON, MissSound, 1, ATTN_NORM);
	}
}

//==========================================================================
//
// A fully customizable combo attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomComboAttack)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_FIXED(SpawnHeight, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_SOUND(MeleeSound, 3);
	ACTION_PARAM_NAME(DamageType, 4);
	ACTION_PARAM_BOOL(bleed, 5);

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type
		if (MeleeSound) S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z+=SpawnHeight-32*FRACUNIT;
		AActor * missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
		self->z-=SpawnHeight-32*FRACUNIT;

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

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfNoAmmo)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (!ACTION_CALL_FROM_WEAPON()) return;

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
	{
		ACTION_JUMP(jump);
	}

}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireBullets)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumberOfBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(PuffType, 4);
	ACTION_PARAM_BOOL(UseAmmo, 5);
	ACTION_PARAM_FIXED(Range, 6);

	if (!self->player) return;

	player_t * player=self->player;
	AWeapon * weapon=player->ReadyWeapon;

	int i;
	int bangle;
	int bslope;

	if (UseAmmo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}
	
	if (Range == 0) Range = PLAYERMISSILERANGE;

	static_cast<APlayerPawn *>(self)->PlayAttacking2 ();

	bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

	if ((NumberOfBullets==1 && !player->refire) || NumberOfBullets==0)
	{
		int damage = ((pr_cwbullet()%3)+1)*DamagePerBullet;
		P_LineAttack(self, bangle, Range, bslope, damage, GetDefaultByType(PuffType)->DamageType, PuffType);
	}
	else 
	{
		if (NumberOfBullets == -1) NumberOfBullets = 1;
		for (i=0 ; i<NumberOfBullets ; i++)
		{
			int angle = bangle + pr_cwbullet.Random2() * (Spread_XY / 255);
			int slope = bslope + pr_cwbullet.Random2() * (Spread_Z / 255);
			int damage = ((pr_cwbullet()%3)+1) * DamagePerBullet;
			P_LineAttack(self, angle, Range, slope, damage, GetDefaultByType(PuffType)->DamageType, PuffType);
		}
	}
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireCustomMissile)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_ANGLE(Angle, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_INT(SpawnOfs_XY, 3);
	ACTION_PARAM_FIXED(SpawnHeight, 4);
	ACTION_PARAM_BOOL(AimAtAngle, 5);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;
	AActor *linetarget;

	if (UseAmmo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	if (ti) 
	{
		angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = SpawnOfs_XY * finecosine[ang];
		fixed_t y = SpawnOfs_XY * finesine[ang];
		fixed_t z = SpawnHeight;
		fixed_t shootangle = self->angle;

		if (AimAtAngle) shootangle+=Angle;

		AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget);
		// automatic handling of seeker missiles
		if (misl)
		{
			if (linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
			if (!AimAtAngle)
			{
				// This original implementation is to aim straight ahead and then offset
				// the angle from the resulting direction. 
				FVector3 velocity(misl->momx, misl->momy, 0);
				fixed_t missilespeed = (fixed_t)velocity.Length();
				misl->angle += Angle;
				angle_t an = misl->angle >> ANGLETOFINESHIFT;
				misl->momx = FixedMul (missilespeed, finecosine[an]);
				misl->momy = FixedMul (missilespeed, finesine[an]);
			}
		}
	}
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
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_BOOL(norandom, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_CLASS(PuffType, 3);
	ACTION_PARAM_FIXED(Range, 4);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;
	AActor *	linetarget;

	if (!norandom) Damage *= (pr_cwpunch()%8+1);

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (Range == 0) Range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, Range, &linetarget);

	// only use ammo when actually hitting something!
	if (UseAmmo && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	P_LineAttack (self, angle, Range, pitch, Damage, GetDefaultByType(PuffType)->DamageType, PuffType, true);

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

		self->angle = R_PointToAngle2 (self->x,
										self->y,
										linetarget->x,
										linetarget->y);
	}
}


//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RailAttack)
{
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_COLOR(Color1, 3);
	ACTION_PARAM_COLOR(Color2, 4);
	ACTION_PARAM_BOOL(Silent, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);

	if (!self->player) return;

	AWeapon * weapon=self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (UseAmmo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	P_RailAttack (self, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, Silent, PuffType);
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
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_COLOR(Color1, 2);
	ACTION_PARAM_COLOR(Color2, 3);
	ACTION_PARAM_BOOL(Silent, 4);
	ACTION_PARAM_INT(aim, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);

	fixed_t saved_x = self->x;
	fixed_t saved_y = self->y;
	angle_t saved_angle = self->angle;

	if (aim && self->target == NULL)
	{
		return;
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
										self->target->x - self->target->momx * 3,
										self->target->y - self->target->momy * 3);

		if (aim == CRF_AIMDIRECT)
		{
			// Tricky: We must offset to the angle of the current position
			// but then change the angle again to ensure proper aim.
			self->x += Spawnofs_XY * finecosine[self->angle];
			self->y += Spawnofs_XY * finesine[self->angle];
			Spawnofs_XY = 0;
			self->angle = R_PointToAngle2 (self->x, self->y,
											self->target->x - self->target->momx * 3,
											self->target->y - self->target->momy * 3);
		}

		if (self->target->flags & MF_SHADOW)
		{
			angle_t rnd = pr_crailgun.Random2() << 21;
			self->angle += rnd;
			saved_angle = rnd;
		}
	}

	angle_t angle = (self->angle - ANG90) >> ANGLETOFINESHIFT;

	P_RailAttack (self, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, Silent, PuffType);

	self->x = saved_x;
	self->y = saved_y;
	self->angle = saved_angle;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_CLASS(mi, 0);
	ACTION_PARAM_INT(amount, 1);

	bool res=true;
	if (receiver == NULL) return;

	if (amount==0) amount=1;
	if (mi) 
	{
		AInventory *item = static_cast<AInventory *>(Spawn (mi, 0, 0, 0, NO_REPLACE));
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
			item->flags&=~MF_COUNTITEM;
			level.total_items--;
		}
		if (!item->CallTryPickup (receiver))
		{
			item->Destroy ();
			res = false;
		}
		else res = true;
	}
	else res = false;
	ACTION_SET_RESULT(res);

}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveInventory)
{
	DoGiveInventory(self, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToTarget)
{
	DoGiveInventory(self->target, PUSH_PARAMINFO);
}	

//===========================================================================
//
// A_TakeInventory
//
//===========================================================================

void DoTakeInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_INT(amount, 1);
	
	if (item == NULL || receiver == NULL) return;

	bool res = false;

	AInventory * inv = receiver->FindInventory(item);

	if (inv && !inv->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
	{
		if (inv->Amount > 0)
		{
			res = true;
		}
		if (!amount || amount>=inv->Amount) 
		{
			if (inv->ItemFlags&IF_KEEPDEPLETED) inv->Amount=0;
			else inv->Destroy();
		}
		else inv->Amount-=amount;
	}
	ACTION_SET_RESULT(res);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeInventory)
{
	DoTakeInventory(self, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromTarget)
{
	DoTakeInventory(self->target, PUSH_PARAMINFO);
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
	SIXF_ABSOLUTEMOMENTUM=8,
	SIXF_SETMASTER=16,
	SIXF_NOCHECKPOSITION=32,
	SIXF_TELEFRAG=64,
	// 128 is used by Skulltag!
	SIXF_TRANSFERAMBUSHFLAG=256,
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

		mo->angle=self->angle;
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
					mo->FriendPlayer = originator->player-players+1;

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
		else 
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
	ACTION_PARAM_START(5);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_FIXED(zheight, 2);
	ACTION_PARAM_BOOL(useammo, 3);
	ACTION_PARAM_BOOL(transfer_translation, 4);

	if (!missile) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && GetDefaultByType(missile)->flags3&MF3_ISMONSTER) return;

	if (distance==0) 
	{
		// use the minimum distance that does not result in an overlap
		distance=(self->radius+GetDefaultByType(missile)->radius)>>FRACBITS;
	}

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon so use some ammo
		AWeapon * weapon=self->player->ReadyWeapon;

		if (!weapon) return;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire)) return;
	}

	AActor * mo = Spawn( missile, 
					self->x + FixedMul(distance, finecosine[self->angle>>ANGLETOFINESHIFT]), 
					self->y + FixedMul(distance, finesine[self->angle>>ANGLETOFINESHIFT]), 
					self->z - self->floorclip + zheight, ALLOW_REPLACE);

	int flags = (transfer_translation? SIXF_TRANSFERTRANSLATION:0) + (useammo? SIXF_SETMASTER:0);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
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
	ACTION_PARAM_START(10);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(xofs, 1);
	ACTION_PARAM_FIXED(yofs, 2);
	ACTION_PARAM_FIXED(zofs, 3);
	ACTION_PARAM_FIXED(xmom, 4);
	ACTION_PARAM_FIXED(ymom, 5);
	ACTION_PARAM_FIXED(zmom, 6);
	ACTION_PARAM_ANGLE(Angle, 7);
	ACTION_PARAM_INT(flags, 8);
	ACTION_PARAM_INT(chance, 9);

	if (!missile) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if (chance > 0 && pr_spawnitemex()<chance) return;

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && GetDefaultByType(missile)->flags3&MF3_ISMONSTER) return;

	fixed_t x,y;

	if (!(flags & SIXF_ABSOLUTEANGLE))
	{
		Angle += self->angle;
	}

	angle_t ang = Angle >> ANGLETOFINESHIFT;

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

	if (!(flags & SIXF_ABSOLUTEMOMENTUM))
	{
		// Same orientation issue here!
		fixed_t newxmom = FixedMul(xmom, finecosine[ang]) + FixedMul(ymom, finesine[ang]);
		ymom = FixedMul(xmom, finesine[ang]) - FixedMul(ymom, finecosine[ang]);
		xmom = newxmom;
	}

	AActor * mo = Spawn( missile, x, y, self->z - self->floorclip + zofs, ALLOW_REPLACE);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	if (mo)
	{
		mo->momx=xmom;
		mo->momy=ymom;
		mo->momz=zmom;
		mo->angle=Angle;
		if (flags & SIXF_TRANSFERAMBUSHFLAG) mo->flags = (mo->flags&~MF_AMBUSH) | (self->flags & MF_AMBUSH);
	}
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
	ACTION_PARAM_START(5);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(zheight, 1);
	ACTION_PARAM_FIXED(xymom, 2);
	ACTION_PARAM_FIXED(zmom, 3);
	ACTION_PARAM_BOOL(useammo, 4);

	if (missile == NULL) return;

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon so use some ammo
		AWeapon * weapon=self->player->ReadyWeapon;

		if (!weapon) return;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire)) return;
	}


	AActor * bo;

	bo = Spawn(missile, self->x, self->y, 
			self->z - self->floorclip + zheight + 35*FRACUNIT + (self->player? self->player->crouchoffset : 0),
			ALLOW_REPLACE);
	if (bo)
	{
		int pitch = self->pitch;

		P_PlaySpawnSound(bo, self);
		if (xymom) bo->Speed=xymom;
		bo->angle = self->angle+(((pr_grenade()&7)-4)<<24);
		bo->momz = zmom + 2*finesine[pitch>>ANGLETOFINESHIFT];
		bo->z += 2 * finesine[pitch>>ANGLETOFINESHIFT];
		P_ThrustMobj(bo, bo->angle, bo->Speed);
		bo->momx += self->momx>>1;
		bo->momy += self->momy>>1;
		bo->target= self;
		if (bo->flags4&MF4_RANDOMIZE) 
		{
			bo->tics -= pr_grenade()&3;
			if (bo->tics<1) bo->tics=1;
		}
		P_CheckMissileSpawn (bo);
	} 
	else ACTION_SET_RESULT(false);
}


//===========================================================================
//
// A_Recoil
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Recoil)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(xymom, 0);

	angle_t angle = self->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	self->momx += FixedMul (xymom, finecosine[angle]);
	self->momy += FixedMul (xymom, finesine[angle]);
}


//===========================================================================
//
// A_SelectWeapon
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SelectWeapon)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(cls, 0);

	if (cls == NULL || self->player == NULL) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	AWeapon * weaponitem = static_cast<AWeapon*>(self->FindInventory(cls));

	if (weaponitem != NULL && weaponitem->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{
		if (self->player->ReadyWeapon != weaponitem)
		{
			self->player->PendingWeapon = weaponitem;
		}
	}
	else ACTION_SET_RESULT(false);

}


//===========================================================================
//
// A_Print
//
//===========================================================================
EXTERN_CVAR(Float, con_midtime)

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Print)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STRING(text, 0);
	ACTION_PARAM_FLOAT(time, 1);
	ACTION_PARAM_NAME(fontname, 2);

	if (self->CheckLocalView (consoleplayer) ||
		(self->target!=NULL && self->target->CheckLocalView (consoleplayer)))
	{
		float saved = con_midtime;
		FFont *font = NULL;
		
		if (fontname != NAME_None)
		{
			font = V_GetFont(fontname);
		}
		if (time > 0)
		{
			con_midtime = time;
		}
		
		C_MidPrint(font != NULL ? font : SmallFont, text);
		con_midtime = saved;
	}
}


//===========================================================================
//
// A_SetTranslucent
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTranslucent)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(alpha, 0);
	ACTION_PARAM_INT(mode, 1);

	mode = mode == 0 ? STYLE_Translucent : mode == 2 ? STYLE_Fuzzy : STYLE_Add;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha = clamp<fixed_t>(alpha, 0, FRACUNIT);
	self->RenderStyle = ERenderStyle(mode);
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
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(reduce, 0);

	if (reduce == 0) reduce = FRACUNIT/10;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;
	//if (self->alpha<=0) self->Destroy();
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
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(reduce, 0);
	
	if (reduce == 0) reduce = FRACUNIT/10;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;
	if (self->alpha<=0) self->Destroy();
}

//===========================================================================
//
// A_SpawnDebris
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnDebris)
{
	int i;
	AActor * mo;

	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(debris, 0);
	ACTION_PARAM_BOOL(transfer_translation, 1);
	ACTION_PARAM_FIXED(mult_h, 2);
	ACTION_PARAM_FIXED(mult_v, 3);

	if (debris == NULL) return;

	// only positive values make sense here
	if (mult_v<=0) mult_v=FRACUNIT;
	if (mult_h<=0) mult_h=FRACUNIT;
	
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
			mo->momz = FixedMul(mult_v, ((pr_spawndebris()&7)+5)*FRACUNIT);
			mo->momx = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
			mo->momy = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
		}
	}
}


//===========================================================================
//
// A_CheckSight
// jumps if no player can see this actor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSight)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	for (int i=0;i<MAXPLAYERS;i++) 
	{
		if (playeringame[i] && P_CheckSight(players[i].camera,self,true)) return;
	}

	ACTION_JUMP(jump);

}


//===========================================================================
//
// Inventory drop
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropInventory)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(drop, 0);

	if (drop)
	{
		AInventory * inv = self->FindInventory(drop);
		if (inv)
		{
			self->DropInventory(inv);
		}
	}
}


//===========================================================================
//
// A_SetBlend
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetBlend)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_COLOR(color, 0);
	ACTION_PARAM_FLOAT(alpha, 1);
	ACTION_PARAM_INT(tics, 2);
	ACTION_PARAM_COLOR(color2, 3);

	if (color == MAKEARGB(255,255,255,255)) color=0;
	if (color2 == MAKEARGB(255,255,255,255)) color2=0;
	if (!color2.a)
		color2 = color;

	new DFlashFader(color.r/255.0f, color.g/255.0f, color.b/255.0f, alpha,
					color2.r/255.0f, color2.g/255.0f, color2.b/255.0f, 0,
					(float)tics/TICRATE, self);
}


//===========================================================================
//
// A_JumpIf
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIf)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_BOOL(expression, 0);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (expression) ACTION_JUMP(jump);

}

//===========================================================================
//
// A_KillMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillMaster)
{
	if (self->master != NULL)
	{
		P_DamageMobj(self->master, self, self, self->master->health, NAME_None, DMG_NO_ARMOR);
	}
}

//===========================================================================
//
// A_KillChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillChildren)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			P_DamageMobj(mo, self, self, mo->health, NAME_None, DMG_NO_ARMOR);
		}
	}
}

//===========================================================================
//
// A_KillSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillSiblings)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			P_DamageMobj(mo, self, self, mo->health, NAME_None, DMG_NO_ARMOR);
		}
	}
}

//===========================================================================
//
// A_CountdownArg
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CountdownArg)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(cnt, 0);

	if (cnt<0 || cnt>=5) return;
	if (!self->args[cnt]--)
	{
		if (self->flags&MF_MISSILE)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else if (self->flags&MF_SHOOTABLE)
		{
			P_DamageMobj (self, NULL, NULL, self->health, NAME_None);
		}
		else
		{
			self->SetState(self->FindState(NAME_Death));
		}
	}

}

//============================================================================
//
// A_Burst
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Burst)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(chunk, 0);

   int i, numChunks;
   AActor * mo;

   if (chunk == NULL) return;

   self->momx = self->momy = self->momz = 0;
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
         mo->momz = FixedDiv(mo->z-self->z, self->height)<<2;
         mo->momx = pr_burst.Random2 () << (FRACBITS-7);
         mo->momy = pr_burst.Random2 () << (FRACBITS-7);
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

   self->Destroy ();
}

//===========================================================================
//
// A_CheckFloor
// [GRB] Jumps if actor is standing on floor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckFloor)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->z <= self->floorz)
	{
		ACTION_JUMP(jump);
	}

}

//===========================================================================
//
// A_CheckCeiling
// [GZ] Totally copied on A_CheckFloor, jumps if actor touches ceiling
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckCeiling)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);
	if (self->z+self->height >= self->ceilingz) // Height needs to be counted
	{
		ACTION_JUMP(jump);
	}

}

//===========================================================================
//
// A_Stop
// resets all momentum of the actor to 0
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Stop)
{
	self->momx = self->momy = self->momz = 0;
	if (self->player && self->player->mo == self && !(self->player->cheats & CF_PREDICTING))
	{
		self->player->mo->PlayIdle ();
		self->player->momx = self->player->momy = 0;
	}
	
}

//===========================================================================
//
// A_Respawn
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Respawn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(fog, 0);

	fixed_t x = self->SpawnPoint[0];
	fixed_t y = self->SpawnPoint[1];
	sector_t *sec;

	self->flags |= MF_SOLID;
	sec = P_PointInSector (x, y);
	self->SetOrigin (x, y, sec->floorplane.ZatPoint (x, y));
	self->height = self->GetDefault()->height;
	if (P_TestMobjLocation (self))
	{
		AActor *defs = self->GetDefault();
		self->health = defs->health;

		self->flags  = (defs->flags & ~MF_FRIENDLY) | (self->flags & MF_FRIENDLY);
		self->flags2 = defs->flags2;
		self->flags3 = (defs->flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (self->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
		self->flags4 = (defs->flags4 & ~MF4_NOHATEPLAYERS) | (self->flags4 & MF4_NOHATEPLAYERS);
		self->flags5 = defs->flags5;
		self->SetState (self->SpawnState);
		self->renderflags &= ~RF_INVISIBLE;

		if (fog)
		{
			Spawn<ATeleportFog> (x, y, self->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		}
		if (self->CountsAsKill()) level.total_monsters++;
	}
	else
	{
		self->flags &= ~MF_SOLID;
	}
}


//==========================================================================
//
// A_PlayerSkinCheck
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlayerSkinCheck)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->player != NULL &&
		skins[self->player->userinfo.skin].othergame)
	{
		ACTION_JUMP(jump);
	}
}

//===========================================================================
//
// A_SetGravity
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetGravity)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(val, 0);
	
	self->gravity = clamp<fixed_t> (val, 0, FRACUNIT*10); 
}


// [KS] *** Start of my modifications ***

//===========================================================================
//
// A_ClearTarget
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClearTarget)
{
	self->target = NULL;
	self->LastHeard = NULL;
	self->lastenemy = NULL;
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
	ACTION_PARAM_START(3);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_ANGLE(fov, 1);
	ACTION_PARAM_BOOL(projtarg, 2);

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

		if (!target) return; // [KS] Let's not call P_CheckSight unnecessarily in this case.

		if (!P_CheckSight (self, target, 1))
			return;

		if (fov && (fov < ANGLE_MAX))
		{
			an = R_PointToAngle2 (self->x,
								  self->y,
								  target->x,
								  target->y)
				- self->angle;

			if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
			{
				return; // [KS] Outside of FOV - return
			}

		}
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}

	if (!target) return;

	ACTION_JUMP(jump);
}

//===========================================================================
//
// A_DamageMaster (int amount)
// Damages the master of this child by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageMaster)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	if (self->master != NULL)
	{
		if (amount > 0)
		{
			P_DamageMobj(self->master, self, self, amount, DamageType, DMG_NO_ARMOR);
		}
		else if (amount < 0)
		{
			amount = -amount;
			P_GiveBody(self->master, amount);
		}
	}
}

//===========================================================================
//
// A_DamageChildren (amount)
// Damages the children of this master by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageChildren)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, DamageType, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
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
	TThinkerIterator<AActor> it;
	AActor * mo;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, DamageType, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
}


//===========================================================================
//
// Modified code pointer from Skulltag
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckForReload)
{
	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
		return;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(count, 0);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_BOOL(dontincrement, 2)

	if (count <= 0) return;

	AWeapon *weapon = self->player->ReadyWeapon;

	int ReloadCounter = weapon->ReloadCounter;
	if(!dontincrement || ReloadCounter != 0)
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
		weapon->CheckAmmo( false, false );
	}

	if(!dontincrement)
		weapon->ReloadCounter = ReloadCounter;
}

//===========================================================================
//
// Resets the counter for the above function
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ResetReloadCounter)
{
	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	weapon->ReloadCounter = 0;
}

//===========================================================================
//
// A_ChangeFlag
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeFlag)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_STRING(flagname, 0);
	ACTION_PARAM_BOOL(expression, 1);

	const char *dot = strchr (flagname, '.');
	FFlagDef *fd;
	const PClass *cls = self->GetClass();

	if (dot != NULL)
	{
		FString part1(flagname, dot-flagname);
		fd = FindFlag (cls, part1, dot+1);
	}
	else
	{
		fd = FindFlag (cls, flagname, NULL);
	}

	if (fd != NULL)
	{
		bool kill_before, kill_after;
		INTBOOL item_before, item_after;

		kill_before = self->CountsAsKill();
		item_before = self->flags & MF_COUNTITEM;

		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags(self, cls->ActorInfo, expression, fd->flagbit);
		}
		else
		{
			DWORD *flagp = (DWORD*) (((char*)self) + fd->structoffset);

			// If these 2 flags get changed we need to update the blockmap and sector links.
			bool linkchange = flagp == &self->flags && (fd->flagbit == MF_NOBLOCKMAP || fd->flagbit == MF_NOSECTOR);

			if (linkchange) self->UnlinkFromWorld();
			if (expression)
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
}

//===========================================================================
//
// A_RemoveMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveMaster)
{
   if (self->master != NULL)
   {
      P_RemoveThing(self->master);
   }
}

//===========================================================================
//
// A_RemoveChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveChildren)
{
   TThinkerIterator<AActor> it;
   AActor * mo;
   ACTION_PARAM_START(1);
   ACTION_PARAM_BOOL(removeall,0);

   while ( (mo = it.Next()) )
   {
      if ( ( mo->master == self ) && ( ( mo->health <= 0 ) || removeall) )
      {
		P_RemoveThing(mo);
      }
   }
}