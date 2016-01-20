/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
*/

static bool CrusaderCheckRange (AActor *self)
{
	if (self->reactiontime == 0 && P_CheckSight (self, self->target))
	{
		return self->AproxDistance (self->target) < 264*FRACUNIT;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderChoose)
{
	if (self->target == NULL)
		return;

	if (CrusaderCheckRange (self))
	{
		A_FaceTarget (self);
		self->angle -= ANGLE_180/16;
		P_SpawnMissileZAimed (self, self->Z() + 40*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	}
	else
	{
		if (P_CheckMissileRange (self))
		{
			A_FaceTarget (self);
			P_SpawnMissileZAimed (self, self->Z() + 56*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle -= ANGLE_45/32;
			P_SpawnMissileZAimed (self, self->Z() + 40*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle += ANGLE_45/16;
			P_SpawnMissileZAimed (self, self->Z() + 40*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle -= ANGLE_45/16;
			self->reactiontime += 15;
		}
		self->SetState (self->SeeState);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderSweepLeft)
{
	self->angle += ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->Z() + 48*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->velz += FRACUNIT;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderSweepRight)
{
	self->angle -= ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->Z() + 48*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->velz += FRACUNIT;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderRefire)
{
	if (self->target == NULL ||
		self->target->health <= 0 ||
		!P_CheckSight (self, self->target))
	{
		self->SetState (self->SeeState);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderDeath)
{
	if (CheckBossDeath (self))
	{
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 667, FRACUNIT, 0, -1, 0, false);
	}
}
