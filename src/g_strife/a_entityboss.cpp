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
	if (CheckBossDeath (self))
	{
		G_ExitLevel (0, false);
	}
}

void A_SpectralMissile (AActor *self, const char *missilename)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, 
			self, self->target, PClass::FindClass(missilename), false);
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
}


DEFINE_ACTION_FUNCTION(AActor, A_SpawnEntity)
{
	AActor *entity = Spawn("EntityBoss", self->x, self->y, self->z + 70*FRACUNIT, ALLOW_REPLACE);
	if (entity != NULL)
	{
		entity->angle = self->angle;
		entity->CopyFriendliness(self, true);
		entity->velz = 5*FRACUNIT;
		entity->tracer = self;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_EntityDeath)
{
	AActor *second;
	fixed_t secondRadius = GetDefaultByName("EntitySecond")->radius * 2;
	angle_t an;

	AActor *spot = self->tracer;
	if (spot == NULL) spot = self;

	fixed_t SpawnX = spot->x;
	fixed_t SpawnY = spot->y;
	fixed_t SpawnZ = spot->z + (self->tracer? 70*FRACUNIT : 0);
	
	an = self->angle >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	A_FaceTarget (second);
	an = second->angle >> ANGLETOFINESHIFT;
	second->velx += FixedMul (finecosine[an], 320000);
	second->vely += FixedMul (finesine[an], 320000);

	an = (self->angle + ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->velx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->vely = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);

	an = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->velx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->vely = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);
}
