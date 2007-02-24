#include "actor.h"
#include "a_action.h"
#include "a_strifeglobal.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "s_sound.h"
#include "vectors.h"

static FRandom pr_atk1 ("FooMelee");

void A_SentinelBob (AActor *);
void A_SpawnSpectre5 (AActor *);

void A_20538 (AActor *);
void A_20598 (AActor *);
void A_205b0 (AActor *);

// Loremaster (aka Priest) --------------------------------------------------

class ALoremaster : public AActor
{
	DECLARE_ACTOR (ALoremaster, AActor)
public:
	void NoBlockingSet ();
};

FState ALoremaster::States[] =
{
#define S_PRIEST_STND 0
	S_NORMAL (PRST, 'A', 10, A_Look,				&States[S_PRIEST_STND+1]),
	S_NORMAL (PRST, 'B', 10, A_SentinelBob,			&States[S_PRIEST_STND]),

#define S_PRIEST_RUN (S_PRIEST_STND+2)
	S_NORMAL (PRST, 'A',  4, A_Chase,				&States[S_PRIEST_RUN+1]),
	S_NORMAL (PRST, 'A',  4, A_SentinelBob,			&States[S_PRIEST_RUN+2]),
	S_NORMAL (PRST, 'B',  4, A_Chase,				&States[S_PRIEST_RUN+3]),
	S_NORMAL (PRST, 'B',  4, A_SentinelBob,			&States[S_PRIEST_RUN+4]),
	S_NORMAL (PRST, 'C',  4, A_Chase,				&States[S_PRIEST_RUN+5]),
	S_NORMAL (PRST, 'C',  4, A_SentinelBob,			&States[S_PRIEST_RUN+6]),
	S_NORMAL (PRST, 'D',  4, A_Chase,				&States[S_PRIEST_RUN+7]),
	S_NORMAL (PRST, 'D',  4, A_SentinelBob,			&States[S_PRIEST_RUN]),

#define S_PRIEST_MELEE (S_PRIEST_RUN+8)
	S_NORMAL (PRST, 'E',  4, A_FaceTarget,			&States[S_PRIEST_MELEE+1]),
	S_NORMAL (PRST, 'F',  4, A_20538,				&States[S_PRIEST_MELEE+2]),
	S_NORMAL (PRST, 'E',  4, A_SentinelBob,			&States[S_PRIEST_RUN]),

#define S_PRIEST_MISSILE (S_PRIEST_MELEE+3)
	S_NORMAL (PRST, 'E',  4, A_FaceTarget,			&States[S_PRIEST_MISSILE+1]),
	S_NORMAL (PRST, 'F',  4, A_20598,				&States[S_PRIEST_MISSILE+2]),
	S_NORMAL (PRST, 'E',  4, A_SentinelBob,			&States[S_PRIEST_RUN]),

#define S_PRIEST_DIE (S_PRIEST_MISSILE+3)
	S_NORMAL (PDED, 'A',  6, NULL,					&States[S_PRIEST_DIE+1]),
	S_NORMAL (PDED, 'B',  6, A_Scream,				&States[S_PRIEST_DIE+2]),
	S_NORMAL (PDED, 'C',  6, NULL,					&States[S_PRIEST_DIE+3]),
	S_NORMAL (PDED, 'D',  6, A_NoBlocking,			&States[S_PRIEST_DIE+4]),
	S_NORMAL (PDED, 'E',  6, NULL,					&States[S_PRIEST_DIE+5]),
	S_NORMAL (PDED, 'F',  5, NULL,					&States[S_PRIEST_DIE+6]),
	S_NORMAL (PDED, 'G',  5, NULL,					&States[S_PRIEST_DIE+7]),
	S_NORMAL (PDED, 'H',  5, NULL,					&States[S_PRIEST_DIE+8]),
	S_NORMAL (PDED, 'I',  5, NULL,					&States[S_PRIEST_DIE+9]),
	S_NORMAL (PDED, 'J',  5, NULL,					&States[S_PRIEST_DIE+10]),
	S_NORMAL (PDED, 'K',  5, NULL,					&States[S_PRIEST_DIE+11]),
	S_NORMAL (PDED, 'L',  5, NULL,					&States[S_PRIEST_DIE+12]),
	S_NORMAL (PDED, 'M',  4, NULL,					&States[S_PRIEST_DIE+13]),
	S_NORMAL (PDED, 'N',  4, NULL,					&States[S_PRIEST_DIE+14]),
	S_NORMAL (PDED, 'O',  4, NULL,					&States[S_PRIEST_DIE+15]),
	S_NORMAL (PDED, 'P',  4, NULL,					&States[S_PRIEST_DIE+16]),
	S_NORMAL (PDED, 'Q',  4, A_SpawnSpectre5,		&States[S_PRIEST_DIE+17]),
	S_NORMAL (PDED, 'R',  4, NULL,					&States[S_PRIEST_DIE+18]),
	S_NORMAL (PDED, 'S',  4, NULL,					&States[S_PRIEST_DIE+19]),
	S_NORMAL (PDED, 'T', -1, NULL,					NULL),
};

IMPLEMENT_ACTOR (ALoremaster, Strife, 12, 0)
	PROP_StrifeType (66)
	PROP_StrifeTeaserType (63)
	PROP_StrifeTeaserType2 (64)
	PROP_SpawnHealth (800)
	PROP_SpawnState (S_PRIEST_STND)
	PROP_SeeState (S_PRIEST_RUN)
	PROP_MeleeState (S_PRIEST_MELEE)
	PROP_MissileState (S_PRIEST_MISSILE)
	PROP_DeathState (S_PRIEST_DIE)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (56)
	PROP_FloatSpeed (5)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_FLOAT|
				MF_NOBLOOD|MF_COUNTKILL|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_INCOMBAT|MF4_LOOKALLAROUND|MF4_FIRERESIST|MF4_NOICEDEATH)
	PROP_MinMissileChance (150)
	PROP_Tag ("PRIEST")
	PROP_SeeSound ("loremaster/sight")
	PROP_AttackSound ("loremaster/attack")
	PROP_PainSound ("loremaster/pain")
	PROP_DeathSound ("loremaster/death")
	PROP_ActiveSound ("loremaster/active")
	PROP_Obituary ("$OB_LOREMASTER")
END_DEFAULTS


//============================================================================
//
// ALoremaster :: NoBlockingSet
//
//============================================================================

void ALoremaster::NoBlockingSet ()
{
	P_DropItem (this, "Junk", -1, 256);
}

// Loremaster Projectile ----------------------------------------------------

class ALoreShot : public AActor
{
	DECLARE_ACTOR (ALoreShot, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState ALoreShot::States[] =
{
	S_NORMAL (OCLW, 'A', 2, A_205b0,	&States[0]),
	S_NORMAL (CCLW, 'A', 6, NULL,		NULL)
};

IMPLEMENT_ACTOR (ALoreShot, Strife, -1, 0)
	PROP_StrifeType (97)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_SpeedFixed (20)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (14)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_SeeSound ("loremaster/chain")
	PROP_ActiveSound ("loremaster/swish")
END_DEFAULTS

int ALoreShot::DoSpecialDamage (AActor *target, int damage)
{
	FVector3 thrust;
	
	if (this->target != NULL)
	{
		thrust.X = float(this->target->x - target->x);
		thrust.Y = float(this->target->y - target->y);
		thrust.Z = float(this->target->z - target->z);
	
		thrust.MakeUnit();
		thrust *= float((255*50*FRACUNIT) / (target->Mass ? target->Mass : 1));
	
		target->momx += fixed_t(thrust.X);
		target->momy += fixed_t(thrust.Y);
		target->momz += fixed_t(thrust.Z);
	}
	return damage;
}

// Loremaster Subprojectile -------------------------------------------------

class ALoreShot2 : public AActor
{
	DECLARE_ACTOR (ALoreShot2, AActor)
};

FState ALoreShot2::States[] =
{
	S_NORMAL (TEND, 'A', 20, NULL, NULL)
};

IMPLEMENT_ACTOR (ALoreShot2, Strife, -1, 0)
	PROP_StrifeType (98)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_SeeSound ("loremaster/active")
END_DEFAULTS

void A_20538 (AActor *self)
{
	int damage;

	if (self->target == NULL)
		return;

	damage = (pr_atk1() & 9) * 5;

	P_DamageMobj (self->target, self, self, damage, NAME_None);
	P_TraceBleed (damage, self->target, self);
}

void A_20598 (AActor *self)
{
	if (self->target != NULL)
	{
		P_SpawnMissile (self, self->target, RUNTIME_CLASS(ALoreShot));
	}
}

void A_205b0 (AActor *self)
{
	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn<ALoreShot2> (self->x, self->y, self->z, ALLOW_REPLACE);
	Spawn<ALoreShot2> (self->x - (self->momx >> 1), self->y - (self->momy >> 1), self->z - (self->momz >> 1), ALLOW_REPLACE);
	Spawn<ALoreShot2> (self->x - self->momx, self->y - self->momy, self->z - self->momz, ALLOW_REPLACE);
}
