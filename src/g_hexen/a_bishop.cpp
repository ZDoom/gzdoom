/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_boom ("BishopBoom");
static FRandom pr_atk ("BishopAttack");
static FRandom pr_decide ("BishopDecide");
static FRandom pr_doblur ("BishopDoBlur");
static FRandom pr_sblur ("BishopSpawnBlur");
static FRandom pr_pain ("BishopPainBlur");

//============================================================================
//
// A_BishopAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopAttack)
{
	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange())
	{
		int damage = pr_atk.HitDice (4);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	self->special1 = (pr_atk() & 3) + 5;
}

//============================================================================
//
// A_BishopAttack2
//
//		Spawns one of a string of bishop missiles
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopAttack2)
{
	AActor *mo;

	if (!self->target || !self->special1)
	{
		self->special1 = 0;
		self->SetState (self->SeeState);
		return;
	}
	mo = P_SpawnMissile (self, self->target, PClass::FindClass("BishopFX"));
	if (mo != NULL)
	{
		mo->tracer = self->target;
	}
	self->special1--;
}

//============================================================================
//
// A_BishopMissileWeave
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopMissileWeave)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;

	// for compatibility this needs to set the value itself if it was never done by the projectile itself
	if (self->weaveindex == -1) self->weaveindex = 16;

	// since these values are now user configurable we have to do a proper range check to avoid array overflows.
	weaveXY = (self->weaveindex >> 16) & 63;
	weaveZ = (self->weaveindex & 63);
	angle = (self->angle + ANG90) >> ANGLETOFINESHIFT;
	newX = self->x - FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY = self->y - FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	weaveXY = (weaveXY + 2) & 63;
	newX += FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY += FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	P_TryMove (self, newX, newY, true);
	self->z -= FloatBobOffsets[weaveZ];
	weaveZ = (weaveZ + 2) & 63;
	self->z += FloatBobOffsets[weaveZ];	
	self->weaveindex = weaveZ + (weaveXY<<16);
}

//============================================================================
//
// A_BishopDecide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDecide)
{
	if (pr_decide() < 220)
	{
		return;
	}
	else
	{
		self->SetState (self->FindState ("Blur"));
	}		
}

//============================================================================
//
// A_BishopDoBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDoBlur)
{
	self->special1 = (pr_doblur() & 3) + 3; // Random number of blurs
	if (pr_doblur() < 120)
	{
		P_ThrustMobj (self, self->angle + ANG90, 11*FRACUNIT);
	}
	else if (pr_doblur() > 125)
	{
		P_ThrustMobj (self, self->angle - ANG90, 11*FRACUNIT);
	}
	else
	{ // Thrust forward
		P_ThrustMobj (self, self->angle, 11*FRACUNIT);
	}
	S_Sound (self, CHAN_BODY, "BishopBlur", 1, ATTN_NORM);
}

//============================================================================
//
// A_BishopSpawnBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopSpawnBlur)
{
	AActor *mo;

	if (!--self->special1)
	{
		self->velx = 0;
		self->vely = 0;
		if (pr_sblur() > 96)
		{
			self->SetState (self->SeeState);
		}
		else
		{
			self->SetState (self->MissileState);
		}
	}
	mo = Spawn ("BishopBlur", self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;
	}
}

//============================================================================
//
// A_BishopChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopChase)
{
	self->z -= FloatBobOffsets[self->special2] >> 1;
	self->special2 = (self->special2 + 4) & 63;
	self->z += FloatBobOffsets[self->special2] >> 1;
}

//============================================================================
//
// A_BishopPuff
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPuff)
{
	AActor *mo;

	mo = Spawn ("BishopPuff", self->x, self->y, self->z + 40*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->velz = FRACUNIT/2;
	}
}

//============================================================================
//
// A_BishopPainBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPainBlur)
{
	AActor *mo;

	if (pr_pain() < 64)
	{
		self->SetState (self->FindState ("Blur"));
		return;
	}
	fixed_t x = self->x + (pr_pain.Random2()<<12);
	fixed_t y = self->y + (pr_pain.Random2()<<12);
	fixed_t z = self->z + (pr_pain.Random2()<<11);
	mo = Spawn ("BishopPainBlur", x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;
	}
}
