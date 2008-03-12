#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_hereticglobal.h"
#include "a_pickups.h"
#include "a_action.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gstrings.h"

static FRandom pr_podpain ("PodPain");
static FRandom pr_makepod ("MakePod");
static FRandom pr_teleg ("TeleGlitter");
static FRandom pr_teleg2 ("TeleGlitter2");
static FRandom pr_volcano ("VolcanoSet");
static FRandom pr_blast ("VolcanoBlast");
static FRandom pr_impact ("VolcBallImpact");

// --- Pods -----------------------------------------------------------------

void A_PodPain (AActor *);
void A_RemovePod (AActor *);
void A_MakePod (AActor *);

// Pod ----------------------------------------------------------------------

class APod : public AActor
{
	DECLARE_ACTOR (APod, AActor)
	HAS_OBJECT_POINTERS
public:
	void BeginPlay ();
	TObjPtr<AActor> Generator;

	void Serialize (FArchive &arc);
};

IMPLEMENT_POINTY_CLASS (APod)
	DECLARE_POINTER (Generator)
END_POINTERS;

void APod::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Generator;
}

FState APod::States[] =
{
#define S_POD_WAIT 0
	S_NORMAL (PPOD, 'A',   10, NULL 						, &States[S_POD_WAIT+0]),

#define S_POD_PAIN (S_POD_WAIT+1)
	S_NORMAL (PPOD, 'B',   14, A_PodPain					, &States[S_POD_WAIT+0]),

#define S_POD_DIE (S_POD_PAIN+1)
	S_BRIGHT (PPOD, 'C',	5, A_RemovePod					, &States[S_POD_DIE+1]),
	S_BRIGHT (PPOD, 'D',	5, A_Scream 					, &States[S_POD_DIE+2]),
	S_BRIGHT (PPOD, 'E',	5, A_Explode					, &States[S_POD_DIE+3]),
	S_BRIGHT (PPOD, 'F',   10, NULL 						, &AActor::States[S_FREETARGMOBJ]),

#define S_POD_GROW (S_POD_DIE+4)
	S_NORMAL (PPOD, 'I',	3, NULL 						, &States[S_POD_GROW+1]),
	S_NORMAL (PPOD, 'J',	3, NULL 						, &States[S_POD_GROW+2]),
	S_NORMAL (PPOD, 'K',	3, NULL 						, &States[S_POD_GROW+3]),
	S_NORMAL (PPOD, 'L',	3, NULL 						, &States[S_POD_GROW+4]),
	S_NORMAL (PPOD, 'M',	3, NULL 						, &States[S_POD_GROW+5]),
	S_NORMAL (PPOD, 'N',	3, NULL 						, &States[S_POD_GROW+6]),
	S_NORMAL (PPOD, 'O',	3, NULL 						, &States[S_POD_GROW+7]),
	S_NORMAL (PPOD, 'P',	3, NULL 						, &States[S_POD_WAIT+0])
};

BEGIN_DEFAULTS (APod, Heretic, 2035, 125)
	PROP_SpawnHealth (45)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (54)
	PROP_PainChance (255)
	PROP_Flags (MF_SOLID|MF_NOBLOOD|MF_SHOOTABLE|MF_DROPOFF)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_PUSHABLE|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOBLOCKMONST|MF3_DONTGIB)
	PROP_Flags5 (MF5_OLDRADIUSDMG)

	PROP_SpawnState (S_POD_WAIT)
	PROP_PainState (S_POD_PAIN)
	PROP_DeathState (S_POD_DIE)

	PROP_DeathSound ("world/podexplode")
END_DEFAULTS

void APod::BeginPlay ()
{
	Super::BeginPlay ();
	Generator = NULL;
}

// Pod goo (falls from pod when damaged) ------------------------------------

class APodGoo : public AActor
{
	DECLARE_ACTOR (APodGoo, AActor)
};

FState APodGoo::States[] =
{
#define S_PODGOO 0
	S_NORMAL (PPOD, 'G',	8, NULL 						, &States[S_PODGOO+1]),
	S_NORMAL (PPOD, 'H',	8, NULL 						, &States[S_PODGOO+0]),

#define S_PODGOOX (S_PODGOO+2)
	S_NORMAL (PPOD, 'G',   10, NULL 						, NULL)
};

IMPLEMENT_ACTOR (APodGoo, Heretic, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)

	PROP_SpawnState (S_PODGOO)
END_DEFAULTS

// Pod generator ------------------------------------------------------------

class APodGenerator : public AActor
{
	DECLARE_ACTOR (APodGenerator, AActor)
};

FState APodGenerator::States[] =
{
	S_NORMAL (TNT1, 'A',   35, A_MakePod					, &States[0])
};

IMPLEMENT_ACTOR (APodGenerator, Heretic, 43, 126)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
	PROP_SpawnState (0)
END_DEFAULTS

// --- Pod action functions -------------------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_PodPain
//
//----------------------------------------------------------------------------

void A_PodPain (AActor *actor)
{
	int count;
	int chance;
	AActor *goo;

	chance = pr_podpain ();
	if (chance < 128)
	{
		return;
	}
	for (count = chance > 240 ? 2 : 1; count; count--)
	{
		goo = Spawn<APodGoo> (actor->x, actor->y, actor->z + 48*FRACUNIT, ALLOW_REPLACE);
		goo->target = actor;
		goo->momx = pr_podpain.Random2() << 9;
		goo->momy = pr_podpain.Random2() << 9;
		goo->momz = FRACUNIT/2 + (pr_podpain() << 9);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RemovePod
//
//----------------------------------------------------------------------------

void A_RemovePod (AActor *actor)
{
	AActor *mo;

	if ( (mo = static_cast<APod *>(actor)->Generator) )
	{
		if (mo->special1 > 0)
		{
			mo->special1--;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MakePod
//
//----------------------------------------------------------------------------

#define MAX_GEN_PODS 16

void A_MakePod (AActor *actor)
{
	APod *mo;
	fixed_t x;
	fixed_t y;
	fixed_t z;

	if (actor->special1 == MAX_GEN_PODS)
	{ // Too many generated pods
		return;
	}
	x = actor->x;
	y = actor->y;
	z = actor->z;
	mo = Spawn<APod> (x, y, ONFLOORZ, ALLOW_REPLACE);
	if (P_CheckPosition (mo, x, y) == false)
	{ // Didn't fit
		mo->Destroy ();
		return;
	}
	mo->SetState (&APod::States[S_POD_GROW]);
	P_ThrustMobj (mo, pr_makepod()<<24, (fixed_t)(4.5*FRACUNIT));
	S_Sound (mo, CHAN_BODY, "world/podgrow", 1, ATTN_IDLE);
	actor->special1++; // Increment generated pod count
	mo->Generator = actor; // Link the generator to the pod
	return;
}

// --- Teleglitter ----------------------------------------------------------

void A_SpawnTeleGlitter (AActor *);
void A_SpawnTeleGlitter2 (AActor *);
void A_AccTeleGlitter (AActor *);

// Teleglitter generator 1 --------------------------------------------------

class ATeleGlitterGenerator1 : public AActor
{
	DECLARE_ACTOR (ATeleGlitterGenerator1, AActor)
};

FState ATeleGlitterGenerator1::States[] =
{
	S_NORMAL (TGLT, 'A',	8, A_SpawnTeleGlitter			, &States[0])
};

IMPLEMENT_ACTOR (ATeleGlitterGenerator1, Heretic, 74, 166)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOSECTOR)
	PROP_SpawnState (0)
END_DEFAULTS

// Teleglitter generator 2 --------------------------------------------------

class ATeleGlitterGenerator2 : public AActor
{
	DECLARE_ACTOR (ATeleGlitterGenerator2, AActor)
};

FState ATeleGlitterGenerator2::States[] =
{
	S_NORMAL (TGLT, 'F',	8, A_SpawnTeleGlitter2			, &States[0])
};

IMPLEMENT_ACTOR (ATeleGlitterGenerator2, Heretic, 52, 167)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOSECTOR)
	PROP_SpawnState (0)
END_DEFAULTS

// Teleglitter 1 ------------------------------------------------------------

class ATeleGlitter1 : public AActor
{
	DECLARE_ACTOR (ATeleGlitter1, AActor)
};

FState ATeleGlitter1::States[] =
{
	S_BRIGHT (TGLT, 'A',	2, NULL 						, &States[1]),
	S_BRIGHT (TGLT, 'B',	2, A_AccTeleGlitter 			, &States[2]),
	S_BRIGHT (TGLT, 'C',	2, NULL 						, &States[3]),
	S_BRIGHT (TGLT, 'D',	2, A_AccTeleGlitter 			, &States[4]),
	S_BRIGHT (TGLT, 'E',	2, NULL 						, &States[0])
};

IMPLEMENT_ACTOR (ATeleGlitter1, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (0)
	PROP_Damage (0)
END_DEFAULTS

// Teleglitter 2 ------------------------------------------------------------

class ATeleGlitter2 : public AActor
{
	DECLARE_ACTOR (ATeleGlitter2, AActor)
};

FState ATeleGlitter2::States[] =
{
	S_BRIGHT (TGLT, 'F',	2, NULL 						, &States[1]),
	S_BRIGHT (TGLT, 'G',	2, A_AccTeleGlitter 			, &States[2]),
	S_BRIGHT (TGLT, 'H',	2, NULL 						, &States[3]),
	S_BRIGHT (TGLT, 'I',	2, A_AccTeleGlitter 			, &States[4]),
	S_BRIGHT (TGLT, 'J',	2, NULL 						, &States[0])
};

IMPLEMENT_ACTOR (ATeleGlitter2, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (0)
	PROP_Damage (0)
END_DEFAULTS

// --- Teleglitter action functions -----------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_SpawnTeleGlitter
//
//----------------------------------------------------------------------------

void A_SpawnTeleGlitter (AActor *actor)
{
	AActor *mo;
	fixed_t x = actor->x+((pr_teleg()&31)-16)*FRACUNIT;
	fixed_t y = actor->y+((pr_teleg()&31)-16)*FRACUNIT;

	mo = Spawn<ATeleGlitter1> (x, y,
		actor->Sector->floorplane.ZatPoint (actor->x, actor->y), ALLOW_REPLACE);
	mo->momz = FRACUNIT/4;
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnTeleGlitter2
//
//----------------------------------------------------------------------------

void A_SpawnTeleGlitter2 (AActor *actor)
{
	AActor *mo;
	fixed_t x = actor->x+((pr_teleg2()&31)-16)*FRACUNIT;
	fixed_t y = actor->y+((pr_teleg2()&31)-16)*FRACUNIT;

	mo = Spawn<ATeleGlitter2> (x, y,
		actor->Sector->floorplane.ZatPoint (actor->x, actor->y), ALLOW_REPLACE);
	mo->momz = FRACUNIT/4;
}

//----------------------------------------------------------------------------
//
// PROC A_AccTeleGlitter
//
//----------------------------------------------------------------------------

void A_AccTeleGlitter (AActor *actor)
{
	if (++actor->health > 35)
	{
		actor->momz += actor->momz/2;
	}
}

// --- Volcano --------------------------------------------------------------

void A_VolcanoSet (AActor *);
void A_VolcanoBlast (AActor *);
void A_VolcBallImpact (AActor *);
extern void A_BeastPuff (AActor *);

// Volcano ------------------------------------------------------------------

class AVolcano : public AActor
{
	DECLARE_ACTOR (AVolcano, AActor)
};

FState AVolcano::States[] =
{
	S_NORMAL (VLCO, 'A',  350, NULL 					, &States[1]),
	S_NORMAL (VLCO, 'A',   35, A_VolcanoSet 			, &States[2]),
	S_NORMAL (VLCO, 'B',	3, NULL 					, &States[3]),
	S_NORMAL (VLCO, 'C',	3, NULL 					, &States[4]),
	S_NORMAL (VLCO, 'D',	3, NULL 					, &States[5]),
	S_NORMAL (VLCO, 'B',	3, NULL 					, &States[6]),
	S_NORMAL (VLCO, 'C',	3, NULL 					, &States[7]),
	S_NORMAL (VLCO, 'D',	3, NULL 					, &States[8]),
	S_NORMAL (VLCO, 'E',   10, A_VolcanoBlast			, &States[1])
};

IMPLEMENT_ACTOR (AVolcano, Heretic, 87, 150)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (20)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (0)
END_DEFAULTS

// Volcano blast ------------------------------------------------------------

class AVolcanoBlast : public AActor
{
	DECLARE_ACTOR (AVolcanoBlast, AActor)
};

FState AVolcanoBlast::States[] =
{
#define S_VOLCANOBALL 0
	S_NORMAL (VFBL, 'A',	4, A_BeastPuff				, &States[S_VOLCANOBALL+1]),
	S_NORMAL (VFBL, 'B',	4, A_BeastPuff				, &States[S_VOLCANOBALL+0]),

#define S_VOLCANOBALLX (S_VOLCANOBALL+2)
	S_NORMAL (XPL1, 'A',	4, A_VolcBallImpact 		, &States[S_VOLCANOBALLX+1]),
	S_NORMAL (XPL1, 'B',	4, NULL 					, &States[S_VOLCANOBALLX+2]),
	S_NORMAL (XPL1, 'C',	4, NULL 					, &States[S_VOLCANOBALLX+3]),
	S_NORMAL (XPL1, 'D',	4, NULL 					, &States[S_VOLCANOBALLX+4]),
	S_NORMAL (XPL1, 'E',	4, NULL 					, &States[S_VOLCANOBALLX+5]),
	S_NORMAL (XPL1, 'F',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AVolcanoBlast, Heretic, -1, 123)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (2)
	PROP_Damage (2)
	PROP_DamageType (NAME_Fire)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_VOLCANOBALL)
	PROP_DeathState (S_VOLCANOBALLX)

	PROP_DeathSound ("world/volcano/blast")
END_DEFAULTS

// Volcano T Blast ----------------------------------------------------------

class AVolcanoTBlast : public AActor
{
	DECLARE_ACTOR (AVolcanoTBlast, AActor)
};

FState AVolcanoTBlast::States[] =
{
#define S_VOLCANOTBALL 0
	S_NORMAL (VTFB, 'A',	4, NULL 					, &States[S_VOLCANOTBALL+1]),
	S_NORMAL (VTFB, 'B',	4, NULL 					, &States[S_VOLCANOTBALL+0]),

#define S_VOLCANOTBALLX (S_VOLCANOTBALL+2)
	S_NORMAL (SFFI, 'C',	4, NULL 					, &States[S_VOLCANOTBALLX+1]),
	S_NORMAL (SFFI, 'B',	4, NULL 					, &States[S_VOLCANOTBALLX+2]),
	S_NORMAL (SFFI, 'A',	4, NULL 					, &States[S_VOLCANOTBALLX+3]),
	S_NORMAL (SFFI, 'B',	4, NULL 					, &States[S_VOLCANOTBALLX+4]),
	S_NORMAL (SFFI, 'C',	4, NULL 					, &States[S_VOLCANOTBALLX+5]),
	S_NORMAL (SFFI, 'D',	4, NULL 					, &States[S_VOLCANOTBALLX+6]),
	S_NORMAL (SFFI, 'E',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AVolcanoTBlast, Heretic, -1, 124)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (2)
	PROP_Damage (1)
	PROP_DamageType (NAME_Fire)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_VOLCANOTBALL)
	PROP_DeathState (S_VOLCANOTBALLX)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_BeastPuff
//
//----------------------------------------------------------------------------

void A_BeastPuff (AActor *actor)
{
	if (pr_volcano() > 64)
	{
		fixed_t x, y, z;

		x = actor->x + (pr_volcano.Random2 () << 10);
		y = actor->y + (pr_volcano.Random2 () << 10);
		z = actor->z + (pr_volcano.Random2 () << 10);
		Spawn ("Puffy", x, y, z, ALLOW_REPLACE);
	}
}//----------------------------------------------------------------------------
//
// PROC A_VolcanoSet
//
//----------------------------------------------------------------------------

void A_VolcanoSet (AActor *volcano)
{
	volcano->tics = 105 + (pr_volcano() & 127);
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoBlast
//
//----------------------------------------------------------------------------

void A_VolcanoBlast (AActor *volcano)
{
	int i;
	int count;
	AActor *blast;
	angle_t angle;

	count = 1 + (pr_blast() % 3);
	for (i = 0; i < count; i++)
	{
		blast = Spawn<AVolcanoBlast> (volcano->x, volcano->y,
			volcano->z + 44*FRACUNIT, ALLOW_REPLACE);
		blast->target = volcano;
		angle = pr_blast () << 24;
		blast->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		blast->momx = FixedMul (1*FRACUNIT, finecosine[angle]);
		blast->momy = FixedMul (1*FRACUNIT, finesine[angle]);
		blast->momz = (FRACUNIT*5/2) + (pr_blast() << 10);
		S_Sound (blast, CHAN_BODY, "world/volcano/shoot", 1, ATTN_NORM);
		P_CheckMissileSpawn (blast);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_VolcBallImpact
//
//----------------------------------------------------------------------------

void A_VolcBallImpact (AActor *ball)
{
	int i;
	AActor *tiny;
	angle_t angle;

	if (ball->z <= ball->floorz)
	{
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		ball->z += 28*FRACUNIT;
		//ball->momz = 3*FRACUNIT;
	}
	P_RadiusAttack (ball, ball->target, 25, 25, NAME_Fire, true);
	for (i = 0; i < 4; i++)
	{
		tiny = Spawn<AVolcanoTBlast> (ball->x, ball->y, ball->z, ALLOW_REPLACE);
		tiny->target = ball;
		angle = i*ANG90;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = FixedMul (FRACUNIT*7/10, finecosine[angle]);
		tiny->momy = FixedMul (FRACUNIT*7/10, finesine[angle]);
		tiny->momz = FRACUNIT + (pr_impact() << 9);
		P_CheckMissileSpawn (tiny);
	}
}

