/*
#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_local.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_sentinelrefire ("SentinelRefire");

DEFINE_ACTION_FUNCTION(AActor, A_SentinelBob)
{
	PARAM_ACTION_PROLOGUE;

	double minz, maxz;

	if (self->flags & MF_INFLOAT)
	{
		self->Vel.Z = 0;
		return 0;
	}
	if (self->threshold != 0)
		return 0;

	maxz =  self->ceilingz - self->_Height() - 16;
	minz = self->floorz + 96;
	if (minz > maxz)
	{
		minz = maxz;
	}
	if (minz < self->Z())
	{
		self->Vel.Z -= 1;
	}
	else
	{
		self->Vel.Z += 1;
	}
	self->reactiontime = (minz >= self->Z()) ? 4 : 0;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SentinelAttack)
{
	PARAM_ACTION_PROLOGUE;

	AActor *missile, *trail;

	// [BB] Without a target the P_SpawnMissileZAimed call will crash.
	if (!self->target)
	{
		return 0;
	}

	missile = P_SpawnMissileZAimed (self, self->_f_Z() + 32*FRACUNIT, self->target, PClass::FindActor("SentinelFX2"));

	if (missile != NULL && (missile->Vel.X != 0 || missile->Vel.Y != 0))
	{
		for (int i = 8; i > 1; --i)
		{
			trail = Spawn("SentinelFX1",
				self->_f_Vec3Angle(missile->_f_radius()*i, missile->_f_angle(), (missile->_f_velz() / 4 * i)), ALLOW_REPLACE);
			if (trail != NULL)
			{
				trail->target = self;
				trail->Vel = missile->Vel;
				P_CheckMissileSpawn (trail, self->radius);
			}
		}
		missile->_f_AddZ(missile->_f_velz() >> 2);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SentinelRefire)
{
	PARAM_ACTION_PROLOGUE;

	A_FaceTarget (self);

	if (pr_sentinelrefire() >= 30)
	{
		if (self->target == NULL ||
			self->target->health <= 0 ||
			!P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES) ||
			P_HitFriend(self) ||
			(self->MissileState == NULL && !self->CheckMeleeRange()) ||
			pr_sentinelrefire() < 40)
		{
			self->SetState (self->SeeState);
		}
	}
	return 0;
}
