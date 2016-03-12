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
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
	{
		return 0;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange())
	{
		int damage = pr_atk.HitDice (4);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return 0;
	}
	self->special1 = (pr_atk() & 3) + 5;
	return 0;
}

//============================================================================
//
// A_BishopAttack2
//
//		Spawns one of a string of bishop missiles
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopAttack2)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	if (!self->target || !self->special1)
	{
		self->special1 = 0;
		self->SetState (self->SeeState);
		return 0;
	}
	mo = P_SpawnMissile (self, self->target, PClass::FindActor("BishopFX"));
	if (mo != NULL)
	{
		mo->tracer = self->target;
	}
	self->special1--;
	return 0;
}

//============================================================================
//
// A_BishopMissileWeave
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopMissileWeave)
{
	PARAM_ACTION_PROLOGUE;

	A_Weave(self, 2, 2, 2*FRACUNIT, FRACUNIT);
	return 0;
}

//============================================================================
//
// A_BishopDecide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDecide)
{
	PARAM_ACTION_PROLOGUE;

	if (pr_decide() < 220)
	{
		return 0;
	}
	else
	{
		self->SetState (self->FindState ("Blur"));
	}
	return 0;
}

//============================================================================
//
// A_BishopDoBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDoBlur)
{
	PARAM_ACTION_PROLOGUE;

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
	return 0;
}

//============================================================================
//
// A_BishopSpawnBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopSpawnBlur)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	if (!--self->special1)
	{
		self->vel.x = 0;
		self->vel.y = 0;
		if (pr_sblur() > 96)
		{
			self->SetState (self->SeeState);
		}
		else
		{
			self->SetState (self->MissileState);
		}
	}
	mo = Spawn ("BishopBlur", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;
	}
	return 0;
}

//============================================================================
//
// A_BishopChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopChase)
{
	PARAM_ACTION_PROLOGUE;

	fixed_t newz = self->Z() - finesine[self->special2 << BOBTOFINESHIFT] * 4;
	self->special2 = (self->special2 + 4) & 63;
	newz += finesine[self->special2 << BOBTOFINESHIFT] * 4;
	self->SetZ(newz);
	return 0;
}

//============================================================================
//
// A_BishopPuff
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPuff)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	mo = Spawn ("BishopPuff", self->PosPlusZ(40*FRACUNIT), ALLOW_REPLACE);
	if (mo)
	{
		mo->vel.z = FRACUNIT/2;
	}
	return 0;
}

//============================================================================
//
// A_BishopPainBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPainBlur)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	if (pr_pain() < 64)
	{
		self->SetState (self->FindState ("Blur"));
		return 0;
	}
	fixed_t xo = (pr_pain.Random2() << 12);
	fixed_t yo = (pr_pain.Random2() << 12);
	fixed_t zo = (pr_pain.Random2() << 11);
	mo = Spawn ("BishopPainBlur", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;
	}
	return 0;
}
