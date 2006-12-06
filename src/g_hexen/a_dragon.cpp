#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"

static FRandom pr_dragonseek ("DragonSeek");
static FRandom pr_dragonflight ("DragonFlight");
static FRandom pr_dragonflap ("DragonFlap");
static FRandom pr_dragonfx2 ("DragonFX2");

void A_DragonInitFlight (AActor *);
void A_DragonFlap (AActor *);
void A_DragonFlight (AActor *);
void A_DragonPain (AActor *);
void A_DragonAttack (AActor *);
void A_DragonCheckCrash (AActor *);
void A_DragonFX2 (AActor *);

// Dragon -------------------------------------------------------------------

class ADragon : public AActor
{
	DECLARE_ACTOR (ADragon, AActor)
};

FState ADragon::States[] =
{
#define S_DRAGON_LOOK1 0
	S_NORMAL (DRAG, 'D',   10, A_Look				    , &States[S_DRAGON_LOOK1]),

#define S_DRAGON_INIT (S_DRAGON_LOOK1+1)
	S_NORMAL (DRAG, 'C',	5, NULL					    , &States[S_DRAGON_INIT+1]),
	S_NORMAL (DRAG, 'B',	5, NULL					    , &States[S_DRAGON_INIT+2]),
	S_NORMAL (DRAG, 'A',	5, A_DragonInitFlight	    , &States[S_DRAGON_INIT+3]),
	S_NORMAL (DRAG, 'B',	3, A_DragonFlap			    , &States[S_DRAGON_INIT+4]),
	S_NORMAL (DRAG, 'B',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+5]),
	S_NORMAL (DRAG, 'C',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+6]),
	S_NORMAL (DRAG, 'C',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+7]),
	S_NORMAL (DRAG, 'D',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+8]),
	S_NORMAL (DRAG, 'D',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+9]),
	S_NORMAL (DRAG, 'C',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+10]),
	S_NORMAL (DRAG, 'C',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+11]),
	S_NORMAL (DRAG, 'B',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+12]),
	S_NORMAL (DRAG, 'B',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+13]),
	S_NORMAL (DRAG, 'A',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+14]),
	S_NORMAL (DRAG, 'A',	3, A_DragonFlight		    , &States[S_DRAGON_INIT+3]),

#define S_DRAGON_PAIN1 (S_DRAGON_INIT+15)
	S_NORMAL (DRAG, 'F',   10, A_DragonPain			    , &States[S_DRAGON_INIT+3]),

#define S_DRAGON_ATK1 (S_DRAGON_PAIN1+1)
	S_NORMAL (DRAG, 'E',	8, A_DragonAttack		    , &States[S_DRAGON_INIT+3]),

#define S_DRAGON_DEATH1 (S_DRAGON_ATK1+1)
	S_NORMAL (DRAG, 'G',	5, A_Scream				    , &States[S_DRAGON_DEATH1+1]),
	S_NORMAL (DRAG, 'H',	4, A_NoBlocking			    , &States[S_DRAGON_DEATH1+2]),
	S_NORMAL (DRAG, 'I',	4, NULL					    , &States[S_DRAGON_DEATH1+3]),
	S_NORMAL (DRAG, 'J',	4, A_DragonCheckCrash	    , &States[S_DRAGON_DEATH1+3]),

#define S_DRAGON_CRASH1 (S_DRAGON_DEATH1+4)
	S_NORMAL (DRAG, 'K',	5, NULL					    , &States[S_DRAGON_CRASH1+1]),
	S_NORMAL (DRAG, 'L',	5, NULL					    , &States[S_DRAGON_CRASH1+2]),
	S_NORMAL (DRAG, 'M',   -1, NULL					    , NULL),

};

IMPLEMENT_ACTOR (ADragon, Hexen, 254, 0)
	PROP_SpawnHealth (640)
	PROP_PainChance (128)
	PROP_SpeedFixed (10)
	PROP_HeightFixed (65)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_FLOAT|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_BOSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOTARGET)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_DRAGON_LOOK1)
	PROP_SeeState (S_DRAGON_INIT)
	PROP_PainState (S_DRAGON_PAIN1)
	PROP_MissileState (S_DRAGON_ATK1)
	PROP_DeathState (S_DRAGON_DEATH1)

	PROP_SeeSound ("DragonSight")
	PROP_AttackSound ("DragonAttack")
	PROP_PainSound ("DragonPain")
	PROP_DeathSound ("DragonDeath")
	PROP_ActiveSound ("DragonActive")
	PROP_Obituary ("$OB_DRAGON")
END_DEFAULTS



// Dragon Fireball ----------------------------------------------------------

class ADragonFireball : public AActor
{
	DECLARE_ACTOR (ADragonFireball, AActor)
};

FState ADragonFireball::States[] =
{
#define S_DRAGON_FX1_1 0
	S_BRIGHT (DRFX, 'A',	4, NULL					    , &States[S_DRAGON_FX1_1+1]),
	S_BRIGHT (DRFX, 'B',	4, NULL					    , &States[S_DRAGON_FX1_1+2]),
	S_BRIGHT (DRFX, 'C',	4, NULL					    , &States[S_DRAGON_FX1_1+3]),
	S_BRIGHT (DRFX, 'D',	4, NULL					    , &States[S_DRAGON_FX1_1+4]),
	S_BRIGHT (DRFX, 'E',	4, NULL					    , &States[S_DRAGON_FX1_1+5]),
	S_BRIGHT (DRFX, 'F',	4, NULL					    , &States[S_DRAGON_FX1_1]),

#define S_DRAGON_FX1_X1 (S_DRAGON_FX1_1+6)
	S_BRIGHT (DRFX, 'G',	4, NULL					    , &States[S_DRAGON_FX1_X1+1]),
	S_BRIGHT (DRFX, 'H',	4, NULL					    , &States[S_DRAGON_FX1_X1+2]),
	S_BRIGHT (DRFX, 'I',	4, NULL					    , &States[S_DRAGON_FX1_X1+3]),
	S_BRIGHT (DRFX, 'J',	4, A_DragonFX2			    , &States[S_DRAGON_FX1_X1+4]),
	S_BRIGHT (DRFX, 'K',	3, NULL					    , &States[S_DRAGON_FX1_X1+5]),
	S_BRIGHT (DRFX, 'L',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADragonFireball, Hexen, -1, 0)
	PROP_SpeedFixed (24)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (10)
	PROP_Damage (6)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_DRAGON_FX1_1)
	PROP_DeathState (S_DRAGON_FX1_X1)

	PROP_DeathSound ("DragonFireballExplode")
END_DEFAULTS

// Dragon Fireball Secondary Explosion --------------------------------------

class ADragonExplosion : public AActor
{
	DECLARE_ACTOR (ADragonExplosion, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
};

FState ADragonExplosion::States[] =
{
#define S_DRAGON_FX2_1 0
	S_BRIGHT (CFCF, 'Q',	1, NULL					    , &States[S_DRAGON_FX2_1+1]),
	S_BRIGHT (CFCF, 'Q',	4, A_UnHideThing		    , &States[S_DRAGON_FX2_1+2]),
	S_BRIGHT (CFCF, 'R',	3, A_Scream				    , &States[S_DRAGON_FX2_1+3]),
	S_BRIGHT (CFCF, 'S',	4, NULL					    , &States[S_DRAGON_FX2_1+4]),
	S_BRIGHT (CFCF, 'T',	3, A_Explode			    , &States[S_DRAGON_FX2_1+5]),
	S_BRIGHT (CFCF, 'U',	4, NULL					    , &States[S_DRAGON_FX2_1+6]),
	S_BRIGHT (CFCF, 'V',	3, NULL					    , &States[S_DRAGON_FX2_1+7]),
	S_BRIGHT (CFCF, 'W',	4, NULL					    , &States[S_DRAGON_FX2_1+8]),
	S_BRIGHT (CFCF, 'X',	3, NULL					    , &States[S_DRAGON_FX2_1+9]),
	S_BRIGHT (CFCF, 'Y',	4, NULL					    , &States[S_DRAGON_FX2_1+10]),
	S_BRIGHT (CFCF, 'Z',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADragonExplosion, Hexen, -1, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_DRAGON_FX2_1)

	PROP_DeathSound ("DragonFireballExplode")
END_DEFAULTS

void ADragonExplosion::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 80;
	hurtSource = false;
}

//============================================================================
//
// DragonSeek
//
//============================================================================

static void DragonSeek (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	int i;
	angle_t bestAngle;
	angle_t angleToSpot, angleToTarget;
	AActor *mo;

	target = actor->tracer;
	if(target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[angle]);
	actor->momy = FixedMul (actor->Speed, finesine[angle]);
	if (actor->z+actor->height < target->z ||
		target->z+target->height < actor->z)
	{
		dist = P_AproxDistance(target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->momz = (target->z-actor->z)/dist;
	}
	else
	{
		dist = P_AproxDistance (target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
	}
	if (target->flags&MF_SHOOTABLE && pr_dragonseek() < 64)
	{ // attack the destination mobj if it's attackable
		AActor *oldTarget;
	
		if (abs(actor->angle-R_PointToAngle2(actor->x, actor->y, 
			target->x, target->y)) < ANGLE_45/2)
		{
			oldTarget = actor->target;
			actor->target = target;
			if (actor->CheckMeleeRange ())
			{
				int damage = pr_dragonseek.HitDice (10);
				P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
				P_TraceBleed (damage, actor->target, actor);
				S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
			}
			else if (pr_dragonseek() < 128 && P_CheckMissileRange(actor))
			{
				P_SpawnMissile(actor, target, RUNTIME_CLASS(ADragonFireball));						
				S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
			}
			actor->target = oldTarget;
		}
	}
	if (dist < 4)
	{ // Hit the target thing
		if (actor->target && pr_dragonseek() < 200)
		{
			AActor *bestActor = NULL;
			bestAngle = ANGLE_MAX;
			angleToTarget = R_PointToAngle2(actor->x, actor->y,
				actor->target->x, actor->target->y);
			for (i = 0; i < 5; i++)
			{
				if (!target->args[i])
				{
					continue;
				}
				FActorIterator iterator (target->args[i]);
				mo = iterator.Next ();
				if (mo == NULL)
				{
					continue;
				}
				angleToSpot = R_PointToAngle2(actor->x, actor->y, 
					mo->x, mo->y);
				if ((angle_t)abs(angleToSpot-angleToTarget) < bestAngle)
				{
					bestAngle = abs(angleToSpot-angleToTarget);
					bestActor = mo;
				}
			}
			if (bestActor != NULL)
			{
				actor->tracer = bestActor;
			}
		}
		else
		{
			// [RH] Don't lock up if the dragon doesn't have any
			// targets defined
			for (i = 0; i < 5; ++i)
			{
				if (target->args[i] != 0)
				{
					break;
				}
			}
			if (i < 5)
			{
				do
				{
					i = (pr_dragonseek()>>2)%5;
				} while(!target->args[i]);
				FActorIterator iterator (target->args[i]);
				actor->tracer = iterator.Next ();
			}
		}
	}
}

//============================================================================
//
// A_DragonInitFlight
//
//============================================================================

void A_DragonInitFlight (AActor *actor)
{
	FActorIterator iterator (actor->tid);

	do
	{ // find the first tid identical to the dragon's tid
		actor->tracer = iterator.Next ();
		if (actor->tracer == NULL)
		{
			actor->SetState (actor->SpawnState);
			return;
		}
	} while (actor->tracer == actor);
	actor->RemoveFromHash ();
}

//============================================================================
//
// A_DragonFlight
//
//============================================================================

void A_DragonFlight (AActor *actor)
{
	angle_t angle;

	DragonSeek (actor, 4*ANGLE_1, 8*ANGLE_1);
	if (actor->target)
	{
		if(!(actor->target->flags&MF_SHOOTABLE))
		{ // target died
			actor->target = NULL;
			return;
		}
		angle = R_PointToAngle2(actor->x, actor->y, actor->target->x,
			actor->target->y);
		if (abs(actor->angle-angle) < ANGLE_45/2 && actor->CheckMeleeRange())
		{
			int damage = pr_dragonflight.HitDice (8);
			P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
			P_TraceBleed (damage, actor->target, actor);
			S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
		}
		else if (abs(actor->angle-angle) <= ANGLE_1*20)
		{
			actor->SetState (actor->MissileState);
			S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
		}
	}
	else
	{
		P_LookForPlayers (actor, true);
	}
}

//============================================================================
//
// A_DragonFlap
//
//============================================================================

void A_DragonFlap (AActor *actor)
{
	A_DragonFlight (actor);
	if (pr_dragonflap() < 240)
	{
		S_Sound (actor, CHAN_BODY, "DragonWingflap", 1, ATTN_NORM);
	}
	else
	{
		actor->PlayActiveSound ();
	}
}

//============================================================================
//
// A_DragonAttack
//
//============================================================================

void A_DragonAttack (AActor *actor)
{
	P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ADragonFireball));						
}

//============================================================================
//
// A_DragonFX2
//
//============================================================================

void A_DragonFX2 (AActor *actor)
{
	AActor *mo;
	int i;
	int delay;

	delay = 16+(pr_dragonfx2()>>3);
	for (i = 1+(pr_dragonfx2()&3); i; i--)
	{
		fixed_t x = actor->x+((pr_dragonfx2()-128)<<14);
		fixed_t y = actor->y+((pr_dragonfx2()-128)<<14);
		fixed_t z = actor->z+((pr_dragonfx2()-128)<<12);

		mo = Spawn<ADragonExplosion> (x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->tics = delay+(pr_dragonfx2()&3)*i*2;
			mo->target = actor->target;
		}
	} 
}

//============================================================================
//
// A_DragonPain
//
//============================================================================

void A_DragonPain (AActor *actor)
{
	A_Pain (actor);
	if (!actor->tracer)
	{ // no destination spot yet
		actor->SetState (&ADragon::States[S_DRAGON_INIT]);
	}
}

//============================================================================
//
// A_DragonCheckCrash
//
//============================================================================

void A_DragonCheckCrash (AActor *actor)
{
	if (actor->z <= actor->floorz)
	{
		actor->SetState (&ADragon::States[S_DRAGON_CRASH1]);
	}
}
