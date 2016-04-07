/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_posattack ("PosAttack");
static FRandom pr_sposattack ("SPosAttack");
static FRandom pr_cposattack ("CPosAttack");
static FRandom pr_cposrefire ("CPosRefire");

//
// A_PosAttack
//
DEFINE_ACTION_FUNCTION(AActor, A_PosAttack)
{
	PARAM_ACTION_PROLOGUE;

	int damage;
	DAngle angle;
	DAngle slope;
		
	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	angle = self->Angles.Yaw;
	slope = P_AimLineAttack (self, angle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM);
	angle += pr_posattack.Random2() * (22.5 / 256);
	damage = ((pr_posattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
	return 0;
}

static void A_SPosAttack2 (AActor *self)
{
	int i;
	DAngle bangle;
	DAngle slope;
		
	A_FaceTarget (self);
	bangle = self->Angles.Yaw;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	for (i=0 ; i<3 ; i++)
    {
		DAngle angle = bangle + pr_sposattack.Random2() * (22.5 / 256);
		int damage = ((pr_sposattack()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
    }
}

DEFINE_ACTION_FUNCTION(AActor, A_SPosAttackUseAtkSound)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_SPosAttack2 (self);
	return 0;
}

// This version of the function, which uses a hard-coded sound, is
// meant for Dehacked only.
DEFINE_ACTION_FUNCTION(AActor, A_SPosAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;

	S_Sound (self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM);
	A_SPosAttack2 (self);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CPosAttack)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	DAngle bangle;
	int damage;
	DAngle slope;
		
	if (!self->target)
		return 0;

	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	bangle = self->Angles.Yaw;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	angle = bangle + pr_cposattack.Random2() * (22.5 / 256);
	damage = ((pr_cposattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CPosRefire)
{
	PARAM_ACTION_PROLOGUE;

	// keep firing unless target got out of sight
	A_FaceTarget (self);

	if (pr_cposrefire() < 40)
		return 0;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES))
	{
		self->SetState (self->SeeState);
	}
	return 0;
}
