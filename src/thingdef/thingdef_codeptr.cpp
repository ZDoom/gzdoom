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

bool ACustomInventory::CallStateChain (AActor *actor, FState *state)
{
	INTBOOL result = false;
	int counter = 0;
	int retval, numret;
	VMReturn ret;
	ret.IntAt(&retval);
	VMValue params[3] = { actor, this, 0 };

	this->flags5 |= MF5_INSTATECALL;
	FState *savedstate = this->state;

	while (state != NULL)
	{
		this->state = state;

		if (state->ActionFunc != NULL)
		{
			VMFrameStack stack;

			params[2] = VMValue(state, ATAG_STATE);
			retval = true;	// assume success
			numret = stack.Call(state->ActionFunc, params, countof(params), &ret, 1);
			// As long as even one state succeeds, the whole chain succeeds unless aborted below.
			result |= retval;
		}

		// Since there are no delays it is a good idea to check for infinite loops here!
		counter++;
		if (counter >= 10000)	break;

		if (this->state == state) 
		{
			FState *next = state->GetNextState();

			if (state == next) 
			{ // Abort immediately if the state jumps to itself!
				result = false;
				break;
			}
			
			// If both variables are still the same there was no jump
			// so we must advance to the next state.
			state = next;
		}
		else 
		{
			state = this->state;
		}
	}
	this->flags5 &= ~MF5_INSTATECALL;
	this->state = savedstate;
	return !!result;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(ptr_target);
	PARAM_INT_OPT	(ptr_master)		{ ptr_master = AAPTR_DEFAULT; }
	PARAM_INT_OPT	(ptr_tracer)		{ ptr_tracer = AAPTR_TRACER; }
	PARAM_INT_OPT	(flags)				{ flags = 0; }

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
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(ptr_source);
	PARAM_INT		(ptr_recipient);
	PARAM_INT		(ptr_sourcefield);
	PARAM_INT_OPT	(ptr_recipientfield)	{ ptr_recipientfield = AAPTR_DEFAULT; }
	PARAM_INT_OPT	(flags)					{ flags = 0; }

	AActor *source, *recipient;

	// Exchange pointers with actors to whom you have pointers (or with yourself, if you must)
	source = COPY_AAPTR(self, ptr_source);
	recipient = COPY_AAPTR(self, ptr_recipient);	// pick an actor to store the provided pointer value
	if (recipient == NULL)
	{
		return 0;
	}

	// convert source from dataprovider to data
	source = COPY_AAPTR(source, ptr_sourcefield);
	if (source == recipient)
	{ // The recepient should not acquire a pointer to itself; will write NULL}
		source = NULL;
	}
	if (ptr_recipientfield == AAPTR_DEFAULT)
	{ // If default: Write to same field as data was read from
		ptr_recipientfield = ptr_sourcefield;
	}
	ASSIGN_AAPTR(recipient, ptr_recipientfield, source, flags);
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(ptr_source)	{ ptr_source = AAPTR_MASTER; }
	
	if (self->player != NULL)
	{
		return 0;
	}

	AActor *source = COPY_AAPTR(self, ptr_source);
	if (source != NULL)
	{ // No change in current target or health
		self->CopyFriendliness(source, false, false);
	}
	return 0;
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
					  int MeleeDamage, FSoundID MeleeSound, PClassActor *MissileType,fixed_t MissileHeight)
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
	PARAM_ACTION_PROLOGUE;
	int MeleeDamage = self->GetClass()->MeleeDamage;
	FSoundID MeleeSound = self->GetClass()->MeleeSound;
	DoAttack(self, true, false, MeleeDamage, MeleeSound, NULL, 0);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_MissileAttack)
{
	PARAM_ACTION_PROLOGUE;
	PClassActor *MissileType = PClass::FindActor(self->GetClass()->MissileName);
	fixed_t MissileHeight = self->GetClass()->MissileHeight;
	DoAttack(self, false, true, 0, 0, MissileType, MissileHeight);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ComboAttack)
{
	PARAM_ACTION_PROLOGUE;
	int MeleeDamage = self->GetClass()->MeleeDamage;
	FSoundID MeleeSound = self->GetClass()->MeleeSound;
	PClassActor *MissileType = PClass::FindActor(self->GetClass()->MissileName);
	fixed_t MissileHeight = self->GetClass()->MissileHeight;
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
		S_Sound (self, channel, soundid, (float)volume, (float)attenuation);
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, channel&7, soundid))
		{
			S_Sound (self, channel | CHAN_LOOP, soundid, (float)volume, (float)attenuation);
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
static FRandom pr_seekermissile ("SeekerMissile");
enum
{
	SMF_LOOK = 1,
	SMF_PRECISE = 2,
	SMF_CURSPEED = 4,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SeekerMissile)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(ang1);
	PARAM_INT(ang2);
	PARAM_INT_OPT(flags)	{ flags = 0; }
	PARAM_INT_OPT(chance)	{ chance = 50; }
	PARAM_INT_OPT(distance)	{ distance = 10; }

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
			NAME_Hitscan, NAME_BulletPuff);
    }
	return 0;
}


//==========================================================================
//
// Do the state jump
//
//==========================================================================
static void DoJump(AActor *self, AActor *stateowner, FState *callingstate, FState *jumpto)
{
	if (jumpto == NULL) return;

	if (stateowner->flags5 & MF5_INSTATECALL)
	{
		stateowner->state = jumpto;
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
		// Rather than using self->SetState(jumpto) to set the state,
		// set the state directly. Since this function is only called by
		// action functions, which are only called by SetState(), we
		// know that somewhere above us in the stack, a SetState()
		// call is waiting for us to return. We use the flag OF_StateChanged
		// to cause it to bypass the normal next state mechanism and use
		// the one we set here instead.
		self->state = jumpto;
		self->ObjectFlags |= OF_StateChanged;
	}
	else
	{ // something went very wrong. This should never happen.
		assert(false);
	}
}

// This is just to avoid having to directly reference the internally defined
// CallingState and statecall parameters in the code below.
#define ACTION_JUMP(offset) DoJump(self, stateowner, callingstate, offset)

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Jump)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT(maxchance) { maxchance = 256; }

	paramnum++;		// Increment paramnum to point at the first jump target
	int count = numparam - paramnum;
	if (count > 0 && (maxchance >= 256 || pr_cajump() < maxchance))
	{
		int jumpnum = (count == 1 ? 0 : (pr_cajump() % count));
		PARAM_STATE_AT(paramnum + jumpnum, jumpto);
		ACTION_JUMP(jumpto);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	return numret;
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
	return numret;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetOutsideMeleeRange)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	if (!self->CheckMeleeRange())
	{
		ACTION_JUMP(jump);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	return numret;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetInsideMeleeRange)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE(jump);

	if (self->CheckMeleeRange())
	{
		ACTION_JUMP(jump);
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	return numret;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
static int DoJumpIfCloser(AActor *target, VM_ARGS)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED	(dist);
	PARAM_STATE	(jump);

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
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfCloser)
{
	PARAM_ACTION_PROLOGUE;

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
	return DoJumpIfCloser(target, VM_ARGS_NAMES);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTracerCloser)
{
	PARAM_ACTION_PROLOGUE;
	return DoJumpIfCloser(self->tracer, VM_ARGS_NAMES);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfMasterCloser)
{
	PARAM_ACTION_PROLOGUE;
	return DoJumpIfCloser(self->master, VM_ARGS_NAMES);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
int DoJumpIfInventory(AActor *owner, AActor *self, AActor *stateowner, FState *callingstate, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	int paramnum = NAP-1;
	PARAM_CLASS		(itemtype, AInventory);
	PARAM_INT		(itemamount);
	PARAM_STATE		(label);
	PARAM_INT_OPT	(setowner) { setowner = AAPTR_DEFAULT; }

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (itemtype == NULL)
	{
		return numret;
	}
	owner = COPY_AAPTR(owner, setowner);
	if (owner == NULL)
	{
		return numret;
	}

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
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInventory)
{
	PARAM_ACTION_PROLOGUE;
	return DoJumpIfInventory(self, self, stateowner, callingstate, param, numparam, ret, numret);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetInventory)
{
	PARAM_ACTION_PROLOGUE;
	return DoJumpIfInventory(self->target, self, stateowner, callingstate, param, numparam, ret, numret);
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
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(damage)		   { damage = -1; }
	PARAM_INT_OPT	(distance)		   { distance = -1; }
	PARAM_INT_OPT	(flags)			   { flags = XF_HURTSOURCE; }
	PARAM_BOOL_OPT	(alert)			   { alert = false; }
	PARAM_INT_OPT	(fulldmgdistance)  { fulldmgdistance = 0; }
	PARAM_INT_OPT	(nails)			   { nails = 0; }
	PARAM_INT_OPT	(naildamage)	   { naildamage = 10; }
	PARAM_CLASS_OPT	(pufftype, AActor) { pufftype = PClass::FindActor("BulletPuff"); }

	if (damage < 0)	// get parameters from metadata
	{
		damage = self->GetClass()->ExplosionDamage;
		distance = self->GetClass()->ExplosionRadius;
		flags = !self->GetClass()->DontHurtShooter;
		alert = false;
	}
	if (distance <= 0) distance = damage;

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
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(force)					{ force = 128; }
	PARAM_INT_OPT	(distance)				{ distance = -1; }
	PARAM_INT_OPT	(flags)					{ flags = RTF_AFFECTSOURCE; }
	PARAM_INT_OPT	(fullthrustdistance)	{ fullthrustdistance = 0; }

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

	bool res = !!P_ExecuteSpecial(special, NULL, self, false, arg1, arg2, arg3, arg4, arg5);

	ACTION_SET_RESULT(res);
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(ti, AActor);
	PARAM_FIXED_OPT	(spawnheight) { spawnheight = 32*FRACUNIT; }
	PARAM_INT_OPT	(spawnofs_xy) { spawnofs_xy = 0; }
	PARAM_ANGLE_OPT	(angle)		  { angle = 0; }
	PARAM_INT_OPT	(flags)		  { flags = 0; }
	PARAM_ANGLE_OPT	(pitch)		  { pitch = 0; }

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
			fixed_t z = spawnheight + self->GetBobOffset() - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

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
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z + self->GetBobOffset() + spawnheight, self, self->target, ti, false);
				break;

			case 2:
				self->x += x;
				self->y += y;
				missile = P_SpawnMissileAngleZSpeed(self, self->z + self->GetBobOffset() + spawnheight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
 				self->x -= x;
				self->y -= y;

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
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_OPT	(pufftype, AActor) { pufftype = PClass::FindActor(NAME_BulletPuff); }
	PARAM_FIXED_OPT	(range)			   { range = MISSILERANGE; }
	PARAM_INT_OPT	(flags)			   { flags = 0; }

	if (range == 0)
		range = MISSILERANGE;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (flags & CBAF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if (self->target || (flags & CBAF_AIMFACING))
	{
		if (!(flags & CBAF_AIMFACING)) A_FaceTarget (self);
		bangle = self->angle;

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
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
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
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z += spawnheight + self->GetBobOffset() - 32*FRACUNIT;
		AActor *missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
		self->z -= spawnheight + self->GetBobOffset() - 32*FRACUNIT;

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
		return numret;

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
	{
		ACTION_JUMP(jump);
	}
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }
	PARAM_INT_OPT	(flags)				{ flags = FBF_USEAMMO; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }

	if (!self->player) return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;

	int i;
	int bangle;
	int bslope = 0;
	int laflags = (flags & FBF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if ((flags & FBF_USEAMMO) && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}
	
	if (range == 0)
		range = PLAYERMISSILERANGE;

	if (!(flags & FBF_NOFLASH)) static_cast<APlayerPawn *>(self)->PlayAttacking2 ();

	if (!(flags & FBF_NOPITCH)) bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (pufftype == NULL)
		pufftype = PClass::FindActor(NAME_BulletPuff);

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

enum
{
	CPF_USEAMMO = 1,
	CPF_DAGGER = 2,
	CPF_PULLIN = 4,
	CPF_NORANDOMPUFFZ = 8,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomPunch)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(damage);
	PARAM_BOOL_OPT	(norandom)			{ norandom = false; }
	PARAM_INT_OPT	(flags)				{ flags = CPF_USEAMMO; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }
	PARAM_FIXED_OPT	(lifesteal)			{ lifesteal = 0; }

	if (!self->player)
		return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;


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
	if ((flags & CPF_USEAMMO) && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	if (pufftype == NULL)
		pufftype = PClass::FindActor(NAME_BulletPuff);
	int puffFlags = LAF_ISMELEEATTACK | (flags & CPF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	P_LineAttack (self, angle, range, pitch, damage, NAME_Melee, pufftype, puffFlags, &linetarget, &actualdamage);

	// turn to face target
	if (linetarget)
	{
		if (lifesteal && !(linetarget->flags5 & MF5_DONTDRAIN))
			P_GiveBody (self, (actualdamage * lifesteal) >> FRACBITS);

		if (weapon != NULL)
		{
			S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
		}

		self->angle = R_PointToAngle2 (self->x, self->y, linetarget->x, linetarget->y);

		if (flags & CPF_PULLIN) self->flags |= MF_JUSTATTACKED;
		if (flags & CPF_DAGGER) P_DaggerAlert (self, linetarget);

	}
	return 0;
}


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
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = PClass::FindActor(NAME_BulletPuff); }
	PARAM_ANGLE_OPT	(spread_xy)			{ spread_xy = 0; }
	PARAM_ANGLE_OPT	(spread_z)			{ spread_z = 0; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }
	PARAM_INT_OPT	(duration)			{ duration = 0; }
	PARAM_FLOAT_OPT	(sparsity)			{ sparsity = 1; }
	PARAM_FLOAT_OPT	(driftspeed)		{ driftspeed = 1; }
	PARAM_CLASS_OPT	(spawnclass, AActor){ spawnclass = NULL; }
	PARAM_FIXED_OPT	(spawnofs_z)		{ spawnofs_z = 0; }
	
	if (range == 0) range = 8192*FRACUNIT;
	if (sparsity == 0) sparsity=1.0;

	if (self->player == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (useammo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
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

	P_RailAttack (self, damage, spawnofs_xy, spawnofs_z, color1, color2, maxdiff, flags, pufftype, angle, slope, range, duration, sparsity, driftspeed, spawnclass);
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
	CRF_AIMDIRECT = 2,
	CRF_EXPLICITANGLE = 4,
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
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = PClass::FindActor(NAME_BulletPuff); }
	PARAM_ANGLE_OPT	(spread_xy)			{ spread_xy = 0; }
	PARAM_ANGLE_OPT	(spread_z)			{ spread_z = 0; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }
	PARAM_INT_OPT	(duration)			{ duration = 0; }
	PARAM_FLOAT_OPT	(sparsity)			{ sparsity = 1; }
	PARAM_FLOAT_OPT	(driftspeed)		{ driftspeed = 1; }
	PARAM_CLASS_OPT	(spawnclass, AActor){ spawnclass = NULL; }
	PARAM_FIXED_OPT	(spawnofs_z)		{ spawnofs_z = 0; }

	if (range == 0) range = 8192*FRACUNIT;
	if (sparsity == 0) sparsity = 1;

	AActor *linetarget;

	fixed_t saved_x = self->x;
	fixed_t saved_y = self->y;
	angle_t saved_angle = self->angle;
	fixed_t saved_pitch = self->pitch;

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

	P_RailAttack (self, damage, spawnofs_xy, spawnofs_z, color1, color2, maxdiff, flags, pufftype, angleoffset, slopeoffset, range, duration, sparsity, driftspeed, spawnclass);

	self->x = saved_x;
	self->y = saved_y;
	self->angle = saved_angle;
	self->pitch = saved_pitch;
	return 0;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static int DoGiveInventory(AActor *receiver, VM_ARGS)
{
	int paramnum = NAP-1;
	PARAM_CLASS		(mi, AInventory);
	PARAM_INT_OPT	(amount)			{ amount = 1; }
	PARAM_INT_OPT	(setreceiver)		{ setreceiver = AAPTR_DEFAULT; }

	receiver = COPY_AAPTR(receiver, setreceiver);
	if (receiver == NULL)
	{ // If there's nothing to receive it, it's obviously a fail, right?
		ACTION_SET_RESULT(false);
		return numret;
	}

	bool res = true;
	
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
		item->ClearCounters();
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
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveInventory)
{
	PARAM_ACTION_PROLOGUE;
	return DoGiveInventory(self, VM_ARGS_NAMES);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToTarget)
{
	PARAM_ACTION_PROLOGUE;
	return DoGiveInventory(self->target, VM_ARGS_NAMES);
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

int DoTakeInventory(AActor *receiver, VM_ARGS)
{
	int paramnum = NAP-1;
	PARAM_CLASS		(itemtype, AInventory);
	PARAM_INT_OPT	(amount)		{ amount = 0; }
	PARAM_INT_OPT	(flags)			{ flags = 0; }
	PARAM_INT_OPT	(setreceiver)	{ setreceiver = AAPTR_DEFAULT; }
	
	if (itemtype == NULL)
	{
		ACTION_SET_RESULT(true);
		return numret;
	}
	receiver = COPY_AAPTR(receiver, setreceiver);
	if (receiver == NULL)
	{
		ACTION_SET_RESULT(false);
		return numret;
	}

	bool res = false;

	AInventory *inv = receiver->FindInventory(itemtype);

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
		else
		{
			inv->Amount -= amount;
		}
	}
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeInventory)
{
	PARAM_ACTION_PROLOGUE;
	return DoTakeInventory(self, VM_ARGS_NAMES);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromTarget)
{
	PARAM_ACTION_PROLOGUE;
	return DoTakeInventory(self->target, VM_ARGS_NAMES);
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
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT	(missile, AActor)		{ missile = PClass::FindActor("Unknown"); }
	PARAM_FIXED_OPT	(distance)				{ distance = 0; }
	PARAM_FIXED_OPT	(zheight)				{ zheight = 0; }
	PARAM_BOOL_OPT	(useammo)				{ useammo = true; }
	PARAM_BOOL_OPT	(transfer_translation)	{ transfer_translation = false; }

	if (missile == NULL)
	{
		ACTION_SET_RESULT(false);
		return numret;
	}

	ACTION_SET_RESULT(true);
	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
	{
		return numret;
	}

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
			return numret;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
			return numret;
	}

	AActor *mo = Spawn(missile, 
					self->x + FixedMul(distance, finecosine[self->angle>>ANGLETOFINESHIFT]), 
					self->y + FixedMul(distance, finesine[self->angle>>ANGLETOFINESHIFT]), 
					self->z - self->floorclip + self->GetBobOffset() + zheight, ALLOW_REPLACE);

	int flags = (transfer_translation ? SIXF_TRANSFERTRANSLATION : 0) + (useammo ? SIXF_SETMASTER : 0);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	return numret;
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
	PARAM_INT_OPT	(tid)		{ tid = 0; }

	if (missile == NULL) 
	{
		ACTION_SET_RESULT(false);
		return numret;
	}

	ACTION_SET_RESULT(true);
	if (chance > 0 && pr_spawnitemex() < chance)
		return numret;

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
		return numret;

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
		mo->angle = angle;
	}
	return numret;
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

	ACTION_SET_RESULT(true);
	if (missile == NULL)
		return numret;

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (weapon == NULL)
			return numret;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
			return numret;
	}

	AActor *bo;

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
	else
	{
		ACTION_SET_RESULT(false);
	}
	return numret;
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
		return numret;
	}

	AWeapon *weaponitem = static_cast<AWeapon*>(self->FindInventory(cls));

	if (weaponitem != NULL && weaponitem->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{
		if (self->player->ReadyWeapon != weaponitem)
		{
			self->player->PendingWeapon = weaponitem;
		}
		ACTION_SET_RESULT(true);
	}
	else
	{
		ACTION_SET_RESULT(false);
	}
	return numret;
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

	if (text[0] == '$') text = GStrings(&text[1]);
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
		FString formatted = strbin1(text);
		C_MidPrint(font != NULL ? font : SmallFont, formatted.GetChars());
		con_midtime = saved;
	}
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
	return numret;
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
	
	if (text[0] == '$') text = GStrings(&text[1]);
	if (fontname != NAME_None)
	{
		font = V_GetFont(fontname);
	}
	if (time > 0)
	{
		con_midtime = float(time);
	}
	FString formatted = strbin1(text);
	C_MidPrintBold(font != NULL ? font : SmallFont, formatted.GetChars());
	con_midtime = saved;
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
	return numret;
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

	if (text[0] == '$') text = GStrings(&text[1]);
	Printf("%s\n", text);
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
	return numret;
}

//=========================================================================
//
// A_LogInt
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LogInt)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(num);
	Printf("%d\n", num);
	ACTION_SET_RESULT(false);	// Prints should never set the result for inventory state chains!
	return numret;
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
	{
		reduce = FRACUNIT/10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;
	// Should this clamp alpha to 1.0?
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
	{
		reduce = FRACUNIT/10;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;
	if (self->alpha <= 0 && remove)
	{
		self->Destroy();
	}
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED		(target);
	PARAM_FIXED_OPT	(amount)		{ amount = fixed_t(0.1*FRACUNIT); }
	PARAM_BOOL_OPT	(remove)		{ remove = false; }

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
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED		(scalex);
	PARAM_FIXED_OPT	(scaley)	{ scaley = scalex; }

	self->scaleX = scalex;
	self->scaleY = scaley;
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(mass);

	self->Mass = mass;
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
			self->y + ((pr_spawndebris()-128)<<12), 
			self->z + (pr_spawndebris()*self->height/256+self->GetBobOffset()), ALLOW_REPLACE);
		if (mo)
		{
			if (transfer_translation)
			{
				mo->Translation = self->Translation;
			}
			if (i < mo->GetClass()->NumOwnedStates)
			{
				mo->SetState (mo->GetClass()->OwnedStates + i);
			}
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
		if (playeringame[i])
		{
			// Always check sight from each player.
			if (P_CheckSight(players[i].mo, self, SF_IGNOREVISIBILITY))
			{
				return numret;
			}
			// If a player is viewing from a non-player, then check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				P_CheckSight(players[i].camera, self, SF_IGNOREVISIBILITY))
			{
				return numret;
			}
		}
	}

	ACTION_JUMP(jump);
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_FLOAT(range);
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckSightOrRange(self, players[i].mo, range))
			{
				return numret;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckSightOrRange(self, players[i].camera, range))
			{
				return numret;
			}
		}
	}
	ACTION_JUMP(jump);
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_FLOAT(range);
	PARAM_STATE(jump);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckRange(self, players[i].mo, range))
			{
				return numret;
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckRange(self, players[i].camera, range))
			{
				return numret;
			}
		}
	}
	ACTION_JUMP(jump);
	return numret;
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
	return numret;
}

//===========================================================================
//
// A_KillMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillMaster)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME_OPT(damagetype)	{ damagetype = NAME_None; }

	if (self->master != NULL)
	{
		P_DamageMobj(self->master, self, self, self->master->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
	}
	return 0;
}

//===========================================================================
//
// A_KillChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillChildren)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME_OPT(damagetype)	{ damagetype = NAME_None; }

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			P_DamageMobj(mo, self, self, mo->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
		}
	}
	return 0;
}

//===========================================================================
//
// A_KillSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillSiblings)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME_OPT(damagetype)	{ damagetype = NAME_None; }

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
	PARAM_STATE_OPT(state)	{ state = self->FindState(NAME_Death); }

	if (argnum > 0 && argnum < (int)countof(self->args))
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
		else
		{
			self->SetState(state);
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
	AActor * mo;

	if (chunk == NULL)
	{
		return 0;
	}

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
	return numret;
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
	return numret;
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

DECLARE_ACTION(A_RestoreSpecialPosition)

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
		skins[self->player->userinfo.GetSkin()].othergame)
	{
		ACTION_JUMP(jump);
	}
	return numret;
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

	PARAM_ACTION_PROLOGUE;
	PARAM_STATE		(jump);
	PARAM_INT_OPT	(flags)			{ flags = 0; }
	PARAM_FIXED_OPT	(range)			{ range = 0; }
	PARAM_FIXED_OPT	(minrange)		{ minrange = 0; }
	{
		PARAM_ANGLE_OPT	(angle)			{ angle = 0; }
		PARAM_ANGLE_OPT	(pitch)			{ pitch = 0; }
		PARAM_FIXED_OPT	(offsetheight)	{ offsetheight = 0; }
		PARAM_FIXED_OPT	(offsetwidth)	{ offsetwidth = 0; }
		PARAM_INT_OPT	(ptr_target)	{ ptr_target = AAPTR_DEFAULT; }

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
				if (distance > range)
					return numret;
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
		else
		{
			return numret;
		}

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
			return numret;
		}
		ACTION_JUMP(jump);
	}
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE		(jump);
	PARAM_ANGLE_OPT	(fov)			{ fov = 0; }
	PARAM_INT_OPT	(flags)			{ flags = 0; }
	PARAM_FIXED_OPT	(dist_max)		{ dist_max = 0; }
	PARAM_FIXED_OPT	(dist_close)	{ dist_close = 0; }

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

		if (target == NULL)
			return numret; // [KS] Let's not call P_CheckSight unnecessarily in this case.
		
		if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0))
		{
			return numret;
		}

		doCheckSight = !(flags & JLOSF_NOSIGHT);
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_AimLineAttack(self, self->angle, MISSILERANGE, &target, (flags & JLOSF_NOAUTOAIM) ? ANGLE_1/2 : 0);
		
		if (!target) return numret;

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
	if ( (flags & JLOSF_COMBATANTONLY) && (!target->player) && !(target->flags3 & MF3_ISMONSTER))
		return numret;

	// [FDARI] If actors share team, don't jump
	if ((flags & JLOSF_ALLYNOJUMP) && self->IsFriend(target))
		return numret;

	fixed_t distance = P_AproxDistance(target->x - self->x, target->y - self->y);
	distance = P_AproxDistance(distance, target->z - self->z);

	if (dist_max && (distance > dist_max))
		return numret;

	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
			return numret;

		if (flags & JLOSF_CLOSENOFOV)
			fov = 0;

		if (flags & JLOSF_CLOSENOSIGHT)
			doCheckSight = false;
	}

	if (flags & JLOSF_TARGETLOS) { viewport = target; target = self; }
	else { viewport = self; }

	if (doCheckSight && !P_CheckSight (viewport, target, SF_IGNOREVISIBILITY))
		return numret;

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
			return numret; // [KS] Outside of FOV - return
		}

	}

	ACTION_JUMP(jump);
	return numret;
}


//==========================================================================
//
// A_JumpIfInTargetLOS (state label, optional fixed fov, optional int flags
// optional fixed dist_max, optional fixed dist_close)
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetLOS)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE		(jump);
	PARAM_ANGLE_OPT	(fov)			{ fov = 0; }
	PARAM_INT_OPT	(flags)			{ flags = 0; }
	PARAM_FIXED_OPT	(dist_max)		{ dist_max = 0; }
	PARAM_FIXED_OPT	(dist_close)	{ dist_close = 0; }

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

	if (target == NULL)
	{ // [KS] Let's not call P_CheckSight unnecessarily in this case.
		return numret;
	}

	if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0))
	{
		return numret;
	}

	fixed_t distance = P_AproxDistance(target->x - self->x, target->y - self->y);
	distance = P_AproxDistance(distance, target->z - self->z);

	if (dist_max && (distance > dist_max))
	{
		return numret;
	}

	bool doCheckSight = !(flags & JLOSF_NOSIGHT);

	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
			return numret;

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
			return numret; // [KS] Outside of FOV - return
		}
	}
	if (doCheckSight && !P_CheckSight (target, self, SF_IGNOREVISIBILITY))
		return numret;

	ACTION_JUMP(jump);
	return numret;
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

	if (self->master != NULL)
	{
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
	PClassActor *cls = self->GetClass();

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
		INTBOOL secret_before, secret_after;

		kill_before = self->CountsAsKill();
		item_before = self->flags & MF_COUNTITEM;
		secret_before = self->flags5 & MF5_COUNTSECRET;

		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags(self, cls, value, fd->flagbit);
		}
		else
		{
			DWORD *flagp = (DWORD*) (((char*)self) + fd->structoffset);

			// If these 2 flags get changed we need to update the blockmap and sector links.
			bool linkchange = flagp == &self->flags && (fd->flagbit == MF_NOBLOCKMAP || fd->flagbit == MF_NOSECTOR);

			if (linkchange) self->UnlinkFromWorld();
			ModActorFlag(self, fd, value);
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
		Printf("Unknown flag '%s' in '%s'\n", flagname.GetChars(), cls->TypeName.GetChars());
	}
	return 0;
}

//===========================================================================
//
// A_CheckFlag
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckFlag)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_STRING	(flagname);
	PARAM_STATE		(jumpto);
	PARAM_INT_OPT	(checkpointer)	{ checkpointer = AAPTR_DEFAULT; }

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	AActor *owner = COPY_AAPTR(self, checkpointer);
	if (owner == NULL)
	{
		return numret;
	}

	if (CheckActorFlag(owner, flagname))
	{
		ACTION_JUMP(jumpto);
	}
	return numret;
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

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self && (mo->health <= 0 || removeall))
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

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self)
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
		return numret;

	if (self->target == NULL
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES) )
	{
		ACTION_JUMP(jump);
	}
	return numret;
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

enum
{
	SPF_FORCECLAMP = 1,	// players always clamp
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetPitch)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE(pitch);
	PARAM_INT_OPT(flags)	{ flags = 0; }

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
// A_SetUserVar
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserVar)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME	(varname);
	PARAM_INT	(value);

	PSymbolVariable *var = dyn_cast<PSymbolVariable>(self->GetClass()->Symbols.FindSymbol(varname, true));

	if (var == NULL || !var->bUserVar || var->ValueType.Type != VAL_Int)
	{
		Printf("%s is not a user variable in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return 0;
	}
	// Set the value of the specified user variable.
	*(int *)(reinterpret_cast<BYTE *>(self) + var->offset) = value;
	return 0;
}

//===========================================================================
//
// A_SetUserArray
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserArray)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME	(varname);
	PARAM_INT	(pos);
	PARAM_INT	(value);

	PSymbolVariable *var = dyn_cast<PSymbolVariable>(self->GetClass()->Symbols.FindSymbol(varname, true));

	if (var == NULL || !var->bUserVar || var->ValueType.Type != VAL_Array || var->ValueType.BaseType != VAL_Int)
	{
		Printf("%s is not a user array in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return 0;
	}
	if (pos < 0 || pos >= var->ValueType.size)
	{
		Printf("%d is out of bounds in array %s in class %s\n", pos, varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return 0;
	}
	// Set the value of the specified user array at index pos.
	((int *)(reinterpret_cast<BYTE *>(self) + var->offset))[pos] = value;
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_STATE_OPT		(teleport_state)			{ teleport_state = NULL; }
	PARAM_CLASS_OPT		(target_type, ASpecialSpot)	{ target_type = PClass::FindActor("BossSpot"); }
	PARAM_CLASS_OPT		(fog_type, AActor)			{ fog_type = PClass::FindActor("TeleportFog"); }
	PARAM_INT_OPT		(flags)						{ flags = 0; }
	PARAM_FIXED_OPT		(mindist)					{ mindist = 128 << FRACBITS; }
	PARAM_FIXED_OPT		(maxdist)					{ maxdist = 128 << FRACBITS; }

	ACTION_SET_RESULT(true);

	// Randomly choose not to teleport like A_Srcr2Decide.
	if (flags & TF_RANDOMDECIDE)
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

		if (pr_teleport() >= chance[chanceindex])
		{
			return numret;
		}
	}

	if (teleport_state == NULL)
	{
		// Default to Teleport.
		teleport_state = self->FindState("Teleport");
		// If still nothing, then return.
		if (teleport_state == NULL)
		{
			return numret;
		}
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL)
	{
		return numret;
	}

	if (target_type == NULL)
	{
		target_type = PClass::FindActor("BossSpot");
	}

	AActor *spot = state->GetSpotWithMinMaxDistance(target_type, self->x, self->y, mindist, maxdist);
	if (spot == NULL)
	{
		return numret;
	}

	fixed_t prevX = self->x;
	fixed_t prevY = self->y;
	fixed_t prevZ = self->z;
	if (P_TeleportMove (self, spot->x, spot->y, spot->z, flags & TF_TELEFRAG))
	{
		ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

		if (fog_type != NULL)
		{
			Spawn(fog_type, prevX, prevY, prevZ, ALLOW_REPLACE);
		}

		ACTION_JUMP(teleport_state);

		self->z = self->floorz;
		self->angle = spot->angle;
		self->velx = self->vely = self->velz = 0;
	}
	return numret;
}

//===========================================================================
//
// A_Turn
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Turn)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_ANGLE_OPT(angle) { angle = 0; }
	self->angle += angle;
	return 0;
}

//===========================================================================
//
// A_Quake
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Quake)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(intensity);
	PARAM_INT		(duration);
	PARAM_INT		(damrad);
	PARAM_INT		(tremrad);
	PARAM_SOUND_OPT	(sound)	{ sound = "world/quake"; }

	P_StartQuake(self, 0, intensity, duration, damrad, tremrad, sound);
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT	(xspeed);
	PARAM_INT	(yspeed);
	PARAM_FIXED	(xdist);
	PARAM_FIXED	(ydist);
	A_Weave(self, xspeed, yspeed, xdist, ydist);
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
			res = !!P_ExecuteSpecial(junk.special, NULL, self, false, junk.args[0], 
				junk.args[1], junk.args[2], junk.args[3], junk.args[4]); 
			if (res && !(junk.flags & ML_REPEAT_SPECIAL))	// If only once,
				self->flags6 |= MF6_LINEDONE;				// no more for this thing
		}
	}
	ACTION_SET_RESULT(res);
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(flags)				{ flags = 0; }
	PARAM_SOUND_OPT	(sound)				{ sound = "weapons/pistol"; }
	PARAM_FIXED_OPT	(snipe)				{ snipe = FRACUNIT; }
	PARAM_INT_OPT	(maxdamage)			{ maxdamage = 64; }
	PARAM_INT_OPT	(blocksize)			{ blocksize = 128; }
	PARAM_INT_OPT	(pointblank)		{ pointblank = 2; }
	PARAM_INT_OPT	(longrange)			{ longrange = 4; }
	PARAM_FIXED_OPT	(runspeed)			{ runspeed = 160*FRACUNIT; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = PClass::FindActor("BulletPuff"); }

	if (!self->target)
		return 0;

	// Enemy can't see target
	if (!P_CheckSight(self, self->target))
		return 0;

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
			AActor *dpuff = GetDefaultByType(pufftype->GetReplacement());
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
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_INT		(destination_selector);
	PARAM_FIXED_OPT	(xofs)				{ xofs = 0; }
	PARAM_FIXED_OPT	(yofs)				{ yofs = 0; }
	PARAM_FIXED_OPT	(zofs)				{ zofs = 0; }
	PARAM_ANGLE_OPT	(angle)				{ angle = 0; }
	PARAM_INT_OPT	(flags)				{ flags = 0; }
	PARAM_STATE_OPT	(success_state)		{ success_state = NULL; }

	fixed_t

		oldx,
		oldy,
		oldz;

	AActor *reference = COPY_AAPTR(self, destination_selector);

	if (!reference)
	{
		ACTION_SET_RESULT(false);
		return numret;
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
			return numret;
		}

		ACTION_SET_RESULT(true);
	}
	else
	{
		self->SetOrigin(oldx, oldy, oldz);
		ACTION_SET_RESULT(false);
	}
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(arg1)				{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)				{ arg2 = 0; }
	PARAM_INT_OPT	(arg3)				{ arg3 = 0; }
	PARAM_INT_OPT	(arg4)				{ arg4 = 0; }

	int res = P_ExecuteSpecial(ACS_ExecuteWithResult, NULL, self, false, -scriptname, arg1, arg2, arg3, arg4);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedExecute)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }
	PARAM_INT_OPT	(arg1)				{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)				{ arg2 = 0; }
	PARAM_INT_OPT	(arg3)				{ arg3 = 0; }

	int res = P_ExecuteSpecial(ACS_Execute, NULL, self, false, -scriptname, mapnum, arg1, arg2, arg3);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedExecuteAlways)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }
	PARAM_INT_OPT	(arg1)				{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)				{ arg2 = 0; }
	PARAM_INT_OPT	(arg3)				{ arg3 = 0; }

	int res = P_ExecuteSpecial(ACS_ExecuteAlways, NULL, self, false, -scriptname, mapnum, arg1, arg2, arg3);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedLockedExecute)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }
	PARAM_INT_OPT	(arg1)				{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)				{ arg2 = 0; }
	PARAM_INT_OPT	(lock)				{ lock = 0; }

	int res = P_ExecuteSpecial(ACS_LockedExecute, NULL, self, false, -scriptname, mapnum, arg1, arg2, lock);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedLockedExecuteDoor)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }
	PARAM_INT_OPT	(arg1)				{ arg1 = 0; }
	PARAM_INT_OPT	(arg2)				{ arg2 = 0; }
	PARAM_INT_OPT	(lock)				{ lock = 0; }

	int res = P_ExecuteSpecial(ACS_LockedExecuteDoor, NULL, self, false, -scriptname, mapnum, arg1, arg2, lock);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedSuspend)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }

	int res = P_ExecuteSpecial(ACS_Suspend, NULL, self, false, -scriptname, mapnum, 0, 0, 0);
	ACTION_SET_RESULT(res);
	return numret;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, ACS_NamedTerminate)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME		(scriptname);
	PARAM_INT_OPT	(mapnum)			{ mapnum = 0; }

	int res = P_ExecuteSpecial(ACS_Terminate, NULL, self, false, -scriptname, mapnum, 0, 0, 0);
	ACTION_SET_RESULT(res);
	return numret;
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
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS		(item, AInventory);
	PARAM_FIXED		(distance);
	PARAM_INT		(flags);
	PARAM_INT_OPT	(amount)	{ amount = 0; }

	// We need a valid item, valid targets, and a valid range
	if (item == NULL || (flags & RGF_MASK) == 0 || distance <= 0)
	{
		return 0;
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
	return 0;
}


//==========================================================================
//
// A_SetTics
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTics)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(tics_to_set);

	self->tics = tics_to_set;
	return 0;
}

//==========================================================================
//
// A_SetDamageType
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetDamageType)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_NAME(damagetype);

	self->DamageType = damagetype;
	return 0;
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
