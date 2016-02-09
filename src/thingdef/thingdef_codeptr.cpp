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
#include "p_effect.h"
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
#include "p_setup.h"
#include "gstrings.h"

AActor *SingleActorFromTID (int tid, AActor *defactor);

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
	ACTION_PARAM_INT(ptr_recipient, 1);
	ACTION_PARAM_INT(ptr_sourcefield, 2);
	ACTION_PARAM_INT(ptr_recipientfield, 3);
	ACTION_PARAM_INT(flags, 4);

	AActor *source, *recipient;

	// Exchange pointers with actors to whom you have pointers (or with yourself, if you must)

	source = COPY_AAPTR(self, ptr_source);
	COPY_AAPTR_NOT_NULL(self, recipient, ptr_recipient); // pick an actor to store the provided pointer value

	// convert source from dataprovider to data
 
	source = COPY_AAPTR(source, ptr_sourcefield);

	if (source == recipient) source = NULL; // The recipient should not acquire a pointer to itself; will write NULL

	if (ptr_recipientfield == AAPTR_DEFAULT) ptr_recipientfield = ptr_sourcefield; // If default: Write to same field as data was read from

	ASSIGN_AAPTR(recipient, ptr_recipientfield, source, flags);
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
		self->AddZ(MissileHeight + self->GetBobOffset() - 32*FRACUNIT);
		AActor *missile = P_SpawnMissileXYZ (self->PosPlusZ(32*FRACUNIT), self, self->target, MissileType, false);
		self->AddZ(-(MissileHeight + self->GetBobOffset() - 32*FRACUNIT));

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
	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(health, 0);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_INT(ptr_selector, 2);

	AActor *measured;

	measured = COPY_AAPTR(self, ptr_selector);
	
	if (measured && measured->health < health)
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
	ACTION_PARAM_START(3);
	ACTION_PARAM_FIXED(dist, 0);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_BOOL(noz, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (!target)
		return;
	if (self->AproxDistance(target) < dist &&
		(noz || 
		((self->Z() > target->Z() && self->Z() - target->Top() < dist) ||
		(self->Z() <= target->Z() && target->Z() - self->Top() < dist))))
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
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_FIXED(spawnheight, 1);
	ACTION_PARAM_INT(spawnofs_xy, 2);
	ACTION_PARAM_ANGLE(angle, 3);
	ACTION_PARAM_INT(flags, 4);
	ACTION_PARAM_ANGLE(pitch, 5);
	ACTION_PARAM_INT(ptr, 6);

	AActor *ref = COPY_AAPTR(self, ptr);

	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile;

	if (ref != NULL || aimmode == 2)
	{
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = spawnofs_xy * finecosine[ang];
			fixed_t y = spawnofs_xy * finesine[ang];
			fixed_t z = spawnheight + self->GetBobOffset() - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

			fixedvec3 pos = self->Pos();
			switch (aimmode)
			{
			case 0:
			default:
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->SetXYZ(self->Vec3Offset(x, y, z));
				missile = P_SpawnMissileXYZ(self->PosPlusZ(32*FRACUNIT), self, ref, ti, false);
				self->SetXYZ(pos);
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->Vec3Offset(x, y, self->GetBobOffset() + spawnheight), self, ref, ti, false);
				break;

			case 2:
				self->SetXYZ(self->Vec3Offset(x, y, 0));
				missile = P_SpawnMissileAngleZSpeed(self, self->Z() + self->GetBobOffset() + spawnheight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
				self->SetXYZ(pos);

				flags |= CMF_ABSOLUTEPITCH;

				break;
			}

			if (missile != NULL)
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

				missile->angle = (CMF_ABSOLUTEANGLE & flags) ? angle : missile->angle + angle ;

				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->velx = FixedMul(missilespeed, finecosine[ang]);
				missile->vely = FixedMul(missilespeed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (self->isMissile(!!(flags & CMF_TRACKOWNER)))
                {
                	AActor *owner = self ;//->target;
                	while (owner->isMissile(!!(flags & CMF_TRACKOWNER)) && owner->target)
						owner = owner->target;
                	targ = owner;
                	missile->target = owner;
					// automatic handling of seeker missiles
					if (self->flags2 & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer = self->tracer;
					}
                }
				else if (missile->flags2 & MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer = self->target;
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
		if (self->SeeState != NULL && (self->health > 0 || !(self->flags3 & MF3_ISMONSTER)))
			self->SetState(self->SeeState);
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
	ACTION_PARAM_START(8);
	ACTION_PARAM_ANGLE(spread_xy, 0);
	ACTION_PARAM_ANGLE(spread_z, 1);
	ACTION_PARAM_INT(numbullets, 2);
	ACTION_PARAM_INT(damageperbullet, 3);
	ACTION_PARAM_CLASS(pufftype, 4);
	ACTION_PARAM_FIXED(range, 5);
	ACTION_PARAM_INT(flags, 6);
	ACTION_PARAM_INT(ptr, 7);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (range == 0)
		range = MISSILERANGE;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (flags & CBAF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if (ref != NULL || (flags & CBAF_AIMFACING))
	{
		if (!(flags & CBAF_AIMFACING))
		{
			A_Face(self, ref);
		}
		bangle = self->angle;

		if (!pufftype) pufftype = PClass::FindClass(NAME_BulletPuff);

		if (!(flags & CBAF_NOPITCH)) bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i = 0; i < numbullets; i++)
		{
			int angle = bangle;
			int slope = bslope;

			if (flags & CBAF_EXPLICITANGLE)
			{
				angle += spread_xy;
				slope += spread_z;
			}
			else
			{
				angle += pr_cwbullet.Random2() * (spread_xy / 255);
				slope += pr_cwbullet.Random2() * (spread_z / 255);
			}

			int damage = damageperbullet;

			if (!(flags & CBAF_NORANDOM))
				damage *= ((pr_cabullet()%3)+1);

			P_LineAttack(self, angle, range, slope, damage, NAME_Hitscan, pufftype, laflags);
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
	ACTION_PARAM_SOUND(meleesound, 1);
	ACTION_PARAM_SOUND(misssound, 2);
	ACTION_PARAM_NAME(damagetype, 3);
	ACTION_PARAM_BOOL(bleed, 4);

	if (damagetype == NAME_None)
		damagetype = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else
	{
		if (misssound)
			S_Sound (self, CHAN_WEAPON, misssound, 1, ATTN_NORM);
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
	ACTION_PARAM_FIXED(spawnheight, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_SOUND(meleesound, 3);
	ACTION_PARAM_NAME(damagetype, 4);
	ACTION_PARAM_BOOL(bleed, 5);

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange())
	{
		if (damagetype == NAME_None)
			damagetype = NAME_Melee;	// Melee is the default type
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->AddZ(spawnheight + self->GetBobOffset() - 32*FRACUNIT);
		AActor *missile = P_SpawnMissileXYZ (self->PosPlusZ(32*FRACUNIT), self, self->target, ti, false);
		self->AddZ(-(spawnheight + self->GetBobOffset() - 32*FRACUNIT));

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2 & MF2_SEEKERMISSILE)
			{
				missile->tracer = self->target;
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
	ACTION_PARAM_ANGLE(spread_xy, 0);
	ACTION_PARAM_ANGLE(spread_z, 1);
	ACTION_PARAM_INT(numbullets, 2);
	ACTION_PARAM_INT(damageperbullet, 3);
	ACTION_PARAM_CLASS(pufftype, 4);
	ACTION_PARAM_INT(flags, 5);
	ACTION_PARAM_FIXED(range, 6);

	if (!self->player) return;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (flags & FBF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if ((flags & FBF_USEAMMO) && weapon && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return;	// out of ammo
	}
	
	if (range == 0)
		range = PLAYERMISSILERANGE;

	if (!(flags & FBF_NOFLASH)) static_cast<APlayerPawn *>(self)->PlayAttacking2 ();

	if (!(flags & FBF_NOPITCH)) bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (pufftype == NULL)
		pufftype = PClass::FindClass(NAME_BulletPuff);

	if (weapon != NULL)
	{
		S_Sound(self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
	}

	if ((numbullets == 1 && !player->refire) || numbullets == 0)
	{
		int damage = damageperbullet;

		if (!(flags & FBF_NORANDOM))
			damage *= ((pr_cwbullet()%3)+1);

		P_LineAttack(self, bangle, range, bslope, damage, NAME_Hitscan, pufftype, laflags);
	}
	else 
	{
		if (numbullets < 0)
			numbullets = 1;
		for (i = 0; i < numbullets; i++)
		{
			int angle = bangle;
			int slope = bslope;

			if (flags & FBF_EXPLICITANGLE)
			{
				angle += spread_xy;
				slope += spread_z;
			}
			else
			{
				angle += pr_cwbullet.Random2() * (spread_xy / 255);
				slope += pr_cwbullet.Random2() * (spread_z / 255);
			}

			int damage = damageperbullet;

			if (!(flags & FBF_NORANDOM))
				damage *= ((pr_cwbullet()%3)+1);

			P_LineAttack(self, angle, range, slope, damage, NAME_Hitscan, pufftype, laflags);
		}
	}
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
enum FP_Flags
{
	FPF_AIMATANGLE = 1,
	FPF_TRANSFERTRANSLATION = 2,
	FPF_NOAUTOAIM = 4,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireCustomMissile)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_ANGLE(angle, 1);
	ACTION_PARAM_BOOL(useammo, 2);
	ACTION_PARAM_INT(spawnofs_xy, 3);
	ACTION_PARAM_FIXED(spawnheight, 4);
	ACTION_PARAM_INT(flags, 5);
	ACTION_PARAM_ANGLE(pitch, 6);

	if (!self->player) return;


	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;
	AActor *linetarget;

		// Only use ammo if called from a weapon
	if (useammo && ACTION_CALL_FROM_WEAPON() && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return;	// out of ammo
	}

	if (ti) 
	{
		angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = spawnofs_xy * finecosine[ang];
		fixed_t y = spawnofs_xy * finesine[ang];
		fixed_t z = spawnheight;
		fixed_t shootangle = self->angle;

		if (flags & FPF_AIMATANGLE) shootangle += angle;

		// Temporarily adjusts the pitch
		fixed_t saved_player_pitch = self->pitch;
		self->pitch -= pitch;
		AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget, NULL, false, (flags & FPF_NOAUTOAIM) != 0);
		self->pitch = saved_player_pitch;

		// automatic handling of seeker missiles
		if (misl)
		{
			if (flags & FPF_TRANSFERTRANSLATION)
				misl->Translation = self->Translation;
			if (linetarget && (misl->flags2 & MF2_SEEKERMISSILE))
				misl->tracer = linetarget;
			if (!(flags & FPF_AIMATANGLE))
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
	CPF_NOTURN = 16,
	CPF_STEALARMOR = 32,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomPunch)
{
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_BOOL(norandom, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(pufftype, 3);
	ACTION_PARAM_FIXED(range, 4);
	ACTION_PARAM_FIXED(lifesteal, 5);
	ACTION_PARAM_INT(lifestealmax, 6);
	ACTION_PARAM_CLASS(armorbonustype, 7);
	ACTION_PARAM_SOUND(MeleeSound, 8);
	ACTION_PARAM_SOUND(MissSound, 9);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;
	AActor *	linetarget;
	int			actualdamage;

	if (!norandom)
		damage *= pr_cwpunch() % 8 + 1;

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (range == 0)
		range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, range, &linetarget);

	// only use ammo when actually hitting something!
	if ((flags & CPF_USEAMMO) && linetarget && weapon && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return;	// out of ammo
	}

	if (pufftype == NULL)
		pufftype = PClass::FindClass(NAME_BulletPuff);
	int puffFlags = LAF_ISMELEEATTACK | ((flags & CPF_NORANDOMPUFFZ) ? LAF_NORANDOMPUFFZ : 0);

	P_LineAttack (self, angle, range, pitch, damage, NAME_Melee, pufftype, puffFlags, &linetarget, &actualdamage);

	if (!linetarget)
	{
		if (MissSound) S_Sound(self, CHAN_WEAPON, MissSound, 1, ATTN_NORM);
	}
	else
	{
		if (lifesteal && !(linetarget->flags5 & MF5_DONTDRAIN))
		{
			if (flags & CPF_STEALARMOR)
			{
				if (armorbonustype == NULL)
				{
					armorbonustype = PClass::FindClass("ArmorBonus");
				}
				if (armorbonustype != NULL)
				{
					assert(armorbonustype->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)));
					ABasicArmorBonus *armorbonus = static_cast<ABasicArmorBonus *>(Spawn(armorbonustype, 0,0,0, NO_REPLACE));
					armorbonus->SaveAmount *= (actualdamage * lifesteal) >> FRACBITS;
					armorbonus->MaxSaveAmount = lifestealmax <= 0 ? armorbonus->MaxSaveAmount : lifestealmax;
					armorbonus->flags |= MF_DROPPED;
					armorbonus->ClearCounters();

					if (!armorbonus->CallTryPickup(self))
					{
						armorbonus->Destroy ();
					}
				}
			}
			else
			{
				P_GiveBody (self, (actualdamage * lifesteal) >> FRACBITS, lifestealmax);
			}
		}
		if (weapon != NULL)
		{
			if (MeleeSound) S_Sound(self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
			else			S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
		}

		if (!(flags & CPF_NOTURN))
		{
			// turn to face target
			self->angle = self->AngleTo(linetarget);
		}

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
	ACTION_PARAM_START(17);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_INT(spawnofs_xy, 1);
	ACTION_PARAM_BOOL(useammo, 2);
	ACTION_PARAM_COLOR(color1, 3);
	ACTION_PARAM_COLOR(color2, 4);
	ACTION_PARAM_INT(flags, 5);
	ACTION_PARAM_DOUBLE(maxdiff, 6);
	ACTION_PARAM_CLASS(pufftype, 7);
	ACTION_PARAM_ANGLE(spread_xy, 8);
	ACTION_PARAM_ANGLE(spread_z, 9);
	ACTION_PARAM_FIXED(range, 10);
	ACTION_PARAM_INT(duration, 11);
	ACTION_PARAM_DOUBLE(sparsity, 12);
	ACTION_PARAM_DOUBLE(driftspeed, 13);
	ACTION_PARAM_CLASS(spawnclass, 14);
	ACTION_PARAM_FIXED(spawnofs_z, 15);
	ACTION_PARAM_INT(SpiralOffset, 16);
	
	if (range == 0) range = 8192*FRACUNIT;
	if (sparsity == 0) sparsity=1.0;

	if (!self->player) return;

	AWeapon *weapon = self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (useammo && weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return;	// out of ammo
	}

	angle_t angle;
	angle_t slope;

	if (flags & RAF_EXPLICITANGLE)
	{
		angle = spread_xy;
		slope = spread_z;
	}
	else
	{
		angle = pr_crailgun.Random2() * (spread_xy / 255);
		slope = pr_crailgun.Random2() * (spread_z / 255);
	}

	P_RailAttack (self, damage, spawnofs_xy, spawnofs_z, color1, color2, maxdiff, flags, pufftype, angle, slope, range, duration, sparsity, driftspeed, spawnclass, SpiralOffset);
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
	ACTION_PARAM_START(17);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_INT(spawnofs_xy, 1);
	ACTION_PARAM_COLOR(color1, 2);
	ACTION_PARAM_COLOR(color2, 3);
	ACTION_PARAM_INT(flags, 4);
	ACTION_PARAM_INT(aim, 5);
	ACTION_PARAM_DOUBLE(maxdiff, 6);
	ACTION_PARAM_CLASS(pufftype, 7);
	ACTION_PARAM_ANGLE(spread_xy, 8);
	ACTION_PARAM_ANGLE(spread_z, 9);
	ACTION_PARAM_FIXED(range, 10);
	ACTION_PARAM_INT(duration, 11);
	ACTION_PARAM_DOUBLE(sparsity, 12);
	ACTION_PARAM_DOUBLE(driftspeed, 13);
	ACTION_PARAM_CLASS(spawnclass, 14);
	ACTION_PARAM_FIXED(spawnofs_z, 15);
	ACTION_PARAM_INT(SpiralOffset, 16);

	if (range == 0) range = 8192*FRACUNIT;
	if (sparsity == 0) sparsity = 1;

	AActor *linetarget;

	fixedvec3 savedpos = self->Pos();
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
		self->angle = self->AngleTo(self->target);
	}
	self->pitch = P_AimLineAttack (self, self->angle, MISSILERANGE, &linetarget, ANGLE_1*60, 0, aim ? self->target : NULL);
	if (linetarget == NULL && aim)
	{
		// We probably won't hit the target, but aim at it anyway so we don't look stupid.
		fixedvec2 pos = self->Vec2To(self->target);
		TVector2<double> xydiff(pos.x, pos.y);
		double zdiff = (self->target->Z() + (self->target->height>>1)) -
						(self->Z() + (self->height>>1) - self->floorclip);
		self->pitch = int(atan2(zdiff, xydiff.Length()) * ANGLE_180 / -M_PI);
	}
	// Let the aim trail behind the player
	if (aim)
	{
		saved_angle = self->angle = self->AngleTo(self->target, -self->target->velx * 3, -self->target->vely * 3);

		if (aim == CRF_AIMDIRECT)
		{
			// Tricky: We must offset to the angle of the current position
			// but then change the angle again to ensure proper aim.
			self->SetXY(self->Vec2Offset(
				spawnofs_xy * finecosine[self->angle],
				spawnofs_xy * finesine[self->angle]));
			spawnofs_xy = 0;
			self->angle = self->AngleTo(self->target,- self->target->velx * 3, -self->target->vely * 3);
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

	if (flags & CRF_EXPLICITANGLE)
	{
		angleoffset = spread_xy;
		slopeoffset = spread_z;
	}
	else
	{
		angleoffset = pr_crailgun.Random2() * (spread_xy / 255);
		slopeoffset = pr_crailgun.Random2() * (spread_z / 255);
	}

	P_RailAttack (self, damage, spawnofs_xy, spawnofs_z, color1, color2, maxdiff, flags, pufftype, angleoffset, slopeoffset, range, duration, sparsity, driftspeed, spawnclass,SpiralOffset);

	self->SetXYZ(savedpos);
	self->angle = saved_angle;
	self->pitch = saved_pitch;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor * receiver, bool use_aaptr, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2+use_aaptr);
	ACTION_PARAM_CLASS(mi, 0);
	ACTION_PARAM_INT(amount, 1);

	if (use_aaptr)
	{
		ACTION_PARAM_INT(setreceiver, 2);
		COPY_AAPTR_NOT_NULL(receiver, receiver, setreceiver);
	}

	bool res=true;
	
	if (amount==0) amount=1;
	if (mi) 
	{
		if (!mi->IsDescendantOf (RUNTIME_CLASS(AInventory)))
		{
			ACTION_SET_RESULT(false);
			return;
		}

		AInventory *item = static_cast<AInventory *>(Spawn (mi, 0, 0, 0, NO_REPLACE));
		if (!item)
		{
			ACTION_SET_RESULT(false);
			return;
		}
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
	DoGiveInventory(self, true, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToTarget)
{
	DoGiveInventory(self->target, true, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToChildren)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	while ((mo = it.Next()))
	{
		if (mo->master == self) DoGiveInventory(mo, false, PUSH_PARAMINFO);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToSiblings)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self) DoGiveInventory(mo, false, PUSH_PARAMINFO);
		}
	}
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

void DoTakeInventory(AActor * receiver, bool use_aaptr, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(3+use_aaptr);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_INT(amount, 1);
	ACTION_PARAM_INT(flags, 2);
	
	if (!item)
	{
		ACTION_SET_RESULT(false);
		return;
	}
	if (use_aaptr)
	{
		ACTION_PARAM_INT(setreceiver, 3);
		COPY_AAPTR_NOT_NULL(receiver, receiver, setreceiver);
	}

	bool res = receiver->TakeInventory(item, amount, true, (flags & TIF_NOTAKEINFINITE) != 0);
	ACTION_SET_RESULT(res);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeInventory)
{
	DoTakeInventory(self, true, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromTarget)
{
	DoTakeInventory(self->target, true, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromChildren)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	while ((mo = it.Next()))
	{
		if (mo->master == self) DoTakeInventory(mo, false, PUSH_PARAMINFO);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromSiblings)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self) DoTakeInventory(mo, false, PUSH_PARAMINFO);
		}
	}
}

//===========================================================================
//
// Common code for A_SpawnItem and A_SpawnItemEx
//
//===========================================================================

enum SIX_Flags
{
	SIXF_TRANSFERTRANSLATION	= 0x00000001,
	SIXF_ABSOLUTEPOSITION		= 0x00000002,
	SIXF_ABSOLUTEANGLE			= 0x00000004,
	SIXF_ABSOLUTEVELOCITY		= 0x00000008,
	SIXF_SETMASTER				= 0x00000010,
	SIXF_NOCHECKPOSITION		= 0x00000020,
	SIXF_TELEFRAG				= 0x00000040,
	SIXF_CLIENTSIDE				= 0x00000080,	// only used by Skulldronum
	SIXF_TRANSFERAMBUSHFLAG		= 0x00000100,
	SIXF_TRANSFERPITCH			= 0x00000200,
	SIXF_TRANSFERPOINTERS		= 0x00000400,
	SIXF_USEBLOODCOLOR			= 0x00000800,
	SIXF_CLEARCALLERTID			= 0x00001000,
	SIXF_MULTIPLYSPEED			= 0x00002000,
	SIXF_TRANSFERSCALE			= 0x00004000,
	SIXF_TRANSFERSPECIAL		= 0x00008000,
	SIXF_CLEARCALLERSPECIAL		= 0x00010000,
	SIXF_TRANSFERSTENCILCOL		= 0x00020000,
	SIXF_TRANSFERALPHA			= 0x00040000,
	SIXF_TRANSFERRENDERSTYLE	= 0x00080000,
	SIXF_SETTARGET				= 0x00100000,
	SIXF_SETTRACER				= 0x00200000,
	SIXF_NOPOINTERS				= 0x00400000,
	SIXF_ORIGINATOR				= 0x00800000,
	SIXF_TRANSFERSPRITEFRAME	= 0x01000000,
	SIXF_TRANSFERROLL			= 0x02000000,
	SIXF_ISTARGET				= 0x04000000,
	SIXF_ISMASTER				= 0x08000000,
	SIXF_ISTRACER				= 0x10000000,
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
	if (!(flags & SIXF_ORIGINATOR))
	{
		while (originator && originator->isMissile())
		{
			originator = originator->target;
		}
	}
	if (flags & SIXF_TELEFRAG) 
	{
		P_TeleportMove(mo, mo->Pos(), true);
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
		else if (originator && !(flags & SIXF_NOPOINTERS))
		{
			if (originator->flags3 & MF3_ISMONSTER)
			{
				// If this is a monster transfer all friendliness information
				mo->CopyFriendliness(originator, true);
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
	if (flags & SIXF_NOPOINTERS)
	{
		//[MC]Intentionally eliminate pointers. Overrides TRANSFERPOINTERS, but is overridden by SETMASTER/TARGET/TRACER.
		mo->LastHeard = NULL; //Sanity check.
		mo->target = NULL;
		mo->master = NULL;
		mo->tracer = NULL;
	}
	if (flags & SIXF_SETMASTER)
	{ // don't let it attack you (optional)!
		mo->master = originator;
	}
	if (flags & SIXF_SETTARGET)
	{
		mo->target = originator;
	}
	if (flags & SIXF_SETTRACER)
	{
		mo->tracer = originator;
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
	if (flags & SIXF_TRANSFERALPHA)
	{
		mo->alpha = self->alpha;
	}
	if (flags & SIXF_TRANSFERRENDERSTYLE)
	{
		mo->RenderStyle = self->RenderStyle;
	}
	
	if (flags & SIXF_TRANSFERSPRITEFRAME)
	{
		mo->sprite = self->sprite;
		mo->frame = self->frame;
	}

	if (flags & SIXF_TRANSFERROLL)
	{
		mo->roll = self->roll;
	}

	if (flags & SIXF_ISTARGET)
	{
		self->target = mo;
	}
	if (flags & SIXF_ISMASTER)
	{
		self->master = mo;
	}
	if (flags & SIXF_ISTRACER)
	{
		self->tracer = mo;
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

	AActor * mo = Spawn( missile, self->Vec3Angle(distance, self->angle, -self->floorclip + self->GetBobOffset() + zheight), ALLOW_REPLACE);

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
	ACTION_PARAM_ANGLE(angle, 7);
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

	fixedvec2 pos;

	if (!(flags & SIXF_ABSOLUTEANGLE))
	{
		angle += self->angle;
	}

	angle_t ang = angle >> ANGLETOFINESHIFT;

	if (flags & SIXF_ABSOLUTEPOSITION)
	{
		pos = self->Vec2Offset(xofs, yofs);
	}
	else
	{
		// in relative mode negative y values mean 'left' and positive ones mean 'right'
		// This is the inverse orientation of the absolute mode!
		pos = self->Vec2Offset(
			FixedMul(xofs, finecosine[ang]) + FixedMul(yofs, finesine[ang]),
			FixedMul(xofs, finesine[ang]) - FixedMul(yofs, finecosine[ang]));
	}

	if (!(flags & SIXF_ABSOLUTEVELOCITY))
	{
		// Same orientation issue here!
		fixed_t newxvel = FixedMul(xvel, finecosine[ang]) + FixedMul(yvel, finesine[ang]);
		yvel = FixedMul(xvel, finesine[ang]) - FixedMul(yvel, finecosine[ang]);
		xvel = newxvel;
	}

	AActor *mo = Spawn(missile, pos.x, pos.y, self->Z() - self->floorclip + self->GetBobOffset() + zofs, ALLOW_REPLACE);
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
		mo->angle = angle;
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

	bo = Spawn(missile, 
			self->PosPlusZ(-self->floorclip + self->GetBobOffset() + zheight + 35*FRACUNIT + (self->player? self->player->crouchoffset : 0)),
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
	self->velx += FixedMul(xyvel, finecosine[angle]);
	self->vely += FixedMul(xyvel, finesine[angle]);
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
	FString formatted = strbin1(text);
	Printf("%s\n", formatted.GetChars());
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

enum FadeFlags
{
	FTF_REMOVE =	1 << 0,
	FTF_CLAMP =		1 << 1,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeIn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(reduce, 0);
	ACTION_PARAM_INT(flags, 1);

	if (reduce == 0)
	{
		reduce = FRACUNIT / 10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;

	if (self->alpha >= FRACUNIT)
	{
		if (flags & FTF_CLAMP)
		{
			self->alpha = FRACUNIT;
		}
		if (flags & FTF_REMOVE)
		{
			P_RemoveThing(self);
		}
	}
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
	ACTION_PARAM_INT(flags, 1);

	if (reduce == 0)
	{
		reduce = FRACUNIT/10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;
	if (self->alpha <= 0)
	{
		if (flags & FTF_CLAMP)
		{
			self->alpha = 0;
		}
		if (flags & FTF_REMOVE)
		{
			P_RemoveThing(self);
		}
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
	ACTION_PARAM_INT(flags, 2);

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
	if (flags & FTF_CLAMP)
	{
		self->alpha = clamp(self->alpha, 0, FRACUNIT);
	}
	if (self->alpha == target && (flags & FTF_REMOVE))
	{
		P_RemoveThing(self);
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
	ACTION_PARAM_START(3);
	ACTION_PARAM_FIXED(scalex, 0);
	ACTION_PARAM_FIXED(scaley, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	ref->scaleX = scalex;
	ref->scaleY = scaley ? scaley : scalex;
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
	if (mult_v <= 0)
		mult_v = FRACUNIT;
	if (mult_h <= 0)
		mult_h = FRACUNIT;
	
	for (i = 0; i < GetDefaultByType(debris)->health; i++)
	{
		fixed_t xo = ((pr_spawndebris() - 128) << 12);
		fixed_t yo = ((pr_spawndebris() - 128) << 12);
		fixed_t zo = (pr_spawndebris()*self->height / 256 + self->GetBobOffset());
		mo = Spawn(debris, self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
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
// A_SpawnParticle
//
//===========================================================================
enum SPFflag
{
	SPF_FULLBRIGHT =	1,
	SPF_RELPOS =		1 << 1,
	SPF_RELVEL =		1 << 2,
	SPF_RELACCEL =		1 << 3,
	SPF_RELANG =		1 << 4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnParticle)
{
	ACTION_PARAM_START(15);
	ACTION_PARAM_COLOR(color,		0);
	ACTION_PARAM_INT(flags,			1);
	ACTION_PARAM_INT(lifetime,		2);
	ACTION_PARAM_INT(size,			3);
	ACTION_PARAM_ANGLE(angle,		4);
	ACTION_PARAM_FIXED(xoff,		5);
	ACTION_PARAM_FIXED(yoff,		6);
	ACTION_PARAM_FIXED(zoff,		7);
	ACTION_PARAM_FIXED(xvel,		8);
	ACTION_PARAM_FIXED(yvel,		9);
	ACTION_PARAM_FIXED(zvel,		10);
	ACTION_PARAM_FIXED(accelx,		11);
	ACTION_PARAM_FIXED(accely,		12);
	ACTION_PARAM_FIXED(accelz,		13);
	ACTION_PARAM_FIXED(startalphaf, 14);
	ACTION_PARAM_FIXED(fadestepf,	15);
	
	BYTE startalpha = (BYTE)Scale(clamp(startalphaf, 0, FRACUNIT), 255, FRACUNIT);
	int fadestep = fadestepf < 0? -1 : Scale(clamp(fadestepf, 0, FRACUNIT), 255, FRACUNIT);
	lifetime = clamp<int>(lifetime, 0, 255); // Clamp to byte
	size = clamp<int>(size, 0, 65535); // Clamp to word

	if (lifetime != 0)
	{
		const angle_t ang = (angle + ((flags & SPF_RELANG) ? self->angle : 0)) >> ANGLETOFINESHIFT;
		fixedvec3 pos;
		//[MC] Code ripped right out of A_SpawnItemEx.
		if (flags & SPF_RELPOS)
		{
			// in relative mode negative y values mean 'left' and positive ones mean 'right'
			// This is the inverse orientation of the absolute mode!
			const fixed_t xof1 = xoff;
			xoff = FixedMul(xof1, finecosine[ang]) + FixedMul(yoff, finesine[ang]);
			yoff = FixedMul(xof1, finesine[ang]) - FixedMul(yoff, finecosine[ang]);
		}
		if (flags & SPF_RELVEL)
		{
			const fixed_t newxvel = FixedMul(xvel, finecosine[ang]) + FixedMul(yvel, finesine[ang]);
			yvel = FixedMul(xvel, finesine[ang]) - FixedMul(yvel, finecosine[ang]);
			xvel = newxvel;
		}
		if (flags & SPF_RELACCEL)
		{
			fixed_t newaccelx = FixedMul(accelx, finecosine[ang]) + FixedMul(accely, finesine[ang]);
			accely = FixedMul(accelx, finesine[ang]) - FixedMul(accely, finecosine[ang]);
			accelx = newaccelx;
		}
		pos = self->Vec3Offset(xoff, yoff, zoff);
		P_SpawnParticle(pos.x, pos.y, pos.z, xvel, yvel, zvel, color, !!(flags & SPF_FULLBRIGHT), startalpha, lifetime, size, fadestep, accelx, accely, accelz);
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
static bool DoCheckSightOrRange(AActor *self, AActor *camera, double range, bool twodi)
{
	if (camera == NULL)
	{
		return false;
	}
	// Check distance first, since it's cheaper than checking sight.
	fixedvec2 pos = camera->Vec2To(self);
	fixed_t dz;
	fixed_t eyez = (camera->Top() - (camera->height>>2));	// same eye height as P_CheckSight
	if (eyez > self->Top())
	{
		dz = self->Top() - eyez;
	}
	else if (eyez < self->Z())
	{
		dz = self->Z() - eyez;
	}
	else
	{
		dz = 0;
	}
	double distance = ((double)pos.x * pos.x) + ((double)pos.y * pos.y) + (twodi == 0? ((double)dz * dz) : 0);
	if (distance <= range){
		// Within range
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
	ACTION_PARAM_START(3);
	double range = EvalExpressionF(ParameterIndex+0, self);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_BOOL(twodi, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckSightOrRange(self, players[i].mo, range, twodi))
			{
				return;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckSightOrRange(self, players[i].camera, range, twodi))
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
static bool DoCheckRange(AActor *self, AActor *camera, double range, bool twodi)
{
	if (camera == NULL)
	{
		return false;
	}
	// Check distance first, since it's cheaper than checking sight.
	fixedvec2 pos = camera->Vec2To(self);
	fixed_t dz;
	fixed_t eyez = (camera->Top() - (camera->height>>2));	// same eye height as P_CheckSight
	if (eyez > self->Top())
	{
		dz = self->Top() - eyez;
	}
	else if (eyez < self->Z())
	{
		dz = self->Z() - eyez;
	}
	else
	{
		dz = 0;
	}
	double distance = ((double)pos.x * pos.x) + ((double)pos.y * pos.y) + (twodi == 0? ((double)dz * dz) : 0);

	if (distance <= range){
		// Within range
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckRange)
{
	ACTION_PARAM_START(3);
	double range = EvalExpressionF(ParameterIndex+0, self);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_BOOL(twodi, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckRange(self, players[i].mo, range, twodi))
			{
				return;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckRange(self, players[i].camera, range, twodi))
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
		fixed_t xo = (((pr_burst() - 128)*self->radius) >> 7);
		fixed_t yo = (((pr_burst() - 128)*self->radius) >> 7);
		fixed_t zo = (pr_burst()*self->height / 255 + self->GetBobOffset());
		mo = Spawn(chunk, self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);

		if (mo)
		{
			mo->velz = FixedDiv(mo->Z() - self->Z(), self->height)<<2;
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
	if (self->Z() <= self->floorz)
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
	if (self->Top() >= self->ceilingz) // Height needs to be counted
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
	fixedvec3 pos = self->Pos();
	self->flags |= MF_SOLID;
	self->height = self->GetDefault()->height;
	self->radius = self->GetDefault()->radius;
	CALL_ACTION(A_RestoreSpecialPosition, self);

	if (flags & RSF_TELEFRAG)
	{
		// [KS] DIE DIE DIE DIE erm *ahem* =)
		oktorespawn = P_TeleportMove(self, self->Pos(), true, false);
	}
	else
	{
		oktorespawn = P_CheckPosition(self, self->X(), self->Y(), true);
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
		self->flags6 = defs->flags6;
		self->flags7 = defs->flags7;
		self->SetState (self->SpawnState);
		self->renderflags &= ~RF_INVISIBLE;

		if (flags & RSF_FOG)
		{
			P_SpawnTeleportFog(self, pos, true, true);
			P_SpawnTeleportFog(self, self->Pos(), false, true);
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
	CLOFF_NOAIM_VERT =			0x00000001,
	CLOFF_NOAIM_HORZ =			0x00000002,

	CLOFF_JUMPENEMY =			0x00000004,
	CLOFF_JUMPFRIEND =			0x00000008,
	CLOFF_JUMPOBJECT =			0x00000010,
	CLOFF_JUMPNONHOSTILE =		0x00000020,

	CLOFF_SKIPENEMY =			0x00000040,
	CLOFF_SKIPFRIEND =			0x00000080,
	CLOFF_SKIPOBJECT =			0x00000100,
	CLOFF_SKIPNONHOSTILE =		0x00000200,

	CLOFF_MUSTBESHOOTABLE =		0x00000400,

	CLOFF_SKIPTARGET =			0x00000800,
	CLOFF_ALLOWNULL =			0x00001000,
	CLOFF_CHECKPARTIAL =		0x00002000,

	CLOFF_MUSTBEGHOST =			0x00004000,
	CLOFF_IGNOREGHOST =			0x00008000,
	
	CLOFF_MUSTBESOLID =			0x00010000,
	CLOFF_BEYONDTARGET =		0x00020000,

	CLOFF_FROMBASE =			0x00040000,
	CLOFF_MUL_HEIGHT =			0x00080000,
	CLOFF_MUL_WIDTH =			0x00100000,

	CLOFF_JUMP_ON_MISS =		0x00200000,
	CLOFF_AIM_VERT_NOOFFSET =	0x00400000,

	CLOFF_SETTARGET =			0x00800000,
	CLOFF_SETMASTER =			0x01000000,
	CLOFF_SETTRACER =			0x02000000,
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
	fixedvec3 pos;
	fixed_t	vx, vy, vz;

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
		
		pos = self->PosPlusZ(offsetheight - self->floorclip);

		if (!(flags & CLOFF_FROMBASE))
		{ // default to hitscan origin

			// Synced with hitscan: self->height is strangely NON-conscientious about getting the right actor for player
			pos.z += (self->height >> 1);
			if (self->player != NULL)
			{
				pos.z += FixedMul (self->player->mo->AttackZOffset, self->player->crouchfactor);
			}
			else
			{
				pos.z += 8*FRACUNIT;
			}
		}

		if (target)
		{
			fixed_t xydist = self->Distance2D(target);
			fixed_t distance = P_AproxDistance(xydist, target->Z() - pos.z);

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
				else ang = self->AngleTo (target);
				
				angle += ang;
				
				ang >>= ANGLETOFINESHIFT;

				fixedvec2 xy = self->Vec2Offset(
					FixedMul(offsetwidth, finesine[ang]),
					-FixedMul(offsetwidth, finecosine[ang]));

				pos.x = xy.x;
				pos.y = xy.y;
			}

			if (flags & CLOFF_NOAIM_VERT)
			{
				pitch += self->pitch;
			}
			else if (flags & CLOFF_AIM_VERT_NOOFFSET)
			{
				pitch -= R_PointToAngle2 (0,0, xydist, target->Z() - pos.z + offsetheight + target->height / 2);
			}
			else
			{
				pitch -= R_PointToAngle2 (0,0, xydist, target->Z() - pos.z + target->height / 2);
			}
		}
		else if (flags & CLOFF_ALLOWNULL)
		{
			angle += self->angle;
			pitch += self->pitch;

			angle_t ang = self->angle >> ANGLETOFINESHIFT;

			fixedvec2 xy = self->Vec2Offset(
				FixedMul(offsetwidth, finesine[ang]),
				-FixedMul(offsetwidth, finecosine[ang]));

			pos.x = xy.x;
			pos.y = xy.y;
		}
		else return;

		angle >>= ANGLETOFINESHIFT;
		pitch >>= ANGLETOFINESHIFT;

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

	sector_t *sec = P_PointInSector(pos.x, pos.y);

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

	Trace(pos.x, pos.y, pos.z, sec, vx, vy, vz, range, ActorFlags::FromInt(0xFFFFFFFF), ML_BLOCKEVERYTHING, self, trace, 0,
		CheckLOFTraceFunc, &lof_data);

	if (trace.HitType == TRACE_HitActor ||
		((flags & CLOFF_JUMP_ON_MISS) && !lof_data.BadActor && trace.HitType != TRACE_HitNone))
	{
		if (minrange > 0 && trace.Distance < minrange)
		{
			return;
		}
		if ((trace.HitType == TRACE_HitActor) && (trace.Actor != NULL) && !(lof_data.BadActor))
		{
			if (flags & (CLOFF_SETTARGET))	self->target = trace.Actor;
			if (flags & (CLOFF_SETMASTER))	self->master = trace.Actor;
			if (flags & (CLOFF_SETTRACER))	self->tracer = trace.Actor;
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
	JLOSF_PROJECTILE =		1,
	JLOSF_NOSIGHT =			1 << 1,
	JLOSF_CLOSENOFOV =		1 << 2,
	JLOSF_CLOSENOSIGHT =	1 << 3,
	JLOSF_CLOSENOJUMP =		1 << 4,
	JLOSF_DEADNOJUMP =		1 << 5,
	JLOSF_CHECKMASTER =		1 << 6,
	JLOSF_TARGETLOS =		1 << 7,
	JLOSF_FLIPFOV =			1 << 8,
	JLOSF_ALLYNOJUMP =		1 << 9,
	JLOSF_COMBATANTONLY =	1 << 10,
	JLOSF_NOAUTOAIM =		1 << 11,
	JLOSF_CHECKTRACER =		1 << 12,
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
		else if ((self->flags & MF_MISSILE && (flags & JLOSF_PROJECTILE)) || (flags & JLOSF_CHECKTRACER))
		{
			if ((self->flags2 & MF2_SEEKERMISSILE) || (flags & JLOSF_CHECKTRACER))
				target = self->tracer;
			else
				target = NULL;
		}
		else
		{
			target = self->target;
		}

		if (target == NULL)
			return; // [KS] Let's not call P_CheckSight unnecessarily in this case.
		
		if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0))
		{
			return;
		}

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

	fixed_t distance = self->AproxDistance3D(target);

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
		an = viewport->AngleTo(target) - viewport->angle;

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

	fixed_t distance = self->AproxDistance3D(target);

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
		an = target->AngleTo(self) - target->angle;

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
		weapon->CheckAmmo(false, false);
	}

	if (!dontincrement)
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
			ActorFlags *flagp = (ActorFlags*) (((char*)self) + fd->structoffset);

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
// A_RaiseMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RaiseMaster)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(copy, 0);

	if (self->master != NULL)
	{
		if (copy)
			P_Thing_Raise(self->master, self);
		else
			P_Thing_Raise(self->master, NULL);
	}
}

//===========================================================================
//
// A_RaiseChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RaiseChildren)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(copy, 0);
	TThinkerIterator<AActor> it;
	AActor *mo;

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self)
		{
			if (copy)
				P_Thing_Raise(mo, self);
			else
				P_Thing_Raise(mo, NULL);
		}
	}
}

//===========================================================================
//
// A_RaiseSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RaiseSiblings)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_BOOL(copy, 0);
	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self)
			{
				if (copy)
					P_Thing_Raise(mo, self);
				else
					P_Thing_Raise(mo, NULL);
			}
		}
	}
}

//===========================================================================
//
// [TP] A_FaceConsolePlayer
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS (AActor, A_FaceConsolePlayer) {
	ACTION_PARAM_START (1);
	ACTION_PARAM_ANGLE (MaxTurnAngle, 0);
	// NOTE: It does nothing for zdoom.
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

	if (self->target == NULL
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
enum
{
	SPF_FORCECLAMP = 1,	// players always clamp
	SPF_INTERPOLATE = 2,
};


DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetAngle)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_ANGLE(angle, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *ref = COPY_AAPTR(self, ptr);
	if (ref == NULL)
	{
		ACTION_SET_RESULT(false);
		return;
	}
	ref->SetAngle(angle, !!(flags & SPF_INTERPOLATE));
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
	ACTION_PARAM_START(3);
	ACTION_PARAM_ANGLE(pitch, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	ref->SetPitch(pitch, !!(flags & SPF_INTERPOLATE), !!(flags & SPF_FORCECLAMP));
}

//===========================================================================
//
// [Nash] A_SetRoll
//
// Set actor's roll (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetRoll)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_ANGLE(roll, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}
	ref->SetRoll(roll, !!(flags & SPF_INTERPOLATE));
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
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(scale, 0);
	ACTION_PARAM_INT(ptr, 1);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	INTBOOL was_moving = ref->velx | ref->vely | ref->velz;

	ref->velx = FixedMul(ref->velx, scale);
	ref->vely = FixedMul(ref->vely, scale);
	ref->velz = FixedMul(ref->velz, scale);

	// If the actor was previously moving but now is not, and is a player,
	// update its player variables. (See A_Stop.)
	if (was_moving)
	{
		CheckStopped(ref);
	}
}

//===========================================================================
//
// A_ChangeVelocity
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeVelocity)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_FIXED(x, 0);
	ACTION_PARAM_FIXED(y, 1);
	ACTION_PARAM_FIXED(z, 2);
	ACTION_PARAM_INT(flags, 3);
	ACTION_PARAM_INT(ptr, 4);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	INTBOOL was_moving = ref->velx | ref->vely | ref->velz;

	fixed_t vx = x, vy = y, vz = z;
	fixed_t sina = finesine[ref->angle >> ANGLETOFINESHIFT];
	fixed_t cosa = finecosine[ref->angle >> ANGLETOFINESHIFT];

	if (flags & 1)	// relative axes - make x, y relative to actor's current angle
	{
		vx = DMulScale16(x, cosa, -y, sina);
		vy = DMulScale16(x, sina,  y, cosa);
	}
	if (flags & 2)	// discard old velocity - replace old velocity with new velocity
	{
		ref->velx = vx;
		ref->vely = vy;
		ref->velz = vz;
	}
	else	// add new velocity to old velocity
	{
		ref->velx += vx;
		ref->vely += vy;
		ref->velz += vz;
	}

	if (was_moving)
	{
		CheckStopped(ref);
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
// A_Teleport([state teleportstate, [class targettype,
// [class fogtype, [int flags, [fixed mindist,
// [fixed maxdist]]]]]])
//
// Attempts to teleport to a targettype at least mindist away and at most
// maxdist away (0 means unlimited). If successful, spawn a fogtype at old
// location and place calling actor in teleportstate. 
//
//===========================================================================
enum T_Flags
{
	TF_TELEFRAG =		0x00000001, // Allow telefrag in order to teleport.
	TF_RANDOMDECIDE =	0x00000002, // Randomly fail based on health. (A_Srcr2Decide)
	TF_FORCED =			0x00000004, // Forget what's in the way. TF_Telefrag takes precedence though.
	TF_KEEPVELOCITY =	0x00000008, // Preserve velocity.
	TF_KEEPANGLE =		0x00000010, // Keep angle.
	TF_USESPOTZ =		0x00000020, // Set the z to the spot's z, instead of the floor.
	TF_NOSRCFOG =		0x00000040, // Don't leave any fog behind when teleporting.
	TF_NODESTFOG =		0x00000080, // Don't spawn any fog at the arrival position.
	TF_USEACTORFOG =	0x00000100, // Use the actor's TeleFogSourceType and TeleFogDestType fogs.
	TF_NOJUMP =			0x00000200, // Don't jump after teleporting.
	TF_OVERRIDE =		0x00000400, // Ignore NOTELEPORT.
	TF_SENSITIVEZ =		0x00000800, // Fail if the actor wouldn't fit in the position (for Z).
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Teleport)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_STATE(teleport_state, 0);
	ACTION_PARAM_CLASS(target_type, 1);
	ACTION_PARAM_CLASS(fog_type, 2);
	ACTION_PARAM_INT(flags, 3);
	ACTION_PARAM_FIXED(mindist, 4);
	ACTION_PARAM_FIXED(maxdist, 5);
	ACTION_PARAM_INT(ptr, 6);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if ((ref->flags2 & MF2_NOTELEPORT) && !(flags & TF_OVERRIDE))
	{
		ACTION_SET_RESULT(false);
		return;
	}

	// Randomly choose not to teleport like A_Srcr2Decide.
	if (flags & TF_RANDOMDECIDE)
	{
		static const int chance[] =
		{
			192, 120, 120, 120, 64, 64, 32, 16, 0
		};

		unsigned int chanceindex = ref->health / ((ref->SpawnHealth()/8 == 0) ? 1 : ref->SpawnHealth()/8);

		if (chanceindex >= countof(chance))
		{
			chanceindex = countof(chance) - 1;
		}

		if (pr_teleport() >= chance[chanceindex]) return;
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if (target_type == NULL)
	{
		target_type = PClass::FindClass("BossSpot");
	}

	AActor * spot = state->GetSpotWithMinMaxDistance(target_type, ref->X(), ref->Y(), mindist, maxdist);
	if (spot == NULL) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	
	// [MC] By default, the function adjusts the actor's Z if it's below the floor or above the ceiling.
	// This can be an issue as actors designed to maintain specific z positions wind up teleporting
	// anyway when they should not, such as a floor rising above or ceiling lowering below the position
	// of the spot.
	if (flags & TF_SENSITIVEZ)
	{
		fixed_t posz = (flags & TF_USESPOTZ) ? spot->Z() : spot->floorz;
		if ((posz + ref->height > spot->ceilingz) || (posz < spot->floorz))
		{
			ACTION_SET_RESULT(false);
			return;
		}
	}
	fixedvec3 prev = ref->Pos();
	fixed_t aboveFloor = spot->Z() - spot->floorz;
	fixed_t finalz = spot->floorz + aboveFloor;

	if (spot->Z() + ref->height > spot->ceilingz)
		finalz = spot->ceilingz - ref->height;
	else if (spot->Z() < spot->floorz)
		finalz = spot->floorz;

	//Take precedence and cooperate with telefragging first.
	bool tele_result = P_TeleportMove(ref, spot->X(), spot->Y(), finalz, !!(flags & TF_TELEFRAG));

	if (!tele_result && (flags & TF_FORCED))
	{
		//If for some reason the original move didn't work, regardless of telefrag, force it to move.
		ref->SetOrigin(spot->X(), spot->Y(), finalz, false);
		tele_result = true;
	}

	AActor *fog1 = NULL, *fog2 = NULL;
	if (tele_result)
	{
		//If a fog type is defined in the parameter, or the user wants to use the actor's predefined fogs,
		//and if there's no desire to be fogless, spawn a fog based upon settings.
		if (fog_type || (flags & TF_USEACTORFOG))
		{ 
			if (!(flags & TF_NOSRCFOG))
			{
				if (flags & TF_USEACTORFOG)
					P_SpawnTeleportFog(ref, prev, true, true);
				else
				{
					fog1 = Spawn(fog_type, prev, ALLOW_REPLACE);
					if (fog1 != NULL)
						fog1->target = ref;
				}
			}
			if (!(flags & TF_NODESTFOG))
			{
				if (flags & TF_USEACTORFOG)
					P_SpawnTeleportFog(ref, ref->Pos(), false, true);
				else
				{
					fog2 = Spawn(fog_type, ref->Pos(), ALLOW_REPLACE);
					if (fog2 != NULL)
						fog2->target = ref;
				}
			}
		}
		
		ref->SetZ((flags & TF_USESPOTZ) ? spot->Z() : ref->floorz, false);

		if (!(flags & TF_KEEPANGLE))
			ref->angle = spot->angle;

		if (!(flags & TF_KEEPVELOCITY))
			ref->velx = ref->vely = ref->velz = 0;

		if (!(flags & TF_NOJUMP)) //The state jump should only happen with the calling actor.
		{
			ACTION_SET_RESULT(false); // Jumps should never set the result for inventory state chains!
			if (teleport_state == NULL)
			{
				// Default to Teleport.
				teleport_state = self->FindState("Teleport");
				// If still nothing, then return.
				if (teleport_state == NULL)
					return;
			}
			ACTION_JUMP(teleport_state);
			return;
		}
	}
	ACTION_SET_RESULT(tele_result);
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
// A_QuakeEx
//
// Extended version of A_Quake. Takes individual axis into account and can
// take a flag.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_QuakeEx)
{
	ACTION_PARAM_START(11);
	ACTION_PARAM_INT(intensityX, 0);
	ACTION_PARAM_INT(intensityY, 1);
	ACTION_PARAM_INT(intensityZ, 2);
	ACTION_PARAM_INT(duration, 3);
	ACTION_PARAM_INT(damrad, 4);
	ACTION_PARAM_INT(tremrad, 5);
	ACTION_PARAM_SOUND(sound, 6);
	ACTION_PARAM_INT(flags, 7);
	ACTION_PARAM_DOUBLE(mulWaveX, 8);
	ACTION_PARAM_DOUBLE(mulWaveY, 9);
	ACTION_PARAM_DOUBLE(mulWaveZ, 10);
	P_StartQuakeXYZ(self, 0, intensityX, intensityY, intensityZ, duration, damrad, tremrad, sound, flags, mulWaveX, mulWaveY, mulWaveZ);
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
		newX = self->X() - FixedMul (finecosine[angle], dist);
		newY = self->Y() - FixedMul (finesine[angle], dist);
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
			// the following 4 lines are for future-proofing this for both interpolation overhaul and line portals.
			// For portals we need to calculate the destination including the portal offset
			// and for interpolation we need to set the performed movement explicitly, because SetXY cannot do that.
			newX -= self->X();
			newY -= self->Y();
			self->SetXY(self->Vec2Offset(newX, newY));
			self->SetMovement(newX, newY, 0);
			self->LinkToWorld ();
		}
		self->WeaveIndexXY = weaveXY;
	}
	if (zdist != 0 && zspeed != 0)
	{
		self->AddZ(-MulScale13(finesine[weaveZ << BOBTOFINESHIFT], zdist));
		weaveZ = (weaveZ + zspeed) & 63;
		self->AddZ(MulScale13(finesine[weaveZ << BOBTOFINESHIFT], zdist));
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
	angle_t angle = self->target->AngleTo(self) - self->target->angle;
	angle >>= 24;
	bool dodge = (P_CheckSight(self->target, self) && (angle>226 || angle<30));

	// Distance check is simplistic
	fixedvec2 vec = self->Vec2To(self->target);
	fixed_t dx = abs (vec.x);
	fixed_t dy = abs (vec.y);
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
		angle = self->target->AngleTo(self);
		
		fixedvec3 bloodpos = self->target->Vec3Angle(self->target->radius, angle, self->target->height >> 1);


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
				P_SpawnPuff(self, pufftype, bloodpos, angle, 0);
			}
		}
		else if (self->target->flags3 & MF3_GHOST)
			damage >>= 2;
		if (damage)
		{
			int newdam = P_DamageMobj(self->target, self, self, damage, mod, DMG_THRUSTLESS);
			if (spawnblood)
			{
				P_SpawnBlood(bloodpos, angle, newdam > 0 ? newdam : damage, self->target);
				P_TraceBleed(newdam > 0 ? newdam : damage, self->target, self->AngleTo(dx, dy, self->target), 0);
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

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Warp)
{
	ACTION_PARAM_START(10);

	ACTION_PARAM_INT(destination_selector, 0);
	ACTION_PARAM_FIXED(xofs, 1);
	ACTION_PARAM_FIXED(yofs, 2);
	ACTION_PARAM_FIXED(zofs, 3);
	ACTION_PARAM_ANGLE(angle, 4);
	ACTION_PARAM_INT(flags, 5);
	ACTION_PARAM_STATE(success_state, 6);
	ACTION_PARAM_FIXED(heightoffset, 7);
	ACTION_PARAM_FIXED(radiusoffset, 8);
	ACTION_PARAM_ANGLE(pitch, 9);
	
	AActor *reference;
	
	if((flags & WARPF_USETID))
	{
		reference = SingleActorFromTID(destination_selector, self);
	}
	else
	{
		reference = COPY_AAPTR(self, destination_selector);
	}

	//If there is no actor to warp to, fail.
	if (!reference)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if (P_Thing_Warp(self, reference, xofs, yofs, zofs, angle, flags, heightoffset, radiusoffset, pitch))
	{
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


static bool DoCheckSpecies(AActor *mo, FName filterSpecies, bool exclude)
{
	FName actorSpecies = mo->GetSpecies();
	if (filterSpecies == NAME_None) return true;
	return exclude ? (actorSpecies != filterSpecies) : (actorSpecies == filterSpecies);
}

static bool DoCheckClass(AActor *mo, const PClass *filterClass, bool exclude)
{
	const PClass *actorClass = mo->GetClass();
	if (filterClass == NULL) return true;
	return exclude ? (actorClass != filterClass) : (actorClass == filterClass);
}
//==========================================================================
//
// A_RadiusGive(item, distance, flags, amount, filter, species)
//
// Uses code roughly similar to A_Explode (but without all the compatibility
// baggage and damage computation code) to give an item to all eligible mobjs
// in range.
//
//==========================================================================
enum RadiusGiveFlags
{
	RGF_GIVESELF	=   1 << 0,
	RGF_PLAYERS		=   1 << 1,
	RGF_MONSTERS	=   1 << 2,
	RGF_OBJECTS		=   1 << 3,
	RGF_VOODOO		=	1 << 4,
	RGF_CORPSES		=	1 << 5,
	RGF_MASK		=	2111,
	RGF_NOTARGET	=	1 << 6,
	RGF_NOTRACER	=	1 << 7,
	RGF_NOMASTER	=	1 << 8,
	RGF_CUBE		=	1 << 9,
	RGF_NOSIGHT		=	1 << 10,
	RGF_MISSILES	=	1 << 11,
	RGF_INCLUSIVE	=	1 << 12,
	RGF_ITEMS		=	1 << 13,
	RGF_KILLED		=	1 << 14,
	RGF_EXFILTER	=	1 << 15,
	RGF_EXSPECIES	=	1 << 16,
	RGF_EITHER		=	1 << 17,
};

static bool DoRadiusGive(AActor *self, AActor *thing, const PClass *item, int amount, fixed_t distance, int flags, const PClass *filter, FName species, fixed_t mindist)
{
	// [MC] We only want to make an exception for missiles here. Nothing else.
	bool missilePass = !!((flags & RGF_MISSILES) && thing->isMissile());
	if (thing == self)
	{
		if (!(flags & RGF_GIVESELF))
			return false;
	}
	else if (thing->isMissile())
	{
		if (!missilePass)
			return false;
	}

	//[MC] Check for a filter, species, and the related exfilter/expecies/either flag(s).
	bool filterpass = DoCheckClass(thing, filter, !!(flags & RGF_EXFILTER)),
		speciespass = DoCheckSpecies(thing, species, !!(flags & RGF_EXSPECIES));

	if ((flags & RGF_EITHER) ? (!(filterpass || speciespass)) : (!(filterpass && speciespass)))
	{
		if (thing != self)	//Don't let filter and species obstruct RGF_GIVESELF.
			return false;
	}

	//Check for target, master, and tracer flagging.
	bool targetPass = true;
	bool masterPass = true;
	bool tracerPass = true;
	bool ptrPass = false;
	if ((thing != self) && (flags & (RGF_NOTARGET | RGF_NOMASTER | RGF_NOTRACER)))
	{
		if ((thing == self->target) && (flags & RGF_NOTARGET))
			targetPass = false;
		if ((thing == self->master) && (flags & RGF_NOMASTER))
			masterPass = false;
		if ((thing == self->tracer) && (flags & RGF_NOTRACER))
			tracerPass = false;

		ptrPass = (flags & RGF_INCLUSIVE) ? (targetPass || masterPass || tracerPass) : (targetPass && masterPass && tracerPass);

		//We should not care about what the actor is here. It's safe to abort this actor.
		if (!ptrPass)
			return false;
	}

	//Next, actor flag checking. 
	bool selfPass = !!((flags & RGF_GIVESELF) && thing == self);
	bool corpsePass = !!((flags & RGF_CORPSES) && thing->flags & MF_CORPSE);
	bool killedPass = !!((flags & RGF_KILLED) && thing->flags6 & MF6_KILLED);
	bool monsterPass = !!((flags & RGF_MONSTERS) && thing->flags3 & MF3_ISMONSTER);
	bool objectPass = !!((flags & RGF_OBJECTS) && (thing->player == NULL) && (!(thing->flags3 & MF3_ISMONSTER))
		&& ((thing->flags & MF_SHOOTABLE) || (thing->flags6 & MF6_VULNERABLE)));
	bool playerPass = !!((flags & RGF_PLAYERS) && (thing->player != NULL) && (thing->player->mo == thing));
	bool voodooPass = !!((flags & RGF_VOODOO) && (thing->player != NULL) && (thing->player->mo != thing));
	//Self calls priority over the rest of this.
	if (!selfPass)
	{
		//If it's specifically a monster/object/player/voodoo... Can be either or...
		if (monsterPass || objectPass || playerPass || voodooPass)
		{
			//...and is dead, without desire to give to the dead...
			if (((thing->health <= 0) && !(corpsePass || killedPass)))
			{
				//Skip!
				return false;
			}
		}
	}

	bool itemPass = !!((flags & RGF_ITEMS) && thing->IsKindOf(RUNTIME_CLASS(AInventory)));

	if (selfPass || monsterPass || corpsePass || killedPass || itemPass || objectPass || missilePass || playerPass || voodooPass)
	{

		fixedvec3 diff = self->Vec3To(thing);
		diff.z += (thing->height - self->height) / 2;
		if (flags & RGF_CUBE)
		{ // check if inside a cube
			double dx = fabs((double)(diff.x));
			double dy = fabs((double)(diff.y));
			double dz = fabs((double)(diff.z));
			double dist = (double)distance;
			double min = (double)mindist;
			if ((dx > dist || dy > dist || dz > dist) || (min && (dx < min && dy < min && dz < min)))
			{
				return false;
			}
		}
		else
		{ // check if inside a sphere
			double distsquared = double(distance) * double(distance);
			double minsquared = double(mindist) * double(mindist);
			double lengthsquared = TVector3<double>(diff.x, diff.y, diff.z).LengthSquared();
			if (lengthsquared > distsquared || (minsquared && (lengthsquared < minsquared)))
			{
				return false;
			}
		}

		if ((flags & RGF_NOSIGHT) || P_CheckSight(thing, self, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY))
		{ // OK to give; target is in direct path, or the monster doesn't care about it being in line of sight.
			AInventory *gift = static_cast<AInventory *>(Spawn(item, 0, 0, 0, NO_REPLACE));
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
			if (!gift->CallTryPickup(thing))
			{
				gift->Destroy();
				return false;
			}
			else
			{
				return true;
			}
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusGive)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(amount, 3);
	ACTION_PARAM_CLASS(filter, 4);
	ACTION_PARAM_NAME(species, 5);
	ACTION_PARAM_FIXED(mindist, 6);

	// We need a valid item, valid targets, and a valid range
	if (item == NULL || (flags & RGF_MASK) == 0 || !flags || distance <= 0 || mindist >= distance)
	{
		ACTION_SET_RESULT(false);
		return;
	}
	
	if (amount == 0)
	{
		amount = 1;
	}
	AActor *thing;
	bool given = false;
	if (flags & RGF_MISSILES)
	{
		TThinkerIterator<AActor> it;
		while ((thing = it.Next()))
		{
			if (DoRadiusGive(self, thing, item, amount, distance, flags, filter, species, mindist))	given = true;
		}
	}
	else
	{
		FBlockThingsIterator it(FBoundingBox(self->X(), self->Y(), distance));
		while ((thing = it.Next()))
		{
			if (DoRadiusGive(self, thing, item, amount, distance, flags, filter, species, mindist))	given = true;
		}
	}
	ACTION_SET_RESULT(given);
}

//===========================================================================
//
// A_CheckSpecies
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSpecies)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_NAME(species, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *mobj = COPY_AAPTR(self, ptr);

	ACTION_SET_RESULT(false);
	//Needs at least one state jump to work. 
	if (!mobj)
	{
		return;
	}

	if (jump && mobj->GetSpecies() == species)
		ACTION_JUMP(jump);
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

//==========================================================================
//
// A_SetSpeed
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetSpeed)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(speed, 0);
	ACTION_PARAM_INT(ptr, 1);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}
	
	ref->Speed = speed;
}

//==========================================================================
//
// A_SetFloatSpeed
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetFloatSpeed)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(speed, 0);
	ACTION_PARAM_INT(ptr, 1);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	ref->FloatSpeed = speed;
}

//==========================================================================
//
// A_SetPainThreshold
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetPainThreshold)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(threshold, 0);
	ACTION_PARAM_INT(ptr, 1);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (!ref)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	ref->PainThreshold = threshold;
}

//===========================================================================
//
// Common A_Damage handler
//
// A_Damage* (int amount, str damagetype, int flags, str filter, str species)
// Damages the specified actor by the specified amount. Negative values heal.
// Flags: See below.
// Filter: Specified actor is the only type allowed to be affected.
// Species: Specified species is the only type allowed to be affected.
//
// Examples: 
// A_Damage(20,"Normal",DMSS_FOILINVUL,0,"DemonicSpecies") <--Only actors 
//	with a species "DemonicSpecies" will be affected. Use 0 to not filter by actor.
//
//===========================================================================

enum DMSS
{
	DMSS_FOILINVUL			= 1,	//Foil invulnerability
	DMSS_AFFECTARMOR		= 2,	//Make it affect armor
	DMSS_KILL				= 4,	//Damages them for their current health
	DMSS_NOFACTOR			= 8,	//Ignore DamageFactors
	DMSS_FOILBUDDHA			= 16,	//Can kill actors with Buddha flag, except the player.
	DMSS_NOPROTECT			= 32,	//Ignores PowerProtection entirely
	DMSS_EXFILTER			= 64,	//Changes filter into a blacklisted class instead of whitelisted.
	DMSS_EXSPECIES			= 128,	// ^ but with species instead.
	DMSS_EITHER				= 256,  //Allow either type or species to be affected.
};

static void DoDamage(AActor *dmgtarget, AActor *self, int amount, FName DamageType, int flags, const PClass *filter, FName species)
{
	bool filterpass = DoCheckClass(dmgtarget, filter, !!(flags & DMSS_EXFILTER)),
		speciespass = DoCheckSpecies(dmgtarget, species, !!(flags & DMSS_EXSPECIES));
	if ((flags & DMSS_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass))
	{
		int dmgFlags = 0;
		if (flags & DMSS_FOILINVUL)
			dmgFlags |= DMG_FOILINVUL;
		if (flags & DMSS_FOILBUDDHA)
			dmgFlags |= DMG_FOILBUDDHA;
		if (flags & (DMSS_KILL | DMSS_NOFACTOR)) //Kill implies NoFactor
			dmgFlags |= DMG_NO_FACTOR;
		if (!(flags & DMSS_AFFECTARMOR) || (flags & DMSS_KILL)) //Kill overrides AffectArmor
			dmgFlags |= DMG_NO_ARMOR;
		if (flags & DMSS_KILL) //Kill adds the value of the damage done to it. Allows for more controlled extreme death types.
			amount += dmgtarget->health;
		if (flags & DMSS_NOPROTECT) //Ignore PowerProtection.
			dmgFlags |= DMG_NO_PROTECT;
	
		if (amount > 0)
			P_DamageMobj(dmgtarget, self, self, amount, DamageType, dmgFlags); //Should wind up passing them through just fine.

		else if (amount < 0)
		{
			amount = -amount;
			P_GiveBody(dmgtarget, amount);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageSelf)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

		DoDamage(self, self, amount, DamageType, flags, filter, species);
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageTarget)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

	if (self->target != NULL)
	{
		DoDamage(self->target, self, amount, DamageType, flags, filter, species);
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageTracer)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

	if (self->tracer != NULL)
	{
		DoDamage(self->tracer, self, amount, DamageType, flags, filter, species);
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageMaster)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

	if (self->master != NULL)
	{
		DoDamage(self->master, self, amount, DamageType, flags, filter, species);
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageChildren)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

	TThinkerIterator<AActor> it;
	AActor * mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			DoDamage(mo, self, amount, DamageType, flags, filter, species);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageSiblings)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_CLASS(filter, 3);
	ACTION_PARAM_NAME(species, 4);

	TThinkerIterator<AActor> it;
	AActor * mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self)
			{
				DoDamage(mo, self, amount, DamageType, flags, filter, species);
			}
		}
	}
}


//===========================================================================
//
// A_Kill*(damagetype, int flags)
//
//===========================================================================
enum KILS
{
	KILS_FOILINVUL		= 1 << 0,
	KILS_KILLMISSILES	= 1 << 1,
	KILS_NOMONSTERS		= 1 << 2,
	KILS_FOILBUDDHA		= 1 << 3,
	KILS_EXFILTER		= 1 << 4,
	KILS_EXSPECIES		= 1 << 5,
	KILS_EITHER			= 1 << 6,
};

static void DoKill(AActor *killtarget, AActor *self, FName damagetype, int flags, const PClass *filter, FName species)
{
	bool filterpass = DoCheckClass(killtarget, filter, !!(flags & KILS_EXFILTER)),
		speciespass = DoCheckSpecies(killtarget, species, !!(flags & KILS_EXSPECIES));
	if ((flags & KILS_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass)) //Check this first. I think it'll save the engine a lot more time this way.
	{
		int dmgFlags = DMG_NO_ARMOR | DMG_NO_FACTOR;

		if (KILS_FOILINVUL)
			dmgFlags |= DMG_FOILINVUL;
		if (KILS_FOILBUDDHA)
			dmgFlags |= DMG_FOILBUDDHA;

	
		if ((killtarget->flags & MF_MISSILE) && (flags & KILS_KILLMISSILES))
		{
			//[MC] Now that missiles can set masters, lets put in a check to properly destroy projectiles. BUT FIRST! New feature~!
			//Check to see if it's invulnerable. Disregarded if foilinvul is on, but never works on a missile with NODAMAGE
			//since that's the whole point of it.
			if ((!(killtarget->flags2 & MF2_INVULNERABLE) || (flags & KILS_FOILINVUL)) &&
				(!(killtarget->flags7 & MF7_BUDDHA) || (flags & KILS_FOILBUDDHA)) && 
				!(killtarget->flags5 & MF5_NODAMAGE))
			{
				P_ExplodeMissile(killtarget, NULL, NULL);
			}
		}
		if (!(flags & KILS_NOMONSTERS))
		{
			P_DamageMobj(killtarget, self, self, killtarget->health, damagetype, dmgFlags);
		}
	}
}


//===========================================================================
//
// A_KillTarget(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillTarget)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_NAME(damagetype, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	if (self->target != NULL)
	{
		DoKill(self->target, self, damagetype, flags, filter, species);
	}
}

//===========================================================================
//
// A_KillTracer(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillTracer)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_NAME(damagetype, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	if (self->tracer != NULL)
	{
		DoKill(self->tracer, self, damagetype, flags, filter, species);
	}
}

//===========================================================================
//
// A_KillMaster(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillMaster)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_NAME(damagetype, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	if (self->master != NULL)
	{
		DoKill(self->master, self, damagetype, flags, filter, species);
	}
}

//===========================================================================
//
// A_KillChildren(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillChildren)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_NAME(damagetype, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self) 
		{
			DoKill(mo, self, damagetype, flags, filter, species);
		}
	}
}

//===========================================================================
//
// A_KillSiblings(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillSiblings)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_NAME(damagetype, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ( (mo = it.Next()) )
		{
			if (mo->master == self->master && mo != self)
			{ 
				DoKill(mo, self, damagetype, flags, filter, species);
			}
		}
	}
}

//===========================================================================
//
// DoRemove
//
//===========================================================================

enum RMVF_flags
{
	RMVF_MISSILES		= 1 << 0,
	RMVF_NOMONSTERS		= 1 << 1,
	RMVF_MISC			= 1 << 2,
	RMVF_EVERYTHING		= 1 << 3,
	RMVF_EXFILTER		= 1 << 4,
	RMVF_EXSPECIES		= 1 << 5,
	RMVF_EITHER			= 1 << 6,
};

static void DoRemove(AActor *removetarget, int flags, const PClass *filter, FName species)
{
	bool filterpass = DoCheckClass(removetarget, filter, !!(flags & RMVF_EXFILTER)),
		speciespass = DoCheckSpecies(removetarget, species, !!(flags & RMVF_EXSPECIES));
	if ((flags & RMVF_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass))
	{
		if ((flags & RMVF_EVERYTHING))
		{
			P_RemoveThing(removetarget);
		}
		if ((flags & RMVF_MISC) && !((removetarget->flags3 & MF3_ISMONSTER) && (removetarget->flags & MF_MISSILE)))
		{
			P_RemoveThing(removetarget);
		}
		if ((removetarget->flags3 & MF3_ISMONSTER) && !(flags & RMVF_NOMONSTERS))
		{
			P_RemoveThing(removetarget);
		}
		if ((removetarget->flags & MF_MISSILE) && (flags & RMVF_MISSILES))
		{
			P_RemoveThing(removetarget);
		}
	}
}

//===========================================================================
//
// A_RemoveTarget
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveTarget)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_CLASS(filter, 1);
	ACTION_PARAM_NAME(species, 2);
	
	if (self->target != NULL)
	{
		DoRemove(self->target, flags, filter, species);
	}
}

//===========================================================================
//
// A_RemoveTracer
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveTracer)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_CLASS(filter, 1);
	ACTION_PARAM_NAME(species, 2);
	
	if (self->tracer != NULL)
	{
		DoRemove(self->tracer, flags, filter, species);
	}
}

//===========================================================================
//
// A_RemoveMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveMaster)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_CLASS(filter, 1);
	ACTION_PARAM_NAME(species, 2);
	
	if (self->master != NULL)
	{
		DoRemove(self->master, flags, filter, species);
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
	ACTION_PARAM_START(4);
	ACTION_PARAM_BOOL(removeall, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);
	

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self && (mo->health <= 0 || removeall))
		{
			DoRemove(mo, flags, filter, species);
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
	ACTION_PARAM_START(4);
	ACTION_PARAM_BOOL(removeall, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self && (mo->health <= 0 || removeall))
			{
				DoRemove(mo, flags, filter, species);
			}
		}
	}
}

//===========================================================================
//
// A_Remove
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Remove)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(removee, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_CLASS(filter, 2);
	ACTION_PARAM_NAME(species, 3);

	AActor *reference = COPY_AAPTR(self, removee);
	if (reference != NULL)
	{
		DoRemove(reference, flags, filter, species);
	}
}

//===========================================================================
//
// A_SetTeleFog
//
// Sets the teleport fog(s) for the calling actor.
// Takes a name of the classes for the source and destination.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTeleFog)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_CLASS(oldpos, 0);
	ACTION_PARAM_CLASS(newpos, 1);

	self->TeleFogSourceType = oldpos;
	self->TeleFogDestType = newpos;
}

//===========================================================================
//
// A_SwapTeleFog
//
// Switches the source and dest telefogs around. 
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SwapTeleFog)
{
	if ((self->TeleFogSourceType != self->TeleFogDestType)) //Does nothing if they're the same.
	{
		const PClass *temp = self->TeleFogSourceType;
		self->TeleFogSourceType = self->TeleFogDestType;
		self->TeleFogDestType = temp;
	}
}

//===========================================================================
//
// A_SetFloatBobPhase
//
// Changes the FloatBobPhase of the actor.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetFloatBobPhase)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(bob, 0);

	//Respect float bob phase limits.
	if (self && (bob >= 0 && bob <= 63))
		self->FloatBobPhase = bob;
}

//===========================================================================
// A_SetHealth
//
// Changes the health of the actor.
// Takes a pointer as well.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetHealth)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(health, 0);
	ACTION_PARAM_INT(ptr, 1);

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (!mobj)
	{
		return;
	}

	player_t *player = mobj->player;
	if (player)
	{
		if (health <= 0)
			player->mo->health = mobj->health = player->health = 1; //Copied from the buddha cheat.
		else
			player->mo->health = mobj->health = player->health = health;
	}
	else if (mobj)
	{
		if (health <= 0)
			mobj->health = 1;
		else
			mobj->health = health;
	}
}

//===========================================================================
// A_ResetHealth
//
// Resets the health of the actor to default, except if their dead.
// Takes a pointer.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ResetHealth)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(ptr, 0);

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (!mobj)
	{
		return;
	}

	player_t *player = mobj->player;
	if (player && (player->mo->health > 0))
	{
		player->health = player->mo->health = player->mo->GetDefault()->health; //Copied from the resurrect cheat.
	}
	else if (mobj && (mobj->health > 0))
	{
		mobj->health = mobj->SpawnHealth();
	}
}

//===========================================================================
// A_JumpIfHigherOrLower
//
// Jumps if a target, master, or tracer is higher or lower than the calling 
// actor. Can also specify how much higher/lower the actor needs to be than 
// itself. Can also take into account the height of the actor in question,
// depending on which it's checking. This means adding height of the
// calling actor's self if the pointer is higher, or height of the pointer 
// if its lower.
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfHigherOrLower)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_STATE(high, 0);
	ACTION_PARAM_STATE(low, 1);
	ACTION_PARAM_FIXED(offsethigh, 2);
	ACTION_PARAM_FIXED(offsetlow, 3);
	ACTION_PARAM_BOOL(includeHeight, 4);
	ACTION_PARAM_INT(ptr, 5);

	AActor *mobj = COPY_AAPTR(self, ptr);


	if (!mobj || (mobj == self)) //AAPTR_DEFAULT is completely useless in this regard.
	{
		return;
	}
	ACTION_SET_RESULT(false); //No inventory jump chains please.

	if ((high) && (mobj->Z() > ((includeHeight ? self->height : 0) + self->Z() + offsethigh)))
		ACTION_JUMP(high);
	else if ((low) && (mobj->Z() + (includeHeight ? mobj->height : 0)) < (self->Z() + offsetlow))
		ACTION_JUMP(low);
}

//===========================================================================
// A_SetSpecies(str species, ptr)
//
// Sets the species of the calling actor('s pointer).
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetSpecies)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_NAME(species, 0);
	ACTION_PARAM_INT(ptr, 1);
	AActor *mobj = COPY_AAPTR(self, ptr);
	if (!mobj)
	{
		ACTION_SET_RESULT(false);
		return;
	}

	mobj->Species = species;
}

//===========================================================================
//
// A_SetRipperLevel(int level)
//
// Sets the ripper level of the calling actor.
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetRipperLevel)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(level, 0);
	self->RipperLevel = level;
}

//===========================================================================
//
// A_SetRipMin(int min)
//
// Sets the minimum level a ripper must be in order to rip through this actor.
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetRipMin)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(min, 0);
	self->RipLevelMin = min; 
}

//===========================================================================
//
// A_SetRipMax(int max)
//
// Sets the minimum level a ripper must be in order to rip through this actor.
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetRipMax)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(max, 0);
	self->RipLevelMax = max;
}

//==========================================================================
//
// A_CheckProximity(jump, classname, distance, count, flags, ptr)
//
// Checks to see if a certain actor class is close to the 
// actor/pointer within distance, in numbers.
//==========================================================================
enum CPXFflags
{
	CPXF_ANCESTOR =			1,
	CPXF_LESSOREQUAL =		1 << 1,
	CPXF_NOZ =				1 << 2,
	CPXF_COUNTDEAD =		1 << 3,
	CPXF_DEADONLY =			1 << 4,
	CPXF_EXACT =			1 << 5,
	CPXF_SETTARGET =		1 << 6,
	CPXF_SETMASTER =		1 << 7,
	CPXF_SETTRACER =		1 << 8,
	CPXF_FARTHEST =			1 << 9,
	CPXF_CLOSEST =			1 << 10,
	CPXF_SETONPTR =			1 << 11,
	CPXF_CHECKSIGHT =		1 << 12,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckProximity)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_CLASS(classname, 1);
	ACTION_PARAM_FIXED(distance, 2);
	ACTION_PARAM_INT(count, 3);
	ACTION_PARAM_INT(flags, 4);
	ACTION_PARAM_INT(ptr, 5);

	ACTION_SET_RESULT(false); //No inventory chain results please.

	if (!jump)
	{
		if (!(flags & (CPXF_SETTARGET | CPXF_SETMASTER | CPXF_SETTRACER)))
			return;
	}
	AActor *ref = COPY_AAPTR(self, ptr);

	//We need these to check out.
	if (!ref || !classname || (distance <= 0))
		return;

	int counter = 0;
	bool result = false;
	fixed_t closer = distance, farther = 0, current = distance;
	const bool ptrWillChange = !!(flags & (CPXF_SETTARGET | CPXF_SETMASTER | CPXF_SETTRACER));
	const bool ptrDistPref = !!(flags & (CPXF_CLOSEST | CPXF_FARTHEST));

	TThinkerIterator<AActor> it;
	AActor *mo, *dist = NULL;

	//[MC] Process of elimination, I think, will get through this as quickly and 
	//efficiently as possible. 
	while ((mo = it.Next()))
	{
		if (mo == ref) //Don't count self.
			continue;

		//Check inheritance for the classname. Taken partly from CheckClass DECORATE function.
		if (flags & CPXF_ANCESTOR)
		{
			if (!(mo->IsKindOf(classname)))
				continue;
		}
		//Otherwise, just check for the regular class name.
		else if (classname != mo->GetClass())
			continue;

		//[MC]Make sure it's in range and respect the desire for Z or not. The function forces it to use
		//Z later for ensuring CLOSEST and FARTHEST flags are respected perfectly.
		//Ripped from sphere checking in A_RadiusGive (along with a number of things).
		if ((ref->AproxDistance(mo) < distance &&
			((flags & CPXF_NOZ) ||
			((ref->Z() > mo->Z() && ref->Z() - mo->Top() < distance) ||
			(ref->Z() <= mo->Z() && mo->Z() - ref->Top() < distance)))))
		{
			if ((flags & CPXF_CHECKSIGHT) && !(P_CheckSight(mo, ref, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY)))
				continue;

			if (ptrWillChange)
			{
				current = ref->AproxDistance(mo);

				if ((flags & CPXF_CLOSEST) && (current < closer))
				{
					dist = mo;
					closer = current;			//This actor's closer. Set the new standard.
				}
				else if ((flags & CPXF_FARTHEST) && (current > farther))
				{
					dist = mo;
					farther = current;
				}
				else if (!dist) 
					dist = mo; //Just get the first one and call it quits if there's nothing selected.
			}

			if (mo->flags6 & MF6_KILLED)
			{
				if (!(flags & (CPXF_COUNTDEAD | CPXF_DEADONLY)))
					continue;
				counter++;
			}
			else
			{
				if (flags & CPXF_DEADONLY)
					continue;
				counter++;
			}

			//Abort if the number of matching classes nearby is greater, we have obviously succeeded in our goal.
			if (counter > count)
			{
				result = (flags & (CPXF_LESSOREQUAL | CPXF_EXACT)) ? false : true;

				//However, if we have one SET* flag and either the closest or farthest flags, keep the function going.
				if (ptrWillChange && ptrDistPref)
					continue;
				else
					break;
			}
		}
	}

	if (ptrWillChange && dist != NULL)
	{
		if (flags & CPXF_SETONPTR)
		{
			if (flags & CPXF_SETTARGET)	ref->target = dist;
			if (flags & CPXF_SETMASTER)	ref->master = dist;
			if (flags & CPXF_SETTRACER)	ref->tracer = dist;
		}
		else
		{
			if (flags & CPXF_SETTARGET)	self->target = dist;
			if (flags & CPXF_SETMASTER)	self->master = dist;
			if (flags & CPXF_SETTRACER)	self->tracer = dist;
		}
	}

	if (counter == count)
		result = true;
	else if (counter < count)
		result = !!((flags & CPXF_LESSOREQUAL) && !(flags & CPXF_EXACT));

	if (!jump) return;

	if (result)
	{
		ACTION_JUMP(jump);
	}
}

/*===========================================================================
A_CheckBlock
(state block, int flags, int ptr)

Checks if something is blocking the actor('s pointer) 'ptr'.

The SET pointer flags only affect the caller, not the pointer.
===========================================================================*/
enum CBF
{
	CBF_NOLINES			= 1 << 0,	//Don't check actors.
	CBF_SETTARGET		= 1 << 1,	//Sets the caller/pointer's target to the actor blocking it. Actors only.
	CBF_SETMASTER		= 1 << 2,	//^ but with master.
	CBF_SETTRACER		= 1 << 3,	//^ but with tracer.
	CBF_SETONPTR		= 1 << 4,	//Sets the pointer change on the actor doing the checking instead of self.
	CBF_DROPOFF			= 1 << 5,	//Check for dropoffs.
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckBlock)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STATE(block, 0);
	ACTION_PARAM_INT(flags, 1);
	ACTION_PARAM_INT(ptr, 2);

	AActor *mobj = COPY_AAPTR(self, ptr);

	ACTION_SET_RESULT(false);
	//Needs at least one state jump to work. 
	if (!mobj)
	{
		return;
	}

	//Nothing to block it so skip the rest.
	bool checker = (flags & CBF_DROPOFF) ? P_CheckMove(mobj, mobj->X(), mobj->Y()) : P_TestMobjLocation(mobj);
	if (checker) return;

	if (mobj->BlockingMobj)
	{
		AActor *setter = (flags & CBF_SETONPTR) ? mobj : self;
		if (setter)
		{
			if (flags & CBF_SETTARGET)	setter->target = mobj->BlockingMobj;
			if (flags & CBF_SETMASTER)	setter->master = mobj->BlockingMobj;
			if (flags & CBF_SETTRACER)	setter->tracer = mobj->BlockingMobj;
		}
	}

	//[MC] If modders don't want jumping, but just getting the pointer, only abort at
	//this point. I.e. A_CheckBlock("",CBF_SETTRACER) is like having CBF_NOLINES.
	//It gets the mobj blocking, if any, and doesn't jump at all.
	if (!block)
		return;

	//[MC] Easiest way to tell if an actor is blocking it, use the pointers.
	if (mobj->BlockingMobj || (!(flags & CBF_NOLINES) && mobj->BlockingLine != NULL))
	{
		ACTION_JUMP(block);
	}
}

//===========================================================================
//
// A_FaceMovementDirection(angle offset, bool pitch, ptr)
//
// Sets the actor('s pointer) to face the direction of travel.
//===========================================================================
enum FMDFlags
{
	FMDF_NOPITCH =			1 << 0,
	FMDF_INTERPOLATE =		1 << 1,
	FMDF_NOANGLE =			1 << 2,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FaceMovementDirection)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_ANGLE(offset, 0);
	ACTION_PARAM_ANGLE(anglelimit, 1);
	ACTION_PARAM_ANGLE(pitchlimit, 2);
	ACTION_PARAM_INT(flags, 3);
	ACTION_PARAM_INT(ptr, 4);

	AActor *mobj = COPY_AAPTR(self, ptr);

	//Need an actor.
	if (!mobj || ((flags & FMDF_NOPITCH) && (flags & FMDF_NOANGLE)))
	{
		ACTION_SET_RESULT(false);
		return;
	}
	
	//Don't bother calculating this if we don't have any horizontal movement.
	if (!(flags & FMDF_NOANGLE) && (mobj->velx != 0 || mobj->vely != 0))
	{
		angle_t current = mobj->angle; 
		const angle_t angle = R_PointToAngle2(0, 0, mobj->velx, mobj->vely);
		//Done because using anglelimit directly causes a signed/unsigned mismatch.
		const angle_t limit = anglelimit; 

		//Code borrowed from A_Face*.
		if (limit > 0 && (absangle(current - angle) > limit))
		{
			if (current < angle)
			{
				// [MC] This may appear backwards, but I assure any who
				// reads this, it works.
				if (current - angle > ANGLE_180)
					current += limit + offset;
				else
					current -= limit + offset;
				mobj->SetAngle(current, !!(flags & FMDF_INTERPOLATE));
			}
			else if (current > angle)
			{
				if (angle - current > ANGLE_180)
					current -= limit + offset;
				else
					current += limit + offset;
				mobj->SetAngle(current, !!(flags & FMDF_INTERPOLATE));
			}
			else
				mobj->SetAngle(angle + ANGLE_180 + offset, !!(flags & FMDF_INTERPOLATE));
			
		}
		else
			mobj->SetAngle(angle + offset, !!(flags & FMDF_INTERPOLATE));
	}

	if (!(flags & FMDF_NOPITCH))
	{
		fixed_t current = mobj->pitch;
		const FVector2 velocity(mobj->velx, mobj->vely);
		const fixed_t pitch = R_PointToAngle2(0, 0, (fixed_t)velocity.Length(), -mobj->velz);
		if (pitchlimit > 0)
		{
			// [MC] angle_t for pitchlimit was required because otherwise
			// we would wind up with less than desirable turn rates that didn't
			// match that of A_SetPitch. We want consistency. Also, I didn't know
			// of a better way to convert from angle_t to fixed_t properly so I
			// used this instead.
			fixed_t plimit = fixed_t(pitchlimit);
			
			if (abs(current - pitch) > plimit)
			{
				fixed_t max = 0;
				
				if (current > pitch)
				{
					max = MIN(plimit, (current - pitch));
					current -= max;
				}
				else //if (current > pitch)
				{
					max = MIN(plimit, (pitch - current));
					current += max;
				}
				mobj->SetPitch(current, !!(flags & FMDF_INTERPOLATE));
			}
			else
			{
				mobj->SetPitch(pitch, !!(flags & FMDF_INTERPOLATE));
			}
			
		}
		else
		{
			mobj->SetPitch(pitch, !!(flags & FMDF_INTERPOLATE));
		}
	}
}

