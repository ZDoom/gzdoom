/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_wizatk3 ("WizAtk3");

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_GhostOff)
{
	PARAM_ACTION_PROLOGUE;

	self->RenderStyle = STYLE_Normal;
	self->flags3 &= ~MF3_GHOST;
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk1)
{
	PARAM_ACTION_PROLOGUE;

	A_FaceTarget (self);
	CALL_ACTION(A_GhostOff, self);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk2)
{
	PARAM_ACTION_PROLOGUE;

	A_FaceTarget (self);
	self->Alpha = HR_SHADOW;
	self->RenderStyle = STYLE_Translucent;
	self->flags3 |= MF3_GHOST;
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk3)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	CALL_ACTION(A_GhostOff, self);
	if (!self->target)
	{
		return 0;
	}
	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange())
	{
		int damage = pr_wizatk3.HitDice (4);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return 0;
	}
	PClassActor *fx = PClass::FindActor("WizardFX1");
	mo = P_SpawnMissile (self, self->target, fx);
	if (mo != NULL)
	{
		P_SpawnMissileAngle(self, fx, mo->Angles.Yaw - 45. / 8, mo->Vel.Z);
		P_SpawnMissileAngle(self, fx, mo->Angles.Yaw + 45. / 8, mo->Vel.Z);
	}
	return 0;
}
