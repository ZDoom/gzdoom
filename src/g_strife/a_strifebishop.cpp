#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "p_local.h"

void A_TossGib (AActor *);

// Bishop -------------------------------------------------------------------

void A_SBishopAttack (AActor *);
void A_SpawnSpectre2 (AActor *);

class AStrifeBishop : public AActor
{
	DECLARE_ACTOR (AStrifeBishop, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 64;
	}
	void NoBlockingSet ()
	{
		P_DropItem (this, "CrateOfMissiles", -1, 256);
	}
};

FState AStrifeBishop::States[] =
{
#define S_BISHOP_STND 0
	S_NORMAL (MLDR, 'A',   10, A_Look,				&States[S_BISHOP_STND]),

#define S_BISHOP_RUN (S_BISHOP_STND+1)
	S_NORMAL (MLDR, 'A',	3, A_Chase,				&States[S_BISHOP_RUN+1]),
	S_NORMAL (MLDR, 'A',	3, A_Chase,				&States[S_BISHOP_RUN+2]),
	S_NORMAL (MLDR, 'B',	3, A_Chase,				&States[S_BISHOP_RUN+3]),
	S_NORMAL (MLDR, 'B',	3, A_Chase,				&States[S_BISHOP_RUN+4]),
	S_NORMAL (MLDR, 'C',	3, A_Chase,				&States[S_BISHOP_RUN+5]),
	S_NORMAL (MLDR, 'C',	3, A_Chase,				&States[S_BISHOP_RUN+6]),
	S_NORMAL (MLDR, 'D',	3, A_Chase,				&States[S_BISHOP_RUN+7]),
	S_NORMAL (MLDR, 'D',	3, A_Chase,				&States[S_BISHOP_RUN]),

#define S_BISHOP_ATK (S_BISHOP_RUN+8)
	S_NORMAL (MLDR, 'E',	3, A_FaceTarget,		&States[S_BISHOP_ATK+1]),
	S_BRIGHT (MLDR, 'F',	2, A_SBishopAttack,		&States[S_BISHOP_RUN]),

#define S_BISHOP_PAIN (S_BISHOP_ATK+2)
	S_NORMAL (MLDR, 'D',	1, A_Pain,				&States[S_BISHOP_RUN]),

#define S_BISHOP_DIE (S_BISHOP_PAIN+1)
	S_BRIGHT (MLDR, 'G',	3, NULL,				&States[S_BISHOP_DIE+1]),
	S_BRIGHT (MLDR, 'H',	5, A_Scream,			&States[S_BISHOP_DIE+2]),
	S_BRIGHT (MLDR, 'I',	4, A_TossGib,			&States[S_BISHOP_DIE+3]),
	S_BRIGHT (MLDR, 'J',	4, A_Explode,			&States[S_BISHOP_DIE+4]),
	S_BRIGHT (MLDR, 'K',	4, NULL,				&States[S_BISHOP_DIE+5]),
	S_BRIGHT (MLDR, 'L',	4, NULL,				&States[S_BISHOP_DIE+6]),
	S_BRIGHT (MLDR, 'M',	4, A_NoBlocking,		&States[S_BISHOP_DIE+7]),
	S_BRIGHT (MLDR, 'N',	4, NULL,				&States[S_BISHOP_DIE+8]),
	S_BRIGHT (MLDR, 'O',	4, A_TossGib,			&States[S_BISHOP_DIE+9]),
	S_BRIGHT (MLDR, 'P',	4, NULL,				&States[S_BISHOP_DIE+10]),
	S_BRIGHT (MLDR, 'Q',	4, A_TossGib,			&States[S_BISHOP_DIE+11]),
	S_BRIGHT (MLDR, 'R',	4, NULL,				&States[S_BISHOP_DIE+12]),
	S_BRIGHT (MLDR, 'S',	4, A_TossGib,			&States[S_BISHOP_DIE+13]),
	S_BRIGHT (MLDR, 'T',	4, NULL,				&States[S_BISHOP_DIE+14]),
	S_BRIGHT (MLDR, 'U',	4, A_TossGib,			&States[S_BISHOP_DIE+15]),
	S_BRIGHT (MLDR, 'V',	4, A_SpawnSpectre2,		NULL),
};

IMPLEMENT_ACTOR (AStrifeBishop, Strife, 187, 0)
	PROP_SpawnState (S_BISHOP_STND)
	PROP_SeeState (S_BISHOP_RUN)
	PROP_PainState (S_BISHOP_PAIN)
	PROP_MissileState (S_BISHOP_ATK)
	PROP_DeathState (S_BISHOP_DIE)

	PROP_SpawnHealth (500)
	PROP_PainChance (128)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (56)
	PROP_Mass (500)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_INCOMBAT|MF4_FIRERESIST|MF4_NOICEDEATH)
	PROP_MinMissileChance (150)
	PROP_StrifeType (64)
	PROP_SeeSound ("bishop/sight")
	PROP_PainSound ("bishop/pain")
	PROP_DeathSound ("bishop/death")
	PROP_ActiveSound ("bishop/active")
END_DEFAULTS

// The Bishop's missile -----------------------------------------------------

void A_RocketInFlight (AActor *);
void A_Tracer2 (AActor *);

class ABishopMissile : public AActor
{
	DECLARE_ACTOR (ABishopMissile, AActor)
public:
	void PreExplode ()
	{
		RenderStyle = STYLE_Add;
		S_StopSound (this, CHAN_VOICE);
	}
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 64;
	}
};

FState ABishopMissile::States[] =
{
	S_BRIGHT (MISS, 'A',	4, A_RocketInFlight,	&States[1]),
	S_BRIGHT (MISS, 'B',	3, A_Tracer2,			&States[0]),

	S_BRIGHT (SMIS, 'A',	5, A_Explode,			&States[3]),
	S_BRIGHT (SMIS, 'B',	5, NULL,				&States[4]),
	S_BRIGHT (SMIS, 'C',	4, NULL,				&States[5]),
	S_BRIGHT (SMIS, 'D',	2, NULL,				&States[6]),
	S_BRIGHT (SMIS, 'E',	2, NULL,				&States[7]),
	S_BRIGHT (SMIS, 'F',	2, NULL,				&States[8]),
	S_BRIGHT (SMIS, 'G',	2, NULL,				NULL),
};

IMPLEMENT_ACTOR (ABishopMissile, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_SpeedFixed (20)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (14)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_SEEKERMISSILE)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_SeeSound ("bishop/misl")
	PROP_DeathSound ("bishop/mislx")
END_DEFAULTS

void A_SBishopAttack (AActor *self)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissileZ (self, self->z + 64*FRACUNIT, self->target, RUNTIME_CLASS(ABishopMissile));

		if (missile != NULL)
		{
			missile->tracer = self->target;
		}
	}
}

// In Strife, this number is stored in the data segment, but it doesn't seem to be
// altered anywhere.
#define TRACEANGLE (0xe000000)

void A_Tracer2 (AActor *self)
{
	AActor *dest;
	angle_t exact;
	fixed_t dist;
	fixed_t slope;

	dest = self->tracer;

	if (dest == NULL || dest->health <= 0)
		return;

	// change angle
	exact = R_PointToAngle2 (self->x, self->y, dest->x, dest->y);

	if (exact != self->angle)
	{
		if (exact - self->angle > 0x80000000)
		{
			self->angle -= TRACEANGLE;
			if (exact - self->angle < 0x80000000)
				self->angle = exact;
		}
		else
		{
			self->angle += TRACEANGLE;
			if (exact - self->angle > 0x80000000)
				self->angle = exact;
		}
	}

	exact = self->angle >> ANGLETOFINESHIFT;
	self->momx = FixedMul (self->Speed, finecosine[exact]);
	self->momy = FixedMul (self->Speed, finesine[exact]);

	// change slope
	dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
	dist /= self->Speed;

	if (dist < 1)
	{
		dist = 1;
	}
	slope = (dest->z + 40*FRACUNIT - self->z) / dist;
	if (slope < self->momz)
	{
		self->momz -= FRACUNIT/8;
	}
	else
	{
		self->momz += FRACUNIT/8;
	}
}
