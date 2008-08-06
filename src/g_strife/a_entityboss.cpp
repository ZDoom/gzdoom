#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

static FRandom pr_entity ("Entity");

void A_SubEntityDeath (AActor *self)
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
			self, self->target, PClass::FindClass("SpectralLightningBigV2"), false);
		if (missile != NULL)
		{
			missile->tracer = self->target;
			missile->health = -2;
			P_CheckMissileSpawn(missile);
		}
	}
}

void A_SpotLightning (AActor *);
void A_Spectre3Attack (AActor *);


void A_EntityAttack (AActor *self)
{
	// Apparent Strife bug: Case 5 was unreachable because they used % 5 instead of % 6.
	// I've fixed that by making case 1 duplicate it, since case 1 did nothing.
	switch (pr_entity() % 5)
	{
	case 0:
		A_SpotLightning(self);
		break;

	case 2:
		A_SpectralMissile (self, "SpectralLightningH3");
		break;

	case 3:
		A_Spectre3Attack (self);
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


void A_SpawnEntity (AActor *self)
{
	AActor *entity = Spawn("EntityBoss", self->x, self->y, self->z + 70*FRACUNIT, ALLOW_REPLACE);
	if (entity != NULL)
	{
		entity->angle = self->angle;
		entity->CopyFriendliness(self, true);
		entity->momz = 5*FRACUNIT;
		entity->tracer = self;
	}
}

void A_EntityDeath (AActor *self)
{
	AActor *second;
	fixed_t secondRadius = GetDefaultByName("EntitySecond")->radius * 2;
	angle_t an;

	AActor *spot = self->tracer;
	if (spot == NULL) spot = self;

	fixed_t SpawnX = spot->x;
	fixed_t SpawnY = spot->y;
	fixed_t SpawnZ = spot->z + self->tracer? 70*FRACUNIT : 0;
	
	an = self->angle >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	A_FaceTarget (second);
	an = second->angle >> ANGLETOFINESHIFT;
	second->momx += FixedMul (finecosine[an], 320000);
	second->momy += FixedMul (finesine[an], 320000);

	an = (self->angle + ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->momx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->momy = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);

	an = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn("EntitySecond", SpawnX + FixedMul (secondRadius, finecosine[an]),
		SpawnY + FixedMul (secondRadius, finesine[an]), SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->momx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->momy = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);
}
