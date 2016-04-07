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
		return self->Distance2D (self->target) < 264.;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderChoose)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	if (CrusaderCheckRange (self))
	{
		A_FaceTarget (self);
		self->Angles.Yaw -= 180./16;
		P_SpawnMissileZAimed (self, self->Z() + 40, self->target, PClass::FindActor("FastFlameMissile"));
	}
	else
	{
		if (P_CheckMissileRange (self))
		{
			A_FaceTarget (self);
			P_SpawnMissileZAimed (self, self->Z() + 56, self->target, PClass::FindActor("CrusaderMissile"));
			self->Angles.Yaw -= 45./32;
			P_SpawnMissileZAimed (self, self->Z() + 40, self->target, PClass::FindActor("CrusaderMissile"));
			self->Angles.Yaw += 45./16;
			P_SpawnMissileZAimed (self, self->Z() + 40, self->target, PClass::FindActor("CrusaderMissile"));
			self->Angles.Yaw -= 45./16;
			self->reactiontime += 15;
		}
		self->SetState (self->SeeState);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderSweepLeft)
{
	PARAM_ACTION_PROLOGUE;

	self->Angles.Yaw += 90./16;
	AActor *misl = P_SpawnMissileZAimed (self, self->Z() + 48, self->target, PClass::FindActor("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->Vel.Z += 1;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderSweepRight)
{
	PARAM_ACTION_PROLOGUE;

	self->Angles.Yaw -= 90./16;
	AActor *misl = P_SpawnMissileZAimed (self, self->Z() + 48, self->target, PClass::FindActor("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->Vel.Z += 1;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderRefire)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL ||
		self->target->health <= 0 ||
		!P_CheckSight (self, self->target))
	{
		self->SetState (self->SeeState);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrusaderDeath)
{
	PARAM_ACTION_PROLOGUE;

	if (CheckBossDeath (self))
	{
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 667, 1., 0., -1, 0, false);
	}
	return 0;
}
