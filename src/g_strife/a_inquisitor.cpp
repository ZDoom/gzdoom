/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_inq ("Inquisitor");

DEFINE_ACTION_FUNCTION(AActor, A_InquisitorWalk)
{
	S_Sound (self, CHAN_BODY, "inquisitor/walk", 1, ATTN_NORM);
	A_Chase (self);
}

bool InquisitorCheckDistance (AActor *self)
{
	if (self->reactiontime == 0 && P_CheckSight (self, self->target))
	{
		return P_AproxDistance (self->x - self->target->x, self->y - self->target->y) < 264*FRACUNIT;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, A_InquisitorDecide)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	if (!InquisitorCheckDistance (self))
	{
		self->SetState (self->FindState("Grenade"));
	}
	if (self->target->z != self->z)
	{
		if (self->z + self->height + 54*FRACUNIT < self->ceilingz)
		{
			self->SetState (self->FindState("Jump"));
		}
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_InquisitorAttack)
{
	AActor *proj;

	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	self->z += 32*FRACUNIT;
	self->angle -= ANGLE_45/32;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, PClass::FindClass("InquisitorShot"));
	if (proj != NULL)
	{
		proj->velz += 9*FRACUNIT;
	}
	self->angle += ANGLE_45/16;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, PClass::FindClass("InquisitorShot"));
	if (proj != NULL)
	{
		proj->velz += 16*FRACUNIT;
	}
	self->z -= 32*FRACUNIT;
}

DEFINE_ACTION_FUNCTION(AActor, A_InquisitorJump)
{
	fixed_t dist;
	fixed_t speed;
	angle_t an;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_ITEM|CHAN_LOOP, "inquisitor/jump", 1, ATTN_NORM);
	self->z += 64*FRACUNIT;
	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	speed = self->Speed * 2/3;
	self->velx += FixedMul (speed, finecosine[an]);
	self->vely += FixedMul (speed, finesine[an]);
	dist = P_AproxDistance (self->target->x - self->x, self->target->y - self->y);
	dist /= speed;
	if (dist < 1)
	{
		dist = 1;
	}
	self->velz = (self->target->z - self->z) / dist;
	self->reactiontime = 60;
	self->flags |= MF_NOGRAVITY;
}

DEFINE_ACTION_FUNCTION(AActor, A_InquisitorCheckLand)
{
	self->reactiontime--;
	if (self->reactiontime < 0 ||
		self->velx == 0 ||
		self->vely == 0 ||
		self->z <= self->floorz)
	{
		self->SetState (self->SeeState);
		self->reactiontime = 0;
		self->flags &= ~MF_NOGRAVITY;
		S_StopSound (self, CHAN_ITEM);
		return;
	}
	if (!S_IsActorPlayingSomething (self, CHAN_ITEM, -1))
	{
		S_Sound (self, CHAN_ITEM|CHAN_LOOP, "inquisitor/jump", 1, ATTN_NORM);
	}

}

DEFINE_ACTION_FUNCTION(AActor, A_TossArm)
{
	AActor *foo = Spawn("InquisitorArm", self->x, self->y, self->z + 24*FRACUNIT, ALLOW_REPLACE);
	foo->angle = self->angle - ANGLE_90 + (pr_inq.Random2() << 22);
	foo->velx = FixedMul (foo->Speed, finecosine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->vely = FixedMul (foo->Speed, finesine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->velz = pr_inq() << 10;
}

