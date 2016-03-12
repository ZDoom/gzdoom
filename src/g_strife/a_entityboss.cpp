/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_entity ("Entity");

DEFINE_ACTION_FUNCTION(AActor, A_SubEntityDeath)
{
	PARAM_ACTION_PROLOGUE;

	if (CheckBossDeath (self))
	{
		G_ExitLevel (0, false);
	}
	return 0;
}

void A_SpectralMissile (AActor *self, const char *missilename)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissileXYZ (self->PosPlusZ(32*FRACUNIT), 
			self, self->target, PClass::FindActor(missilename), false);
		if (missile != NULL)
		{
			missile->tracer = self->target;
			P_CheckMissileSpawn(missile, self->radius);
		}
	}
}

DECLARE_ACTION(A_SpotLightning)
DECLARE_ACTION(A_Spectre3Attack)


DEFINE_ACTION_FUNCTION(AActor, A_EntityAttack)
{
	PARAM_ACTION_PROLOGUE;

	// Apparent Strife bug: Case 5 was unreachable because they used % 5 instead of % 6.
	// I've fixed that by making case 1 duplicate it, since case 1 did nothing.
	switch (pr_entity() % 5)
	{
	case 0:
		CALL_ACTION(A_SpotLightning, self);
		break;

	case 2:
		A_SpectralMissile (self, "SpectralLightningH3");
		break;

	case 3:
		CALL_ACTION(A_Spectre3Attack, self);
		break;

	case 4:
		A_SpectralMissile (self, "SpectralLightningBigV2");
		break;

	case 1:
	case 5:
		A_SpectralMissile (self, "SpectralLightningBigBall2");
		break;
	}
	return 0;
}


DEFINE_ACTION_FUNCTION(AActor, A_SpawnEntity)
{
	PARAM_ACTION_PROLOGUE;

	AActor *entity = Spawn("EntityBoss", self->PosPlusZ(70*FRACUNIT), ALLOW_REPLACE);
	if (entity != NULL)
	{
		entity->angle = self->angle;
		entity->CopyFriendliness(self, true);
		entity->vel.z = 5*FRACUNIT;
		entity->tracer = self;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_EntityDeath)
{
	PARAM_ACTION_PROLOGUE;

	AActor *second;
	fixed_t secondRadius = GetDefaultByName("EntitySecond")->radius * 2;
	angle_t an;

	AActor *spot = self->tracer;
	if (spot == NULL) spot = self;

	fixedvec3 pos = spot->Vec3Angle(secondRadius, self->angle, self->tracer? 70*FRACUNIT : 0);
	
	an = self->angle >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", pos, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	A_FaceTarget (second);
	an = second->angle >> ANGLETOFINESHIFT;
	second->vel.x += FixedMul (finecosine[an], 320000);
	second->vel.y += FixedMul (finesine[an], 320000);

	pos = spot->Vec3Angle(secondRadius, self->angle + ANGLE_90, self->tracer? 70*FRACUNIT : 0);
	an = (self->angle + ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", pos, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->vel.x = FixedMul (secondRadius, finecosine[an]) << 2;
	second->vel.y = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);

	pos = spot->Vec3Angle(secondRadius, self->angle - ANGLE_90, self->tracer? 70*FRACUNIT : 0);
	an = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", pos, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->vel.x = FixedMul (secondRadius, finecosine[an]) << 2;
	second->vel.y = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);
	return 0;
}
