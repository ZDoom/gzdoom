#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

bool Sys_1ed64 (AActor *self)
{
	if (P_CheckSight (self, self->target) && self->reactiontime == 0)
	{
		return P_AproxDistance (self->x - self->target->x, self->y - self->target->y) < 264*FRACUNIT;
	}
	return false;
}

void A_CrusaderChoose (AActor *self)
{
	if (self->target == NULL)
		return;

	if (Sys_1ed64 (self))
	{
		A_FaceTarget (self);
		self->angle -= ANGLE_180/16;
		P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	}
	else
	{
		if (P_CheckMissileRange (self))
		{
			A_FaceTarget (self);
			P_SpawnMissileZAimed (self, self->z + 56*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle -= ANGLE_45/32;
			P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle += ANGLE_45/16;
			P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, PClass::FindClass("CrusaderMissile"));
			self->angle -= ANGLE_45/16;
			self->reactiontime += 15;
		}
		self->SetState (self->SeeState);
	}
}

void A_CrusaderSweepLeft (AActor *self)
{
	self->angle += ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->z + 48*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->momz += FRACUNIT;
	}
}

void A_CrusaderSweepRight (AActor *self)
{
	self->angle -= ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->z + 48*FRACUNIT, self->target, PClass::FindClass("FastFlameMissile"));
	if (misl != NULL)
	{
		misl->momz += FRACUNIT;
	}
}

void A_CrusaderRefire (AActor *self)
{
	if (self->target == NULL ||
		self->target->health <= 0 ||
		!P_CheckSight (self, self->target))
	{
		self->SetState (self->SeeState);
	}
}

void A_CrusaderDeath (AActor *self)
{
	if (CheckBossDeath (self))
	{
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 667, FRACUNIT, 0, 0, 0, false);
	}
}
