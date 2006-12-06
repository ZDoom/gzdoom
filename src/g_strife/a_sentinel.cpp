#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_local.h"
#include "m_random.h"

static FRandom pr_sentinelrefire ("SentinelRefire");

void A_TossGib (AActor *);

// Sentinel -----------------------------------------------------------------

void A_SentinelBob (AActor *);
void A_SentinelAttack (AActor *);
void A_SentinelRefire (AActor *);

class ASentinel : public AActor
{
	DECLARE_ACTOR (ASentinel, AActor)
};

FState ASentinel::States[] =
{
#define S_SENTINEL_STND 0
	S_NORMAL (SEWR, 'A',   10, A_Look,				&States[S_SENTINEL_STND]),

#define S_SENTINEL_RUN (S_SENTINEL_STND+1)
	S_NORMAL (SEWR, 'A',	6, A_SentinelBob,		&States[S_SENTINEL_RUN+1]),
	S_NORMAL (SEWR, 'A',	6, A_Chase,				&States[S_SENTINEL_RUN]),

#define S_SENTINEL_ATK (S_SENTINEL_RUN+2)
	S_NORMAL (SEWR, 'B',	4, A_FaceTarget,		&States[S_SENTINEL_ATK+1]),
	S_BRIGHT (SEWR, 'C',	8, A_SentinelAttack,	&States[S_SENTINEL_ATK+2]),
	S_BRIGHT (SEWR, 'C',	4, A_SentinelRefire,	&States[S_SENTINEL_ATK+1]),

#define S_SENTINEL_PAIN (S_SENTINEL_ATK+3)
	S_NORMAL (SEWR, 'D',	5, A_Pain,				&States[S_SENTINEL_ATK+2]),

#define S_SENTINEL_DIE (S_SENTINEL_PAIN+1)
	S_NORMAL (SEWR, 'D',	7, A_NoBlocking,		&States[S_SENTINEL_DIE+1]),
	S_BRIGHT (SEWR, 'E',	8, A_TossGib,			&States[S_SENTINEL_DIE+2]),
	S_BRIGHT (SEWR, 'F',	5, A_Scream,			&States[S_SENTINEL_DIE+3]),
	S_BRIGHT (SEWR, 'G',	4, A_TossGib,			&States[S_SENTINEL_DIE+4]),
	S_BRIGHT (SEWR, 'H',	4, A_TossGib,			&States[S_SENTINEL_DIE+5]),
	S_NORMAL (SEWR, 'I',	4, NULL,				&States[S_SENTINEL_DIE+6]),
	S_NORMAL (SEWR, 'J',	5, NULL,				NULL)
};

IMPLEMENT_ACTOR (ASentinel, Strife, 3006, 0)
	PROP_SpawnState (S_SENTINEL_STND)
	PROP_SeeState (S_SENTINEL_RUN)
	PROP_PainState (S_SENTINEL_PAIN)
	PROP_MissileState (S_SENTINEL_ATK)
	PROP_DeathState (S_SENTINEL_DIE)

	PROP_SpawnHealth (100)
	PROP_PainChance (255)
	PROP_SpeedFixed (7)
	PROP_RadiusFixed (23)
	PROP_HeightFixed (53)
	PROP_Mass (300)
	PROP_StrifeType (91)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_SPAWNCEILING|MF_NOGRAVITY|MF_DROPOFF|
				/*MF_FLOAT|*/MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_INCOMBAT|MF4_MISSILEMORE|MF4_LOOKALLAROUND)
	PROP_MinMissileChance (150)

	PROP_SeeSound ("sentinel/sight")
	PROP_DeathSound ("sentinel/death")
	PROP_ActiveSound ("sentinel/active")
	PROP_Obituary ("$OB_SENTINEL")
END_DEFAULTS

// Sentinel FX 1 ------------------------------------------------------------

class ASentinelFX1 : public AActor
{
	DECLARE_ACTOR (ASentinelFX1, AActor)
};

FState ASentinelFX1::States[] =
{
	S_NORMAL (SHT1, 'A', 4, NULL, &States[1]),
	S_NORMAL (SHT1, 'B', 4, NULL, &States[0]),

#define S_SENTINELFX2_X 2
	S_NORMAL (POW1, 'F', 4, NULL, &States[S_SENTINELFX2_X+1]),
	S_NORMAL (POW1, 'G', 4, NULL, &States[S_SENTINELFX2_X+2]),
	S_NORMAL (POW1, 'H', 4, NULL, &States[S_SENTINELFX2_X+3]),
	S_NORMAL (POW1, 'I', 4, NULL, &States[S_SENTINELFX2_X+4]),
#define S_SENTINELFX1_X (S_SENTINELFX2_X+4)
	S_NORMAL (POW1, 'J', 4, NULL, NULL),
};

IMPLEMENT_ACTOR (ASentinelFX1, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (S_SENTINELFX1_X)
	PROP_SpeedFixed (40)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (8)
	PROP_Damage (0)
	PROP_DamageType (NAME_Disintegrate)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

// Sentinel FX 2 ------------------------------------------------------------

class ASentinelFX2 : public ASentinelFX1
{
	DECLARE_STATELESS_ACTOR (ASentinelFX2, ASentinelFX1)
};

IMPLEMENT_STATELESS_ACTOR (ASentinelFX2, Strife, -1, 0)
	PROP_DeathState (S_SENTINELFX2_X)
	PROP_Damage (1)
	PROP_SeeSound ("sentinel/plasma")
END_DEFAULTS

void A_SentinelBob (AActor *self)
{
	fixed_t minz, maxz;

	if (self->flags & MF_INFLOAT)
	{
		self->momz = 0;
		return;
	}
	if (self->threshold != 0)
		return;

	maxz =  self->ceilingz - self->height - 16*FRACUNIT;
	minz = self->floorz + 96*FRACUNIT;
	if (minz > maxz)
	{
		minz = maxz;
	}
	if (minz < self->z)
	{
		self->momz -= FRACUNIT;
	}
	else
	{
		self->momz += FRACUNIT;
	}
	self->reactiontime = (minz >= self->z) ? 4 : 0;
}

void A_SentinelAttack (AActor *self)
{
	AActor *missile, *trail;

	missile = P_SpawnMissileZAimed (self, self->z + 32*FRACUNIT, self->target, RUNTIME_CLASS(ASentinelFX2));

	if (missile != NULL && (missile->momx|missile->momy) != 0)
	{
		for (int i = 8; i > 1; --i)
		{
			trail = Spawn<ASentinelFX1> (
				self->x + FixedMul (missile->radius * i, finecosine[missile->angle >> ANGLETOFINESHIFT]),
				self->y + FixedMul (missile->radius * i, finesine[missile->angle >> ANGLETOFINESHIFT]),
				missile->z + (missile->momz / 4 * i), ALLOW_REPLACE);
			if (trail != NULL)
			{
				trail->target = self;
				trail->momx = missile->momx;
				trail->momy = missile->momy;
				trail->momz = missile->momz;
				P_CheckMissileSpawn (trail);
			}
		}
		missile->z += missile->momz >> 2;
	}
}

void A_SentinelRefire (AActor *self)
{
	A_FaceTarget (self);

	if (pr_sentinelrefire() >= 30)
	{
		if (self->target == NULL ||
			self->target->health <= 0 ||
			!P_CheckSight (self, self->target) ||
			P_HitFriend(self) ||
			pr_sentinelrefire() < 40)
		{
			self->SetState (self->SeeState);
		}
	}
}
