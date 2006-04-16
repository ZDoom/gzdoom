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

// SwitchableDecoration: Activate and Deactivate change state ---------------

IMPLEMENT_ABSTRACT_ACTOR (ASwitchableDecoration)

void ASwitchableDecoration::Activate (AActor *activator)
{
	SetState (SeeState);
}

void ASwitchableDecoration::Deactivate (AActor *activator)
{
	SetState (MeleeState);
}

// SwitchingDecoration: Only Activate changes state -------------------------

class ASwitchingDecoration : public ASwitchableDecoration
{
	DECLARE_STATELESS_ACTOR (ASwitchingDecoration, ASwitchableDecoration)
public:
	void Deactivate (AActor *activator) {}
};

IMPLEMENT_ABSTRACT_ACTOR (ASwitchingDecoration)

// Winged Statue (no skull) -------------------------------------------------

class AZWingedStatueNoSkull : public ASwitchingDecoration
{
	DECLARE_ACTOR (AZWingedStatueNoSkull, ASwitchingDecoration)
};

FState AZWingedStatueNoSkull::States[] =
{
	S_NORMAL (STWN, 'A', -1, NULL, NULL),
	S_NORMAL (STWN, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AZWingedStatueNoSkull, Hexen, 9011, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (62)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_MeleeState (0)
END_DEFAULTS

// Gem pedestal -------------------------------------------------------------

class AZGemPedestal : public ASwitchingDecoration
{
	DECLARE_ACTOR (AZGemPedestal, ASwitchingDecoration)
};

FState AZGemPedestal::States[] =
{
	S_NORMAL (GMPD, 'A', -1, NULL, NULL),
	S_NORMAL (GMPD, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AZGemPedestal, Hexen, 9012, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_MeleeState (0)
END_DEFAULTS

// Tree (destructible) ------------------------------------------------------

class ATreeDestructible : public AActor
{
	DECLARE_ACTOR (ATreeDestructible, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	void Die (AActor *source, AActor *inflictor);
};

FState ATreeDestructible::States[] =
{
#define S_ZTREEDESTRUCTIBLE 0
	S_NORMAL (TRDT, 'A',   -1, NULL 					, NULL),

#define S_ZTREEDES_D (S_ZTREEDESTRUCTIBLE+1)
	S_NORMAL (TRDT, 'B',	5, NULL 					, &States[S_ZTREEDES_D+1]),
	S_NORMAL (TRDT, 'C',	5, A_Scream 				, &States[S_ZTREEDES_D+2]),
	S_NORMAL (TRDT, 'D',	5, NULL 					, &States[S_ZTREEDES_D+3]),
	S_NORMAL (TRDT, 'E',	5, NULL 					, &States[S_ZTREEDES_D+4]),
	S_NORMAL (TRDT, 'F',	5, NULL 					, &States[S_ZTREEDES_D+5]),
	S_NORMAL (TRDT, 'G',   -1, NULL 					, NULL),

#define S_ZTREEDES_X (S_ZTREEDES_D+6)
	S_BRIGHT (TRDT, 'H',	5, A_Pain 					, &States[S_ZTREEDES_X+1]),
	S_BRIGHT (TRDT, 'I',	5, NULL 					, &States[S_ZTREEDES_X+2]),
	S_BRIGHT (TRDT, 'J',	5, NULL 					, &States[S_ZTREEDES_X+3]),
	S_BRIGHT (TRDT, 'K',	5, NULL 					, &States[S_ZTREEDES_X+4]),
	S_BRIGHT (TRDT, 'L',	5, NULL 					, &States[S_ZTREEDES_X+5]),
	S_BRIGHT (TRDT, 'M',	5, A_Explode				, &States[S_ZTREEDES_X+6]),
	S_BRIGHT (TRDT, 'N',	5, NULL 					, &States[S_ZTREEDES_X+7]),
	S_NORMAL (TRDT, 'O',	5, NULL 					, &States[S_ZTREEDES_X+8]),
	S_NORMAL (TRDT, 'P',	5, NULL 					, &States[S_ZTREEDES_X+9]),
	S_NORMAL (TRDT, 'Q',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ATreeDestructible, Hexen, 8062, 0)
	PROP_SpawnHealth (70)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (180)
	PROP_MassLong (INT_MAX)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZTREEDESTRUCTIBLE)
	PROP_DeathState (S_ZTREEDES_D)
	PROP_BDeathState (S_ZTREEDES_X)

	PROP_PainSound ("TreeExplode")
	PROP_DeathSound ("TreeBreak")
END_DEFAULTS

void ATreeDestructible::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 10;
}

void ATreeDestructible::Die (AActor *source, AActor *inflictor)
{
	Super::Die (source, inflictor);
	height = 24*FRACUNIT;
	flags &= ~MF_CORPSE;
}

// Pottery1 ------------------------------------------------------------------

void A_PotteryExplode (AActor *);
void A_PotteryChooseBit (AActor *);
void A_PotteryCheck (AActor *);

class APottery1 : public AActor
{
	DECLARE_ACTOR (APottery1, AActor)
public:
	void HitFloor ();
};

FState APottery1::States[] =
{
#define S_ZPOTTERY 0
	S_NORMAL (POT1, 'A',   -1, NULL 					, NULL),

#define S_ZPOTTERY_EXPLODE (S_ZPOTTERY+1)
	S_NORMAL (POT1, 'A',	0, A_PotteryExplode 		, NULL)
};

IMPLEMENT_ACTOR (APottery1, Hexen, 104, 0)
	PROP_SpawnHealth (15)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_DROPOFF)
	PROP_Flags2 (MF2_SLIDE|MF2_PUSHABLE|MF2_TELESTOMP|MF2_PASSMOBJ)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZPOTTERY)
	PROP_DeathState (S_ZPOTTERY_EXPLODE)
END_DEFAULTS

void APottery1::HitFloor ()
{
	Super::HitFloor ();
	P_DamageMobj (this, NULL, NULL, 25, MOD_UNKNOWN);
}

// Pottery2 -----------------------------------------------------------------

class APottery2 : public APottery1
{
	DECLARE_ACTOR (APottery2, APottery1)
};

FState APottery2::States[] =
{
	S_NORMAL (POT2, 'A',   -1, NULL 					, NULL),
};

IMPLEMENT_ACTOR (APottery2, Hexen, 105, 0)
	PROP_HeightFixed (25)
	PROP_SpawnState (0)
END_DEFAULTS

// Pottery3 -----------------------------------------------------------------

class APottery3 : public APottery1
{
	DECLARE_ACTOR (APottery3, APottery1)
};

FState APottery3::States[] =
{
	S_NORMAL (POT3, 'A',   -1, NULL 					, NULL),
};

IMPLEMENT_ACTOR (APottery3, Hexen, 106, 0)
	PROP_HeightFixed (25)
	PROP_SpawnState (0)
END_DEFAULTS

// Pottery Bit --------------------------------------------------------------

class APotteryBit : public AActor
{
	DECLARE_ACTOR (APotteryBit, AActor)
};

FState APotteryBit::States[] =
{
#define S_POTTERYBIT 0
	S_NORMAL (PBIT, 'A',   -1, NULL 					, NULL),
	S_NORMAL (PBIT, 'B',   -1, NULL 					, NULL),
	S_NORMAL (PBIT, 'C',   -1, NULL 					, NULL),
	S_NORMAL (PBIT, 'D',   -1, NULL 					, NULL),
	S_NORMAL (PBIT, 'E',   -1, NULL 					, NULL),

#define S_POTTERYBIT_EX0 (S_POTTERYBIT+5)
	S_NORMAL (PBIT, 'F',	0, A_PotteryChooseBit		, NULL),

#define S_POTTERYBIT_EX1 (S_POTTERYBIT_EX0+1)
	S_NORMAL (PBIT, 'F',  140, NULL 					, &States[S_POTTERYBIT_EX0+1]),
	S_NORMAL (PBIT, 'F',	1, A_PotteryCheck			, NULL),

#define S_POTTERYBIT_EX2 (S_POTTERYBIT_EX1+2)
	S_NORMAL (PBIT, 'G',  140, NULL 					, &States[S_POTTERYBIT_EX1+1]),
	S_NORMAL (PBIT, 'G',	1, A_PotteryCheck			, NULL),

#define S_POTTERYBIT_EX3 (S_POTTERYBIT_EX2+2)
	S_NORMAL (PBIT, 'H',  140, NULL 					, &States[S_POTTERYBIT_EX2+1]),
	S_NORMAL (PBIT, 'H',	1, A_PotteryCheck			, NULL),

#define S_POTTERYBIT_EX4 (S_POTTERYBIT_EX3+2)
	S_NORMAL (PBIT, 'I',  140, NULL 					, &States[S_POTTERYBIT_EX3+1]),
	S_NORMAL (PBIT, 'I',	1, A_PotteryCheck			, NULL),

#define S_POTTERYBIT_EX5 (S_POTTERYBIT_EX4+2)
	S_NORMAL (PBIT, 'J',  140, NULL 					, &States[S_POTTERYBIT_EX4+1]),
	S_NORMAL (PBIT, 'J',	1, A_PotteryCheck			, NULL)
};

IMPLEMENT_ACTOR (APotteryBit, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_POTTERYBIT)
	PROP_DeathState (S_POTTERYBIT_EX0)
END_DEFAULTS

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
		mo = Spawn<APotteryBit> (actor->x, actor->y, actor->z);
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
		if (!(dmflags & DF_NO_MONSTERS) 
		|| !(GetDefaultByType (SpawnableThings[actor->args[0]])->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (SpawnableThings[actor->args[0]],
				actor->x, actor->y, actor->z);
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
	actor->SetState (actor->DeathState+1 + 2*(pr_bit()%5));
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

// Blood pool ---------------------------------------------------------------

class ABloodPool : public AActor
{
	DECLARE_ACTOR (ABloodPool, AActor)
};

FState ABloodPool::States[] =
{
	S_NORMAL (BDPL, 'A',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABloodPool, Hexen, 111, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Lynched corpse (no heart) ------------------------------------------------

void A_CorpseBloodDrip (AActor *);

class AZCorpseLynchedNoHeart : public AActor
{
	DECLARE_ACTOR (AZCorpseLynchedNoHeart, AActor)
public:
	void PostBeginPlay ();
};

FState AZCorpseLynchedNoHeart::States[] =
{
	S_NORMAL (CPS5, 'A',  140, A_CorpseBloodDrip		, &States[0])
};

IMPLEMENT_ACTOR (AZCorpseLynchedNoHeart, Hexen, 109, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (100)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)

	PROP_SpawnState (0)
END_DEFAULTS

void AZCorpseLynchedNoHeart::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Spawn<ABloodPool> (x, y, ONFLOORZ);
}

// CorpseBloodDrip ----------------------------------------------------------

class ACorpseBloodDrip : public AActor
{
	DECLARE_ACTOR (ACorpseBloodDrip, AActor)
};

FState ACorpseBloodDrip::States[] =
{
#define S_CORPSEBLOODDRIP 0
	S_NORMAL (BDRP, 'A',   -1, NULL 					, NULL),

#define S_CORPSEBLOODDRIP_X (S_CORPSEBLOODDRIP+1)
	S_NORMAL (BDSH, 'A',	3, NULL 					, &States[S_CORPSEBLOODDRIP_X+1]),
	S_NORMAL (BDSH, 'B',	3, NULL 					, &States[S_CORPSEBLOODDRIP_X+2]),
	S_NORMAL (BDSH, 'C',	2, NULL 					, &States[S_CORPSEBLOODDRIP_X+3]),
	S_NORMAL (BDSH, 'D',	2, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ACorpseBloodDrip, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (4)
	PROP_Flags (MF_MISSILE)
	PROP_Flags2 (MF2_LOGRAV)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_CORPSEBLOODDRIP)
	PROP_DeathState (S_CORPSEBLOODDRIP_X)
END_DEFAULTS

//============================================================================
//
// A_CorpseBloodDrip
//
//============================================================================

void A_CorpseBloodDrip (AActor *actor)
{
	if (pr_drip() <= 128)
	{
		Spawn<ACorpseBloodDrip> (actor->x, actor->y, actor->z + actor->height/2);
	}
}

// Corpse bit ---------------------------------------------------------------

class ACorpseBit : public AActor
{
	DECLARE_ACTOR (ACorpseBit, AActor)
};

FState ACorpseBit::States[] =
{
	S_NORMAL (CPB1, 'A',   -1, NULL 					, NULL),
	S_NORMAL (CPB2, 'A',   -1, NULL 					, NULL),
	S_NORMAL (CPB3, 'A',   -1, NULL 					, NULL),
	S_NORMAL (CPB4, 'A',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ACorpseBit, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_TELESTOMP)

	PROP_SpawnState (0)
END_DEFAULTS

// Corpse (sitting, splatterable) -------------------------------------------

void A_CorpseExplode (AActor *);

class AZCorpseSitting : public AActor
{
	DECLARE_ACTOR (AZCorpseSitting, AActor)
};

FState AZCorpseSitting::States[] =
{
	S_NORMAL (CPS6, 'A',   -1, NULL 					, NULL),
	S_NORMAL (CPS6, 'A',	1, A_CorpseExplode			, NULL)
};

IMPLEMENT_ACTOR (AZCorpseSitting, Hexen, 110, 0)
	PROP_SpawnHealth (30)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (35)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (0)
	PROP_DeathState (1)

	PROP_DeathSound ("FireDemonDeath")
END_DEFAULTS

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
		mo = Spawn<ACorpseBit> (actor->x, actor->y, actor->z);
		mo->SetState (mo->SpawnState + (pr_foo()%3));
		if (mo)
		{
			mo->momz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
			mo->momx = pr_foo.Random2()<<(FRACBITS-6);
			mo->momy = pr_foo.Random2()<<(FRACBITS-6);
		}
	}
	// Spawn a skull
	mo = Spawn<ACorpseBit> (actor->x, actor->y, actor->z);
	mo->SetState (mo->SpawnState + 3);
	if (mo)
	{
		mo->momz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
		mo->momx = pr_foo.Random2()<<(FRACBITS-6);
		mo->momy = pr_foo.Random2()<<(FRACBITS-6);
	}
	S_SoundID (actor, CHAN_BODY, actor->DeathSound, 1, ATTN_IDLE);
	actor->Destroy ();
}

// Leaf Spawner -------------------------------------------------------------

void A_LeafSpawn (AActor *);
void A_LeafThrust (AActor *);
void A_LeafCheck (AActor *);

class ALeafSpawner : public AActor
{
	DECLARE_ACTOR (ALeafSpawner, AActor)
};

FState ALeafSpawner::States[] =
{
	S_NORMAL (TNT1, 'A',   20, A_LeafSpawn				, &States[0])
};

IMPLEMENT_ACTOR (ALeafSpawner, Hexen, 113, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_SpawnState (0)
END_DEFAULTS

// Leaves -------------------------------------------------------------------

class ALeaf1 : public AActor
{
	DECLARE_ACTOR (ALeaf1, AActor)
};

FState ALeaf1::States[] =
{
#define S_LEAF1 0
	S_NORMAL (LEF1, 'A',	4, NULL 					, &States[S_LEAF1+1]),
	S_NORMAL (LEF1, 'B',	4, NULL 					, &States[S_LEAF1+2]),
	S_NORMAL (LEF1, 'C',	4, NULL 					, &States[S_LEAF1+3]),
	S_NORMAL (LEF1, 'D',	4, A_LeafThrust 			, &States[S_LEAF1+4]),
	S_NORMAL (LEF1, 'E',	4, NULL 					, &States[S_LEAF1+5]),
	S_NORMAL (LEF1, 'F',	4, NULL 					, &States[S_LEAF1+6]),
	S_NORMAL (LEF1, 'G',	4, NULL 					, &States[S_LEAF1+7]),
	S_NORMAL (LEF1, 'H',	4, A_LeafThrust 			, &States[S_LEAF1+8]),
	S_NORMAL (LEF1, 'I',	4, NULL 					, &States[S_LEAF1+9]),
	S_NORMAL (LEF1, 'A',	4, NULL 					, &States[S_LEAF1+10]),
	S_NORMAL (LEF1, 'B',	4, NULL 					, &States[S_LEAF1+11]),
	S_NORMAL (LEF1, 'C',	4, A_LeafThrust 			, &States[S_LEAF1+12]),
	S_NORMAL (LEF1, 'D',	4, NULL 					, &States[S_LEAF1+13]),
	S_NORMAL (LEF1, 'E',	4, NULL 					, &States[S_LEAF1+14]),
	S_NORMAL (LEF1, 'F',	4, NULL 					, &States[S_LEAF1+15]),
	S_NORMAL (LEF1, 'G',	4, A_LeafThrust 			, &States[S_LEAF1+16]),
	S_NORMAL (LEF1, 'H',	4, NULL 					, &States[S_LEAF1+17]),
	S_NORMAL (LEF1, 'I',	4, NULL 					, NULL),

#define S_LEAF_X (S_LEAF1+18)
	S_NORMAL (LEF3, 'D',   10, A_LeafCheck				, &States[S_LEAF_X+0])
};

IMPLEMENT_ACTOR (ALeaf1, Hexen, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_LOGRAV)
	PROP_Flags3 (MF3_DONTSPLASH)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_LEAF1)
	PROP_DeathState (S_LEAF_X)
END_DEFAULTS

class ALeaf2 : public ALeaf1
{
	DECLARE_ACTOR (ALeaf2, ALeaf1)
};

FState ALeaf2::States[] =
{
#define S_LEAF2 0
	S_NORMAL (LEF2, 'A',	4, NULL 					, &States[S_LEAF2+1]),
	S_NORMAL (LEF2, 'B',	4, NULL 					, &States[S_LEAF2+2]),
	S_NORMAL (LEF2, 'C',	4, NULL 					, &States[S_LEAF2+3]),
	S_NORMAL (LEF2, 'D',	4, A_LeafThrust 			, &States[S_LEAF2+4]),
	S_NORMAL (LEF2, 'E',	4, NULL 					, &States[S_LEAF2+5]),
	S_NORMAL (LEF2, 'F',	4, NULL 					, &States[S_LEAF2+6]),
	S_NORMAL (LEF2, 'G',	4, NULL 					, &States[S_LEAF2+7]),
	S_NORMAL (LEF2, 'H',	4, A_LeafThrust 			, &States[S_LEAF2+8]),
	S_NORMAL (LEF2, 'I',	4, NULL 					, &States[S_LEAF2+9]),
	S_NORMAL (LEF2, 'A',	4, NULL 					, &States[S_LEAF2+10]),
	S_NORMAL (LEF2, 'B',	4, NULL 					, &States[S_LEAF2+11]),
	S_NORMAL (LEF2, 'C',	4, A_LeafThrust 			, &States[S_LEAF2+12]),
	S_NORMAL (LEF2, 'D',	4, NULL 					, &States[S_LEAF2+13]),
	S_NORMAL (LEF2, 'E',	4, NULL 					, &States[S_LEAF2+14]),
	S_NORMAL (LEF2, 'F',	4, NULL 					, &States[S_LEAF2+15]),
	S_NORMAL (LEF2, 'G',	4, A_LeafThrust 			, &States[S_LEAF2+16]),
	S_NORMAL (LEF2, 'H',	4, NULL 					, &States[S_LEAF2+17]),
	S_NORMAL (LEF2, 'I',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ALeaf2, Hexen, -1, 0)
	PROP_SpawnState (S_LEAF2)
END_DEFAULTS

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
		mo = Spawn (pr_leaf()&1 ? RUNTIME_CLASS(ALeaf1) : RUNTIME_CLASS(ALeaf2),
			actor->x + (pr_leaf.Random2()<<14),
			actor->y + (pr_leaf.Random2()<<14),
			actor->z + (pr_leaf()<<14));
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

// Torch base class ---------------------------------------------------------

// Twined torch -------------------------------------------------------------

class AZTwinedTorch : public ASwitchableDecoration
{
	DECLARE_ACTOR (AZTwinedTorch, ASwitchableDecoration)
public:
	void Activate (AActor *activator);
};

FState AZTwinedTorch::States[] =
{
#define S_ZTWINEDTORCH 0
	S_BRIGHT (TWTR, 'A',	4, NULL 					, &States[S_ZTWINEDTORCH+1]),
	S_BRIGHT (TWTR, 'B',	4, NULL 					, &States[S_ZTWINEDTORCH+2]),
	S_BRIGHT (TWTR, 'C',	4, NULL 					, &States[S_ZTWINEDTORCH+3]),
	S_BRIGHT (TWTR, 'D',	4, NULL 					, &States[S_ZTWINEDTORCH+4]),
	S_BRIGHT (TWTR, 'E',	4, NULL 					, &States[S_ZTWINEDTORCH+5]),
	S_BRIGHT (TWTR, 'F',	4, NULL 					, &States[S_ZTWINEDTORCH+6]),
	S_BRIGHT (TWTR, 'G',	4, NULL 					, &States[S_ZTWINEDTORCH+7]),
	S_BRIGHT (TWTR, 'H',	4, NULL 					, &States[S_ZTWINEDTORCH+0]),

#define S_ZTWINEDTORCH_UNLIT (S_ZTWINEDTORCH+8)
	S_NORMAL (TWTR, 'I',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZTwinedTorch, Hexen, 116, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (S_ZTWINEDTORCH)
	PROP_SeeState (S_ZTWINEDTORCH)
	PROP_MeleeState (S_ZTWINEDTORCH_UNLIT)
END_DEFAULTS

void AZTwinedTorch::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
}

class AZTwinedTorchUnlit : public AZTwinedTorch
{
	DECLARE_STATELESS_ACTOR (AZTwinedTorchUnlit, AZTwinedTorch)
};

IMPLEMENT_STATELESS_ACTOR (AZTwinedTorchUnlit, Hexen, 117, 0)
	PROP_SpawnState (S_ZTWINEDTORCH_UNLIT)
END_DEFAULTS

// Wall torch ---------------------------------------------------------------

class AZWallTorch : public ASwitchableDecoration
{
	DECLARE_ACTOR (AZWallTorch, ASwitchableDecoration)
public:
	void Activate (AActor *activator);
};

FState AZWallTorch::States[] =
{
#define S_ZWALLTORCH 0
	S_BRIGHT (WLTR, 'A',	5, NULL 					, &States[S_ZWALLTORCH+1]),
	S_BRIGHT (WLTR, 'B',	5, NULL 					, &States[S_ZWALLTORCH+2]),
	S_BRIGHT (WLTR, 'C',	5, NULL 					, &States[S_ZWALLTORCH+3]),
	S_BRIGHT (WLTR, 'D',	5, NULL 					, &States[S_ZWALLTORCH+4]),
	S_BRIGHT (WLTR, 'E',	5, NULL 					, &States[S_ZWALLTORCH+5]),
	S_BRIGHT (WLTR, 'F',	5, NULL 					, &States[S_ZWALLTORCH+6]),
	S_BRIGHT (WLTR, 'G',	5, NULL 					, &States[S_ZWALLTORCH+7]),
	S_BRIGHT (WLTR, 'H',	5, NULL 					, &States[S_ZWALLTORCH+0]),

#define S_ZWALLTORCH_U (S_ZWALLTORCH+8)
	S_NORMAL (WLTR, 'I',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZWallTorch, Hexen, 54, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)

	PROP_SpawnState (S_ZWALLTORCH)
	PROP_SeeState (S_ZWALLTORCH)
	PROP_MeleeState (S_ZWALLTORCH_U)
END_DEFAULTS

void AZWallTorch::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
}

class AZWallTorchUnlit : public AZWallTorch
{
	DECLARE_STATELESS_ACTOR (AZWallTorchUnlit, AZWallTorch)
};

IMPLEMENT_STATELESS_ACTOR (AZWallTorchUnlit, Hexen, 55, 0)
	PROP_SpawnState (S_ZWALLTORCH_U)
END_DEFAULTS

// Shrub1 -------------------------------------------------------------------

class AZShrub1 : public AActor
{
	DECLARE_ACTOR (AZShrub1, AActor)
};

FState AZShrub1::States[] =
{
#define S_ZSHRUB1 0
	S_NORMAL (SHB1, 'A',   -1, NULL 					, NULL),

#define S_ZSHRUB1_X (S_ZSHRUB1+1)
	S_BRIGHT (SHB1, 'B',	7, NULL 					, &States[S_ZSHRUB1_X+1]),
	S_BRIGHT (SHB1, 'C',	6, A_Scream 				, &States[S_ZSHRUB1_X+2]),
	S_BRIGHT (SHB1, 'D',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZShrub1, Hexen, 8101, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (24)
	PROP_MassLong (FIXED_MAX)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZSHRUB1)
	PROP_BDeathState (S_ZSHRUB1_X)

	PROP_DeathSound ("TreeExplode")
END_DEFAULTS

// Shrub2 -------------------------------------------------------------------

class AZShrub2 : public AActor
{
	DECLARE_ACTOR (AZShrub2, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSrc);
};

FState AZShrub2::States[] =
{
#define S_ZSHRUB2 0
	S_NORMAL (SHB2, 'A',   -1, NULL 					, NULL),

#define S_ZSHRUB2_X (S_ZSHRUB2+1)
	S_BRIGHT (SHB2, 'B',	7, NULL 					, &States[S_ZSHRUB2_X+1]),
	S_BRIGHT (SHB2, 'C',	6, A_Scream 				, &States[S_ZSHRUB2_X+2]),
	S_BRIGHT (SHB2, 'D',	5, A_Explode				, &States[S_ZSHRUB2_X+3]),
	S_BRIGHT (SHB2, 'E',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZShrub2, Hexen, 8102, 0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_MassLong (FIXED_MAX)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZSHRUB2)
	PROP_BDeathState (S_ZSHRUB2_X)

	PROP_DeathSound ("TreeExplode")
END_DEFAULTS

void AZShrub2::GetExplodeParms (int &damage, int &distance, bool &hurtSrc)
{
	damage = 30;
	distance = 64;
}

//===========================================================================
//
// A_TreeDeath
//
//===========================================================================

void A_TreeDeath (AActor *actor)
{
	if (actor->DamageType != MOD_FIRE)
	{
		actor->height <<= 2;
		actor->flags |= MF_SHOOTABLE;
		actor->flags &= ~(MF_CORPSE+MF_DROPOFF);
		actor->health = 35;
		return;
	}
	else
	{
		actor->SetState (actor->MeleeState);
	}
}

// Poison Shroom ------------------------------------------------------------

void A_PoisonShroom (AActor *);
void A_PoisonBagInit (AActor *);

class AZPoisonShroom : public AActor
{
	DECLARE_ACTOR (AZPoisonShroom, AActor)
};

FState AZPoisonShroom::States[] =
{
#define S_ZPOISONSHROOM_P 0
	S_NORMAL (SHRM, 'A',	6, NULL 			, &States[S_ZPOISONSHROOM_P+1]),
	S_NORMAL (SHRM, 'B',	8, A_Pain			, &States[S_ZPOISONSHROOM_P+2]),//<-- Intentional state

#define S_ZPOISONSHROOM (S_ZPOISONSHROOM_P+2)
	S_NORMAL (SHRM, 'A',	5, A_PoisonShroom	, &States[S_ZPOISONSHROOM_P+1]),

#define S_ZPOISONSHROOM_X (S_ZPOISONSHROOM+1)
	S_NORMAL (SHRM, 'C',	5, NULL 			, &States[S_ZPOISONSHROOM_X+1]),
	S_NORMAL (SHRM, 'D',	5, NULL 			, &States[S_ZPOISONSHROOM_X+2]),
	S_NORMAL (SHRM, 'E',	5, A_PoisonBagInit	, &States[S_ZPOISONSHROOM_X+3]),
	S_NORMAL (SHRM, 'F',   -1, NULL 			, NULL)
};

IMPLEMENT_ACTOR (AZPoisonShroom, Hexen, 8104, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (20)
	PROP_PainChance (255)
	PROP_SpawnHealth (30)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_SHOOTABLE|MF_SOLID|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZPOISONSHROOM)
	PROP_PainState (S_ZPOISONSHROOM_P)
	PROP_DeathState (S_ZPOISONSHROOM_X)

	PROP_PainSound ("PoisonShroomPain")
	PROP_DeathSound ("PoisonShroomDeath")
END_DEFAULTS

//===========================================================================
//
// A_PoisonShroom
//
//===========================================================================

void A_PoisonShroom (AActor *actor)
{
	actor->tics = 128+(pr_shroom()<<1);
}

// Fire Bull ----------------------------------------------------------------

class AZFireBull : public ASwitchableDecoration
{
	DECLARE_ACTOR (AZFireBull, ASwitchableDecoration)
public:
	void Activate (AActor *activator);
};

FState AZFireBull::States[] =
{
#define S_ZFIREBULL 0
	S_BRIGHT (FBUL, 'A',	4, NULL 		, &States[S_ZFIREBULL+1]),
	S_BRIGHT (FBUL, 'B',	4, NULL 		, &States[S_ZFIREBULL+2]),
	S_BRIGHT (FBUL, 'C',	4, NULL 		, &States[S_ZFIREBULL+3]),
	S_BRIGHT (FBUL, 'D',	4, NULL 		, &States[S_ZFIREBULL+4]),
	S_BRIGHT (FBUL, 'E',	4, NULL 		, &States[S_ZFIREBULL+5]),
	S_BRIGHT (FBUL, 'F',	4, NULL 		, &States[S_ZFIREBULL+6]),
	S_BRIGHT (FBUL, 'G',	4, NULL 		, &States[S_ZFIREBULL+0]),

#define S_ZFIREBULL_U (S_ZFIREBULL+7)
	S_NORMAL (FBUL, 'H',   -1, NULL 		, NULL),

#define S_ZFIREBULL_DEATH (S_ZFIREBULL_U+1)
	S_BRIGHT (FBUL, 'J',	4, NULL 		, &States[S_ZFIREBULL_DEATH+1]),
	S_BRIGHT (FBUL, 'I',	4, NULL 		, &States[S_ZFIREBULL_U]),

#define S_ZFIREBULL_BIRTH (S_ZFIREBULL_DEATH+2)
	S_BRIGHT (FBUL, 'I',	4, NULL 		, &States[S_ZFIREBULL_BIRTH+1]),
	S_BRIGHT (FBUL, 'J',	4, NULL 		, &States[S_ZFIREBULL+0])
};

IMPLEMENT_ACTOR (AZFireBull, Hexen, 8042, 0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (S_ZFIREBULL)
	PROP_SeeState (S_ZFIREBULL_BIRTH)
	PROP_MeleeState (S_ZFIREBULL_U)
END_DEFAULTS

void AZFireBull::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
}

class AZFireBullUnlit : public AZFireBull
{
	DECLARE_STATELESS_ACTOR (AZFireBullUnlit, AZFireBull)
};

IMPLEMENT_STATELESS_ACTOR (AZFireBullUnlit, Hexen, 8043, 0)
	PROP_SpawnState (S_ZFIREBULL_U)
END_DEFAULTS

// Suit of armor ------------------------------------------------------------

void A_SoAExplode (AActor *);

class AZSuitOfArmor : public AActor
{
	DECLARE_ACTOR (AZSuitOfArmor, AActor)
};

FState AZSuitOfArmor::States[] =
{
	S_NORMAL (ZSUI, 'A',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'A',	1, A_SoAExplode 			, NULL)
};

IMPLEMENT_ACTOR (AZSuitOfArmor, Hexen, 8064, 0)
	PROP_SpawnHealth (60)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (72)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (0)
	PROP_DeathState (1)

	PROP_DeathSound ("SuitofArmorBreak")
END_DEFAULTS

// Armor chunk --------------------------------------------------------------

class AZArmorChunk : public AActor
{
	DECLARE_ACTOR (AZArmorChunk, AActor)
};

FState AZArmorChunk::States[] =
{
	S_NORMAL (ZSUI, 'B',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'C',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'D',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'E',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'F',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'G',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'H',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'I',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'J',   -1, NULL 					, NULL),
	S_NORMAL (ZSUI, 'K',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZArmorChunk, Hexen, -1, 0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (8)
	PROP_SpawnState (0)
END_DEFAULTS

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
		mo = Spawn<AZArmorChunk> (actor->x+((pr_soaexplode()-128)<<12),
			actor->y+((pr_soaexplode()-128)<<12), 
			actor->z+(pr_soaexplode()*actor->height/256));
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
		if (!(dmflags & DF_NO_MONSTERS) 
		|| !(GetDefaultByType (SpawnableThings[actor->args[0]])->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (SpawnableThings[actor->args[0]],
				actor->x, actor->y, actor->z);
		}
	}
	S_SoundID (actor, CHAN_BODY, actor->DeathSound, 1, ATTN_NORM);
	actor->Destroy ();
}

// Bell ---------------------------------------------------------------------

void A_BellReset1 (AActor *);
void A_BellReset2 (AActor *);

class AZBell : public AActor
{
	DECLARE_ACTOR (AZBell, AActor)
public:
	void Activate (AActor *activator);
};

FState AZBell::States[] =
{
#define S_ZBELL 0
	S_NORMAL (BBLL, 'F',   -1, NULL 					, NULL),

#define S_ZBELL_X (S_ZBELL+1)
	S_NORMAL (BBLL, 'A',	4, A_BellReset1 			, &States[S_ZBELL_X+1]),
	S_NORMAL (BBLL, 'B',	4, NULL 					, &States[S_ZBELL_X+2]),
	S_NORMAL (BBLL, 'C',	4, NULL 					, &States[S_ZBELL_X+3]),
	S_NORMAL (BBLL, 'D',	5, A_Scream 				, &States[S_ZBELL_X+4]),
	S_NORMAL (BBLL, 'C',	4, NULL 					, &States[S_ZBELL_X+5]),
	S_NORMAL (BBLL, 'B',	4, NULL 					, &States[S_ZBELL_X+6]),
	S_NORMAL (BBLL, 'A',	3, NULL 					, &States[S_ZBELL_X+7]),
	S_NORMAL (BBLL, 'E',	4, NULL 					, &States[S_ZBELL_X+8]),
	S_NORMAL (BBLL, 'F',	5, NULL 					, &States[S_ZBELL_X+9]),
	S_NORMAL (BBLL, 'G',	6, A_Scream 				, &States[S_ZBELL_X+10]),
	S_NORMAL (BBLL, 'F',	5, NULL 					, &States[S_ZBELL_X+11]),
	S_NORMAL (BBLL, 'E',	4, NULL 					, &States[S_ZBELL_X+12]),
	S_NORMAL (BBLL, 'A',	4, NULL 					, &States[S_ZBELL_X+13]),
	S_NORMAL (BBLL, 'B',	5, NULL 					, &States[S_ZBELL_X+14]),
	S_NORMAL (BBLL, 'C',	5, NULL 					, &States[S_ZBELL_X+15]),
	S_NORMAL (BBLL, 'D',	6, A_Scream 				, &States[S_ZBELL_X+16]),
	S_NORMAL (BBLL, 'C',	5, NULL 					, &States[S_ZBELL_X+17]),
	S_NORMAL (BBLL, 'B',	5, NULL 					, &States[S_ZBELL_X+18]),
	S_NORMAL (BBLL, 'A',	4, NULL 					, &States[S_ZBELL_X+19]),
	S_NORMAL (BBLL, 'E',	5, NULL 					, &States[S_ZBELL_X+20]),
	S_NORMAL (BBLL, 'F',	5, NULL 					, &States[S_ZBELL_X+21]),
	S_NORMAL (BBLL, 'G',	7, A_Scream 				, &States[S_ZBELL_X+22]),
	S_NORMAL (BBLL, 'F',	5, NULL 					, &States[S_ZBELL_X+23]),
	S_NORMAL (BBLL, 'E',	5, NULL 					, &States[S_ZBELL_X+24]),
	S_NORMAL (BBLL, 'A',	5, NULL 					, &States[S_ZBELL_X+25]),
	S_NORMAL (BBLL, 'B',	6, NULL 					, &States[S_ZBELL_X+26]),
	S_NORMAL (BBLL, 'C',	6, NULL 					, &States[S_ZBELL_X+27]),
	S_NORMAL (BBLL, 'D',	7, A_Scream 				, &States[S_ZBELL_X+28]),
	S_NORMAL (BBLL, 'C',	6, NULL 					, &States[S_ZBELL_X+29]),
	S_NORMAL (BBLL, 'B',	6, NULL 					, &States[S_ZBELL_X+30]),
	S_NORMAL (BBLL, 'A',	5, NULL 					, &States[S_ZBELL_X+31]),
	S_NORMAL (BBLL, 'E',	6, NULL 					, &States[S_ZBELL_X+32]),
	S_NORMAL (BBLL, 'F',	6, NULL 					, &States[S_ZBELL_X+33]),
	S_NORMAL (BBLL, 'G',	7, A_Scream 				, &States[S_ZBELL_X+34]),
	S_NORMAL (BBLL, 'F',	6, NULL 					, &States[S_ZBELL_X+35]),
	S_NORMAL (BBLL, 'E',	6, NULL 					, &States[S_ZBELL_X+36]),
	S_NORMAL (BBLL, 'A',	6, NULL 					, &States[S_ZBELL_X+37]),
	S_NORMAL (BBLL, 'B',	6, NULL 					, &States[S_ZBELL_X+38]),
	S_NORMAL (BBLL, 'C',	6, NULL 					, &States[S_ZBELL_X+39]),
	S_NORMAL (BBLL, 'B',	7, NULL 					, &States[S_ZBELL_X+40]),
	S_NORMAL (BBLL, 'A',	8, NULL 					, &States[S_ZBELL_X+41]),
	S_NORMAL (BBLL, 'E',   12, NULL 					, &States[S_ZBELL_X+42]),
	S_NORMAL (BBLL, 'A',   10, NULL 					, &States[S_ZBELL_X+43]),
	S_NORMAL (BBLL, 'B',   12, NULL 					, &States[S_ZBELL_X+44]),
	S_NORMAL (BBLL, 'A',   12, NULL 					, &States[S_ZBELL_X+45]),
	S_NORMAL (BBLL, 'E',   14, NULL 					, &States[S_ZBELL_X+46]),
	S_NORMAL (BBLL, 'A',	1, A_BellReset2 			, &States[S_ZBELL])
};

IMPLEMENT_ACTOR (AZBell, Hexen, 8065, 0)
	PROP_SpawnHealth (5)
	PROP_RadiusFixed (56)
	PROP_HeightFixed (120)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_NOGRAVITY|MF_SPAWNCEILING)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZBELL)
	PROP_DeathState (S_ZBELL_X)

	PROP_DeathSound ("BellRing")
END_DEFAULTS

void AZBell::Activate (AActor *activator)
{
	if (health > 0)
	{
		P_DamageMobj (this, activator, activator, 10, MOD_HIT); // 'ring' the bell
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

// "Christmas" Tree ---------------------------------------------------------

class AZXmasTree : public AActor
{
	DECLARE_ACTOR (AZXmasTree, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &dmgSource);
};

FState AZXmasTree::States[] =
{
#define S_ZXMAS_TREE 0
	S_NORMAL (XMAS, 'A',   -1, NULL 					, NULL),

#define S_ZXMAS_TREE_X (S_ZXMAS_TREE+1)
	S_BRIGHT (XMAS, 'B',	6, NULL 					, &States[S_ZXMAS_TREE_X+1]),
	S_BRIGHT (XMAS, 'C',	6, A_Scream 				, &States[S_ZXMAS_TREE_X+2]),
	S_BRIGHT (XMAS, 'D',	5, NULL 					, &States[S_ZXMAS_TREE_X+3]),
	S_BRIGHT (XMAS, 'E',	5, A_Explode				, &States[S_ZXMAS_TREE_X+4]),
	S_BRIGHT (XMAS, 'F',	5, NULL 					, &States[S_ZXMAS_TREE_X+5]),
	S_BRIGHT (XMAS, 'G',	4, NULL 					, &States[S_ZXMAS_TREE_X+6]),
	S_NORMAL (XMAS, 'H',	5, NULL 					, &States[S_ZXMAS_TREE_X+7]),
	S_NORMAL (XMAS, 'I',	4, A_NoBlocking 			, &States[S_ZXMAS_TREE_X+8]),
	S_NORMAL (XMAS, 'J',	4, NULL 					, &States[S_ZXMAS_TREE_X+9]),
	S_NORMAL (XMAS, 'K',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZXmasTree, Hexen, 8068, 0)
	PROP_SpawnHealth (20)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (130)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ZXMAS_TREE)
	PROP_BDeathState (S_ZXMAS_TREE_X)

	PROP_DeathSound ("TreeExplode")
END_DEFAULTS

void AZXmasTree::GetExplodeParms (int &damage, int &dist, bool &dmgSource)
{
	damage = 30;
	dist = 64;
}

// Cauldron -----------------------------------------------------------------

class AZCauldron : public ASwitchableDecoration
{
	DECLARE_ACTOR (AZCauldron, ASwitchableDecoration)
public:
	void Activate (AActor *activator);
};

FState AZCauldron::States[] =
{
#define S_ZCAULDRON 0
	S_BRIGHT (CDRN, 'B',	4, NULL 					, &States[S_ZCAULDRON+1]),
	S_BRIGHT (CDRN, 'C',	4, NULL 					, &States[S_ZCAULDRON+2]),
	S_BRIGHT (CDRN, 'D',	4, NULL 					, &States[S_ZCAULDRON+3]),
	S_BRIGHT (CDRN, 'E',	4, NULL 					, &States[S_ZCAULDRON+4]),
	S_BRIGHT (CDRN, 'F',	4, NULL 					, &States[S_ZCAULDRON+5]),
	S_BRIGHT (CDRN, 'G',	4, NULL 					, &States[S_ZCAULDRON+6]),
	S_BRIGHT (CDRN, 'H',	4, NULL 					, &States[S_ZCAULDRON+0]),

#define S_ZCAULDRON_U (S_ZCAULDRON+7)
	S_NORMAL (CDRN, 'A',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AZCauldron, Hexen, 8069, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (26)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (S_ZCAULDRON)
	PROP_SeeState (S_ZCAULDRON)
	PROP_MeleeState (S_ZCAULDRON_U)
END_DEFAULTS

void AZCauldron::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
}

class AZCauldronUnlit : public AZCauldron
{
	DECLARE_STATELESS_ACTOR (AZCauldronUnlit, AZCauldron)
};

IMPLEMENT_STATELESS_ACTOR (AZCauldronUnlit, Hexen, 8070, 0)
	PROP_SpawnState (S_ZCAULDRON_U)
END_DEFAULTS

// Water Drip ---------------------------------------------------------------

class AHWaterDrip : public AActor
{
	DECLARE_ACTOR (AHWaterDrip, AActor)
};

FState AHWaterDrip::States[] =
{
	S_NORMAL (HWAT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AHWaterDrip, Hexen, -1, 95)
	PROP_Flags (MF_MISSILE)
	PROP_Flags2 (MF2_LOGRAV|MF2_NOTELEPORT)
	PROP_Mass (1)
	PROP_SpawnState (0)
	PROP_DeathSound ("Drip")
END_DEFAULTS
