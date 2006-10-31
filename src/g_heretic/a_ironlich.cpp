#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_foo ("WhirlwindDamage");
static FRandom pr_atk ("LichAttack");
static FRandom pr_seek ("WhirlwindSeek");

void A_LichAttack (AActor *);
void A_LichIceImpact (AActor *);
void A_LichFireGrow (AActor *);
void A_WhirlwindSeek (AActor *);

// Ironlich -----------------------------------------------------------------

class AIronlich : public AActor
{
	DECLARE_ACTOR (AIronlich, AActor)
public:
	void NoBlockingSet ();
};

FState AIronlich::States[] =
{
#define S_HEAD_LOOK 0
	S_NORMAL (LICH, 'A',   10, A_Look					, &States[S_HEAD_LOOK]),

#define S_HEAD_FLOAT (S_HEAD_LOOK+1)
	S_NORMAL (LICH, 'A',	4, A_Chase					, &States[S_HEAD_FLOAT]),

#define S_HEAD_ATK (S_HEAD_FLOAT+1)
	S_NORMAL (LICH, 'A',	5, A_FaceTarget 			, &States[S_HEAD_ATK+1]),
	S_NORMAL (LICH, 'B',   20, A_LichAttack 			, &States[S_HEAD_FLOAT]),

#define S_HEAD_PAIN (S_HEAD_ATK+2)
	S_NORMAL (LICH, 'A',	4, NULL 					, &States[S_HEAD_PAIN+1]),
	S_NORMAL (LICH, 'A',	4, A_Pain					, &States[S_HEAD_FLOAT]),

#define S_HEAD_DIE (S_HEAD_PAIN+2)
	S_NORMAL (LICH, 'C',	7, NULL 					, &States[S_HEAD_DIE+1]),
	S_NORMAL (LICH, 'D',	7, A_Scream 				, &States[S_HEAD_DIE+2]),
	S_NORMAL (LICH, 'E',	7, NULL 					, &States[S_HEAD_DIE+3]),
	S_NORMAL (LICH, 'F',	7, NULL 					, &States[S_HEAD_DIE+4]),
	S_NORMAL (LICH, 'G',	7, A_NoBlocking 			, &States[S_HEAD_DIE+5]),
	S_NORMAL (LICH, 'H',	7, NULL 					, &States[S_HEAD_DIE+6]),
	S_NORMAL (LICH, 'I',   -1, A_BossDeath				, NULL)
};

IMPLEMENT_ACTOR (AIronlich, Heretic, 6, 20)
	PROP_SpawnHealth (700)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (72)
	PROP_Mass (325)
	PROP_SpeedFixed (6)
	PROP_PainChance (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_NOBLOOD)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH|MF3_DONTSQUASH)
	PROP_Flags4 (MF4_BOSSDEATH)

	PROP_SpawnState (S_HEAD_LOOK)
	PROP_SeeState (S_HEAD_FLOAT)
	PROP_PainState (S_HEAD_PAIN)
	PROP_MissileState (S_HEAD_ATK)
	PROP_DeathState (S_HEAD_DIE)

	PROP_SeeSound ("ironlich/sight")
	PROP_AttackSound ("ironlich/attack")
	PROP_PainSound ("ironlich/pain")
	PROP_DeathSound ("ironlich/death")
	PROP_ActiveSound ("ironlich/active")
	PROP_Obituary("$OB_IRONLICH")
	PROP_HitObituary("$OB_IRONLICHHIT")
END_DEFAULTS

void AIronlich::NoBlockingSet ()
{
	P_DropItem (this, "BlasterAmmo", 10, 84);
	P_DropItem (this, "ArtiEgg", 0, 51);
}

// Head FX 1 ----------------------------------------------------------------

class AHeadFX1 : public AActor
{
	DECLARE_ACTOR (AHeadFX1, AActor)
};

FState AHeadFX1::States[] =
{
#define S_HEADFX1 0
	S_BRIGHT (FX05, 'A',	6, NULL 					, &States[S_HEADFX1+1]),
	S_BRIGHT (FX05, 'B',	6, NULL 					, &States[S_HEADFX1+2]),
	S_BRIGHT (FX05, 'C',	6, NULL 					, &States[S_HEADFX1+0]),

#define S_HEADFXI1 (S_HEADFX1+3)
	S_BRIGHT (FX05, 'D',	5, A_LichIceImpact			, &States[S_HEADFXI1+1]),
	S_BRIGHT (FX05, 'E',	5, NULL 					, &States[S_HEADFXI1+2]),
	S_BRIGHT (FX05, 'F',	5, NULL 					, &States[S_HEADFXI1+3]),
	S_BRIGHT (FX05, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHeadFX1, Heretic, -1, 164)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (13)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_THRUGHOST)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_HEADFX1)
	PROP_DeathState (S_HEADFXI1)
END_DEFAULTS

AT_SPEED_SET (HeadFX1, speed)
{
	SimpleSpeedSetter (AHeadFX1, 13*FRACUNIT, 20*FRACUNIT, speed);
}

// Head FX 2 ----------------------------------------------------------------

class AHeadFX2 : public AActor
{
	DECLARE_ACTOR (AHeadFX2, AActor)
};

FState AHeadFX2::States[] =
{
#define S_HEADFX2 0
	S_BRIGHT (FX05, 'H',	6, NULL 					, &States[S_HEADFX2+1]),
	S_BRIGHT (FX05, 'I',	6, NULL 					, &States[S_HEADFX2+2]),
	S_BRIGHT (FX05, 'J',	6, NULL 					, &States[S_HEADFX2+0]),

#define S_HEADFXI2 (S_HEADFX2+3)
	S_BRIGHT (FX05, 'D',	5, NULL 					, &States[S_HEADFXI2+1]),
	S_BRIGHT (FX05, 'E',	5, NULL 					, &States[S_HEADFXI2+2]),
	S_BRIGHT (FX05, 'F',	5, NULL 					, &States[S_HEADFXI2+3]),
	S_BRIGHT (FX05, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHeadFX2, Heretic, -1, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (8)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_HEADFX2)
	PROP_DeathState (S_HEADFXI2)
END_DEFAULTS

// Head FX 3 ----------------------------------------------------------------

class AHeadFX3 : public AActor
{
	DECLARE_ACTOR (AHeadFX3, AActor)
};

FState AHeadFX3::States[] =
{
#define S_HEADFX3 0
	S_BRIGHT (FX06, 'A',	4, A_LichFireGrow			, &States[S_HEADFX3+1]),
	S_BRIGHT (FX06, 'B',	4, A_LichFireGrow			, &States[S_HEADFX3+2]),
	S_BRIGHT (FX06, 'C',	4, A_LichFireGrow			, &States[S_HEADFX3+0]),
	S_BRIGHT (FX06, 'A',	5, NULL 					, &States[S_HEADFX3+4]),
	S_BRIGHT (FX06, 'B',	5, NULL 					, &States[S_HEADFX3+5]),
	S_BRIGHT (FX06, 'C',	5, NULL 					, &States[S_HEADFX3+3]),

#define S_HEADFXI3 (S_HEADFX3+6)
	S_BRIGHT (FX06, 'D',	5, NULL 					, &States[S_HEADFXI3+1]),
	S_BRIGHT (FX06, 'E',	5, NULL 					, &States[S_HEADFXI3+2]),
	S_BRIGHT (FX06, 'F',	5, NULL 					, &States[S_HEADFXI3+3]),
	S_BRIGHT (FX06, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHeadFX3, Heretic, -1, 0)
	PROP_RadiusFixed (14)
	PROP_HeightFixed (12)
	PROP_SpeedFixed (10)
	PROP_Damage (5)
	PROP_Flags (MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_HEADFX3)
	PROP_DeathState (S_HEADFXI3)
END_DEFAULTS

AT_SPEED_SET (HeadFX3, speed)
{
	SimpleSpeedSetter (AHeadFX3, 10*FRACUNIT, 18*FRACUNIT, speed);
}

// Whirlwind ----------------------------------------------------------------

class AWhirlwind : public AActor
{
	DECLARE_ACTOR (AWhirlwind, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState AWhirlwind::States[] =
{
#define S_HEADFX4 0
	S_NORMAL (FX07, 'D',	3, NULL 					, &States[S_HEADFX4+1]),
	S_NORMAL (FX07, 'E',	3, NULL 					, &States[S_HEADFX4+2]),
	S_NORMAL (FX07, 'F',	3, NULL 					, &States[S_HEADFX4+3]),
	S_NORMAL (FX07, 'G',	3, NULL 					, &States[S_HEADFX4+4]),
	S_NORMAL (FX07, 'A',	3, A_WhirlwindSeek			, &States[S_HEADFX4+5]),
	S_NORMAL (FX07, 'B',	3, A_WhirlwindSeek			, &States[S_HEADFX4+6]),
	S_NORMAL (FX07, 'C',	3, A_WhirlwindSeek			, &States[S_HEADFX4+4]),

#define S_HEADFXI4 (S_HEADFX4+7)
	S_NORMAL (FX07, 'G',	4, NULL 					, &States[S_HEADFXI4+1]),
	S_NORMAL (FX07, 'F',	4, NULL 					, &States[S_HEADFXI4+2]),
	S_NORMAL (FX07, 'E',	4, NULL 					, &States[S_HEADFXI4+3]),
	S_NORMAL (FX07, 'D',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AWhirlwind, Heretic, -1, 165)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (74)
	PROP_SpeedFixed (10)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_SEEKERMISSILE)
	PROP_Flags3 (MF3_EXPLOCOUNT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (S_HEADFX4)
	PROP_DeathState (S_HEADFXI4)
END_DEFAULTS

int AWhirlwind::DoSpecialDamage (AActor *target, int damage)
{
	int randVal;

	target->angle += pr_foo.Random2() << 20;
	target->momx += pr_foo.Random2() << 10;
	target->momy += pr_foo.Random2() << 10;
	if ((level.time & 16) && !(target->flags2 & MF2_BOSS))
	{
		randVal = pr_foo();
		if (randVal > 160)
		{
			randVal = 160;
		}
		target->momz += randVal << 11;
		if (target->momz > 12*FRACUNIT)
		{
			target->momz = 12*FRACUNIT;
		}
	}
	if (!(level.time & 7))
	{
		P_DamageMobj (target, NULL, this->target, 3, NAME_Melee);
	}
	return -1;
}

//----------------------------------------------------------------------------
//
// PROC A_LichAttack
//
//----------------------------------------------------------------------------

void A_LichAttack (AActor *actor)
{
	int i;
	AActor *fire;
	AActor *baseFire;
	AActor *mo;
	AActor *target;
	int randAttack;
	static const int atkResolve1[] = { 50, 150 };
	static const int atkResolve2[] = { 150, 200 };
	int dist;

	// Ice ball		(close 20% : far 60%)
	// Fire column	(close 40% : far 20%)
	// Whirlwind	(close 40% : far 20%)
	// Distance threshold = 8 cells

	target = actor->target;
	if (target == NULL)
	{
		return;
	}
	A_FaceTarget (actor);
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_atk.HitDice (6);
		P_DamageMobj (target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, target, actor);
		return;
	}
	dist = P_AproxDistance (actor->x-target->x, actor->y-target->y)
		> 8*64*FRACUNIT;
	randAttack = pr_atk ();
	if (randAttack < atkResolve1[dist])
	{ // Ice ball
		P_SpawnMissile (actor, target, RUNTIME_CLASS(AHeadFX1));
		S_Sound (actor, CHAN_BODY, "ironlich/attack2", 1, ATTN_NORM);
	}
	else if (randAttack < atkResolve2[dist])
	{ // Fire column
		baseFire = P_SpawnMissile (actor, target, RUNTIME_CLASS(AHeadFX3));
		if (baseFire != NULL)
		{
			baseFire->SetState (&AHeadFX3::States[S_HEADFX3+3]); // Don't grow
			for (i = 0; i < 5; i++)
			{
				fire = Spawn<AHeadFX3> (baseFire->x, baseFire->y,
					baseFire->z, ALLOW_REPLACE);
				if (i == 0)
				{
					S_Sound (actor, CHAN_BODY, "ironlich/attack1", 1, ATTN_NORM);
				}
				fire->target = baseFire->target;
				fire->angle = baseFire->angle;
				fire->momx = baseFire->momx;
				fire->momy = baseFire->momy;
				fire->momz = baseFire->momz;
				fire->Damage = 0;
				fire->health = (i+1) * 2;
				P_CheckMissileSpawn (fire);
			}
		}
	}
	else
	{ // Whirlwind
		mo = P_SpawnMissile (actor, target, RUNTIME_CLASS(AWhirlwind));
		if (mo != NULL)
		{
			mo->z -= 32*FRACUNIT;
			mo->tracer = target;
			mo->special1 = 60;
			mo->special2 = 50; // Timer for active sound
			mo->health = 20*TICRATE; // Duration
			S_Sound (actor, CHAN_BODY, "ironlich/attack3", 1, ATTN_NORM);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_WhirlwindSeek
//
//----------------------------------------------------------------------------

void A_WhirlwindSeek (AActor *actor)
{
	actor->health -= 3;
	if (actor->health < 0)
	{
		actor->momx = actor->momy = actor->momz = 0;
		actor->SetState (actor->FindState(NAME_Death));
		actor->flags &= ~MF_MISSILE;
		return;
	}
	if ((actor->special2 -= 3) < 0)
	{
		actor->special2 = 58 + (pr_seek() & 31);
		S_Sound (actor, CHAN_BODY, "ironlich/attack3", 1, ATTN_NORM);
	}
	if (actor->tracer && actor->tracer->flags&MF_SHADOW)
	{
		return;
	}
	P_SeekerMissile (actor, ANGLE_1*10, ANGLE_1*30);
}

//----------------------------------------------------------------------------
//
// PROC A_LichIceImpact
//
//----------------------------------------------------------------------------

void A_LichIceImpact (AActor *ice)
{
	int i;
	angle_t angle;
	AActor *shard;

	for (i = 0; i < 8; i++)
	{
		shard = Spawn<AHeadFX2> (ice->x, ice->y, ice->z, ALLOW_REPLACE);
		angle = i*ANG45;
		shard->target = ice->target;
		shard->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		shard->momx = FixedMul (shard->Speed, finecosine[angle]);
		shard->momy = FixedMul (shard->Speed, finesine[angle]);
		shard->momz = -FRACUNIT*6/10;
		P_CheckMissileSpawn (shard);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_LichFireGrow
//
//----------------------------------------------------------------------------

void A_LichFireGrow (AActor *fire)
{
	fire->health--;
	fire->z += 9*FRACUNIT;
	if (fire->health == 0)
	{
		fire->Damage = fire->GetDefault()->Damage;
		fire->SetState (&AHeadFX3::States[S_HEADFX3+3]);
	}
}

