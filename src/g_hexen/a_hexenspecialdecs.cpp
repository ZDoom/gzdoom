/*
** Decorations that do special things
*/

#include "actor.h"
#include "info.h"
#include "a_action.h"
#include "p_enemy.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_hexenglobal.h"

static FRandom pr_pottery ("PotteryExplode");
static FRandom pr_bit ("PotteryChooseBit");
static FRandom pr_drip ("CorpseBloodDrip");
static FRandom pr_foo ("CorpseExplode");
static FRandom pr_leaf ("LeafSpawn");
static FRandom pr_leafthrust ("LeafThrust");
static FRandom pr_leafcheck ("LeafCheck");
static FRandom pr_shroom ("PoisonShroom");
static FRandom pr_soaexplode ("SoAExplode");

// Pottery1 ------------------------------------------------------------------

void A_PotteryExplode (AActor *);
void A_PotteryChooseBit (AActor *);
void A_PotteryCheck (AActor *);

class APottery1 : public AActor
{
	DECLARE_CLASS (APottery1, AActor)
public:
	void HitFloor ();
};

IMPLEMENT_CLASS (APottery1)

void APottery1::HitFloor ()
{
	Super::HitFloor ();
	P_DamageMobj (this, NULL, NULL, 25, NAME_None);
}

//============================================================================
//
// A_PotteryExplode
//
//============================================================================

void A_PotteryExplode (AActor *actor)
{
	AActor *mo = NULL;
	int i;

	for(i = (pr_pottery()&3)+3; i; i--)
	{
		mo = Spawn ("PotteryBit", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		mo->SetState (mo->SpawnState + (pr_pottery()%5));
		if (mo)
		{
			mo->momz = ((pr_pottery()&7)+5)*(3*FRACUNIT/4);
			mo->momx = (pr_pottery.Random2())<<(FRACBITS-6);
			mo->momy = (pr_pottery.Random2())<<(FRACBITS-6);
		}
	}
	S_Sound (mo, CHAN_BODY, "PotteryExplode", 1, ATTN_NORM);
	if (actor->args[0]>=0 && actor->args[0]<=255 && SpawnableThings[actor->args[0]])
	{ // Spawn an item
		if (!((level.flags & LEVEL_NOMONSTERS) || (dmflags & DF_NO_MONSTERS))
		|| !(GetDefaultByType (SpawnableThings[actor->args[0]])->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (SpawnableThings[actor->args[0]],
				actor->x, actor->y, actor->z, ALLOW_REPLACE);
		}
	}
}

//============================================================================
//
// A_PotteryChooseBit
//
//============================================================================

void A_PotteryChooseBit (AActor *actor)
{
	actor->SetState (actor->FindState(NAME_Death) + 1 + 2*(pr_bit()%5));
	actor->tics = 256+(pr_bit()<<1);
}

//============================================================================
//
// A_PotteryCheck
//
//============================================================================

void A_PotteryCheck (AActor *actor)
{
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			AActor *pmo = players[i].mo;
			if (P_CheckSight (actor, pmo) && (abs (R_PointToAngle2 (pmo->x,
				pmo->y, actor->x, actor->y) - pmo->angle) <= ANGLE_45))
			{ // Previous state (pottery bit waiting state)
				actor->SetState (actor->state - 1);
				return;
			}
		}
	}		
}

// Lynched corpse (no heart) ------------------------------------------------

void A_CorpseBloodDrip (AActor *);

class AZCorpseLynchedNoHeart : public AActor
{
DECLARE_CLASS (AZCorpseLynchedNoHeart, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_CLASS (AZCorpseLynchedNoHeart)

void AZCorpseLynchedNoHeart::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Spawn ("BloodPool", x, y, ONFLOORZ, ALLOW_REPLACE);
}

//============================================================================
//
// A_CorpseBloodDrip
//
//============================================================================

void A_CorpseBloodDrip (AActor *actor)
{
	if (pr_drip() <= 128)
	{
		Spawn ("CorpseBloodDrip", actor->x, actor->y, actor->z + actor->height/2, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_CorpseExplode
//
//============================================================================

void A_CorpseExplode (AActor *actor)
{
	AActor *mo;
	int i;

	for (i = (pr_foo()&3)+3; i; i--)
	{
		mo = Spawn ("CorpseBit", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		mo->SetState (mo->SpawnState + (pr_foo()%3));
		if (mo)
		{
			mo->momz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
			mo->momx = pr_foo.Random2()<<(FRACBITS-6);
			mo->momy = pr_foo.Random2()<<(FRACBITS-6);
		}
	}
	// Spawn a skull
	mo = Spawn ("CorpseBit", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	mo->SetState (mo->SpawnState + 3);
	if (mo)
	{
		mo->momz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
		mo->momx = pr_foo.Random2()<<(FRACBITS-6);
		mo->momy = pr_foo.Random2()<<(FRACBITS-6);
	}
	S_Sound (actor, CHAN_BODY, actor->DeathSound, 1, ATTN_IDLE);
	actor->Destroy ();
}

//============================================================================
//
// A_LeafSpawn
//
//============================================================================

void A_LeafSpawn (AActor *actor)
{
	AActor *mo;
	int i;

	for (i = (pr_leaf()&3)+1; i; i--)
	{
		mo = Spawn (pr_leaf()&1 ? PClass::FindClass ("Leaf1") : PClass::FindClass ("Leaf2"),
			actor->x + (pr_leaf.Random2()<<14),
			actor->y + (pr_leaf.Random2()<<14),
			actor->z + (pr_leaf()<<14), ALLOW_REPLACE);
		if (mo)
		{
			P_ThrustMobj (mo, actor->angle, (pr_leaf()<<9)+3*FRACUNIT);
			mo->target = actor;
			mo->special1 = 0;
		}
	}
}

//============================================================================
//
// A_LeafThrust
//
//============================================================================

void A_LeafThrust (AActor *actor)
{
	if (pr_leafthrust() <= 96)
	{
		actor->momz += (pr_leafthrust()<<9)+FRACUNIT;
	}
}

//============================================================================
//
// A_LeafCheck
//
//============================================================================

void A_LeafCheck (AActor *actor)
{
	actor->special1++;
	if (actor->special1 >= 20)
	{
		actor->SetState (NULL);
		return;
	}
	angle_t ang = actor->target ? actor->target->angle : actor->angle;
	if (pr_leafcheck() > 64)
	{
		if (!actor->momx && !actor->momy)
		{
			P_ThrustMobj (actor, ang, (pr_leafcheck()<<9)+FRACUNIT);
		}
		return;
	}
	actor->SetState (actor->SpawnState + 7);
	actor->momz = (pr_leafcheck()<<9)+FRACUNIT;
	P_ThrustMobj (actor, ang, (pr_leafcheck()<<9)+2*FRACUNIT);
	actor->flags |= MF_MISSILE;
}

//===========================================================================
//
// A_PoisonShroom
//
//===========================================================================

void A_PoisonShroom (AActor *actor)
{
	actor->tics = 128+(pr_shroom()<<1);
}

//===========================================================================
//
// A_SoAExplode - Suit of Armor Explode
//
//===========================================================================

void A_SoAExplode (AActor *actor)
{
	AActor *mo;
	int i;

	for (i = 0; i < 10; i++)
	{
		mo = Spawn ("ZArmorChunk", actor->x+((pr_soaexplode()-128)<<12),
			actor->y+((pr_soaexplode()-128)<<12), 
			actor->z+(pr_soaexplode()*actor->height/256), ALLOW_REPLACE);
		mo->SetState (mo->SpawnState + i);
		if (mo)
		{
			mo->momz = ((pr_soaexplode()&7)+5)*FRACUNIT;
			mo->momx = pr_soaexplode.Random2()<<(FRACBITS-6);
			mo->momy = pr_soaexplode.Random2()<<(FRACBITS-6);
		}
	}
	if (actor->args[0]>=0 && actor->args[0]<=255 && SpawnableThings[actor->args[0]])
	{ // Spawn an item
		if (!((level.flags & LEVEL_NOMONSTERS) || (dmflags & DF_NO_MONSTERS))
		|| !(GetDefaultByType (SpawnableThings[actor->args[0]])->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (SpawnableThings[actor->args[0]],
				actor->x, actor->y, actor->z, ALLOW_REPLACE);
		}
	}
	S_Sound (actor, CHAN_BODY, actor->DeathSound, 1, ATTN_NORM);
	actor->Destroy ();
}

// Bell ---------------------------------------------------------------------

void A_BellReset1 (AActor *);
void A_BellReset2 (AActor *);

class AZBell : public AActor
{
	DECLARE_CLASS (AZBell, AActor)
public:
	void Activate (AActor *activator);
};

IMPLEMENT_CLASS (AZBell)

void AZBell::Activate (AActor *activator)
{
	if (health > 0)
	{
		P_DamageMobj (this, activator, activator, 10, NAME_Melee); // 'ring' the bell
	}
}

//===========================================================================
//
// A_BellReset1
//
//===========================================================================

void A_BellReset1 (AActor *actor)
{
	actor->flags |= MF_NOGRAVITY;
	actor->height <<= 2;
	if (actor->special)
	{ // Initiate death action
		LineSpecials[actor->special] (NULL, NULL, false, actor->args[0],
			actor->args[1], actor->args[2], actor->args[3], actor->args[4]);
		actor->special = 0;
	}
}

//===========================================================================
//
// A_BellReset2
//
//===========================================================================

void A_BellReset2 (AActor *actor)
{
	actor->flags |= MF_SHOOTABLE;
	actor->flags &= ~MF_CORPSE;
	actor->health = 5;
}

