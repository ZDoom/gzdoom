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
		AActor *missile = P_SpawnMissileXYZ (self->PosPlusZ(32.), self, self->target, PClass::FindActor(missilename), false);
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

	AActor *entity = Spawn("EntityBoss", self->PosPlusZ(70.), ALLOW_REPLACE);
	if (entity != NULL)
	{
		entity->Angles.Yaw = self->Angles.Yaw;
		entity->CopyFriendliness(self, true);
		entity->Vel.Z = 5;
		entity->tracer = self;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_EntityDeath)
{
	PARAM_ACTION_PROLOGUE;

	AActor *second;
	double secondRadius = GetDefaultByName("EntitySecond")->radius * 2;

	static const double turns[3] = { 0, 90, -90 };
	const double velmul[3] = { 4.8828125f, secondRadius*4, secondRadius*4 };

	AActor *spot = self->tracer;
	if (spot == NULL) spot = self;

	for (int i = 0; i < 3; i++)
	{
		DAngle an = self->Angles.Yaw + turns[i];
		DVector3 pos = spot->Vec3Angle(secondRadius, an, self->tracer ? 70. : 0.);

		second = Spawn("EntitySecond", pos, ALLOW_REPLACE);
		second->CopyFriendliness(self, true);
		A_FaceTarget(second);
		second->VelFromAngle(an, velmul[i]);
	}
	return 0;
}
