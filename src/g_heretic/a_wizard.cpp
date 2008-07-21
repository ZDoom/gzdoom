#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_hereticglobal.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_wizatk3 ("WizAtk3");

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

void A_GhostOff (AActor *actor)
{
	actor->RenderStyle = STYLE_Normal;
	actor->flags3 &= ~MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

void A_WizAtk1 (AActor *actor)
{
	A_FaceTarget (actor);
	A_GhostOff (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

void A_WizAtk2 (AActor *actor)
{
	A_FaceTarget (actor);
	actor->alpha = HR_SHADOW;
	actor->RenderStyle = STYLE_Translucent;
	actor->flags3 |= MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

void A_WizAtk3 (AActor *actor)
{
	AActor *mo;

	A_GhostOff (actor);
	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
	if (actor->CheckMeleeRange())
	{
		int damage = pr_wizatk3.HitDice (4);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	const PClass *fx = PClass::FindClass("WizardFX1");
	mo = P_SpawnMissile (actor, actor->target, fx);
	if (mo != NULL)
	{
		P_SpawnMissileAngle(actor, fx, mo->angle-(ANG45/8), mo->momz);
		P_SpawnMissileAngle(actor, fx, mo->angle+(ANG45/8), mo->momz);
	}
}
