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

class AWhirlwind : public AActor
{
	DECLARE_CLASS (AWhirlwind, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS(AWhirlwind)

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
		P_SpawnMissile (actor, target, PClass::FindClass("HeadFX1"));
		S_Sound (actor, CHAN_BODY, "ironlich/attack2", 1, ATTN_NORM);
	}
	else if (randAttack < atkResolve2[dist])
	{ // Fire column
		baseFire = P_SpawnMissile (actor, target, PClass::FindClass("HeadFX3"));
		if (baseFire != NULL)
		{
			baseFire->SetState (baseFire->FindState("NoGrow"));
			for (i = 0; i < 5; i++)
			{
				fire = Spawn("HeadFX3", baseFire->x, baseFire->y,
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
		shard = Spawn("HeadFX2", ice->x, ice->y, ice->z, ALLOW_REPLACE);
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
		fire->SetState (fire->FindState("NoGrow"));
	}
}

