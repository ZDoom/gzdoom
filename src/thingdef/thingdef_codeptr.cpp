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
#include "c_console.h"
#include "doomerrors.h"
#include "vectors.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "thingdef/thingdef.h"
#include "v_video.h"


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


// A truly awful hack to get to the state that called an action function
// without knowing whether it has been called from a weapon or actor.
FState * CallingState;

struct StateCallData
{
	FState * State;
	AActor * Item;
	bool Result;
};

StateCallData * pStateCall;

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
	StateCallData *pSavedCall = pStateCall;
	bool result = false;
	int counter = 0;

	pStateCall = &StateCall;
	StateCall.Item = this;
	while (State != NULL)
	{
		// Assume success. The code pointer will set this to false if necessary
		CallingState = StateCall.State = State;
		if (State->GetAction() != NULL) 
		{
			StateCall.Result = true;	
			State->GetAction() (actor);
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
				pStateCall = pSavedCall;
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
	pStateCall = pSavedCall;
	return result;
}

//==========================================================================
//
// Let's isolate all handling of CallingState in this one place 
// so that removing it later becomes easier
//
//==========================================================================
int CheckIndex(int paramsize, FState ** pcallstate)
{
	if (CallingState->ParameterIndex == 0) return -1;

	unsigned int index = (unsigned int) CallingState->ParameterIndex-1;
	if (index > StateParameters.Size()-paramsize) return -1;
	if (pcallstate) *pcallstate=CallingState;
	return index;
}


//==========================================================================
//
// Simple flag changers
//
//==========================================================================
void A_SetSolid(AActor * self)
{
	self->flags |= MF_SOLID;
}

void A_UnsetSolid(AActor * self)
{
	self->flags &= ~MF_SOLID;
}

void A_SetFloat(AActor * self)
{
	self->flags |= MF_FLOAT;
}

void A_UnsetFloat(AActor * self)
{
	self->flags &= ~(MF_FLOAT|MF_INFLOAT);
}

//==========================================================================
//
// Customizable attack functions which use actor parameters.
// I think this is among the most requested stuff ever ;-)
//
//==========================================================================
static void DoAttack (AActor *self, bool domelee, bool domissile)
{
	int index=CheckIndex(4);
	int MeleeDamage;
	int MeleeSound;
	FName MissileName;
	fixed_t MissileHeight;

	if (self->target == NULL) return;

	if (index > 0)
	{
		MeleeDamage=StateParameters[index];
		MeleeSound=StateParameters[index+1];
		MissileName=(ENamedName)StateParameters[index+2];
		MissileHeight=StateParameters[index+3];
	}
	else
	{
		MeleeDamage = self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeDamage, 0);
		MeleeSound =  self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeSound, 0);
		MissileName=(ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None);
		MissileHeight= self->GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight, 32*FRACUNIT);
	}

	A_FaceTarget (self);
	if (domelee && MeleeDamage>0 && self->CheckMeleeRange ())
	{
		int damage = pr_camelee.HitDice(MeleeDamage);
		if (MeleeSound) S_SoundID (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
	}
	else if (domissile && MissileName != NAME_None)
	{
		const PClass * ti=PClass::FindClass(MissileName);
		if (ti) 
		{
			// Although there is a P_SpawnMissileZ function its
			// aiming is much too bad to be of any use
			self->z+=MissileHeight-32*FRACUNIT;
			AActor * missile = P_SpawnMissile (self, self->target, ti);
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
// This has been changed to use the parameter array instead of using the
// misc field directly so they can be used in weapon states
//
//==========================================================================
static void DoPlaySound(AActor * self, int channel)
{
	int index=CheckIndex(1);
	if (index<0) return;

	int soundid = StateParameters[index];
	S_SoundID (self, channel, soundid, 1, ATTN_NORM);
}

void A_PlaySound(AActor * self)
{
	DoPlaySound(self, CHAN_BODY);
}

void A_PlayWeaponSound(AActor * self)
{
	DoPlaySound(self, CHAN_WEAPON);
}

void A_StopSound(AActor * self)
{
	S_StopSound(self, CHAN_VOICE);
}

void A_PlaySoundEx (AActor *self)
{
	int index = CheckIndex(4);
	if (index < 0) return;

	int soundid = StateParameters[index];
	ENamedName channel = ENamedName(StateParameters[index + 1]);
	INTBOOL looping = StateParameters[index + 2];
	int attenuation_raw = EvalExpressionI(StateParameters[index + 3], self);

	int attenuation;
	switch (attenuation_raw)
	{
		case -1: attenuation=ATTN_STATIC;	break; // drop off rapidly
		default:
		case  0: attenuation=ATTN_NORM;		break; // normal
		case  1: attenuation=ATTN_NONE;		break; // full volume
		case  2: attenuation=ATTN_SURROUND;	break; // full volume surround
	}

	if (channel < NAME_Auto || channel > NAME_SoundSlot7)
	{
		channel = NAME_Auto;
	}

	if (!looping)
	{
		S_SoundID (self, channel - NAME_Auto, soundid, 1, attenuation);
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, channel - NAME_Auto, soundid))
		{
			S_LoopedSoundID (self, channel - NAME_Auto, soundid, 1, attenuation);
		}
	}
}

void A_StopSoundEx (AActor *self)
{
	int index = CheckIndex (1);
	if (index < 0) return;

	ENamedName channel = ENamedName(StateParameters[index]);

	if (channel > NAME_Auto && channel <= NAME_SoundSlot7)
	{
		S_StopSound (self, channel - NAME_Auto);
	}
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
void A_SeekerMissile(AActor * self)
{
	int index=CheckIndex(2);
	if (index<0) return;

	P_SeekerMissile(self,
		clamp<int>(EvalExpressionI (StateParameters[index], self), 0, 90) * ANGLE_1,
		clamp<int>(EvalExpressionI (StateParameters[index+1], self), 0, 90) * ANGLE_1);
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
		
	if (!self->target) return;

	A_FaceTarget (self);
	bangle = self->angle;

	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
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
// Resolves a label parameter
//
//==========================================================================

FState *P_GetState(AActor *self, FState *CallingState, int offset)
{
	if (offset == 0)
	{
		return NULL;	// 0 means 'no state'
	}
	else if (offset>0)
	{
		return CallingState + offset;
	}
	else if (self != NULL)
	{
		offset = -offset;

		FName classname = JumpParameters[offset];
		const PClass *cls;
		cls = classname==NAME_None?  RUNTIME_TYPE(self) : PClass::FindClass(classname);
		if (cls==NULL || cls->ActorInfo==NULL) return NULL;	// shouldn't happen

		int numnames = (int)JumpParameters[offset+1];
		FState *jumpto = cls->ActorInfo->FindState(numnames, &JumpParameters[offset+2]);
		if (jumpto == NULL)
		{
			const char *dot="";
			Printf("Jump target '");
			if (classname != NAME_None) Printf("%s::", classname.GetChars());
			for (int i=0;i<numnames;i++)
			{
				Printf("%s%s", dot, JumpParameters[offset+2+i].GetChars());
				dot = ".";
			}
			Printf("' not found in %s\n", self->GetClass()->TypeName.GetChars());
		}
		return jumpto;
	}
	else return NULL;
}

//==========================================================================
//
// Do the state jump
//
//==========================================================================
static void DoJump(AActor * self, FState * CallingState, int offset)
{

	if (pStateCall != NULL && CallingState == pStateCall->State)
	{
		FState *jumpto = P_GetState(pStateCall->Item, CallingState, offset);
		if (jumpto == NULL) return;
		pStateCall->State = jumpto;
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_weapon].state)
	{
		FState *jumpto = P_GetState(self->player->ReadyWeapon, CallingState, offset);
		if (jumpto == NULL) return;
		P_SetPsprite(self->player, ps_weapon, jumpto);
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_flash].state)
	{
		FState *jumpto = P_GetState(self->player->ReadyWeapon, CallingState, offset);
		if (jumpto == NULL) return;
		P_SetPsprite(self->player, ps_flash, jumpto);
	}
	else
	{
		FState *jumpto = P_GetState(self, CallingState, offset);
		if (jumpto == NULL) return;
		self->SetState (jumpto);
	}
}
//==========================================================================
//
// State jump function
//
//==========================================================================
void A_Jump(AActor * self)
{
	FState * CallingState;
	int index = CheckIndex(3, &CallingState);
	int maxchance;

	if (index >= 0 &&
		StateParameters[index] >= 2 &&
		(maxchance = clamp<int>(EvalExpressionI (StateParameters[index + 1], self), 0, 256),
		 maxchance == 256 || pr_cajump() < maxchance))
	{
		if (StateParameters[index] == 2)
		{
			DoJump(self, CallingState, StateParameters[index + 2]);
		}
		else
		{
			DoJump(self, CallingState, StateParameters[index + (pr_cajump() % (StateParameters[index] - 1)) + 2]);
		}
	}
	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_JumpIfHealthLower(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(2, &CallingState);

	if (index>=0 && self->health < EvalExpressionI (StateParameters[index], self))
		DoJump(self, CallingState, StateParameters[index+1]);

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_JumpIfCloser(AActor * self)
{
	FState * CallingState;
	int index = CheckIndex(2, &CallingState);
	AActor * target;

	if (!self->player)
	{
		target=self->target;
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self);
		target = linetarget;
	}

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (target==NULL) return;

	fixed_t dist = fixed_t(EvalExpressionF (StateParameters[index], self) * FRACUNIT);
	if (index > 0 && P_AproxDistance(self->x-target->x, self->y-target->y) < dist)
		DoJump(self, CallingState, StateParameters[index+1]);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfInventory(AActor * self, AActor * owner)
{
	FState * CallingState;
	int index=CheckIndex(3, &CallingState);
	if (index<0) return;

	ENamedName ItemType=(ENamedName)StateParameters[index];
	int ItemAmount = EvalExpressionI (StateParameters[index+1], self);
	int JumpOffset = StateParameters[index+2];
	const PClass * Type=PClass::FindClass(ItemType);

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!

	if (!Type || owner == NULL) return;

	AInventory * Item=owner->FindInventory(Type);

	if (Item)
	{
		if (ItemAmount>0 && Item->Amount>=ItemAmount) DoJump(self, CallingState, JumpOffset);
		else if (Item->Amount>=Item->MaxAmount) DoJump(self, CallingState, JumpOffset);
	}
}

void A_JumpIfInventory(AActor * self)
{
	DoJumpIfInventory(self, self);
}

void A_JumpIfInTargetInventory(AActor * self)
{
	DoJumpIfInventory(self, self->target);
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

void A_ExplodeParms (AActor *self)
{
	int damage;
	int distance;
	bool hurtSource;

	int index=CheckIndex(3);
	if (index>=0) 
	{
		damage = EvalExpressionI (StateParameters[index], self);
		distance = EvalExpressionI (StateParameters[index+1], self);
		hurtSource = EvalExpressionN (StateParameters[index+2], self);
		if (damage == 0) damage = 128;
		if (distance == 0) distance = damage;
	}
	else
	{
		damage = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionDamage, 128);
		distance = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionRadius, damage);
		hurtSource = !self->GetClass()->Meta.GetMetaInt (ACMETA_DontHurtShooter);
	}

	P_RadiusAttack (self, self->target, damage, distance, self->DamageType, hurtSource);
	if (self->z <= self->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (self);
	}
}


//==========================================================================
//
// A_RadiusThrust
//
//==========================================================================

void A_RadiusThrust (AActor *self)
{
	int force = 0;
	int distance = 0;
	bool affectSource = true;
	
	int index=CheckIndex(3);
	if (index>=0) 
	{
		force = EvalExpressionI (StateParameters[index], self);
		distance = EvalExpressionI (StateParameters[index+1], self);
		affectSource = EvalExpressionN (StateParameters[index+2], self);
	}
	if (force == 0) force = 128;
	if (distance == 0) distance = force;

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
void A_CallSpecial(AActor * self)
{
	int index=CheckIndex(6);
	if (index<0) return;

	bool res = !!LineSpecials[StateParameters[index]](NULL, self, false,
		EvalExpressionI (StateParameters[index+1], self),
		EvalExpressionI (StateParameters[index+2], self),
		EvalExpressionI (StateParameters[index+3], self),
		EvalExpressionI (StateParameters[index+4], self),
		EvalExpressionI (StateParameters[index+5], self));

	if (pStateCall != NULL) pStateCall->Result = res;
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

void A_CustomMissile(AActor * self)
{
	int index=CheckIndex(6);
	if (index<0) return;

	ENamedName MissileName=(ENamedName)StateParameters[index];
	fixed_t SpawnHeight=fixed_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	int Spawnofs_XY=EvalExpressionI (StateParameters[index+2], self);
	angle_t Angle=angle_t(EvalExpressionF (StateParameters[index+3], self) * ANGLE_1);
	int flags=EvalExpressionI (StateParameters[index+4], self);
	angle_t pitch=angle_t(EvalExpressionF (StateParameters[index+5], self) * ANGLE_1);
	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile;

	if (self->target != NULL || aimmode==2)
	{
		const PClass * ti=PClass::FindClass(MissileName);
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
				missile = P_SpawnMissile(self, self->target, ti);
				self->x-=x;
				self->y-=y;
				self->z-=z;
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+SpawnHeight, self, self->target, ti);
				break;

			case 2:
				missile = P_SpawnMissileAngleZ(self, self->z+SpawnHeight, ti, self->angle, 0);

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
void A_CustomBulletAttack (AActor *self)
{
	int index=CheckIndex(7);
	if (index<0) return;

	angle_t Spread_XY=angle_t(EvalExpressionF (StateParameters[index], self) * ANGLE_1);
	angle_t Spread_Z=angle_t(EvalExpressionF (StateParameters[index+1], self) * ANGLE_1);
	int NumBullets=EvalExpressionI (StateParameters[index+2], self);
	int DamagePerBullet=EvalExpressionI (StateParameters[index+3], self);
	ENamedName PuffType=(ENamedName)StateParameters[index+4];
	fixed_t Range = fixed_t(EvalExpressionF (StateParameters[index+5], self) * FRACUNIT);
	bool AimFacing = !!EvalExpressionI (StateParameters[index+6], self);

	if(Range==0) Range=MISSILERANGE;


	int i;
	int bangle;
	int bslope;
	const PClass *pufftype;

	if (self->target || AimFacing)
	{
		if (!AimFacing) A_FaceTarget (self);
		bangle = self->angle;

		pufftype = PClass::FindClass(PuffType);
		if (!pufftype) pufftype = PClass::FindClass(NAME_BulletPuff);

		bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
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
void A_CustomMeleeAttack (AActor *self)
{
	int index=CheckIndex(5);
	if (index<0) return;

	int damage = EvalExpressionI (StateParameters[index], self);
	int MeleeSound = StateParameters[index+1];
	int MissSound = StateParameters[index+2];
	ENamedName DamageType = (ENamedName)StateParameters[index+3];
	bool bleed = EvalExpressionN (StateParameters[index+4], self);

	if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (MeleeSound) S_SoundID (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else
	{
		if (MissSound) S_SoundID (self, CHAN_WEAPON, MissSound, 1, ATTN_NORM);
	}
}

//==========================================================================
//
// A fully customizable combo attack
//
//==========================================================================
void A_CustomComboAttack (AActor *self)
{
	int index=CheckIndex(6);
	if (index<0) return;

	ENamedName MissileName=(ENamedName)StateParameters[index];
	fixed_t SpawnHeight=fixed_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	int damage = EvalExpressionI (StateParameters[index+2], self);
	int MeleeSound = StateParameters[index+3];
	ENamedName DamageType = (ENamedName)StateParameters[index+4];
	bool bleed = EvalExpressionN (StateParameters[index+5], self);


	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type
		if (MeleeSound) S_SoundID (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else
	{
		const PClass * ti=PClass::FindClass(MissileName);
		if (ti) 
		{
			// Although there is a P_SpawnMissileZ function its
			// aiming is much too bad to be of any use
			self->z+=SpawnHeight-32*FRACUNIT;
			AActor * missile = P_SpawnMissile (self, self->target, ti);
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
			}
		}
	}
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_JumpIfNoAmmo(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(1, &CallingState);

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
	if (index<0 || !self->player || !self->player->ReadyWeapon || pStateCall != NULL) return;	// only for weapons!

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
		DoJump(self, CallingState, StateParameters[index]);

}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
void A_FireBullets (AActor *self)
{
	int index=CheckIndex(7);
	if (index<0 || !self->player) return;

	angle_t Spread_XY=angle_t(EvalExpressionF (StateParameters[index], self) * ANGLE_1);
	angle_t Spread_Z=angle_t(EvalExpressionF (StateParameters[index+1], self) * ANGLE_1);
	int NumberOfBullets=EvalExpressionI (StateParameters[index+2], self);
	int DamagePerBullet=EvalExpressionI (StateParameters[index+3], self);
	ENamedName PuffTypeName=(ENamedName)StateParameters[index+4];
	bool UseAmmo=EvalExpressionN (StateParameters[index+5], self);
	fixed_t Range=fixed_t(EvalExpressionF (StateParameters[index+6], self) * FRACUNIT);
	
	const PClass * PuffType;

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

	P_BulletSlope(self);
	bangle = self->angle;
	bslope = bulletpitch;

	PuffType = PClass::FindClass(PuffTypeName);
	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	S_SoundID (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

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
void A_FireCustomMissile (AActor * self)
{
	int index=CheckIndex(6);
	if (index<0 || !self->player) return;

	ENamedName MissileName=(ENamedName)StateParameters[index];
	angle_t Angle=angle_t(EvalExpressionF (StateParameters[index+1], self) * ANGLE_1);
	bool UseAmmo=EvalExpressionN (StateParameters[index+2], self);
	int SpawnOfs_XY=EvalExpressionI (StateParameters[index+3], self);
	fixed_t SpawnHeight=fixed_t(EvalExpressionF (StateParameters[index+4], self) * FRACUNIT);
	INTBOOL AimAtAngle=EvalExpressionI (StateParameters[index+5], self);

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;

	if (UseAmmo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	const PClass * ti=PClass::FindClass(MissileName);
	if (ti) 
	{
		angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = SpawnOfs_XY * finecosine[ang];
		fixed_t y = SpawnOfs_XY * finesine[ang];
		fixed_t z = SpawnHeight;
		fixed_t shootangle = self->angle;

		if (AimAtAngle) shootangle+=Angle;

		AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle);
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
			if (misl->flags4&MF4_SPECTRAL) misl->health=-1;
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
void A_CustomPunch (AActor *self)
{
	int index=CheckIndex(5);
	if (index<0 || !self->player) return;

	int Damage=EvalExpressionI (StateParameters[index], self);
	bool norandom=!!EvalExpressionI (StateParameters[index+1], self);
	bool UseAmmo=EvalExpressionN (StateParameters[index+2], self);
	ENamedName PuffTypeName=(ENamedName)StateParameters[index+3];
	fixed_t Range=fixed_t(EvalExpressionF (StateParameters[index+4], self) * FRACUNIT);

	const PClass * PuffType;


	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;

	if (!norandom) Damage *= (pr_cwpunch()%8+1);

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (Range == 0) Range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, Range);

	// only use ammo when actually hitting something!
	if (UseAmmo && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	PuffType = PClass::FindClass(PuffTypeName);
	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	P_LineAttack (self, angle, Range, pitch, Damage, GetDefaultByType(PuffType)->DamageType, PuffType);

	// turn to face target
	if (linetarget)
	{
		S_SoundID (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

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
void A_RailAttack (AActor * self)
{
	int index=CheckIndex(7);
	if (index<0 || !self->player) return;

	int Damage=EvalExpressionI (StateParameters[index], self);
	int Spawnofs_XY=EvalExpressionI (StateParameters[index+1], self);
	bool UseAmmo=EvalExpressionN (StateParameters[index+2], self);
	int Color1=StateParameters[index+3];
	int Color2=StateParameters[index+4];
	bool Silent=!!EvalExpressionI (StateParameters[index+5], self);
	float MaxDiff=EvalExpressionF (StateParameters[index+6], self);
	ENamedName PuffTypeName=(ENamedName)StateParameters[index+7];

	AWeapon * weapon=self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (UseAmmo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	P_RailAttack (self, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, Silent, PuffTypeName);
}

//==========================================================================
//
// also for monsters
//
//==========================================================================

void A_CustomRailgun (AActor *actor)
{
	int index = CheckIndex(7);
	if (index < 0) return;

	int Damage = EvalExpressionI (StateParameters[index], actor);
	int Spawnofs_XY = EvalExpressionI (StateParameters[index+1], actor);
	int Color1 = StateParameters[index+2];
	int Color2 = StateParameters[index+3];
	bool Silent = !!EvalExpressionI (StateParameters[index+4], actor);
	bool aim = !!EvalExpressionI (StateParameters[index+5], actor);
	float MaxDiff = EvalExpressionF (StateParameters[index+6], actor);
	ENamedName PuffTypeName = (ENamedName)StateParameters[index+7];

	if (aim && actor->target == NULL)
	{
		return;
	}
	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
	}

	actor->flags &= ~MF_AMBUSH;

	if (aim)
	{
		actor->angle = R_PointToAngle2 (actor->x,
										actor->y,
										actor->target->x,
										actor->target->y);
	}

	actor->pitch = P_AimLineAttack (actor, actor->angle, MISSILERANGE);

	// Let the aim trail behind the player
	if (aim)
	{
		actor->angle = R_PointToAngle2 (actor->x,
										actor->y,
										actor->target->x - actor->target->momx * 3,
										actor->target->y - actor->target->momy * 3);

		if (actor->target->flags & MF_SHADOW)
		{
			actor->angle += pr_crailgun.Random2() << 21;
		}
	}

	P_RailAttack (actor, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, Silent, PuffTypeName);
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor * self, AActor * receiver)
{
	int index=CheckIndex(2);
	bool res=true;
	if (index<0 || receiver == NULL) return;

	ENamedName item =(ENamedName)StateParameters[index];
	int amount=EvalExpressionI (StateParameters[index+1], self);

	if (amount==0) amount=1;
	const PClass * mi=PClass::FindClass(item);
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
		if (!item->TryPickup (receiver))
		{
			item->Destroy ();
			res = false;
		}
		else res = true;
	}
	else res = false;
	if (pStateCall != NULL) pStateCall->Result = res;

}	

void A_GiveInventory(AActor * self)
{
	DoGiveInventory(self, self);
}	

void A_GiveToTarget(AActor * self)
{
	DoGiveInventory(self, self->target);
}	

//===========================================================================
//
// A_TakeInventory
//
//===========================================================================

void DoTakeInventory(AActor * self, AActor * receiver)
{
	int index=CheckIndex(2);
	if (index<0 || receiver == NULL) return;

	ENamedName item =(ENamedName)StateParameters[index];
	int amount=EvalExpressionI (StateParameters[index+1], self);

	if (pStateCall != NULL) pStateCall->Result=false;

	AInventory * inv = receiver->FindInventory(item);

	if (inv && !inv->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
	{
		if (inv->Amount > 0 && pStateCall != NULL) pStateCall->Result=true;
		if (!amount || amount>=inv->Amount) 
		{
			if (inv->ItemFlags&IF_KEEPDEPLETED) inv->Amount=0;
			else inv->Destroy();
		}
		else inv->Amount-=amount;
	}
}	

void A_TakeInventory(AActor * self)
{
	DoTakeInventory(self, self);
}	

void A_TakeFromTarget(AActor * self)
{
	DoTakeInventory(self, self->target);
}	

//===========================================================================
//
// Common code for A_SpawnItem and A_SpawnItemEx
//
//===========================================================================

static void InitSpawnedItem(AActor *self, AActor *mo, INTBOOL transfer_translation, INTBOOL setmaster, INTBOOL nocheckpos)
{
	if (mo)
	{
		AActor * originator = self;

		if (transfer_translation && !(mo->flags2 & MF2_DONTTRANSLATE))
		{
			mo->Translation = self->Translation;
		}

		mo->angle=self->angle;
		while (originator && isMissile(originator)) originator = originator->target;

		if (mo->flags3&MF3_ISMONSTER)
		{
			if (!nocheckpos && !P_TestMobjLocation(mo))
			{
				// The monster is blocked so don't spawn it at all!
				if (mo->CountsAsKill()) level.total_monsters--;
				mo->Destroy();
				if (pStateCall != NULL) pStateCall->Result=false;	// for an inventory item's use state
				return;
			}
			else if (originator)
			{
				if (originator->flags3&MF3_ISMONSTER)
				{
					// If this is a monster transfer all friendliness information
					mo->CopyFriendliness(originator, true);
					if (setmaster) mo->master = originator;	// don't let it attack you (optional)!
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
							mo->target = attacker;
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
}

//===========================================================================
//
// A_SpawnItem
//
// Spawns an item in front of the caller like Heretic's time bomb
//
//===========================================================================

void A_SpawnItem(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(5, &CallingState);
	if (index<0) return;

	const PClass * missile= PClass::FindClass((ENamedName)StateParameters[index]);
	fixed_t distance = fixed_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	fixed_t zheight = fixed_t(EvalExpressionF (StateParameters[index+2], self) * FRACUNIT);
	bool useammo = EvalExpressionN (StateParameters[index+3], self);
	INTBOOL transfer_translation = EvalExpressionI (StateParameters[index+4], self);

	if (!missile) 
	{
		if (pStateCall != NULL) pStateCall->Result=false;
		return;
	}

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && GetDefaultByType(missile)->flags3&MF3_ISMONSTER) return;

	if (distance==0) 
	{
		// use the minimum distance that does not result in an overlap
		distance=(self->radius+GetDefaultByType(missile)->radius)>>FRACBITS;
	}

	if (self->player && CallingState != self->state && (pStateCall==NULL || CallingState != pStateCall->State))
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

	InitSpawnedItem(self, mo, transfer_translation, useammo, false);
}

//===========================================================================
//
// A_SpawnItemEx
//
// Enhanced spawning function
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
	SIXF_TELEFRAG=64
};

void A_SpawnItemEx(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(9, &CallingState);
	if (index<0) return;

	const PClass * missile= PClass::FindClass((ENamedName)StateParameters[index]);
	fixed_t xofs = fixed_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	fixed_t yofs = fixed_t(EvalExpressionF (StateParameters[index+2], self) * FRACUNIT);
	fixed_t zofs = fixed_t(EvalExpressionF (StateParameters[index+3], self) * FRACUNIT);
	fixed_t xmom = fixed_t(EvalExpressionF (StateParameters[index+4], self) * FRACUNIT);
	fixed_t ymom = fixed_t(EvalExpressionF (StateParameters[index+5], self) * FRACUNIT);
	fixed_t zmom = fixed_t(EvalExpressionF (StateParameters[index+6], self) * FRACUNIT);
	angle_t Angle= angle_t(EvalExpressionF (StateParameters[index+7], self) * ANGLE_1);
	int flags = EvalExpressionI (StateParameters[index+8], self);
	int chance = EvalExpressionI (StateParameters[index+9], self);

	if (!missile) 
	{
		if (pStateCall != NULL) pStateCall->Result=false;
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

	AActor * mo = Spawn( missile, x, y, self->z + self->floorclip + zofs, ALLOW_REPLACE);
	InitSpawnedItem(self, mo, (flags & SIXF_TRANSFERTRANSLATION), (flags&SIXF_SETMASTER), (flags&SIXF_NOCHECKPOSITION));
	if (mo)
	{
		mo->momx=xmom;
		mo->momy=ymom;
		mo->momz=zmom;
		mo->angle=Angle;
		if (flags & SIXF_TELEFRAG) P_TeleportMove(mo, mo->x, mo->y, mo->z, true);
	}
}

//===========================================================================
//
// A_ThrowGrenade
//
// Throws a grenade (like Hexen's fighter flechette)
//
//===========================================================================
void A_ThrowGrenade(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(5, &CallingState);
	if (index<0) return;

	const PClass * missile= PClass::FindClass((ENamedName)StateParameters[index]);
	fixed_t zheight = fixed_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	fixed_t xymom = fixed_t(EvalExpressionF (StateParameters[index+2], self) * FRACUNIT);
	fixed_t zmom = fixed_t(EvalExpressionF (StateParameters[index+3], self) * FRACUNIT);
	bool useammo = EvalExpressionN (StateParameters[index+4], self);

	if (self->player && CallingState != self->state && (pStateCall==NULL || CallingState != pStateCall->State))
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
	else if (pStateCall != NULL) pStateCall->Result=false;
}


//===========================================================================
//
// A_Recoil
//
//===========================================================================
void A_Recoil(AActor * actor)
{
	int index=CheckIndex(1, NULL);
	if (index<0) return;
	fixed_t xymom = fixed_t(EvalExpressionF (StateParameters[index], actor) * FRACUNIT);

	angle_t angle = actor->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	actor->momx += FixedMul (xymom, finecosine[angle]);
	actor->momy += FixedMul (xymom, finesine[angle]);
}


//===========================================================================
//
// A_SelectWeapon
//
//===========================================================================
void A_SelectWeapon(AActor * actor)
{
	int index=CheckIndex(1, NULL);
	if (index<0 || actor->player == NULL) return;

	AWeapon * weaponitem = static_cast<AWeapon*>(actor->FindInventory((ENamedName)StateParameters[index]));

	if (weaponitem != NULL && weaponitem->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{
		if (actor->player->ReadyWeapon != weaponitem)
		{
			actor->player->PendingWeapon = weaponitem;
		}
	}
	else if (pStateCall != NULL) pStateCall->Result=false;
}


//===========================================================================
//
// A_Print
//
//===========================================================================
EXTERN_CVAR(Float, con_midtime)

void A_Print(AActor * actor)
{
	int index=CheckIndex(3, NULL);
	if (index<0) return;

	if (actor->CheckLocalView (consoleplayer) ||
		(actor->target!=NULL && actor->target->CheckLocalView (consoleplayer)))
	{
		float time = EvalExpressionF (StateParameters[index+1], actor);
		FName fontname = (ENamedName)StateParameters[index+2];
		FFont * oldfont = screen->Font;
		float saved = con_midtime;

		
		if (fontname != NAME_None)
		{
			FFont * font = V_GetFont(fontname);
			if (font != NULL) screen->SetFont(font);
		}
		if (time > 0)
		{
			con_midtime = time;
		}
		
		C_MidPrint(FName((ENamedName)StateParameters[index]).GetChars());
		screen->SetFont(oldfont);
		con_midtime = saved;
	}
}


//===========================================================================
//
// A_SetTranslucent
//
//===========================================================================
void A_SetTranslucent(AActor * self)
{
	int index=CheckIndex(2, NULL);
	if (index<0) return;

	fixed_t alpha = fixed_t(EvalExpressionF (StateParameters[index], self) * FRACUNIT);
	int mode = EvalExpressionI (StateParameters[index+1], self);
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
void A_FadeIn(AActor * self)
{
	fixed_t reduce = 0;
	
	int index=CheckIndex(1, NULL);
	if (index>=0) 
	{
		reduce = fixed_t(EvalExpressionF (StateParameters[index], self) * FRACUNIT);
	}
	
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
void A_FadeOut(AActor * self)
{
	fixed_t reduce = 0;
	
	int index=CheckIndex(1, NULL);
	if (index>=0) 
	{
		reduce = fixed_t(EvalExpressionF (StateParameters[index], self) * FRACUNIT);
	}
	
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
void A_SpawnDebris(AActor * self)
{
	int i;
	AActor * mo;
	const PClass * debris;

	int index=CheckIndex(4, NULL);
	if (index<0) return;

	debris = PClass::FindClass((ENamedName)StateParameters[index]);
	if (debris == NULL) return;

	INTBOOL transfer_translation = EvalExpressionI (StateParameters[index+1], self);
	fixed_t mult_h = fixed_t(EvalExpressionF (StateParameters[index+2], self) * FRACUNIT);
	fixed_t mult_v = fixed_t(EvalExpressionF (StateParameters[index+3], self) * FRACUNIT);

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
void A_CheckSight(AActor * self)
{
	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!

	for (int i=0;i<MAXPLAYERS;i++) 
	{
		if (playeringame[i] && P_CheckSight(players[i].camera,self,true)) return;
	}

	FState * CallingState;
	int index=CheckIndex(1, &CallingState);

	if (index>=0) DoJump(self, CallingState, StateParameters[index]);

}


//===========================================================================
//
// Inventory drop
//
//===========================================================================
void A_DropInventory(AActor * self)
{
	int index=CheckIndex(1, &CallingState);
	if (index<0) return;

	AInventory * inv = self->FindInventory((ENamedName)StateParameters[index]);
	if (inv)
	{
		self->DropInventory(inv);
	}
}


//===========================================================================
//
// A_SetBlend
//
//===========================================================================
void A_SetBlend(AActor * self)
{
	int index=CheckIndex(3);
	if (index<0) return;
	PalEntry color = StateParameters[index];
	float alpha = clamp<float> (EvalExpressionF (StateParameters[index+1], self), 0, 1);
	int tics = EvalExpressionI (StateParameters[index+2], self);
	PalEntry color2 = StateParameters[index+3];

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
void A_JumpIf(AActor * self)
{
	FState * CallingState;
	int index=CheckIndex(2, &CallingState);
	if (index<0) return;
	INTBOOL expression = EvalExpressionI (StateParameters[index], self);

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
	if (expression) DoJump(self, CallingState, StateParameters[index+1]);

}

//===========================================================================
//
// A_KillMaster
//
//===========================================================================
void A_KillMaster(AActor * self)
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
void A_KillChildren(AActor * self)
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
// A_CountdownArg
//
//===========================================================================
void A_CountdownArg(AActor * self)
{
	int index=CheckIndex(1);
	if (index<0) return;
	index = EvalExpressionI (StateParameters[index], self) - 1;

	if (index<0 || index>=5) return;
	if (!self->args[index]--)
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

void A_Burst (AActor *actor)
{
   int i, numChunks;
   AActor * mo;
   int index=CheckIndex(1, NULL);
   if (index<0) return;
   const PClass * chunk = PClass::FindClass((ENamedName)StateParameters[index]);
   if (chunk == NULL) return;

   actor->momx = actor->momy = actor->momz = 0;
   actor->height = actor->GetDefault()->height;

   // [RH] In Hexen, this creates a random number of shards (range [24,56])
   // with no relation to the size of the actor shattering. I think it should
   // base the number of shards on the size of the dead thing, so bigger
   // things break up into more shards than smaller things.
   // An actor with radius 20 and height 64 creates ~40 chunks.
   numChunks = MAX<int> (4, (actor->radius>>FRACBITS)*(actor->height>>FRACBITS)/32);
   i = (pr_burst.Random2()) % (numChunks/4);
   for (i = MAX (24, numChunks + i); i >= 0; i--)
   {
      mo = Spawn(chunk,
         actor->x + (((pr_burst()-128)*actor->radius)>>7),
         actor->y + (((pr_burst()-128)*actor->radius)>>7),
         actor->z + (pr_burst()*actor->height/255), ALLOW_REPLACE);

	  if (mo)
      {
         mo->momz = FixedDiv(mo->z-actor->z, actor->height)<<2;
         mo->momx = pr_burst.Random2 () << (FRACBITS-7);
         mo->momy = pr_burst.Random2 () << (FRACBITS-7);
         mo->RenderStyle = actor->RenderStyle;
         mo->alpha = actor->alpha;
		 mo->CopyFriendliness(actor, true);
      }
   }

   // [RH] Do some stuff to make this more useful outside Hexen
   if (actor->flags4 & MF4_BOSSDEATH)
   {
      A_BossDeath (actor);
   }
   A_NoBlocking (actor);

   actor->Destroy ();
}

//===========================================================================
//
// A_CheckFloor
// [GRB] Jumps if actor is standing on floor
//
//===========================================================================
void A_CheckFloor (AActor *self)
{
	FState *CallingState;
	int index = CheckIndex (1, &CallingState);

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
	if (self->z <= self->floorz && index >= 0)
	{
		DoJump (self, CallingState, StateParameters[index]);
	}

}

//===========================================================================
//
// A_Stop
// resets all momentum of the actor to 0
//
//===========================================================================
void A_Stop (AActor *self)
{
	self->momx = self->momy = self->momz = 0;
}

//===========================================================================
//
// A_Respawn
//
//===========================================================================
void A_Respawn (AActor *actor)
{
	fixed_t x = actor->SpawnPoint[0] << FRACBITS;
	fixed_t y = actor->SpawnPoint[1] << FRACBITS;
	sector_t *sec;

	actor->flags |= MF_SOLID;
	sec = P_PointInSector (x, y);
	actor->SetOrigin (x, y, sec->floorplane.ZatPoint (x, y));
	actor->height = actor->GetDefault()->height;
	if (P_TestMobjLocation (actor))
	{
		AActor *defs = actor->GetDefault();
		actor->health = defs->health;

		actor->flags  = (defs->flags & ~MF_FRIENDLY) | (actor->flags & MF_FRIENDLY);
		actor->flags2 = defs->flags2;
		actor->flags3 = (defs->flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (actor->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
		actor->flags4 = (defs->flags4 & ~MF4_NOHATEPLAYERS) | (actor->flags4 & MF4_NOHATEPLAYERS);
		actor->flags5 = defs->flags5;
		actor->SetState (actor->SpawnState);
		actor->renderflags &= ~RF_INVISIBLE;

		int index=CheckIndex(1, NULL);
		if (index<0 || EvalExpressionN (StateParameters[index], actor))
		{
			Spawn<ATeleportFog> (x, y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		}
		if (actor->CountsAsKill()) level.total_monsters++;
	}
	else
	{
		actor->flags &= ~MF_SOLID;
	}
}


//==========================================================================
//
// A_PlayerSkinCheck
//
//==========================================================================

void A_PlayerSkinCheck (AActor *actor)
{
	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!
	if (actor->player != NULL &&
		skins[actor->player->userinfo.skin].othergame)
	{
		int index = CheckIndex(1, &CallingState);
	
		if (index >= 0)
		{
			DoJump(actor, CallingState, StateParameters[index]);
		}	
	}
}

//===========================================================================
//
// A_SetGravity
//
//===========================================================================
void A_SetGravity(AActor * self)
{
	int index=CheckIndex(1);
	if (index<0) return;
	
	self->gravity = clamp<fixed_t> (fixed_t(EvalExpressionF (StateParameters[index], self)*FRACUNIT), 0, FRACUNIT); 
}


// [KS] *** Start of my modifications ***

//===========================================================================
//
// A_ClearTarget
//
//===========================================================================

void A_ClearTarget(AActor * self)
{
	self->target = NULL;
	self->LastHeard = NULL;
	self->lastenemy = NULL;
}

//==========================================================================
//
// A_JumpIfTargetInLOS (state label, optional fixed fov)
// Jumps if the actor can see its target, or if the player has a linetarget.
//
//==========================================================================

void A_JumpIfTargetInLOS(AActor * self)
{
	FState * CallingState;
	int index = CheckIndex(2, &CallingState);
	angle_t an;
	angle_t fov = angle_t(EvalExpressionF (StateParameters[index+1], self) * FRACUNIT);
	AActor * target;

	if (pStateCall != NULL) pStateCall->Result=false;	// Jumps should never set the result for inventory state chains!

	if (!self->player)
	{
		target=self->target;

		if (!P_CheckSight (self, target, 1))
			return; // [KS] Cannot see target - return

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
		P_BulletSlope(self);
		target = linetarget;
	}

	// No target - return
	if (target==NULL) return;

	DoJump(self, CallingState, StateParameters[index]);
}

// [KS] *** End of my modifications ***
