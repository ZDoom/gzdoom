#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

static FRandom pr_entity ("Entity");

void A_SpotLightning (AActor *);

void A_SpawnEntity (AActor *);
void A_EntityAttack (AActor *);
void A_SpawnSubEntities (AActor *);

void A_SpectreMelee (AActor *);
void A_SpectreChunkSmall (AActor *);
void A_SpectreChunkLarge (AActor *);
void A_Spectre2Attack (AActor *);
void A_Spectre3Attack (AActor *);
void A_Spectre4Attack (AActor *);
void A_Spectre5Attack (AActor *);
void A_SentinelBob (AActor *);
void A_TossGib (AActor *);

// Entity Nest --------------------------------------------------------------

class AEntityNest : public AActor
{
	DECLARE_ACTOR (AEntityNest, AActor)
};

FState AEntityNest::States[] =
{
	S_NORMAL (NEST, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AEntityNest, Strife, 26, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (84)
	PROP_HeightFixed (47)
	PROP_Flags (MF_SOLID|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (76)
END_DEFAULTS

// Entity Pod ---------------------------------------------------------------

class AEntityPod : public AActor
{
	DECLARE_ACTOR (AEntityPod, AActor)
};

FState AEntityPod::States[] =
{
	S_NORMAL (PODD, 'A',  60, A_Look,			&States[0]),

	S_NORMAL (PODD, 'A', 360, NULL,				&States[2]),
	S_NORMAL (PODD, 'B',   9, A_NoBlocking,		&States[3]),
	S_NORMAL (PODD, 'C',   9, NULL,				&States[4]),
	S_NORMAL (PODD, 'D',   9, A_SpawnEntity,	&States[5]),
	S_NORMAL (PODD, 'E',  -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AEntityPod, Strife, 198, 0)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_RadiusFixed (25)
	PROP_HeightFixed (91)
	PROP_Flags (MF_SOLID|MF_NOTDMATCH)
	PROP_StrifeType (77)
	PROP_SeeSound ("misc/gibbed")
END_DEFAULTS

// Entity Boss --------------------------------------------------------------

class AEntityBoss : public AActor
{
	DECLARE_ACTOR (AEntityBoss, AActor)
public:
	void Serialize (FArchive &arc);
	void BeginPlay ();
	void Touch (AActor *toucher);

	fixed_t SpawnX, SpawnY, SpawnZ;
};

FState AEntityBoss::States[] =
{
#define S_ENTITY_SPAWN 0
	S_NORMAL (MNAM, 'A', 100, NULL,					&States[S_ENTITY_SPAWN+1]),
	S_BRIGHT (MNAM, 'B',  60, NULL,					&States[S_ENTITY_SPAWN+2]),
	S_BRIGHT (MNAM, 'C',   4, NULL,					&States[S_ENTITY_SPAWN+3]),
	S_BRIGHT (MNAM, 'D',   4, NULL,					&States[S_ENTITY_SPAWN+4]),
	S_BRIGHT (MNAM, 'E',   4, NULL,					&States[S_ENTITY_SPAWN+5]),
	S_BRIGHT (MNAM, 'F',   4, NULL,					&States[S_ENTITY_SPAWN+6]),
	S_BRIGHT (MNAM, 'G',   4, NULL,					&States[S_ENTITY_SPAWN+7]),
	S_BRIGHT (MNAM, 'H',   4, NULL,					&States[S_ENTITY_SPAWN+8]),
	S_BRIGHT (MNAM, 'I',   4, NULL,					&States[S_ENTITY_SPAWN+9]),
	S_BRIGHT (MNAM, 'J',   4, NULL,					&States[S_ENTITY_SPAWN+10]),
	S_BRIGHT (MNAM, 'K',   4, NULL,					&States[S_ENTITY_SPAWN+11]),
	S_BRIGHT (MNAM, 'L',   4, NULL,					&States[S_ENTITY_SPAWN+12]),
	S_BRIGHT (MNAL, 'A',   4, A_Look,				&States[S_ENTITY_SPAWN+13]),
	S_BRIGHT (MNAL, 'B',   4, A_SentinelBob,		&States[S_ENTITY_SPAWN+12]),

#define S_ENTITY_SEE (S_ENTITY_SPAWN+14)
	S_BRIGHT (MNAL, 'A',   4, A_Chase,				&States[S_ENTITY_SEE+1]),
	S_BRIGHT (MNAL, 'B',   4, A_Chase,				&States[S_ENTITY_SEE+2]),
	S_BRIGHT (MNAL, 'C',   4, A_SentinelBob,		&States[S_ENTITY_SEE+3]),
	S_BRIGHT (MNAL, 'D',   4, A_Chase,				&States[S_ENTITY_SEE+4]),
	S_BRIGHT (MNAL, 'E',   4, A_Chase,				&States[S_ENTITY_SEE+5]),
	S_BRIGHT (MNAL, 'F',   4, A_Chase,				&States[S_ENTITY_SEE+6]),
	S_BRIGHT (MNAL, 'G',   4, A_SentinelBob,		&States[S_ENTITY_SEE+7]),
	S_BRIGHT (MNAL, 'H',   4, A_Chase,				&States[S_ENTITY_SEE+8]),
	S_BRIGHT (MNAL, 'I',   4, A_Chase,				&States[S_ENTITY_SEE+9]),
	S_BRIGHT (MNAL, 'J',   4, A_Chase,				&States[S_ENTITY_SEE+10]),
	S_BRIGHT (MNAL, 'K',   4, A_SentinelBob,		&States[S_ENTITY_SEE]),

#define S_ENTITY_MELEE (S_ENTITY_SEE+11)
	S_BRIGHT (MNAL, 'J',   4, A_FaceTarget,			&States[S_ENTITY_MELEE+1]),
	S_BRIGHT (MNAL, 'I',   4, A_SpectreMelee,		&States[S_ENTITY_MELEE+2]),
	S_BRIGHT (MNAL, 'C',   4, NULL,					&States[S_ENTITY_SEE+2]),

#define S_ENTITY_MISSILE (S_ENTITY_MELEE+3)
	S_BRIGHT (MNAL, 'F',   4, A_FaceTarget,			&States[S_ENTITY_MISSILE+1]),
	S_BRIGHT (MNAL, 'I',   4, A_EntityAttack,		&States[S_ENTITY_MISSILE+2]),
	S_BRIGHT (MNAL, 'E',   4, NULL,					&States[S_ENTITY_SEE+10]),

#define S_ENTITY_PAIN (S_ENTITY_MISSILE+3)
	S_BRIGHT (MNAL, 'J',   2, A_Pain,				&States[S_ENTITY_SEE+6]),

#define S_ENTITY_DIE (S_ENTITY_PAIN+1)
	S_BRIGHT (MNAL, 'L',   7, A_SpectreChunkSmall,	&States[S_ENTITY_DIE+1]),
	S_BRIGHT (MNAL, 'M',   7, A_Scream,				&States[S_ENTITY_DIE+2]),
	S_BRIGHT (MNAL, 'N',   7, A_SpectreChunkSmall,	&States[S_ENTITY_DIE+3]),
	S_BRIGHT (MNAL, 'O',   7, A_SpectreChunkSmall,	&States[S_ENTITY_DIE+4]),
	S_BRIGHT (MNAL, 'P',   7, A_SpectreChunkLarge,	&States[S_ENTITY_DIE+5]),
	S_BRIGHT (MNAL, 'Q',  64, A_SpectreChunkSmall,	&States[S_ENTITY_DIE+6]),
	S_BRIGHT (MNAL, 'Q',   6, A_SpawnSubEntities,	NULL),
};

IMPLEMENT_ACTOR (AEntityBoss, Strife, 128, 0)
	PROP_StrifeType (74)
	PROP_SpawnHealth (2500)
	PROP_SpawnState (S_ENTITY_SPAWN)
	PROP_SeeState (S_ENTITY_SEE)
	PROP_PainState (S_ENTITY_PAIN)
	PROP_PainChance (255)
	PROP_MeleeState (S_ENTITY_MELEE)
	PROP_MissileState (S_ENTITY_MISSILE)
	PROP_DeathState (S_ENTITY_DIE)
	PROP_SpeedFixed (13)
	PROP_RadiusFixed (130)
	PROP_HeightFixed (200)
	PROP_FloatSpeed (5)
	PROP_Mass (1000)
	PROP_Flags (MF_SPECIAL|MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|
				MF_FLOAT|MF_SHADOW|MF_COUNTKILL|MF_NOTDMATCH|
				MF_STRIFEx8000000)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_NOTARGET|MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_INCOMBAT|MF4_LOOKALLAROUND|MF4_SPECTRAL|MF4_NOICEDEATH)
	PROP_MinMissileChance (150)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC50)
	PROP_SeeSound ("entity/sight")
	PROP_AttackSound ("entity/melee")
	PROP_PainSound ("entity/pain")
	PROP_DeathSound ("entity/death")
	PROP_ActiveSound ("entity/active")
	PROP_Obituary ("$OB_ENTITY")
END_DEFAULTS

void AEntityBoss::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SpawnX << SpawnY << SpawnZ;
}

void AEntityBoss::BeginPlay ()
{
	SpawnX = x;
	SpawnY = y;
	SpawnZ = z;
}

void AEntityBoss::Touch (AActor *toucher)
{
	P_DamageMobj (toucher, this, this, 5, NAME_Melee);
}

// Second Entity Boss -------------------------------------------------------

void A_SubEntityDeath (AActor *);

class AEntitySecond : public AActor
{
	DECLARE_ACTOR (AEntitySecond, AActor)
public:
	void Touch (AActor *toucher);
};

FState AEntitySecond::States[] =
{
#define S_ENTITY2_SPAWN 0
	S_BRIGHT (MNAL, 'R', 10, A_Look,			&States[S_ENTITY2_SPAWN]),

#define S_ENTITY2_SEE (S_ENTITY2_SPAWN+1)
	S_BRIGHT (MNAL, 'R',  5, A_SentinelBob,		&States[S_ENTITY2_SEE+1]),
	S_BRIGHT (MNAL, 'S',  5, A_Chase,			&States[S_ENTITY2_SEE+2]),
	S_BRIGHT (MNAL, 'T',  5, A_Chase,			&States[S_ENTITY2_SEE+3]),
	S_BRIGHT (MNAL, 'U',  5, A_SentinelBob,		&States[S_ENTITY2_SEE+4]),
	S_BRIGHT (MNAL, 'V',  5, A_Chase,			&States[S_ENTITY2_SEE+5]),
	S_BRIGHT (MNAL, 'W',  5, A_SentinelBob,		&States[S_ENTITY2_SEE]),

#define S_ENTITY2_MELEE (S_ENTITY2_SEE+6)
	S_BRIGHT (MNAL, 'S',  4, A_FaceTarget,		&States[S_ENTITY2_MELEE+1]),
	S_BRIGHT (MNAL, 'R',  4, A_SpectreMelee,	&States[S_ENTITY2_MELEE+2]),
	S_BRIGHT (MNAL, 'T',  4, A_SentinelBob,		&States[S_ENTITY2_SEE+1]),

#define S_ENTITY2_MISSILE (S_ENTITY2_MELEE+3)
	S_BRIGHT (MNAL, 'W',  4, A_FaceTarget,		&States[S_ENTITY2_MISSILE+1]),
	S_BRIGHT (MNAL, 'U',  4, A_Spectre2Attack,	&States[S_ENTITY2_MISSILE+2]),
	S_BRIGHT (MNAL, 'V',  4, A_SentinelBob,		&States[S_ENTITY2_SEE+4]),

#define S_ENTITY2_PAIN (S_ENTITY2_MISSILE+3)
	S_BRIGHT (MNAL, 'R',  2, A_Pain,			&States[S_ENTITY2_SEE]),

#define S_ENTITY2_DIE (S_ENTITY2_PAIN+1)
	S_BRIGHT (MDTH, 'A',  3, A_Scream,			&States[S_ENTITY2_DIE+1]),
	S_BRIGHT (MDTH, 'B',  3, A_TossGib,			&States[S_ENTITY2_DIE+2]),
	S_BRIGHT (MDTH, 'C',  3, A_NoBlocking,		&States[S_ENTITY2_DIE+3]),
	S_BRIGHT (MDTH, 'D',  3, A_TossGib,			&States[S_ENTITY2_DIE+4]),
	S_BRIGHT (MDTH, 'E',  3, A_TossGib,			&States[S_ENTITY2_DIE+5]),
	S_BRIGHT (MDTH, 'F',  3, A_TossGib,			&States[S_ENTITY2_DIE+6]),
	S_BRIGHT (MDTH, 'G',  3, A_TossGib,			&States[S_ENTITY2_DIE+7]),
	S_BRIGHT (MDTH, 'H',  3, A_TossGib,			&States[S_ENTITY2_DIE+8]),
	S_BRIGHT (MDTH, 'I',  3, A_TossGib,			&States[S_ENTITY2_DIE+9]),
	S_BRIGHT (MDTH, 'J',  3, A_TossGib,			&States[S_ENTITY2_DIE+10]),
	S_BRIGHT (MDTH, 'K',  3, A_TossGib,			&States[S_ENTITY2_DIE+11]),
	S_BRIGHT (MDTH, 'L',  3, A_TossGib,			&States[S_ENTITY2_DIE+12]),
	S_BRIGHT (MDTH, 'M',  3, A_TossGib,			&States[S_ENTITY2_DIE+13]),
	S_BRIGHT (MDTH, 'N',  3, A_TossGib,			&States[S_ENTITY2_DIE+14]),
	S_BRIGHT (MDTH, 'O',  3, A_SubEntityDeath,	NULL)
};

IMPLEMENT_ACTOR (AEntitySecond, Strife, -1, 0)
	PROP_StrifeType (75)
	PROP_SpawnHealth (990)
	PROP_SpawnState (S_ENTITY2_SPAWN)
	PROP_SeeState (S_ENTITY2_SEE)
	PROP_PainState (S_ENTITY2_PAIN)
	PROP_PainChance (255)
	PROP_MeleeState (S_ENTITY2_MELEE)
	PROP_MissileState (S_ENTITY2_MISSILE)
	PROP_DeathState (S_ENTITY2_DIE)
	PROP_SpeedFixed (14)
	PROP_RadiusFixed (130)
	PROP_HeightFixed (200)
	PROP_FloatSpeed (5)
	PROP_Mass (1000)
	PROP_Flags (MF_SPECIAL|MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|
				MF_FLOAT|MF_SHADOW|MF_COUNTKILL|MF_NOTDMATCH|
				MF_STRIFEx8000000)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_INCOMBAT|MF4_LOOKALLAROUND|MF4_SPECTRAL|MF4_NOICEDEATH)
	PROP_MinMissileChance (150)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC25)
	PROP_SeeSound ("alienspectre/sight")
	PROP_AttackSound ("alienspectre/blade")
	PROP_PainSound ("alienspectre/pain")
	PROP_DeathSound ("alienspectre/death")
	PROP_ActiveSound ("alienspectre/active")
	PROP_Obituary ("$OB_ENTITY")
END_DEFAULTS

void AEntitySecond::Touch (AActor *toucher)
{
	P_DamageMobj (toucher, this, this, 5, NAME_Melee);
}

void A_SubEntityDeath (AActor *self)
{
	if (CheckBossDeath (self))
	{
		G_ExitLevel (0, false);
	}
}

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
		A_Spectre2Attack (self);
		break;

	case 3:
		A_Spectre3Attack (self);
		break;

	case 4:
		A_Spectre4Attack (self);
		break;

	case 1:
	case 5:
		A_Spectre5Attack (self);
		break;
	}
}


void A_SpawnEntity (AActor *self)
{
	AActor *entity = Spawn<AEntityBoss> (self->x, self->y, self->z + 70*FRACUNIT, ALLOW_REPLACE);
	if (entity != NULL)
	{
		entity->angle = self->angle;
		entity->CopyFriendliness(self, true);
		//entity->target = self->target;
		entity->momz = 5*FRACUNIT;
	}
}

void A_SpawnSubEntities (AActor *selfa)
{
	AEntityBoss *self = static_cast<AEntityBoss *>(selfa);
	AEntitySecond *second;
	fixed_t secondRadius = GetDefault<AEntitySecond>()->radius * 2;
	angle_t an;
	
	an = self->angle >> ANGLETOFINESHIFT;
	second = Spawn<AEntitySecond> (self->SpawnX + FixedMul (secondRadius, finecosine[an]),
		self->SpawnY + FixedMul (secondRadius, finesine[an]), self->SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	A_FaceTarget (second);
	an = second->angle >> ANGLETOFINESHIFT;
	second->momx += FixedMul (finecosine[an], 320000);
	second->momy += FixedMul (finesine[an], 320000);

	an = (self->angle + ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn<AEntitySecond> (self->SpawnX + FixedMul (secondRadius, finecosine[an]),
		self->SpawnY + FixedMul (secondRadius, finesine[an]), self->SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->momx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->momy = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);

	an = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	second = Spawn<AEntitySecond> (self->SpawnX + FixedMul (secondRadius, finecosine[an]),
		self->SpawnY + FixedMul (secondRadius, finesine[an]), self->SpawnZ, ALLOW_REPLACE);
	second->CopyFriendliness(self, true);
	//second->target = self->target;
	second->momx = FixedMul (secondRadius, finecosine[an]) << 2;
	second->momy = FixedMul (secondRadius, finesine[an]) << 2;
	A_FaceTarget (second);
}
