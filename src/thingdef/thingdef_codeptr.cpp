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
#include "g_shared/a_specialspot.h"
#include "actorptrselect.h"
#include "m_bbox.h"
#include "r_data/r_translate.h"
#include "p_trace.h"
#include "gstrings.h"


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
static FRandom pr_teleport("A_Teleport");

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
// A_RearrangePointers
//
// Allow an actor to change its relationship to other actors by
// copying pointers freely between TARGET MASTER and TRACER.
// Can also assign null value, but does not duplicate A_ClearTarget.
//
//==========================================================================


DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RearrangePointers)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(ptr_target, 0);
	ACTION_PARAM_INT(ptr_master, 1);
	ACTION_PARAM_INT(ptr_tracer, 2);
	ACTION_PARAM_INT(flags, 3);

	// Rearrange pointers internally

	// Fetch all values before modification, so that all fields can get original values
	AActor
		*gettarget = self->target,
		*getmaster = self->master,
		*gettracer = self->tracer;

	switch (ptr_target) // pick the new target
	{
	case AAPTR_MASTER:
		self->target = getmaster;
		if (!(PTROP_UNSAFETARGET & flags)) VerifyTargetChain(self);
		break;
	case AAPTR_TRACER:
		self->target = gettracer;
		if (!(PTROP_UNSAFETARGET & flags)) VerifyTargetChain(self);
		break;
	case AAPTR_NULL:
		self->target = NULL;
		// THIS IS NOT "A_ClearTarget", so no other targeting info is removed
		break;
	}

	// presently permitting non-monsters to set master
	switch (ptr_master) // pick the new master
	{
	case AAPTR_TARGET:
		self->master = gettarget;
		if (!(PTROP_UNSAFEMASTER & flags)) VerifyMasterChain(self);
		break;
	case AAPTR_TRACER:
		self->master = gettracer;
		if (!(PTROP_UNSAFEMASTER & flags)) VerifyMasterChain(self);
		break;
	case AAPTR_NULL:
		self->master = NULL;
		break;
	}

	switch (ptr_tracer) // pick the new tracer
	{
	case AAPTR_TARGET:
		self->tracer = gettarget;
		break; // no verification deemed necessary; the engine never follows a tracer chain(?)
	case AAPTR_MASTER:
		self->tracer = getmaster;
		break; // no verification deemed necessary; the engine never follows a tracer chain(?)
	case AAPTR_NULL:
		self->tracer = NULL;
		break;
	}
}

//==========================================================================
//
// A_TransferPointer
//
// Copy one pointer (MASTER, TARGET or TRACER) from this actor (SELF),
// or from this actor's MASTER, TARGET or TRACER.
//
// You can copy any one of that actor's pointers
//
// Assign the copied pointer to any one pointer in SELF,
// MASTER, TARGET or TRACER.
//
// Any attempt to make an actor point to itself will replace the pointer
// with a null value.
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TransferPointer)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(ptr_source, 0);
	ACTION_PARAM_INT(ptr_recepient, 1);
	ACTION_PARAM_INT(ptr_sourcefield, 2);
	ACTION_PARAM_INT(ptr_recepientfield, 3);
	ACTION_PARAM_INT(flags, 4);

	AActor *source, *recepient;

	// Exchange pointers with actors to whom you have pointers (or with yourself, if you must)

	source = COPY_AAPTR(self, ptr_source);
	COPY_AAPTR_NOT_NULL(self, recepient, ptr_recepient); // pick an actor to store the provided pointer value

	// convert source from dataprovider to data
 
	source = COPY_AAPTR(source, ptr_sourcefield);

	if (source == recepient) source = NULL; // The recepient should not acquire a pointer to itself; will write NULL

	if (ptr_recepientfield == AAPTR_DEFAULT) ptr_recepientfield = ptr_sourcefield; // If default: Write to same field as data was read from

	ASSIGN_AAPTR(recepient, ptr_recepientfield, source, flags);
}

//==========================================================================
//
// A_CopyFriendliness
//
// Join forces with one of the actors you are pointing to (MASTER by default)
//
// Normal CopyFriendliness reassigns health. This function will not.
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CopyFriendliness)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(ptr_source, 0);
	
	if (self->player) return;

	AActor *source;
	COPY_AAPTR_NOT_NULL(self, source, ptr_source);
	self->CopyFriendliness(source, false, false); // No change in current target or health
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
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else if (domissile && MissileType != NULL)
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z += MissileHeight + self->GetBobOffset() - 32*FRACUNIT;
		AActor *missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, MissileType, false);
		self->z -= MissileHeight + self->GetBobOffset() - 32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
			P_CheckMissileSpawn(missile, self->radius);
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
static FRandom pr_seekermissile ("SeekerMissile");
enum
{
	SMF_LOOK = 1,
	SMF_PRECISE = 2,
	SMF_CURSPEED = 4,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SeekerMissile)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(ang1, 0);
	ACTION_PARAM_INT(ang2, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(chance, 3);
	ACTION_PARAM_INT(distance, 4);

	if ((flags & SMF_LOOK) && (self->tracer == 0) && (pr_seekermissile()<chance))
	{
		self->tracer = P_RoughMonsterSearch (self, distance, true);
	}
	if (!P_SeekerMissile(self, clamp<int>(ang1, 0, 90) * ANGLE_1, clamp<int>(ang2, 0, 90) * ANGLE_1, !!(flags & SMF_PRECISE), !!(flags & SMF_CURSPEED)))
	{
		if (flags & SMF_LOOK)
		{ // This monster is no longer seekable, so let us look for another one next time.
			self->tracer = NULL;
		}
	}
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
			NAME_Hitscan, NAME_BulletPuff);
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
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetOutsideMeleeRange)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	if (!self->CheckMeleeRange())
	{
		ACTION_JUMP(jump);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetInsideMeleeRange)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	if (self->CheckMeleeRange())
	{
		ACTION_JUMP(jump);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}
//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfCloser(AActor *target, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(dist, 0);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (target != NULL && P_AproxDistance(self->x-target->x, self->y-target->y) < dist &&
		( (self->z > target->z && self->z - (target->z + target->height) < dist) || 
		  (self->z <=target->z && target->z - (self->z + self->height) < dist) 
		)
	   )
	{
		ACTION_JUMP(jump);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfCloser)
{
	AActor *target;

	if (!self->player)
	{
		target = self->target;
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}
	DoJumpIfCloser(target, PUSH_PARAMINFO);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTracerCloser)
{
	DoJumpIfCloser(self->tracer, PUSH_PARAMINFO);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfMasterCloser)
{
	DoJumpIfCloser(self->master, PUSH_PARAMINFO);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfInventory(AActor * owner, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(Type, 0);
	ACTION_PARAM_INT(ItemAmount, 1);
	ACTION_PARAM_STATE(JumpOffset, 2);
	ACTION_PARAM_INT(setowner, 3);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (!Type) return;
	COPY_AAPTR_NOT_NULL(owner, owner, setowner); //  returns if owner ends up being NULL

	AInventory *Item = owner->FindInventory(Type);

	if (Item)
	{
		if (ItemAmount > 0)
		{
			if (Item->Amount >= ItemAmount)
				ACTION_JUMP(JumpOffset);
		}
		else if (Item->Amount >= Item->MaxAmount)
		{
			ACTION_JUMP(JumpOffset);
		}
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
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfArmorType)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_NAME(Type, 0);
	ACTION_PARAM_STATE(JumpOffset, 1);
	ACTION_PARAM_INT(amount, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	ABasicArmor * armor = (ABasicArmor *) self->FindInventory(NAME_BasicArmor);

	if (armor && armor->ArmorType == Type && armor->Amount >= amount)
		ACTION_JUMP(JumpOffset);
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

enum
{
	XF_HURTSOURCE = 1,
	XF_NOTMISSILE = 4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Explode)
{
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_INT(distance, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_BOOL(alert, 3);
	ACTION_PARAM_INT(fulldmgdistance, 4);
	ACTION_PARAM_INT(nails, 5);
	ACTION_PARAM_INT(naildamage, 6);
	ACTION_PARAM_CLASS(pufftype, 7);

	if (damage < 0)	// get parameters from metadata
	{
		damage = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionDamage, 128);
		distance = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionRadius, damage);
		flags = !self->GetClass()->Meta.GetMetaInt (ACMETA_DontHurtShooter);
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
				naildamage, NAME_Hitscan, pufftype);
		}
	}

	P_RadiusAttack (self, self->target, damage, distance, self->DamageType, flags, fulldmgdistance);
	P_CheckSplash(self, distance<<FRACBITS);
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

enum
{
	RTF_AFFECTSOURCE = 1,
	RTF_NOIMPACTDAMAGE = 2,
	RTF_NOTMISSILE = 4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusThrust)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(force, 0);
	ACTION_PARAM_INT(distance, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(fullthrustdistance, 3);

	bool sourcenothrust = false;

	if (force == 0) force = 128;
	if (distance <= 0) distance = abs(force);

	// Temporarily negate MF2_NODMGTHRUST on the shooter, since it renders this function useless.
	if (!(flags & RTF_NOTMISSILE) && self->target != NULL && self->target->flags2 & MF2_NODMGTHRUST)
	{
		sourcenothrust = true;
		self->target->flags2 &= ~MF2_NODMGTHRUST;
	}

	P_RadiusAttack (self, self->target, force, distance, self->DamageType, flags | RADF_NODAMAGE, fullthrustdistance);
	P_CheckSplash(self, distance << FRACBITS);

	if (sourcenothrust)
	{
		self->target->flags2 |= MF2_NODMGTHRUST;
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

	bool res = !!P_ExecuteSpecial(special, NULL, self, false, arg1, arg2, arg3, arg4, arg5);

	ACTION_SET_RESULT(res);
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

	CMF_ABSOLUTEPITCH = 16,
	CMF_OFFSETPITCH = 32,
	CMF_SAVEPITCH = 64,

	CMF_ABSOLUTEANGLE = 128
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
			fixed_t z = SpawnHeight + self->GetBobOffset() - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

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
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z + self->GetBobOffset() + SpawnHeight, self, self->target, ti, false);
				break;

			case 2:
				self->x += x;
				self->y += y;
				missile = P_SpawnMissileAngleZSpeed(self, self->z + self->GetBobOffset() + SpawnHeight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
 				self->x -= x;
				self->y -= y;

				flags |= CMF_ABSOLUTEPITCH;

				break;
			}

			if (missile)
			{
				// Use the actual velocity instead of the missile's Speed property
				// so that this can handle missiles with a high vertical velocity 
				// component properly.

				fixed_t missilespeed;

				if ( (CMF_ABSOLUTEPITCH|CMF_OFFSETPITCH) & flags)
				{
					if (CMF_OFFSETPITCH & flags)
					{
							FVector2 velocity (missile->velx, missile->vely);
							pitch += R_PointToAngle2(0,0, (fixed_t)velocity.Length(), missile->velz);
					}
					ang = pitch >> ANGLETOFINESHIFT;
					missilespeed = abs(FixedMul(finecosine[ang], missile->Speed));
					missile->velz = FixedMul(finesine[ang], missile->Speed);
				}
				else
				{
					FVector2 velocity (missile->velx, missile->vely);
					missilespeed = (fixed_t)velocity.Length();
				}

				if (CMF_SAVEPITCH & flags)
				{
					missile->pitch = pitch;
					// In aimmode 0 and 1 without absolutepitch or offsetpitch, the pitch parameter
					// contains the unapplied parameter. In that case, it is set as pitch without
					// otherwise affecting the spawned actor.
				}

				missile->angle = (CMF_ABSOLUTEANGLE & flags) ? Angle : missile->angle + Angle ;

				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->velx = FixedMul (missilespeed, finecosine[ang]);
				missile->vely = FixedMul (missilespeed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (self->isMissile(!!(flags & CMF_TRACKOWNER)))
                {
                	AActor * owner=self ;//->target;
                	while (owner->isMissile(!!(flags & CMF_TRACKOWNER)) && owner->target) owner=owner->target;
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
				// we must redo the spectral check here because the owner is set after spawning so the FriendPlayer value may be wrong
				if (missile->flags4 & MF4_SPECTRAL)
				{
					if (missile->target != NULL)
					{
						missile->SetFriendPlayer(missile->target->player);
					}
					else
					{
						missile->FriendPlayer = 0;
					}
				}
				P_CheckMissileSpawn(missile, self->radius);
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
enum CBA_Flags
{
	CBAF_AIMFACING = 1,
	CBAF_NORANDOM = 2,
	CBAF_EXPLICITANGLE = 4,
	CBAF_NOPITCH = 8,
	CBAF_NORANDOMPUFFZ = 16,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomBulletAttack)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(pufftype, 4);
	ACTION_PARAM_FIXED(Range, 5);
	ACTION_PARAM_INT(Flags, 6);

	if(Range==0) Range=MISSILERANGE;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (Flags & CBAF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if (self->target || (Flags & CBAF_AIMFACING))
	{
		if (!(Flags & CBAF_AIMFACING)) A_FaceTarget (self);
		bangle = self->angle;

		if (!pufftype) pufftype = PClass::FindClass(NAME_BulletPuff);

		if (!(Flags & CBAF_NOPITCH)) bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i=0 ; i<NumBullets ; i++)
		{
			int angle = bangle;
			int slope = bslope;

			if (Flags & CBAF_EXPLICITANGLE)
			{
				angle += Spread_XY;
				slope += Spread_Z;
			}
			else
			{
				angle += pr_cwbullet.Random2() * (Spread_XY / 255);
				slope += pr_cwbullet.Random2() * (Spread_Z / 255);
			}

			int damage = DamagePerBullet;

			if (!(Flags & CBAF_NORANDOM))
				damage *= ((pr_cabullet()%3)+1);

			P_LineAttack(self, angle, Range, slope, damage, NAME_Hitscan, pufftype, laflags);
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
		int newdam = P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
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
		int newdam = P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z += SpawnHeight + self->GetBobOffset() - 32*FRACUNIT;
		AActor *missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
		self->z -= SpawnHeight + self->GetBobOffset() - 32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
			P_CheckMissileSpawn(missile, self->radius);
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
enum FB_Flags
{
	FBF_USEAMMO = 1,
	FBF_NORANDOM = 2,
	FBF_EXPLICITANGLE = 4,
	FBF_NOPITCH = 8,
	FBF_NOFLASH = 16,
	FBF_NORANDOMPUFFZ = 32,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireBullets)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumberOfBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(PuffType, 4);
	ACTION_PARAM_INT(Flags, 5);
	ACTION_PARAM_FIXED(Range, 6);

	if (!self->player) return;

	player_t * player=self->player;
	AWeapon * weapon=player->ReadyWeapon;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (Flags & FBF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if ((Flags & FBF_USEAMMO) && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}
	
	if (Range == 0) Range = PLAYERMISSILERANGE;

	if (!(Flags & FBF_NOFLASH)) static_cast<APlayerPawn *>(self)->PlayAttacking2 ();

	if (!(Flags & FBF_NOPITCH)) bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	if (weapon != NULL)
	{
		S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
	}

	if ((NumberOfBullets==1 && !player->refire) || NumberOfBullets==0)
	{
		int damage = DamagePerBullet;

		if (!(Flags & FBF_NORANDOM))
			damage *= ((pr_cwbullet()%3)+1);

		P_LineAttack(self, bangle, Range, bslope, damage, NAME_Hitscan, PuffType, laflags);
	}
	else 
	{
		if (NumberOfBullets == -1) NumberOfBullets = 1;
		for (i=0 ; i<NumberOfBullets ; i++)
		{
			int angle = bangle;
			int slope = bslope;

			if (Flags & FBF_EXPLICITANGLE)
			{
				angle += Spread_XY;
				slope += Spread_Z;
			}
			else
			{
				angle += pr_cwbullet.Random2() * (Spread_XY / 255);
				slope += pr_cwbullet.Random2() * (Spread_Z / 255);
			}

			int damage = DamagePerBullet;

			if (!(Flags & FBF_NORANDOM))
				damage *= ((pr_cwbullet()%3)+1);

			P_LineAttack(self, angle, Range, slope, damage, NAME_Hitscan, PuffType, laflags);
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
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_ANGLE(Angle, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_INT(SpawnOfs_XY, 3);
	ACTION_PARAM_FIXED(SpawnHeight, 4);
	ACTION_PARAM_BOOL(AimAtAngle, 5);
	ACTION_PARAM_ANGLE(pitch, 6);

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

		// Temporarily adjusts the pitch
		fixed_t SavedPlayerPitch = self->pitch;
		self->pitch -= pitch;
		AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget);
		self->pitch = SavedPlayerPitch;
		// automatic handling of seeker missiles
		if (misl)
		{
			if (linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
			if (!AimAtAngle)
			{
				// This original implementation is to aim straight ahead and then offset
				// the angle from the resulting direction. 
				FVector3 velocity(misl->velx, misl->vely, 0);
				fixed_t missilespeed = (fixed_t)velocity.Length();
				misl->angle += Angle;
				angle_t an = misl->angle >> ANGLETOFINESHIFT;
				misl->velx = FixedMul (missilespeed, finecosine[an]);
				misl->vely = FixedMul (missilespeed, finesine[an]);
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

enum
{
	CPF_USEAMMO = 1,
	CPF_DAGGER = 2,
	CPF_PULLIN = 4,
	CPF_NORANDOMPUFFZ = 8,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomPunch)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_BOOL(norandom, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(PuffType, 3);
	ACTION_PARAM_FIXED(Range, 4);
	ACTION_PARAM_FIXED(LifeSteal, 5);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;
	AActor *	linetarget;
	int			actualdamage;

	if (!norandom) Damage *= (pr_cwpunch()%8+1);

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (Range == 0) Range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, Range, &linetarget);

	// only use ammo when actually hitting something!
	if ((flags & CPF_USEAMMO) && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);
	int puffFlags = LAF_ISMELEEATTACK | ((flags & CPF_NORANDOMPUFFZ) ? LAF_NORANDOMPUFFZ : 0);

	P_LineAttack (self, angle, Range, pitch, Damage, NAME_Melee, PuffType, puffFlags, &linetarget, &actualdamage);

	// turn to face target
	if (linetarget)
	{
		if (LifeSteal && !(linetarget->flags5 & MF5_DONTDRAIN))
			P_GiveBody (self, (actualdamage * LifeSteal) >> FRACBITS);

		if (weapon != NULL)
		{
			S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
		}

		self->angle = R_PointToAngle2 (self->x,
										self->y,
										linetarget->x,
										linetarget->y);

		if (flags & CPF_PULLIN) self->flags |= MF_JUSTATTACKED;
		if (flags & CPF_DAGGER) P_DaggerAlert (self, linetarget);
	}
}


//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RailAttack)
{
	ACTION_PARAM_START(16);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_COLOR(Color1, 3);
	ACTION_PARAM_COLOR(Color2, 4);
	ACTION_PARAM_INT(Flags, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);
	ACTION_PARAM_ANGLE(Spread_XY, 8);
	ACTION_PARAM_ANGLE(Spread_Z, 9);
	ACTION_PARAM_FIXED(Range, 10);
	ACTION_PARAM_INT(Duration, 11);
	ACTION_PARAM_FLOAT(Sparsity, 12);
	ACTION_PARAM_FLOAT(DriftSpeed, 13);
	ACTION_PARAM_CLASS(SpawnClass, 14);
	ACTION_PARAM_FIXED(Spawnofs_Z, 15);
	
	if(Range==0) Range=8192*FRACUNIT;
	if(Sparsity==0) Sparsity=1.0;

	if (!self->player) return;

	AWeapon * weapon=self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (UseAmmo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	angle_t angle;
	angle_t slope;

	if (Flags & RAF_EXPLICITANGLE)
	{
		angle = Spread_XY;
		slope = Spread_Z;
	}
	else
	{
		angle = pr_crailgun.Random2() * (Spread_XY / 255);
		slope = pr_crailgun.Random2() * (Spread_Z / 255);
	}

	P_RailAttack (self, Damage, Spawnofs_XY, Spawnofs_Z, Color1, Color2, MaxDiff, Flags, PuffType, angle, slope, Range, Duration, Sparsity, DriftSpeed, SpawnClass);
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
	CRF_AIMDIRECT = 2,
	CRF_EXPLICITANGLE = 4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomRailgun)
{
	ACTION_PARAM_START(16);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_COLOR(Color1, 2);
	ACTION_PARAM_COLOR(Color2, 3);
	ACTION_PARAM_INT(Flags, 4);
	ACTION_PARAM_INT(aim, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);
	ACTION_PARAM_ANGLE(Spread_XY, 8);
	ACTION_PARAM_ANGLE(Spread_Z, 9);
	ACTION_PARAM_FIXED(Range, 10);
	ACTION_PARAM_INT(Duration, 11);
	ACTION_PARAM_FLOAT(Sparsity, 12);
	ACTION_PARAM_FLOAT(DriftSpeed, 13);
	ACTION_PARAM_CLASS(SpawnClass, 14);
	ACTION_PARAM_FIXED(Spawnofs_Z, 15);

	if(Range==0) Range=8192*FRACUNIT;
	if(Sparsity==0) Sparsity=1.0;

	AActor *linetarget;

	fixed_t saved_x = self->x;
	fixed_t saved_y = self->y;
	angle_t saved_angle = self->angle;
	fixed_t saved_pitch = self->pitch;

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
	self->pitch = P_AimLineAttack (self, self->angle, MISSILERANGE, &linetarget, ANGLE_1*60, 0, aim ? self->target : NULL);
	if (linetarget == NULL && aim)
	{
		// We probably won't hit the target, but aim at it anyway so we don't look stupid.
		FVector2 xydiff(self->target->x - self->x, self->target->y - self->y);
		double zdiff = (self->target->z + (self->target->height>>1)) -
						(self->z + (self->height>>1) - self->floorclip);
		self->pitch = int(atan2(zdiff, xydiff.Length()) * ANGLE_180 / -M_PI);
	}
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
			self->x += Spawnofs_XY * finecosine[self->angle];
			self->y += Spawnofs_XY * finesine[self->angle];
			Spawnofs_XY = 0;
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

	angle_t angleoffset;
	angle_t slopeoffset;

	if (Flags & CRF_EXPLICITANGLE)
	{
		angleoffset = Spread_XY;
		slopeoffset = Spread_Z;
	}
	else
	{
		angleoffset = pr_crailgun.Random2() * (Spread_XY / 255);
		slopeoffset = pr_crailgun.Random2() * (Spread_Z / 255);
	}

	P_RailAttack (self, Damage, Spawnofs_XY, Spawnofs_Z, Color1, Color2, MaxDiff, Flags, PuffType, angleoffset, slopeoffset, Range, Duration, Sparsity, DriftSpeed, SpawnClass);

	self->x = saved_x;
	self->y = saved_y;
	self->angle = saved_angle;
	self->pitch = saved_pitch;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(mi, 0);
	ACTION_PARAM_INT(amount, 1);
	ACTION_PARAM_INT(setreceiver, 2);

	COPY_AAPTR_NOT_NULL(receiver, receiver, setreceiver);

	bool res=true;
	
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
		item->ClearCounters();
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

enum
{
	TIF_NOTAKEINFINITE = 1,
};

void DoTakeInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_INT(amount, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(setreceiver, 3);
	
	if (!item) return;
	COPY_AAPTR_NOT_NULL(receiver, receiver, setreceiver);

	bool res = false;

	AInventory * inv = receiver->FindInventory(item);

	if (inv && !inv->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
	{
		if (inv->Amount > 0)
		{
			res = true;
		}
		// Do not take ammo if the "no take infinite/take as ammo depletion" flag is set
		// and infinite ammo is on
		if (flags & TIF_NOTAKEINFINITE &&
			((dmflags & DF_INFINITE_AMMO) || (receiver->player->cheats & CF_INFINITEAMMO)) &&
			inv->IsKindOf(RUNTIME_CLASS(AAmmo)))
		{
			// Nothing to do here, except maybe res = false;? Would it make sense?
		}
		else if (!amount || amount>=inv->Amount) 
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
	SIXF_TRANSFERTRANSLATION	= 1 << 0,
	SIXF_ABSOLUTEPOSITION		= 1 << 1,
	SIXF_ABSOLUTEANGLE			= 1 << 2,
	SIXF_ABSOLUTEVELOCITY		= 1 << 3,
	SIXF_SETMASTER				= 1 << 4,
	SIXF_NOCHECKPOSITION		= 1 << 5,
	SIXF_TELEFRAG				= 1 << 6,
	SIXF_CLIENTSIDE				= 1 << 7,	// only used by Skulldronum
	SIXF_TRANSFERAMBUSHFLAG		= 1 << 8,
	SIXF_TRANSFERPITCH			= 1 << 9,
	SIXF_TRANSFERPOINTERS		= 1 << 10,
	SIXF_USEBLOODCOLOR			= 1 << 11,
	SIXF_CLEARCALLERTID			= 1 << 12,
	SIXF_MULTIPLYSPEED			= 1 << 13,
	SIXF_TRANSFERSCALE			= 1 << 14,
	SIXF_TRANSFERSPECIAL		= 1 << 15,
	SIXF_CLEARCALLERSPECIAL		= 1 << 16,
	SIXF_TRANSFERSTENCILCOL		= 1 << 17,
};

static bool InitSpawnedItem(AActor *self, AActor *mo, int flags)
{
	if (mo == NULL)
	{
		return false;
	}
	AActor *originator = self;

	if (!(mo->flags2 & MF2_DONTTRANSLATE))
	{
		if (flags & SIXF_TRANSFERTRANSLATION)
		{
			mo->Translation = self->Translation;
		}
		else if (flags & SIXF_USEBLOODCOLOR)
		{
			// [XA] Use the spawning actor's BloodColor to translate the newly-spawned object.
			PalEntry bloodcolor = self->GetBloodColor();
			mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}
	}
	if (flags & SIXF_TRANSFERPOINTERS)
	{
		mo->target = self->target;
		mo->master = self->master; // This will be overridden later if SIXF_SETMASTER is set
		mo->tracer = self->tracer;
	}

	mo->angle = self->angle;
	if (flags & SIXF_TRANSFERPITCH)
	{
		mo->pitch = self->pitch;
	}
	while (originator && originator->isMissile())
	{
		originator = originator->target;
	}

	if (flags & SIXF_TELEFRAG) 
	{
		P_TeleportMove(mo, mo->x, mo->y, mo->z, true);
		// This is needed to ensure consistent behavior.
		// Otherwise it will only spawn if nothing gets telefragged
		flags |= SIXF_NOCHECKPOSITION;	
	}
	if (mo->flags3 & MF3_ISMONSTER)
	{
		if (!(flags & SIXF_NOCHECKPOSITION) && !P_TestMobjLocation(mo))
		{
			// The monster is blocked so don't spawn it at all!
			mo->ClearCounters();
			mo->Destroy();
			return false;
		}
		else if (originator)
		{
			if (originator->flags3 & MF3_ISMONSTER)
			{
				// If this is a monster transfer all friendliness information
				mo->CopyFriendliness(originator, true);
				if (flags & SIXF_SETMASTER) mo->master = originator;	// don't let it attack you (optional)!
			}
			else if (originator->player)
			{
				// A player always spawns a monster friendly to him
				mo->flags |= MF_FRIENDLY;
				mo->SetFriendPlayer(originator->player);

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
		mo->target = originator ? originator : self;
	}
	if (flags & SIXF_TRANSFERSCALE)
	{
		mo->scaleX = self->scaleX;
		mo->scaleY = self->scaleY;
	}
	if (flags & SIXF_TRANSFERAMBUSHFLAG)
	{
		mo->flags = (mo->flags & ~MF_AMBUSH) | (self->flags & MF_AMBUSH);
	}
	if (flags & SIXF_CLEARCALLERTID)
	{
		self->RemoveFromHash();
		self->tid = 0;
	}
	if (flags & SIXF_TRANSFERSPECIAL)
	{
		mo->special = self->special;
		memcpy(mo->args, self->args, sizeof(self->args));
	}
	if (flags & SIXF_CLEARCALLERSPECIAL)
	{
		self->special = 0;
		memset(self->args, 0, sizeof(self->args));
	}
	if (flags & SIXF_TRANSFERSTENCILCOL)
	{
		mo->fillcolor = self->fillcolor;
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
					self->z - self->floorclip + self->GetBobOffset() + zheight, ALLOW_REPLACE);

	int flags = (transfer_translation ? SIXF_TRANSFERTRANSLATION : 0) + (useammo ? SIXF_SETMASTER : 0);
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
	ACTION_PARAM_START(11);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(xofs, 1);
	ACTION_PARAM_FIXED(yofs, 2);
	ACTION_PARAM_FIXED(zofs, 3);
	ACTION_PARAM_FIXED(xvel, 4);
	ACTION_PARAM_FIXED(yvel, 5);
	ACTION_PARAM_FIXED(zvel, 6);
	ACTION_PARAM_ANGLE(Angle, 7);
	ACTION_PARAM_INT(flags, 8);
	ACTION_PARAM_INT(chance, 9);
	ACTION_PARAM_INT(tid, 10);

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

	if (!(flags & SIXF_ABSOLUTEVELOCITY))
	{
		// Same orientation issue here!
		fixed_t newxvel = FixedMul(xvel, finecosine[ang]) + FixedMul(yvel, finesine[ang]);
		yvel = FixedMul(xvel, finesine[ang]) - FixedMul(yvel, finecosine[ang]);
		xvel = newxvel;
	}

	AActor *mo = Spawn(missile, x, y, self->z - self->floorclip + self->GetBobOffset() + zofs, ALLOW_REPLACE);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	if (res)
	{
		if (tid != 0)
		{
			assert(mo->tid == 0);
			mo->tid = tid;
			mo->AddToHash();
		}
		if (flags & SIXF_MULTIPLYSPEED)
		{
			mo->velx = FixedMul(xvel, mo->Speed);
			mo->vely = FixedMul(yvel, mo->Speed);
			mo->velz = FixedMul(zvel, mo->Speed);
		}
		else
		{
			mo->velx = xvel;
			mo->vely = yvel;
			mo->velz = zvel;
		}
		mo->angle = Angle;
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
	ACTION_PARAM_FIXED(xyvel, 2);
	ACTION_PARAM_FIXED(zvel, 3);
	ACTION_PARAM_BOOL(useammo, 4);

	if (missile == NULL) return;

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (!weapon) return;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire)) return;
	}


	AActor * bo;

	bo = Spawn(missile, self->x, self->y, 
			self->z - self->floorclip + self->GetBobOffset() + zheight + 35*FRACUNIT + (self->player? self->player->crouchoffset : 0),
			ALLOW_REPLACE);
	if (bo)
	{
		P_PlaySpawnSound(bo, self);
		if (xyvel != 0)
			bo->Speed = xyvel;
		bo->angle = self->angle + (((pr_grenade()&7) - 4) << 24);

		angle_t pitch = angle_t(-self->pitch) >> ANGLETOFINESHIFT;
		angle_t angle = bo->angle >> ANGLETOFINESHIFT;

		// There are two vectors we are concerned about here: xy and z. We rotate
		// them separately according to the shooter's pitch and then sum them to
		// get the final velocity vector to shoot with.

		fixed_t xy_xyscale = FixedMul(bo->Speed, finecosine[pitch]);
		fixed_t xy_velz = FixedMul(bo->Speed, finesine[pitch]);
		fixed_t xy_velx = FixedMul(xy_xyscale, finecosine[angle]);
		fixed_t xy_vely = FixedMul(xy_xyscale, finesine[angle]);

		pitch = angle_t(self->pitch) >> ANGLETOFINESHIFT;
		fixed_t z_xyscale = FixedMul(zvel, finesine[pitch]);
		fixed_t z_velz = FixedMul(zvel, finecosine[pitch]);
		fixed_t z_velx = FixedMul(z_xyscale, finecosine[angle]);
		fixed_t z_vely = FixedMul(z_xyscale, finesine[angle]);

		bo->velx = xy_velx + z_velx + (self->velx >> 1);
		bo->vely = xy_vely + z_vely + (self->vely >> 1);
		bo->velz = xy_velz + z_velz;

		bo->target = self;
		P_CheckMissileSpawn (bo, self->radius);
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
	ACTION_PARAM_FIXED(xyvel, 0);

	angle_t angle = self->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	self->velx += FixedMul (xyvel, finecosine[angle]);
	self->vely += FixedMul (xyvel, finesine[angle]);
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

	if (text[0] == '$') text = GStrings(text+1);
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
		
		FString formatted = strbin1(text);
		C_MidPrint(font != NULL ? font : SmallFont, formatted.GetChars());
		con_midtime = saved;
	}
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
}

//===========================================================================
//
// A_PrintBold
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PrintBold)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STRING(text, 0);
	ACTION_PARAM_FLOAT(time, 1);
	ACTION_PARAM_NAME(fontname, 2);

	float saved = con_midtime;
	FFont *font = NULL;
	
	if (text[0] == '$') text = GStrings(text+1);
	if (fontname != NAME_None)
	{
		font = V_GetFont(fontname);
	}
	if (time > 0)
	{
		con_midtime = time;
	}
	
	FString formatted = strbin1(text);
	C_MidPrintBold(font != NULL ? font : SmallFont, formatted.GetChars());
	con_midtime = saved;
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
}

//===========================================================================
//
// A_Log
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Log)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STRING(text, 0);

	if (text[0] == '$') text = GStrings(text+1);
	Printf("%s\n", text);
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
}

//=========================================================================
//
// A_LogInt
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LogInt)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(num, 0);
	Printf("%d\n", num);
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
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

	if (reduce == 0)
	{
		reduce = FRACUNIT/10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;
	// Should this clamp alpha to 1.0?
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
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(reduce, 0);
	ACTION_PARAM_BOOL(remove, 1);

	if (reduce == 0)
	{
		reduce = FRACUNIT/10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;
	if (self->alpha <= 0 && remove)
	{
		self->Destroy();
	}
}

//===========================================================================
//
// A_FadeTo
//
// fades the actor to a specified transparency by a specified amount and
// destroys it if so desired
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeTo)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_FIXED(target, 0);
	ACTION_PARAM_FIXED(amount, 1);
	ACTION_PARAM_BOOL(remove, 2);

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;

	if (self->alpha > target)
	{
		self->alpha -= amount;

		if (self->alpha < target)
		{
			self->alpha = target;
		}
	}
	else if (self->alpha < target)
	{
		self->alpha += amount;

		if (self->alpha > target)
		{
			self->alpha = target;
		}
	}
	if (self->alpha == target && remove)
	{
		self->Destroy();
	}
}

//===========================================================================
//
// A_Scale(float scalex, optional float scaley)
//
// Scales the actor's graphics. If scaley is 0, use scalex.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetScale)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(scalex, 0);
	ACTION_PARAM_FIXED(scaley, 1);

	self->scaleX = scalex;
	self->scaleY = scaley ? scaley : scalex;
}

//===========================================================================
//
// A_SetMass(int mass)
//
// Sets the actor's mass.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetMass)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(mass, 0);

	self->Mass = mass;
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
			self->y + ((pr_spawndebris()-128)<<12), 
			self->z + (pr_spawndebris()*self->height/256+self->GetBobOffset()), ALLOW_REPLACE);
		if (mo)
		{
			if (transfer_translation)
			{
				mo->Translation = self->Translation;
			}
			if (i < mo->GetClass()->ActorInfo->NumOwnedStates)
			{
				mo->SetState(mo->GetClass()->ActorInfo->OwnedStates + i);
			}
			mo->velz = FixedMul(mult_v, ((pr_spawndebris()&7)+5)*FRACUNIT);
			mo->velx = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
			mo->vely = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
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

	for (int i = 0; i < MAXPLAYERS; i++) 
	{
		if (playeringame[i])
		{
			// Always check sight from each player.
			if (P_CheckSight(players[i].mo, self, SF_IGNOREVISIBILITY))
			{
				return;
			}
			// If a player is viewing from a non-player, then check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				P_CheckSight(players[i].camera, self, SF_IGNOREVISIBILITY))
			{
				return;
			}
		}
	}

	ACTION_JUMP(jump);
}

//===========================================================================
//
// A_CheckSightOrRange
// Jumps if this actor is out of range of all players *and* out of sight.
// Useful for maps with many multi-actor special effects.
//
//===========================================================================
static bool DoCheckSightOrRange(AActor *self, AActor *camera, double range)
{
	if (camera == NULL)
	{
		return false;
	}
	// Check distance first, since it's cheaper than checking sight.
	double dx = self->x - camera->x;
	double dy = self->y - camera->y;
	double dz;
	fixed_t eyez = (camera->z + camera->height - (camera->height>>2));	// same eye height as P_CheckSight
	if (eyez > self->z + self->height)
	{
		dz = self->z + self->height - eyez;
	}
	else if (eyez < self->z)
	{
		dz = self->z - eyez;
	}
	else
	{
		dz = 0;
	}
	if ((dx*dx) + (dy*dy) + (dz*dz) <= range)
	{ // Within range
		return true;
	}

	// Now check LOS.
	if (P_CheckSight(camera, self, SF_IGNOREVISIBILITY))
	{ // Visible
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSightOrRange)
{
	ACTION_PARAM_START(2);
	double range = EvalExpressionF(ParameterIndex+0, self);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckSightOrRange(self, players[i].mo, range))
			{
				return;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckSightOrRange(self, players[i].camera, range))
			{
				return;
			}
		}
	}
	ACTION_JUMP(jump);
}

//===========================================================================
//
// A_CheckRange
// Jumps if this actor is out of range of all players.
//
//===========================================================================
static bool DoCheckRange(AActor *self, AActor *camera, double range)
{
	if (camera == NULL)
	{
		return false;
	}
	// Check distance first, since it's cheaper than checking sight.
	double dx = self->x - camera->x;
	double dy = self->y - camera->y;
	double dz;
	fixed_t eyez = (camera->z + camera->height - (camera->height>>2));	// same eye height as P_CheckSight
	if (eyez > self->z + self->height){
		dz = self->z + self->height - eyez;
	}
	else if (eyez < self->z){
		dz = self->z - eyez;
	}
	else{
		dz = 0;
	}
	if ((dx*dx) + (dy*dy) + (dz*dz) <= range){
		// Within range
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckRange)
{
	ACTION_PARAM_START(2);
	double range = EvalExpressionF(ParameterIndex+0, self);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckRange(self, players[i].mo, range))
			{
				return;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckRange(self, players[i].camera, range))
			{
				return;
			}
		}
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
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillMaster)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	if (self->master != NULL)
	{
		P_DamageMobj(self->master, self, self, self->master->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
	}
}

//===========================================================================
//
// A_KillChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillChildren)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			P_DamageMobj(mo, self, self, mo->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
		}
	}
}

//===========================================================================
//
// A_KillSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillSiblings)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ( (mo = it.Next()) )
		{
			if (mo->master == self->master && mo != self)
			{
				P_DamageMobj(mo, self, self, mo->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
			}
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
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(cnt, 0);
	ACTION_PARAM_STATE(state, 1);

	if (cnt<0 || cnt>=5) return;
	if (!self->args[cnt]--)
	{
		if (self->flags&MF_MISSILE)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else if (self->flags&MF_SHOOTABLE)
		{
			P_DamageMobj (self, NULL, NULL, self->health, NAME_None, DMG_FORCED);
		}
		else
		{
			// can't use "Death" as default parameter with current DECORATE parser.
			if (state == NULL) state = self->FindState(NAME_Death);
			self->SetState(state);
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
			self->z + (pr_burst()*self->height/255 + self->GetBobOffset()), ALLOW_REPLACE);

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
	A_Unblock(self, true);

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
// resets all velocity of the actor to 0
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Stop)
{
	self->velx = self->vely = self->velz = 0;
	if (self->player && self->player->mo == self && !(self->player->cheats & CF_PREDICTING))
	{
		self->player->mo->PlayIdle();
		self->player->velx = self->player->vely = 0;
	}
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

extern void AF_A_RestoreSpecialPosition(DECLARE_PARAMINFO);

enum RS_Flags
{
	RSF_FOG=1,
	RSF_KEEPTARGET=2,
	RSF_TELEFRAG=4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Respawn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(flags, 0);

	bool oktorespawn = false;

	self->flags |= MF_SOLID;
	self->height = self->GetDefault()->height;
	CALL_ACTION(A_RestoreSpecialPosition, self);

	if (flags & RSF_TELEFRAG)
	{
		// [KS] DIE DIE DIE DIE erm *ahem* =)
		oktorespawn = P_TeleportMove(self, self->x, self->y, self->z, true);
		if (oktorespawn)
		{ // Need to do this over again, since P_TeleportMove() will redo
		  // it with the proper point-on-side calculation.
			self->UnlinkFromWorld();
			self->LinkToWorld(true);
			sector_t *sec = self->Sector;
			self->dropoffz =
			self->floorz = sec->floorplane.ZatPoint(self->x, self->y);
			self->ceilingz = sec->ceilingplane.ZatPoint(self->x, self->y);
			P_FindFloorCeiling(self, FFCF_ONLYSPAWNPOS);
		}
	}
	else
	{
		oktorespawn = P_CheckPosition(self, self->x, self->y, true);
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
			if (self->target == self) self->target = NULL;
			if (self->lastenemy == self) self->lastenemy = NULL;
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
			Spawn<ATeleportFog> (self->x, self->y, self->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		}
		if (self->CountsAsKill())
		{
			level.total_monsters++;
		}
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
		skins[self->player->userinfo.GetSkin()].othergame)
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
// A_CheckLOF (state jump, int flags = CRF_AIM_VERT|CRF_AIM_HOR,
//    fixed range = 0, angle angle = 0, angle pitch = 0, 
//    fixed offsetheight = 32, fixed offsetwidth = 0,
//	  int ptr_target = AAPTR_DEFAULT (target) )
//
//==========================================================================

enum CLOF_flags
{
	CLOFF_NOAIM_VERT =			0x1,
	CLOFF_NOAIM_HORZ =			0x2,

	CLOFF_JUMPENEMY =			0x4,
	CLOFF_JUMPFRIEND =			0x8,
	CLOFF_JUMPOBJECT =			0x10,
	CLOFF_JUMPNONHOSTILE =		0x20,

	CLOFF_SKIPENEMY =			0x40,
	CLOFF_SKIPFRIEND =			0x80,
	CLOFF_SKIPOBJECT =			0x100,
	CLOFF_SKIPNONHOSTILE =		0x200,

	CLOFF_MUSTBESHOOTABLE =		0x400,

	CLOFF_SKIPTARGET =			0x800,
	CLOFF_ALLOWNULL =			0x1000,
	CLOFF_CHECKPARTIAL =		0x2000,

	CLOFF_MUSTBEGHOST =			0x4000,
	CLOFF_IGNOREGHOST =			0x8000,
	
	CLOFF_MUSTBESOLID =			0x10000,
	CLOFF_BEYONDTARGET =		0x20000,

	CLOFF_FROMBASE =			0x40000,
	CLOFF_MUL_HEIGHT =			0x80000,
	CLOFF_MUL_WIDTH =			0x100000,

	CLOFF_JUMP_ON_MISS =		0x200000,
	CLOFF_AIM_VERT_NOOFFSET =	0x400000,
};

struct LOFData
{
	AActor *Self;
	AActor *Target;
	int Flags;
	bool BadActor;
};

ETraceStatus CheckLOFTraceFunc(FTraceResults &trace, void *userdata)
{
	LOFData *data = (LOFData *)userdata;
	int flags = data->Flags;

	if (trace.HitType != TRACE_HitActor)
	{
		return TRACE_Stop;
	}
	if (trace.Actor == data->Target)
	{
		if (flags & CLOFF_SKIPTARGET)
		{
			if (flags & CLOFF_BEYONDTARGET)
			{
				return TRACE_Skip;
			}
			return TRACE_Abort;
		}
		return TRACE_Stop;
	}
	if (flags & CLOFF_MUSTBESHOOTABLE)
	{ // all shootability checks go here
		if (!(trace.Actor->flags & MF_SHOOTABLE))
		{
			return TRACE_Skip;
		}
		if (trace.Actor->flags2 & MF2_NONSHOOTABLE)
		{
			return TRACE_Skip;
		}
	}
	if ((flags & CLOFF_MUSTBESOLID) && !(trace.Actor->flags & MF_SOLID))
	{
		return TRACE_Skip;
	}
	if (flags & CLOFF_MUSTBEGHOST)
	{
		if (!(trace.Actor->flags3 & MF3_GHOST))
		{
			return TRACE_Skip;
		}
	}
	else if (flags & CLOFF_IGNOREGHOST)
	{
		if (trace.Actor->flags3 & MF3_GHOST)
		{
			return TRACE_Skip;
		}
	}
	if (
			((flags & CLOFF_JUMPENEMY) && data->Self->IsHostile(trace.Actor)) ||
			((flags & CLOFF_JUMPFRIEND) && data->Self->IsFriend(trace.Actor)) ||
			((flags & CLOFF_JUMPOBJECT) && !(trace.Actor->flags3 & MF3_ISMONSTER)) ||
			((flags & CLOFF_JUMPNONHOSTILE) && (trace.Actor->flags3 & MF3_ISMONSTER) && !data->Self->IsHostile(trace.Actor))
		)
	{
		return TRACE_Stop;
	}
	if (
			((flags & CLOFF_SKIPENEMY) && data->Self->IsHostile(trace.Actor)) ||
			((flags & CLOFF_SKIPFRIEND) && data->Self->IsFriend(trace.Actor)) ||
			((flags & CLOFF_SKIPOBJECT) && !(trace.Actor->flags3 & MF3_ISMONSTER)) ||
			((flags & CLOFF_SKIPNONHOSTILE) && (trace.Actor->flags3 & MF3_ISMONSTER) && !data->Self->IsHostile(trace.Actor))
		)
	{
		return TRACE_Skip;
	}
	data->BadActor = true;
	return TRACE_Abort;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckLOF)
{
	// Check line of fire

	/*
		Not accounted for / I don't know how it works: FLOORCLIP
	*/

	AActor *target;
	fixed_t
		x1, y1, z1,
		vx, vy, vz;

	ACTION_PARAM_START(9);
	
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_FIXED(range, 2);
	ACTION_PARAM_FIXED(minrange, 3);
	{
		ACTION_PARAM_ANGLE(angle, 4);
		ACTION_PARAM_ANGLE(pitch, 5);
		ACTION_PARAM_FIXED(offsetheight, 6);
		ACTION_PARAM_FIXED(offsetwidth, 7);
		ACTION_PARAM_INT(ptr_target, 8);

		ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
		
		target = COPY_AAPTR(self, ptr_target == AAPTR_DEFAULT ? AAPTR_TARGET|AAPTR_PLAYER_GETTARGET|AAPTR_NULL : ptr_target); // no player-support by default

		if (flags & CLOFF_MUL_HEIGHT)
		{
			if (self->player != NULL)
			{
				// Synced with hitscan: self->player->mo->height is strangely conscientious about getting the right actor for player
				offsetheight = FixedMul(offsetheight, FixedMul (self->player->mo->height, self->player->crouchfactor));
			}
			else
			{
				offsetheight = FixedMul(offsetheight, self->height);
			}
		}
		if (flags & CLOFF_MUL_WIDTH)
		{
			offsetwidth = FixedMul(self->radius, offsetwidth);
		}
		
		x1 = self->x;
		y1 = self->y;
		z1 = self->z + offsetheight - self->floorclip;

		if (!(flags & CLOFF_FROMBASE))
		{ // default to hitscan origin

			// Synced with hitscan: self->height is strangely NON-conscientious about getting the right actor for player
			z1 += (self->height >> 1);
			if (self->player != NULL)
			{
				z1 += FixedMul (self->player->mo->AttackZOffset, self->player->crouchfactor);
			}
			else
			{
				z1 += 8*FRACUNIT;
			}
		}

		if (target)
		{
			FVector2 xyvec(target->x - x1, target->y - y1);
			fixed_t distance = P_AproxDistance((fixed_t)xyvec.Length(), target->z - z1);

			if (range && !(flags & CLOFF_CHECKPARTIAL))
			{
				if (distance > range) return;
			}

			{
				angle_t ang;

				if (flags & CLOFF_NOAIM_HORZ)
				{
					ang = self->angle;
				}
				else ang = R_PointToAngle2 (x1, y1, target->x, target->y);
				
				angle += ang;
				
				ang >>= ANGLETOFINESHIFT;
				x1 += FixedMul(offsetwidth, finesine[ang]);
				y1 -= FixedMul(offsetwidth, finecosine[ang]);
			}

			if (flags & CLOFF_NOAIM_VERT)
			{
				pitch += self->pitch;
			}
			else if (flags & CLOFF_AIM_VERT_NOOFFSET)
			{
				pitch += R_PointToAngle2 (0,0, (fixed_t)xyvec.Length(), target->z - z1 + offsetheight + target->height / 2);
			}
			else
			{
				pitch += R_PointToAngle2 (0,0, (fixed_t)xyvec.Length(), target->z - z1 + target->height / 2);
			}
		}
		else if (flags & CLOFF_ALLOWNULL)
		{
			angle += self->angle;
			pitch += self->pitch;

			angle_t ang = self->angle >> ANGLETOFINESHIFT;
			x1 += FixedMul(offsetwidth, finesine[ang]);
			y1 -= FixedMul(offsetwidth, finecosine[ang]);
		}
		else return;

		angle >>= ANGLETOFINESHIFT;
		pitch = (0-pitch)>>ANGLETOFINESHIFT;

		vx = FixedMul (finecosine[pitch], finecosine[angle]);
		vy = FixedMul (finecosine[pitch], finesine[angle]);
		vz = -finesine[pitch];
	}

	/* Variable set:

		jump, flags, target
		x1,y1,z1 (trace point of origin)
		vx,vy,vz (trace unit vector)
		range
	*/

	sector_t *sec = P_PointInSector(x1, y1);

	if (range == 0)
	{
		range = (self->player != NULL) ? PLAYERMISSILERANGE : MISSILERANGE;
	}

	FTraceResults trace;
	LOFData lof_data;

	lof_data.Self = self;
	lof_data.Target = target;
	lof_data.Flags = flags;
	lof_data.BadActor = false;

	Trace(x1, y1, z1, sec, vx, vy, vz, range, 0xFFFFFFFF, ML_BLOCKEVERYTHING, self, trace, 0,
		CheckLOFTraceFunc, &lof_data);

	if (trace.HitType == TRACE_HitActor ||
		((flags & CLOFF_JUMP_ON_MISS) && !lof_data.BadActor && trace.HitType != TRACE_HitNone))
	{
		if (minrange > 0 && trace.Distance < minrange)
		{
			return;
		}
		ACTION_JUMP(jump);
	}
}

//==========================================================================
//
// A_JumpIfTargetInLOS (state label, optional fixed fov, optional int flags,
// optional fixed dist_max, optional fixed dist_close)
//
// Jumps if the actor can see its target, or if the player has a linetarget.
// ProjectileTarget affects how projectiles are treated. If set, it will use
// the target of the projectile for seekers, and ignore the target for
// normal projectiles. If not set, it will use the missile's owner instead
// (the default). ProjectileTarget is now flag JLOSF_PROJECTILE. dist_max
// sets the maximum distance that actor can see, 0 means forever. dist_close
// uses special behavior if certain flags are set, 0 means no checks.
//
//==========================================================================

enum JLOS_flags
{
	JLOSF_PROJECTILE=1,
	JLOSF_NOSIGHT=2,
	JLOSF_CLOSENOFOV=4,
	JLOSF_CLOSENOSIGHT=8,
	JLOSF_CLOSENOJUMP=16,
	JLOSF_DEADNOJUMP=32,
	JLOSF_CHECKMASTER=64,
	JLOSF_TARGETLOS=128,
	JLOSF_FLIPFOV=256,
	JLOSF_ALLYNOJUMP=512,
	JLOSF_COMBATANTONLY=1024,
	JLOSF_NOAUTOAIM=2048,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetInLOS)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_ANGLE(fov, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_FIXED(dist_max, 3);
	ACTION_PARAM_FIXED(dist_close, 4);

	angle_t an;
	AActor *target, *viewport;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	bool doCheckSight;

	if (!self->player)
	{
		if (flags & JLOSF_CHECKMASTER)
		{
			target = self->master;
		}
		else if (self->flags & MF_MISSILE && (flags & JLOSF_PROJECTILE))
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
		
		if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0)) return;

		doCheckSight = !(flags & JLOSF_NOSIGHT);
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_AimLineAttack(self, self->angle, MISSILERANGE, &target, (flags & JLOSF_NOAUTOAIM) ? ANGLE_1/2 : 0);
		
		if (!target) return;

		switch (flags & (JLOSF_TARGETLOS|JLOSF_FLIPFOV))
		{
		case JLOSF_TARGETLOS|JLOSF_FLIPFOV:
			// target makes sight check, player makes fov check; player has verified fov
			fov = 0;
			// fall-through
		case JLOSF_TARGETLOS:
			doCheckSight = !(flags & JLOSF_NOSIGHT); // The target is responsible for sight check and fov
			break;
		default:
			// player has verified sight and fov
			fov = 0;
			// fall-through
		case JLOSF_FLIPFOV: // Player has verified sight, but target must verify fov
			doCheckSight = false;
			break;
		}
	}

	// [FDARI] If target is not a combatant, don't jump
	if ( (flags & JLOSF_COMBATANTONLY) && (!target->player) && !(target->flags3 & MF3_ISMONSTER)) return;

	// [FDARI] If actors share team, don't jump
	if ((flags & JLOSF_ALLYNOJUMP) && self->IsFriend(target)) return;

	fixed_t distance = P_AproxDistance(target->x - self->x, target->y - self->y);
	distance = P_AproxDistance(distance, target->z - self->z);

	if (dist_max && (distance > dist_max)) return;

	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
			return;

		if (flags & JLOSF_CLOSENOFOV)
			fov = 0;

		if (flags & JLOSF_CLOSENOSIGHT)
			doCheckSight = false;
	}

	if (flags & JLOSF_TARGETLOS) { viewport = target; target = self; }
	else { viewport = self; }

	if (doCheckSight && !P_CheckSight (viewport, target, SF_IGNOREVISIBILITY))
		return;

	if (flags & JLOSF_FLIPFOV)
	{
		if (viewport == self) { viewport = target; target = self; }
		else { target = viewport; viewport = self; }
	}

	if (fov && (fov < ANGLE_MAX))
	{
		an = R_PointToAngle2 (viewport->x,
							  viewport->y,
							  target->x,
							  target->y)
			- viewport->angle;

		if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
		{
			return; // [KS] Outside of FOV - return
		}

	}

	ACTION_JUMP(jump);
}


//==========================================================================
//
// A_JumpIfInTargetLOS (state label, optional fixed fov, optional int flags
// optional fixed dist_max, optional fixed dist_close)
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetLOS)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_ANGLE(fov, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_FIXED(dist_max, 3);
	ACTION_PARAM_FIXED(dist_close, 4);

	angle_t an;
	AActor *target;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (flags & JLOSF_CHECKMASTER)
	{
		target = self->master;
	}
	else if (self->flags & MF_MISSILE && (flags & JLOSF_PROJECTILE))
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

	if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0)) return;

	fixed_t distance = P_AproxDistance(target->x - self->x, target->y - self->y);
	distance = P_AproxDistance(distance, target->z - self->z);

	if (dist_max && (distance > dist_max)) return;

	bool doCheckSight = !(flags & JLOSF_NOSIGHT);

	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
			return;

		if (flags & JLOSF_CLOSENOFOV)
			fov = 0;

		if (flags & JLOSF_CLOSENOSIGHT)
			doCheckSight = false;
	}

	if (fov && (fov < ANGLE_MAX))
	{
		an = R_PointToAngle2 (target->x,
							  target->y,
							  self->x,
							  self->y)
			- target->angle;

		if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
		{
			return; // [KS] Outside of FOV - return
		}
	}

	if (doCheckSight && !P_CheckSight (target, self, SF_IGNOREVISIBILITY))
		return;

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

	if (self->master != NULL)
	{
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
		INTBOOL secret_before, secret_after;

		kill_before = self->CountsAsKill();
		item_before = self->flags & MF_COUNTITEM;
		secret_before = self->flags5 & MF5_COUNTSECRET;

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
			ModActorFlag(self, fd, expression);
			if (linkchange) self->LinkToWorld();
		}
		kill_after = self->CountsAsKill();
		item_after = self->flags & MF_COUNTITEM;
		secret_after = self->flags5 & MF5_COUNTSECRET;
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
		// and secretd
		if (secret_before != secret_after)
		{
			if (secret_after)
			{ // It counts as an secret now.
				level.total_secrets++;
			}
			else
			{ // It no longer counts as an secret
				level.total_secrets--;
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
// A_CheckFlag
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckFlag)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STRING(flagname, 0);
	ACTION_PARAM_STATE(jumpto, 1);
	ACTION_PARAM_INT(checkpointer, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	AActor *owner;

	COPY_AAPTR_NOT_NULL(self, owner, checkpointer);
	
	if (CheckActorFlag(owner, flagname))
	{
		ACTION_JUMP(jumpto);
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
	AActor *mo;
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(removeall,0);

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self && (mo->health <= 0 || removeall))
		{
			P_RemoveThing(mo);
		}
	}
}

//===========================================================================
//
// A_RemoveSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveSiblings)
{
	TThinkerIterator<AActor> it;
	AActor *mo;
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(removeall,0);

	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self && (mo->health <= 0 || removeall))
			{
				P_RemoveThing(mo);
			}
		}
	}
}

//===========================================================================
//
// A_RaiseMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseMaster)
{
	if (self->master != NULL)
	{
		P_Thing_Raise(self->master);
	}
}

//===========================================================================
//
// A_RaiseChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseChildren)
{
	TThinkerIterator<AActor> it;
	AActor *mo;

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self)
		{
			P_Thing_Raise(mo);
		}
	}
}

//===========================================================================
//
// A_RaiseSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseSiblings)
{
	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self)
			{
				P_Thing_Raise(mo);
			}
		}
	}
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
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(prob, 0);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	A_FaceTarget (self);

	if (pr_monsterrefire() < prob)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES) )
	{
		ACTION_JUMP(jump);
	}
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
	ACTION_PARAM_START(1);
	ACTION_PARAM_ANGLE(angle, 0);
	self->SetAngle(angle);
}

//===========================================================================
//
// A_SetPitch
//
// Set actor's pitch (in degrees).
//
//===========================================================================

enum
{
	SPF_FORCECLAMP = 1,	// players always clamp
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetPitch)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_ANGLE(pitch, 0);
	ACTION_PARAM_INT(flags, 1);

	if (self->player != NULL || (flags & SPF_FORCECLAMP))
	{ // clamp the pitch we set
		int min, max;

		if (self->player != NULL)
		{
			min = self->player->MinPitch;
			max = self->player->MaxPitch;
		}
		else
		{
			min = -ANGLE_90 + (1 << ANGLETOFINESHIFT);
			max = ANGLE_90 - (1 << ANGLETOFINESHIFT);
		}
		pitch = clamp<int>(pitch, min, max);
	}
	self->SetPitch(pitch);
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
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(scale, 0);

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
}

//===========================================================================
//
// A_ChangeVelocity
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeVelocity)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_FIXED(x, 0);
	ACTION_PARAM_FIXED(y, 1);
	ACTION_PARAM_FIXED(z, 2);
	ACTION_PARAM_INT(flags, 3);

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
}

//===========================================================================
//
// A_SetArg
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetArg)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(pos, 0);
	ACTION_PARAM_INT(value, 1);	

	// Set the value of the specified arg
	if ((size_t)pos < countof(self->args))
	{
		self->args[pos] = value;
	}
}

//===========================================================================
//
// A_SetSpecial
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetSpecial)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_INT(spec, 0);
	ACTION_PARAM_INT(arg0, 1);	
	ACTION_PARAM_INT(arg1, 2);	
	ACTION_PARAM_INT(arg2, 3);	
	ACTION_PARAM_INT(arg3, 4);	
	ACTION_PARAM_INT(arg4, 5);	
	
	self->special = spec;
	self->args[0] = arg0;
	self->args[1] = arg1;
	self->args[2] = arg2;
	self->args[3] = arg3;
	self->args[4] = arg4;
}

//===========================================================================
//
// A_SetUserVar
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserVar)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_NAME(varname, 0);
	ACTION_PARAM_INT(value, 1);	

	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar ||
		var->ValueType.Type != VAL_Int)
	{
		Printf("%s is not a user variable in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	// Set the value of the specified user variable.
	*(int *)(reinterpret_cast<BYTE *>(self) + var->offset) = value;
}

//===========================================================================
//
// A_SetUserArray
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserArray)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_NAME(varname, 0);
	ACTION_PARAM_INT(pos, 1);
	ACTION_PARAM_INT(value, 2);

	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar ||
		var->ValueType.Type != VAL_Array || var->ValueType.BaseType != VAL_Int)
	{
		Printf("%s is not a user array in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	if (pos < 0 || pos >= var->ValueType.size)
	{
		Printf("%d is out of bounds in array %s in class %s\n", pos, varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	// Set the value of the specified user array at index pos.
	((int *)(reinterpret_cast<BYTE *>(self) + var->offset))[pos] = value;
}

//===========================================================================
//
// A_Teleport(optional state teleportstate, optional class targettype,
// optional class fogtype, optional int flags, optional fixed mindist,
// optional fixed maxdist)
//
// Attempts to teleport to a targettype at least mindist away and at most
// maxdist away (0 means unlimited). If successful, spawn a fogtype at old
// location and place calling actor in teleportstate. 
//
//===========================================================================
enum T_Flags
{
	TF_TELEFRAG = 1, // Allow telefrag in order to teleport.
	TF_RANDOMDECIDE = 2, // Randomly fail based on health. (A_Srcr2Decide)
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Teleport)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_STATE(TeleportState, 0);
	ACTION_PARAM_CLASS(TargetType, 1);
	ACTION_PARAM_CLASS(FogType, 2);
	ACTION_PARAM_INT(Flags, 3);
	ACTION_PARAM_FIXED(MinDist, 4);
	ACTION_PARAM_FIXED(MaxDist, 5);

	// Randomly choose not to teleport like A_Srcr2Decide.
	if (Flags & TF_RANDOMDECIDE)
	{
		static const int chance[] =
		{
			192, 120, 120, 120, 64, 64, 32, 16, 0
		};

		unsigned int chanceindex = self->health / ((self->SpawnHealth()/8 == 0) ? 1 : self->SpawnHealth()/8);

		if (chanceindex >= countof(chance))
		{
			chanceindex = countof(chance) - 1;
		}

		if (pr_teleport() >= chance[chanceindex]) return;
	}

	if (TeleportState == NULL)
	{
		// Default to Teleport.
		TeleportState = self->FindState("Teleport");
		// If still nothing, then return.
		if (!TeleportState) return;
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL) return;

	if (!TargetType) TargetType = PClass::FindClass("BossSpot");

	AActor * spot = state->GetSpotWithMinMaxDistance(TargetType, self->x, self->y, MinDist, MaxDist);
	if (spot == NULL) return;

	fixed_t prevX = self->x;
	fixed_t prevY = self->y;
	fixed_t prevZ = self->z;
	if (P_TeleportMove (self, spot->x, spot->y, spot->z, Flags & TF_TELEFRAG))
	{
		ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

		if (FogType)
		{
			Spawn(FogType, prevX, prevY, prevZ, ALLOW_REPLACE);
		}

		ACTION_JUMP(TeleportState);

		self->z = self->floorz;
		self->angle = spot->angle;
		self->velx = self->vely = self->velz = 0;
	}
}

//===========================================================================
//
// A_Turn
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Turn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_ANGLE(angle, 0);
	self->angle += angle;
}

//===========================================================================
//
// A_Quake
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Quake)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(intensity, 0);
	ACTION_PARAM_INT(duration, 1);
	ACTION_PARAM_INT(damrad, 2);
	ACTION_PARAM_INT(tremrad, 3);
	ACTION_PARAM_SOUND(sound, 4);
	P_StartQuake(self, 0, intensity, duration, damrad, tremrad, sound);
}

//===========================================================================
//
// A_Weave
//
//===========================================================================

void A_Weave(AActor *self, int xyspeed, int zspeed, fixed_t xydist, fixed_t zdist)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;
	fixed_t dist;

	weaveXY = self->WeaveIndexXY & 63;
	weaveZ = self->WeaveIndexZ & 63;
	angle = (self->angle + ANG90) >> ANGLETOFINESHIFT;

	if (xydist != 0 && xyspeed != 0)
	{
		dist = MulScale13(finesine[weaveXY << BOBTOFINESHIFT], xydist);
		newX = self->x - FixedMul (finecosine[angle], dist);
		newY = self->y - FixedMul (finesine[angle], dist);
		weaveXY = (weaveXY + xyspeed) & 63;
		dist = MulScale13(finesine[weaveXY << BOBTOFINESHIFT], xydist);
		newX += FixedMul (finecosine[angle], dist);
		newY += FixedMul (finesine[angle], dist);
		if (!(self->flags5 & MF5_NOINTERACTION))
		{
			P_TryMove (self, newX, newY, true);
		}
		else
		{
			self->UnlinkFromWorld ();
			self->flags |= MF_NOBLOCKMAP;
			self->x = newX;
			self->y = newY;
			self->LinkToWorld ();
		}
		self->WeaveIndexXY = weaveXY;
	}
	if (zdist != 0 && zspeed != 0)
	{
		self->z -= MulScale13(finesine[weaveZ << BOBTOFINESHIFT], zdist);
		weaveZ = (weaveZ + zspeed) & 63;
		self->z += MulScale13(finesine[weaveZ << BOBTOFINESHIFT], zdist);
		self->WeaveIndexZ = weaveZ;
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Weave)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(xspeed, 0);
	ACTION_PARAM_INT(yspeed, 1);
	ACTION_PARAM_FIXED(xdist, 2);
	ACTION_PARAM_FIXED(ydist, 3);
	A_Weave(self, xspeed, yspeed, xdist, ydist);
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
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(special, 0);
	ACTION_PARAM_INT(tag, 1);

	line_t junk; maplinedef_t oldjunk;
	bool res = false;
	if (!(self->flags6 & MF6_LINEDONE))						// Unless already used up
	{
		if ((oldjunk.special = special))					// Linedef type
		{
			oldjunk.tag = tag;								// Sector tag for linedef
			P_TranslateLineDef(&junk, &oldjunk);			// Turn into native type
			res = !!P_ExecuteSpecial(junk.special, NULL, self, false, junk.args[0], 
				junk.args[1], junk.args[2], junk.args[3], junk.args[4]); 
			if (res && !(junk.flags & ML_REPEAT_SPECIAL))	// If only once,
				self->flags6 |= MF6_LINEDONE;				// no more for this thing
		}
	}
	ACTION_SET_RESULT(res);
}

//==========================================================================
//
// A Wolf3D-style attack codepointer
//
//==========================================================================
enum WolfAttackFlags
{
	WAF_NORANDOM	= 1,
	WAF_USEPUFF		= 2,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_WolfAttack)
{
	ACTION_PARAM_START(9);
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_SOUND(sound, 1);
	ACTION_PARAM_FIXED(snipe, 2);
	ACTION_PARAM_INT(maxdamage, 3);
	ACTION_PARAM_INT(blocksize, 4);
	ACTION_PARAM_INT(pointblank, 5);
	ACTION_PARAM_INT(longrange, 6);
	ACTION_PARAM_FIXED(runspeed, 7);
	ACTION_PARAM_CLASS(pufftype, 8);

	if (!self->target)
		return;

	// Enemy can't see target
	if (!P_CheckSight(self, self->target))
		return;

	A_FaceTarget (self);


	// Target can dodge if it can see enemy
	angle_t angle = R_PointToAngle2(self->target->x, self->target->y, self->x, self->y) - self->target->angle;
	angle >>= 24;
	bool dodge = (P_CheckSight(self->target, self) && (angle>226 || angle<30));

	// Distance check is simplistic
	fixed_t dx = abs (self->x - self->target->x);
	fixed_t dy = abs (self->y - self->target->y);
	fixed_t dz;
	fixed_t dist = dx > dy ? dx : dy;

	// Some enemies are more precise
	dist = FixedMul(dist, snipe);

	// Convert distance into integer number of blocks
	dist >>= FRACBITS;
	dist /= blocksize;

	// Now for the speed accuracy thingie
	fixed_t speed = FixedMul(self->target->velx, self->target->velx)
				  + FixedMul(self->target->vely, self->target->vely)
				  + FixedMul(self->target->velz, self->target->velz);
	int hitchance = speed < runspeed ? 256 : 160;

	// Distance accuracy (factoring dodge)
	hitchance -= dist * (dodge ? 16 : 8);

	// While we're here, we may as well do something for this:
	if (self->target->flags & MF_SHADOW)
	{
		hitchance >>= 2;
	}

	// The attack itself
	if (pr_cabullet() < hitchance)
	{
		// Compute position for spawning blood/puff
		dx = self->target->x;
		dy = self->target->y;
		dz = self->target->z + (self->target->height>>1);
		angle = R_PointToAngle2(dx, dy, self->x, self->y);
		
		dx += FixedMul(self->target->radius, finecosine[angle>>ANGLETOFINESHIFT]);
		dy += FixedMul(self->target->radius, finesine[angle>>ANGLETOFINESHIFT]);

		int damage = flags & WAF_NORANDOM ? maxdamage : (1 + (pr_cabullet() % maxdamage));
		if (dist >= pointblank)
			damage >>= 1;
		if (dist >= longrange)
			damage >>= 1;
		FName mod = NAME_None;
		bool spawnblood = !((self->target->flags & MF_NOBLOOD) 
			|| (self->target->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)));
		if (flags & WAF_USEPUFF && pufftype)
		{
			AActor * dpuff = GetDefaultByType(pufftype->GetReplacement());
			mod = dpuff->DamageType;

			if (dpuff->flags2 & MF2_THRUGHOST && self->target->flags3 & MF3_GHOST)
				damage = 0;
			
			if ((0 && dpuff->flags3 & MF3_PUFFONACTORS) || !spawnblood)
			{
				spawnblood = false;
				P_SpawnPuff(self, pufftype, dx, dy, dz, angle, 0);
			}
		}
		else if (self->target->flags3 & MF3_GHOST)
			damage >>= 2;
		if (damage)
		{
			int newdam = P_DamageMobj(self->target, self, self, damage, mod, DMG_THRUSTLESS);
			if (spawnblood)
			{
				P_SpawnBlood(dx, dy, dz, angle, newdam > 0 ? newdam : damage, self->target);
				P_TraceBleed(newdam > 0 ? newdam : damage, self->target, R_PointToAngle2(self->x, self->y, dx, dy), 0);
			}
		}
	}

	// And finally, let's play the sound
	S_Sound (self, CHAN_WEAPON, sound, 1, ATTN_NORM);
}


//==========================================================================
//
// A_Warp
//
//==========================================================================

enum WARPF
{
	WARPF_ABSOLUTEOFFSET = 0x1,
	WARPF_ABSOLUTEANGLE = 0x2,
	WARPF_USECALLERANGLE = 0x4,

	WARPF_NOCHECKPOSITION = 0x8,

	WARPF_INTERPOLATE = 0x10,
	WARPF_WARPINTERPOLATION = 0x20,
	WARPF_COPYINTERPOLATION = 0x40,

	WARPF_STOP = 0x80,
	WARPF_TOFLOOR = 0x100,
	WARPF_TESTONLY = 0x200
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Warp)
{
	ACTION_PARAM_START(7);

	ACTION_PARAM_INT(destination_selector, 0);
	ACTION_PARAM_FIXED(xofs, 1);
	ACTION_PARAM_FIXED(yofs, 2);
	ACTION_PARAM_FIXED(zofs, 3);
	ACTION_PARAM_ANGLE(angle, 4);
	ACTION_PARAM_INT(flags, 5);
	ACTION_PARAM_STATE(success_state, 6);

	fixed_t

		oldx,
		oldy,
		oldz;

	AActor *reference = COPY_AAPTR(self, destination_selector);

	if (!reference)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if (!(flags & WARPF_ABSOLUTEANGLE))
	{
		angle += (flags & WARPF_USECALLERANGLE) ? self->angle : reference->angle;
	}

	if (!(flags & WARPF_ABSOLUTEOFFSET))
	{
		angle_t fineangle = angle>>ANGLETOFINESHIFT;
		oldx = xofs;

		// (borrowed from A_SpawnItemEx, assumed workable)
		// in relative mode negative y values mean 'left' and positive ones mean 'right'
		// This is the inverse orientation of the absolute mode!

		xofs = FixedMul(oldx, finecosine[fineangle]) + FixedMul(yofs, finesine[fineangle]);
		yofs = FixedMul(oldx, finesine[fineangle]) - FixedMul(yofs, finecosine[fineangle]);
	}

	oldx = self->x;
	oldy = self->y;
	oldz = self->z;

	if (flags & WARPF_TOFLOOR)
	{
		// set correct xy

		self->SetOrigin(
			reference->x + xofs,
			reference->y + yofs,
			reference->z);

		// now the caller's floorz should be appropriate for the assigned xy-position
		// assigning position again with
		
		if (zofs)
		{
			// extra unlink, link and environment calculation
			self->SetOrigin(
				self->x,
				self->y,
				self->floorz + zofs);
		}
		else
		{
			// if there is no offset, there should be no ill effect from moving down to the
			// already identified floor

			// A_Teleport does the same thing anyway
			self->z = self->floorz;
		}
	}
	else
	{
		self->SetOrigin(
			reference->x + xofs,
			reference->y + yofs,
			reference->z + zofs);
	}
	
	if ((flags & WARPF_NOCHECKPOSITION) || P_TestMobjLocation(self))
	{
		if (flags & WARPF_TESTONLY)
		{
			self->SetOrigin(oldx, oldy, oldz);
		}
		else
		{
			self->angle = angle;

			if (flags & WARPF_STOP)
			{
				self->velx = 0;
				self->vely = 0;
				self->velz = 0;
			}

			if (flags & WARPF_WARPINTERPOLATION)
			{
				self->PrevX += self->x - oldx;
				self->PrevY += self->y - oldy;
				self->PrevZ += self->z - oldz;
			}
			else if (flags & WARPF_COPYINTERPOLATION)
			{
				self->PrevX = self->x + reference->PrevX - reference->x;
				self->PrevY = self->y + reference->PrevY - reference->y;
				self->PrevZ = self->z + reference->PrevZ - reference->z;
			}
			else if (! (flags & WARPF_INTERPOLATE))
			{
				self->PrevX = self->x;
				self->PrevY = self->y;
				self->PrevZ = self->z;
			}
		}

		if (success_state)
		{
			ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
			// in this case, you have the statejump to help you handle all the success anyway.
			ACTION_JUMP(success_state);
			return;
		}

		ACTION_SET_RESULT(true);
	}
	else
	{
		self->SetOrigin(oldx, oldy, oldz);
		ACTION_SET_RESULT(false);
	}

}

//==========================================================================
//
// ACS_Named* stuff

//
// These are exactly like their un-named line special equivalents, except
// they take strings instead of integers to indicate which script to run.
// Some of these probably aren't very useful, but they are included for
// the sake of completeness.
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedExecuteWithResult)
{
	ACTION_PARAM_START(5);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(arg1, 1);
	ACTION_PARAM_INT(arg2, 2);
	ACTION_PARAM_INT(arg3, 3);
	ACTION_PARAM_INT(arg4, 4);

	bool res = !!P_ExecuteSpecial(ACS_ExecuteWithResult, NULL, self, false, -scriptname, arg1, arg2, arg3, arg4);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedExecute)
{
	ACTION_PARAM_START(5);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);
	ACTION_PARAM_INT(arg1, 2);
	ACTION_PARAM_INT(arg2, 3);
	ACTION_PARAM_INT(arg3, 4);

	bool res = !!P_ExecuteSpecial(ACS_Execute, NULL, self, false, -scriptname, mapnum, arg1, arg2, arg3);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedExecuteAlways)
{
	ACTION_PARAM_START(5);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);
	ACTION_PARAM_INT(arg1, 2);
	ACTION_PARAM_INT(arg2, 3);
	ACTION_PARAM_INT(arg3, 4);

	bool res = !!P_ExecuteSpecial(ACS_ExecuteAlways, NULL, self, false, -scriptname, mapnum, arg1, arg2, arg3);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedLockedExecute)
{
	ACTION_PARAM_START(5);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);
	ACTION_PARAM_INT(arg1, 2);
	ACTION_PARAM_INT(arg2, 3);
	ACTION_PARAM_INT(lock, 4);

	bool res = !!P_ExecuteSpecial(ACS_LockedExecute, NULL, self, false, -scriptname, mapnum, arg1, arg2, lock);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedLockedExecuteDoor)
{
	ACTION_PARAM_START(5);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);
	ACTION_PARAM_INT(arg1, 2);
	ACTION_PARAM_INT(arg2, 3);
	ACTION_PARAM_INT(lock, 4);

	bool res = !!P_ExecuteSpecial(ACS_LockedExecuteDoor, NULL, self, false, -scriptname, mapnum, arg1, arg2, lock);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedSuspend)
{
	ACTION_PARAM_START(2);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);

	bool res = !!P_ExecuteSpecial(ACS_Suspend, NULL, self, false, -scriptname, mapnum, 0, 0, 0);

	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedTerminate)
{
	ACTION_PARAM_START(2);

	ACTION_PARAM_NAME(scriptname, 0);
	ACTION_PARAM_INT(mapnum, 1);

	bool res = !!P_ExecuteSpecial(ACS_Terminate, NULL, self, false, -scriptname, mapnum, 0, 0, 0);

	ACTION_SET_RESULT(res);
}


//==========================================================================
//
// A_RadiusGive
//
// Uses code roughly similar to A_Explode (but without all the compatibility
// baggage and damage computation code to give an item to all eligible mobjs
// in range.
//
//==========================================================================
enum RadiusGiveFlags
{
	RGF_GIVESELF	=   1,
	RGF_PLAYERS		=   2,
	RGF_MONSTERS	=   4,
	RGF_OBJECTS		=   8,
	RGF_VOODOO		=  16,
	RGF_CORPSES		=  32,
	RGF_MASK		=  63,
	RGF_NOTARGET	=  64,
	RGF_NOTRACER	= 128,
	RGF_NOMASTER	= 256,
	RGF_CUBE		= 512,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusGive)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(amount, 3);

	// We need a valid item, valid targets, and a valid range
	if (item == NULL || (flags & RGF_MASK) == 0 || distance <= 0)
	{
		return;
	}
	if (amount == 0)
	{
		amount = 1;
	}
	FBlockThingsIterator it(FBoundingBox(self->x, self->y, distance));
	double distsquared = double(distance) * double(distance);

	AActor *thing;
	while ((thing = it.Next()))
	{
		// Don't give to inventory items
		if (thing->flags & MF_SPECIAL)
		{
			continue;
		}
		// Avoid giving to self unless requested
		if (thing == self && !(flags & RGF_GIVESELF))
		{
			continue;
		}
		// Avoiding special pointers if requested
		if (((thing == self->target) && (flags & RGF_NOTARGET)) ||
			((thing == self->tracer) && (flags & RGF_NOTRACER)) ||
			((thing == self->master) && (flags & RGF_NOMASTER)))
		{
			continue;
		}
		// Don't give to dead thing unless requested
		if (thing->flags & MF_CORPSE)
		{
			if (!(flags & RGF_CORPSES))
			{
				continue;
			}
		}
		else if (thing->health <= 0 || thing->flags6 & MF6_KILLED)
		{
			continue;
		}
		// Players, monsters, and other shootable objects
		if (thing->player)
		{
			if ((thing->player->mo == thing) && !(flags & RGF_PLAYERS))
			{
				continue;
			}
			if ((thing->player->mo != thing) && !(flags & RGF_VOODOO))
			{
				continue;
			}
		}
		else if (thing->flags3 & MF3_ISMONSTER)
		{
			if (!(flags & RGF_MONSTERS))
			{
				continue;
			}
		}
		else if (thing->flags & MF_SHOOTABLE || thing->flags6 & MF6_VULNERABLE)
		{
			if (!(flags & RGF_OBJECTS))
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		if (flags & RGF_CUBE)
		{ // check if inside a cube
			if (abs(thing->x - self->x) > distance ||
				abs(thing->y - self->y) > distance ||
				abs((thing->z + thing->height/2) - (self->z + self->height/2)) > distance)
			{
				continue;
			}
		}
		else
		{ // check if inside a sphere
			TVector3<double> tpos(thing->x, thing->y, thing->z + thing->height/2);
			TVector3<double> spos(self->x, self->y, self->z + self->height/2);
			if ((tpos - spos).LengthSquared() > distsquared)
			{
				continue;
			}
		}
		fixed_t dz = abs ((thing->z + thing->height/2) - (self->z + self->height/2));

		if (P_CheckSight (thing, self, SF_IGNOREVISIBILITY|SF_IGNOREWATERBOUNDARY))
		{ // OK to give; target is in direct path
			AInventory *gift = static_cast<AInventory *>(Spawn (item, 0, 0, 0, NO_REPLACE));
			if (gift->IsKindOf(RUNTIME_CLASS(AHealth)))
			{
				gift->Amount *= amount;
			}
			else
			{
				gift->Amount = amount;
			}
			gift->flags |= MF_DROPPED;
			gift->ClearCounters();
			if (!gift->CallTryPickup (thing))
			{
				gift->Destroy ();
			}
		}
	}
}


//==========================================================================
//
// A_SetTics
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTics)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(tics_to_set, 0);

	if (stateowner != self && self->player != NULL && stateowner->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{ // Is this a weapon? Need to check psp states for a match, then. Blah.
		for (int i = 0; i < NUMPSPRITES; ++i)
		{
			if (self->player->psprites[i].state == CallingState)
			{
				self->player->psprites[i].tics = tics_to_set;
				return;
			}
		}
	}
	// Just set tics for self.
	self->tics = tics_to_set;
}

//==========================================================================
//
// A_SetDamageType
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetDamageType)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	self->DamageType = damagetype;
}

//==========================================================================
//
// A_DropItem
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropItem)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(spawntype, 0);
	ACTION_PARAM_INT(amount, 1);
	ACTION_PARAM_INT(chance, 2);

	P_DropItem(self, spawntype, amount, chance);
}
