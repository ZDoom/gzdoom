#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "a_doomglobal.h"

static FRandom pr_posattack ("PosAttack");
static FRandom pr_sposattack ("SPosAttack");
static FRandom pr_cposattack ("CPosAttack");
static FRandom pr_cposrefire ("CPosRefire");

//
// A_PosAttack
//
void A_PosAttack (AActor *self)
{
	int angle;
	int damage;
	int slope;
		
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	angle = self->angle;
	slope = P_AimLineAttack (self, angle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM);
	angle += pr_posattack.Random2() << 20;
	damage = ((pr_posattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
}

static void A_SPosAttack2 (AActor *self)
{
	int i;
	int bangle;
	int slope;
		
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	for (i=0 ; i<3 ; i++)
    {
		int angle = bangle + (pr_sposattack.Random2() << 20);
		int damage = ((pr_sposattack()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
    }
}

void A_SPosAttackUseAtkSound (AActor *self)
{
	if (!self->target)
		return;

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

// This version of the function, which uses a hard-coded sound, is
// meant for Dehacked only.
void A_SPosAttack (AActor *self)
{
	if (!self->target)
		return;

	S_Sound (self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

void A_CPosAttack (AActor *self)
{
	int angle;
	int bangle;
	int damage;
	int slope;
		
	if (!self->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	angle = bangle + (pr_cposattack.Random2() << 20);
	damage = ((pr_cposattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
}

void A_CPosRefire (AActor *self)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	if (pr_cposrefire() < 40)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, 0) )
	{
		self->SetState (self->SeeState);
	}
}
