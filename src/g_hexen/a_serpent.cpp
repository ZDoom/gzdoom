#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "p_terrain.h"

static FRandom pr_serpentchase ("SerpentChase");
static FRandom pr_serpenthump ("SerpentHump");
static FRandom pr_serpentattack ("SerpentAttack");
static FRandom pr_serpentmeattack ("SerpentMeAttack");
static FRandom pr_serpentgibs ("SerpentGibs");
static FRandom pr_delaygib ("DelayGib");

void A_DoChase(AActor * actor, bool fastchase, FState * meleestate, FState * missilestate, bool playactive, bool nightmarefast,bool dontmove);

void A_SerpentChase (AActor *);
void A_SerpentHumpDecide (AActor *);
void A_SerpentDiveSound (AActor *);
void A_SerpentHide (AActor *);
void A_SerpentBirthScream (AActor *);
void A_SerpentDiveSound (AActor *);
void A_SerpentCheckForAttack (AActor *);
void A_SerpentHeadPop (AActor *);
void A_SerpentSpawnGibs (AActor *);
void A_SerpentWalk (AActor *);
void A_SerpentUnHide (AActor *);
void A_SerpentRaiseHump (AActor *);
void A_SerpentLowerHump (AActor *);
void A_SerpentMeleeAttack (AActor *);
void A_SerpentMissileAttack (AActor *);
void A_SerpentChooseAttack (AActor *);
void A_SerpentFXSound (AActor *);
void A_StopSerpentFXSound (AActor *);
void A_SerpentHeadCheck (AActor *);
void A_FloatGib (AActor *);
void A_DelayGib (AActor *);
void A_SinkGib (AActor *);

// Serpent ------------------------------------------------------------------

class ASerpent : public AActor
{
	DECLARE_ACTOR (ASerpent, AActor)
public:
	bool bLeader;
	void Serialize (FArchive &arc);
};

FState ASerpent::States[] =
{
#define S_SERPENT_LOOK1 0
	S_NORMAL (SSPT, 'H',   10, A_Look				    , &States[S_SERPENT_LOOK1]),

#define S_SERPENT_SWIM1 (S_SERPENT_LOOK1+1)
	S_NORMAL (SSPT, 'H',	1, A_SerpentChase		    , &States[S_SERPENT_SWIM1+1]),
	S_NORMAL (SSPT, 'H',	1, A_SerpentChase		    , &States[S_SERPENT_SWIM1+2]),
	S_NORMAL (SSPT, 'H',	2, A_SerpentHumpDecide	    , &States[S_SERPENT_SWIM1]),

#define S_SERPENT_PAIN1 (S_SERPENT_SWIM1+3)
	S_NORMAL (SSPT, 'L',	5, NULL					    , &States[S_SERPENT_PAIN1+1]),
	S_NORMAL (SSPT, 'L',	5, A_Pain				    , &States[S_SERPENT_PAIN1+2]),
	S_NORMAL (SSDV, 'A',	4, NULL					    , &States[S_SERPENT_PAIN1+3]),
	S_NORMAL (SSDV, 'B',	4, NULL					    , &States[S_SERPENT_PAIN1+4]),
	S_NORMAL (SSDV, 'C',	4, NULL					    , &States[S_SERPENT_PAIN1+5]),
	S_NORMAL (SSDV, 'D',	4, A_UnSetShootable		    , &States[S_SERPENT_PAIN1+6]),
	S_NORMAL (SSDV, 'E',	3, A_SerpentDiveSound	    , &States[S_SERPENT_PAIN1+7]),
	S_NORMAL (SSDV, 'F',	3, NULL					    , &States[S_SERPENT_PAIN1+8]),
	S_NORMAL (SSDV, 'G',	4, NULL					    , &States[S_SERPENT_PAIN1+9]),
	S_NORMAL (SSDV, 'H',	4, NULL					    , &States[S_SERPENT_PAIN1+10]),
	S_NORMAL (SSDV, 'I',	3, NULL					    , &States[S_SERPENT_PAIN1+11]),
	S_NORMAL (SSDV, 'J',	3, A_SerpentHide		    , &States[S_SERPENT_SWIM1]),

#define S_SERPENT_SURFACE1 (S_SERPENT_PAIN1+12)
	S_NORMAL (SSPT, 'A',	1, A_UnHideThing		    , &States[S_SERPENT_SURFACE1+1]),
	S_NORMAL (SSPT, 'A',	1, A_SerpentBirthScream	    , &States[S_SERPENT_SURFACE1+2]),
	S_NORMAL (SSPT, 'B',	3, A_SetShootable		    , &States[S_SERPENT_SURFACE1+3]),
	S_NORMAL (SSPT, 'C',	3, NULL					    , &States[S_SERPENT_SURFACE1+4]),
	S_NORMAL (SSPT, 'D',	4, A_SerpentCheckForAttack  , &States[S_SERPENT_PAIN1+2]),

#define S_SERPENT_DIE1 (S_SERPENT_SURFACE1+5)
	S_NORMAL (SSPT, 'O',	4, NULL					    , &States[S_SERPENT_DIE1+1]),
	S_NORMAL (SSPT, 'P',	4, A_Scream				    , &States[S_SERPENT_DIE1+2]),
	S_NORMAL (SSPT, 'Q',	4, A_NoBlocking			    , &States[S_SERPENT_DIE1+3]),
	S_NORMAL (SSPT, 'R',	4, NULL					    , &States[S_SERPENT_DIE1+4]),
	S_NORMAL (SSPT, 'S',	4, NULL					    , &States[S_SERPENT_DIE1+5]),
	S_NORMAL (SSPT, 'T',	4, NULL					    , &States[S_SERPENT_DIE1+6]),
	S_NORMAL (SSPT, 'U',	4, NULL					    , &States[S_SERPENT_DIE1+7]),
	S_NORMAL (SSPT, 'V',	4, NULL					    , &States[S_SERPENT_DIE1+8]),
	S_NORMAL (SSPT, 'W',	4, NULL					    , &States[S_SERPENT_DIE1+9]),
	S_NORMAL (SSPT, 'X',	4, NULL					    , &States[S_SERPENT_DIE1+10]),
	S_NORMAL (SSPT, 'Y',	4, NULL					    , &States[S_SERPENT_DIE1+11]),
	S_NORMAL (SSPT, 'Z',	4, NULL					    , NULL),

#define S_SERPENT_XDIE1 (S_SERPENT_DIE1+12)
	S_NORMAL (SSXD, 'A',	4, NULL					    , &States[S_SERPENT_XDIE1+1]),
	S_NORMAL (SSXD, 'B',	4, A_SerpentHeadPop		    , &States[S_SERPENT_XDIE1+2]),
	S_NORMAL (SSXD, 'C',	4, A_NoBlocking			    , &States[S_SERPENT_XDIE1+3]),
	S_NORMAL (SSXD, 'D',	4, NULL					    , &States[S_SERPENT_XDIE1+4]),
	S_NORMAL (SSXD, 'E',	4, NULL					    , &States[S_SERPENT_XDIE1+5]),
	S_NORMAL (SSXD, 'F',	3, NULL					    , &States[S_SERPENT_XDIE1+6]),
	S_NORMAL (SSXD, 'G',	3, NULL					    , &States[S_SERPENT_XDIE1+7]),
	S_NORMAL (SSXD, 'H',	3, A_SerpentSpawnGibs	    , NULL),

#define S_SERPENT_WALK1 (S_SERPENT_XDIE1+8)
	S_NORMAL (SSPT, 'I',	5, A_SerpentWalk		    , &States[S_SERPENT_WALK1+1]),
	S_NORMAL (SSPT, 'J',	5, A_SerpentWalk		    , &States[S_SERPENT_WALK1+2]),
	S_NORMAL (SSPT, 'I',	5, A_SerpentWalk		    , &States[S_SERPENT_WALK1+3]),
	S_NORMAL (SSPT, 'J',	5, A_SerpentCheckForAttack  , &States[S_SERPENT_PAIN1+2]),

#define S_SERPENT_HUMP1 (S_SERPENT_WALK1+4)
	S_NORMAL (SSPT, 'H',	3, A_SerpentUnHide		    , &States[S_SERPENT_HUMP1+1]),
	S_NORMAL (SSPT, 'E',	3, A_SerpentRaiseHump	    , &States[S_SERPENT_HUMP1+2]),
	S_NORMAL (SSPT, 'F',	3, A_SerpentRaiseHump	    , &States[S_SERPENT_HUMP1+3]),
	S_NORMAL (SSPT, 'G',	3, A_SerpentRaiseHump	    , &States[S_SERPENT_HUMP1+4]),
	S_NORMAL (SSPT, 'E',	3, A_SerpentRaiseHump	    , &States[S_SERPENT_HUMP1+5]),
	S_NORMAL (SSPT, 'F',	3, A_SerpentRaiseHump	    , &States[S_SERPENT_HUMP1+6]),
	S_NORMAL (SSPT, 'G',	3, NULL					    , &States[S_SERPENT_HUMP1+7]),
	S_NORMAL (SSPT, 'E',	3, NULL					    , &States[S_SERPENT_HUMP1+8]),
	S_NORMAL (SSPT, 'F',	3, NULL					    , &States[S_SERPENT_HUMP1+9]),
	S_NORMAL (SSPT, 'G',	3, A_SerpentLowerHump	    , &States[S_SERPENT_HUMP1+10]),
	S_NORMAL (SSPT, 'E',	3, A_SerpentLowerHump	    , &States[S_SERPENT_HUMP1+11]),
	S_NORMAL (SSPT, 'F',	3, A_SerpentLowerHump	    , &States[S_SERPENT_HUMP1+12]),
	S_NORMAL (SSPT, 'G',	3, A_SerpentLowerHump	    , &States[S_SERPENT_HUMP1+13]),
	S_NORMAL (SSPT, 'E',	3, A_SerpentLowerHump	    , &States[S_SERPENT_HUMP1+14]),
	S_NORMAL (SSPT, 'F',	3, A_SerpentHide		    , &States[S_SERPENT_SWIM1]),

#define S_SERPENT_MELEE1 (S_SERPENT_HUMP1+15)
	S_NORMAL (SSPT, 'N',	5, A_SerpentMeleeAttack	    , &ASerpent::States[S_SERPENT_PAIN1+2]),

#define S_SERPENT_MISSILE1 (S_SERPENT_MELEE1+1)
	S_NORMAL (SSPT, 'N',	5, A_SerpentMissileAttack   , &ASerpent::States[S_SERPENT_PAIN1+2]),

#define S_SERPENT_ATK1 (S_SERPENT_MISSILE1+1)
	S_NORMAL (SSPT, 'K',	6, A_FaceTarget			    , &States[S_SERPENT_ATK1+1]),
	S_NORMAL (SSPT, 'L',	5, A_SerpentChooseAttack    , &States[S_SERPENT_MELEE1]),

#define S_SERPENT_ICE (S_SERPENT_ATK1+2)
	S_NORMAL (SSPT, '[',	5, A_FreezeDeath		    , &States[S_SERPENT_ICE+1]),
	S_NORMAL (SSPT, '[',	1, A_FreezeDeathChunks	    , &States[S_SERPENT_ICE+1]),
};

IMPLEMENT_ACTOR (ASerpent, Hexen, 121, 6)
	PROP_SpawnHealth (90)
	PROP_PainChance (96)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (70)
	PROP_MassLong (0x7fffffff)		// [RH] Is this a mistake?
	PROP_Flags (MF_SOLID|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_MCROSS|MF2_CANTLEAVEFLOORPIC|MF2_NONSHOOTABLE)
	PROP_Flags3 (MF3_STAYMORPHED|MF3_DONTBLAST|MF3_NOTELEOTHER)
	PROP_RenderFlags (RF_INVISIBLE)

	PROP_SpawnState (S_SERPENT_LOOK1)
	PROP_SeeState (S_SERPENT_SWIM1)
	PROP_PainState (S_SERPENT_PAIN1)
	PROP_MeleeState (S_SERPENT_SURFACE1)
	PROP_DeathState (S_SERPENT_DIE1)
	PROP_XDeathState (S_SERPENT_XDIE1)
	PROP_IDeathState (S_SERPENT_ICE)

	PROP_SeeSound ("SerpentSight")
	PROP_AttackSound ("SerpentAttack")
	PROP_PainSound ("SerpentPain")
	PROP_DeathSound ("SerpentDeath")
	PROP_HitObituary("$OB_SERPENTHIT")
END_DEFAULTS

void ASerpent::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bLeader;
}

// Serpent Leader -----------------------------------------------------------

class ASerpentLeader : public ASerpent
{
	DECLARE_STATELESS_ACTOR (ASerpentLeader, ASerpent)
public:
	void BeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ASerpentLeader, Hexen, 120, 7)
	PROP_Mass (200)
	PROP_Obituary("$OB_SERPENT")
END_DEFAULTS

void ASerpentLeader::BeginPlay ()
{
	bLeader = true;
}

// Serpent Missile Ball -----------------------------------------------------

class ASerpentFX : public AActor
{
	DECLARE_ACTOR (ASerpentFX, AActor)
public:
	void Tick ()
	{
		Super::Tick ();
	}
};

FState ASerpentFX::States[] =
{
#define S_SERPENT_FX1 0
	// [RH] This 0-length state was added so that the looping sound can start
	// playing as soon as possible, because action functions are not called
	// when an actor is spawned. (Should I change that? No.)
	S_BRIGHT (SSFX, 'A',	0, NULL					    , &States[S_SERPENT_FX1+1]),
	S_BRIGHT (SSFX, 'A',	3, A_SerpentFXSound		    , &States[S_SERPENT_FX1+2]),
	S_BRIGHT (SSFX, 'B',	3, NULL					    , &States[S_SERPENT_FX1+3]),
	S_BRIGHT (SSFX, 'A',	3, NULL					    , &States[S_SERPENT_FX1+4]),
	S_BRIGHT (SSFX, 'B',	3, NULL					    , &States[S_SERPENT_FX1+1]),

#define S_SERPENT_FX_X1 (S_SERPENT_FX1+5)
	S_BRIGHT (SSFX, 'C',	4, A_StopSerpentFXSound	    , &States[S_SERPENT_FX_X1+1]),
	S_BRIGHT (SSFX, 'D',	4, NULL					    , &States[S_SERPENT_FX_X1+2]),
	S_BRIGHT (SSFX, 'E',	4, NULL					    , &States[S_SERPENT_FX_X1+3]),
	S_BRIGHT (SSFX, 'F',	4, NULL					    , &States[S_SERPENT_FX_X1+4]),
	S_BRIGHT (SSFX, 'G',	4, NULL					    , &States[S_SERPENT_FX_X1+5]),
	S_BRIGHT (SSFX, 'H',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASerpentFX, Hexen, -1, 0)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (10)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SERPENT_FX1)
	PROP_DeathState (S_SERPENT_FX_X1)

	PROP_DeathSound ("SerpentFXHit")
END_DEFAULTS

// Serpent Head -------------------------------------------------------------

class ASerpentHead : public AActor
{
	DECLARE_ACTOR (ASerpentHead, AActor)
};

FState ASerpentHead::States[] =
{
#define S_SERPENT_HEAD1 0
	S_NORMAL (SSXD, 'I',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+1]),
	S_NORMAL (SSXD, 'J',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+2]),
	S_NORMAL (SSXD, 'K',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+3]),
	S_NORMAL (SSXD, 'L',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+4]),
	S_NORMAL (SSXD, 'M',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+5]),
	S_NORMAL (SSXD, 'N',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+6]),
	S_NORMAL (SSXD, 'O',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1+7]),
	S_NORMAL (SSXD, 'P',	4, A_SerpentHeadCheck	    , &States[S_SERPENT_HEAD1]),

#define S_SERPENT_HEAD_X1 (S_SERPENT_HEAD1+8)
	S_NORMAL (SSXD, 'S',   -1, NULL					    , &States[S_SERPENT_HEAD_X1]),
};

IMPLEMENT_ACTOR (ASerpentHead, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (10)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP)

	PROP_SpawnState (S_SERPENT_HEAD1)
	PROP_DeathState (S_SERPENT_HEAD_X1)
END_DEFAULTS

// Serpent Gib 1 ------------------------------------------------------------

class ASerpentGib1 : public AActor
{
	DECLARE_ACTOR (ASerpentGib1, AActor)
};

FState ASerpentGib1::States[] =
{
#define S_SERPENT_GIB1_1 0
	S_NORMAL (SSXD, 'Q',	6, NULL					    , &States[S_SERPENT_GIB1_1+1]),
	S_NORMAL (SSXD, 'Q',	6, A_FloatGib			    , &States[S_SERPENT_GIB1_1+2]),
	S_NORMAL (SSXD, 'Q',	8, A_FloatGib			    , &States[S_SERPENT_GIB1_1+3]),
	S_NORMAL (SSXD, 'Q',	8, A_FloatGib			    , &States[S_SERPENT_GIB1_1+4]),
	S_NORMAL (SSXD, 'Q',   12, A_FloatGib			    , &States[S_SERPENT_GIB1_1+5]),
	S_NORMAL (SSXD, 'Q',   12, A_FloatGib			    , &States[S_SERPENT_GIB1_1+6]),
	S_NORMAL (SSXD, 'Q',  232, A_DelayGib			    , &States[S_SERPENT_GIB1_1+7]),
	S_NORMAL (SSXD, 'Q',   12, A_SinkGib			    , &States[S_SERPENT_GIB1_1+8]),
	S_NORMAL (SSXD, 'Q',   12, A_SinkGib			    , &States[S_SERPENT_GIB1_1+9]),
	S_NORMAL (SSXD, 'Q',	8, A_SinkGib			    , &States[S_SERPENT_GIB1_1+10]),
	S_NORMAL (SSXD, 'Q',	8, A_SinkGib			    , &States[S_SERPENT_GIB1_1+11]),
	S_NORMAL (SSXD, 'Q',	8, A_SinkGib			    , NULL),
};

IMPLEMENT_ACTOR (ASerpentGib1, Hexen, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)

	PROP_SpawnState (S_SERPENT_GIB1_1)
END_DEFAULTS

// Serpent Gib 2 ------------------------------------------------------------

class ASerpentGib2 : public AActor
{
	DECLARE_ACTOR (ASerpentGib2, AActor)
};

FState ASerpentGib2::States[] =
{
#define S_SERPENT_GIB2_1 0
	S_NORMAL (SSXD, 'R',	6, NULL					    , &States[S_SERPENT_GIB2_1+1]),
	S_NORMAL (SSXD, 'R',	6, A_FloatGib			    , &States[S_SERPENT_GIB2_1+2]),
	S_NORMAL (SSXD, 'R',	8, A_FloatGib			    , &States[S_SERPENT_GIB2_1+3]),
	S_NORMAL (SSXD, 'R',	8, A_FloatGib			    , &States[S_SERPENT_GIB2_1+4]),
	S_NORMAL (SSXD, 'R',   12, A_FloatGib			    , &States[S_SERPENT_GIB2_1+5]),
	S_NORMAL (SSXD, 'R',   12, A_FloatGib			    , &States[S_SERPENT_GIB2_1+6]),
	S_NORMAL (SSXD, 'R',  232, A_DelayGib			    , &States[S_SERPENT_GIB2_1+7]),
	S_NORMAL (SSXD, 'R',   12, A_SinkGib			    , &States[S_SERPENT_GIB2_1+8]),
	S_NORMAL (SSXD, 'R',   12, A_SinkGib			    , &States[S_SERPENT_GIB2_1+9]),
	S_NORMAL (SSXD, 'R',	8, A_SinkGib			    , &States[S_SERPENT_GIB2_1+10]),
	S_NORMAL (SSXD, 'R',	8, A_SinkGib			    , &States[S_SERPENT_GIB2_1+11]),
	S_NORMAL (SSXD, 'R',	8, A_SinkGib			    , NULL),
};

IMPLEMENT_ACTOR (ASerpentGib2, Hexen, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)

	PROP_SpawnState (S_SERPENT_GIB2_1)
END_DEFAULTS

// Serpent Gib 3 ------------------------------------------------------------

class ASerpentGib3 : public AActor
{
	DECLARE_ACTOR (ASerpentGib3, AActor)
};

FState ASerpentGib3::States[] =
{
#define S_SERPENT_GIB3_1 0
	S_NORMAL (SSXD, 'T',	6, NULL					    , &States[S_SERPENT_GIB3_1+1]),
	S_NORMAL (SSXD, 'T',	6, A_FloatGib			    , &States[S_SERPENT_GIB3_1+2]),
	S_NORMAL (SSXD, 'T',	8, A_FloatGib			    , &States[S_SERPENT_GIB3_1+3]),
	S_NORMAL (SSXD, 'T',	8, A_FloatGib			    , &States[S_SERPENT_GIB3_1+4]),
	S_NORMAL (SSXD, 'T',   12, A_FloatGib			    , &States[S_SERPENT_GIB3_1+5]),
	S_NORMAL (SSXD, 'T',   12, A_FloatGib			    , &States[S_SERPENT_GIB3_1+6]),
	S_NORMAL (SSXD, 'T',  232, A_DelayGib			    , &States[S_SERPENT_GIB3_1+7]),
	S_NORMAL (SSXD, 'T',   12, A_SinkGib			    , &States[S_SERPENT_GIB3_1+8]),
	S_NORMAL (SSXD, 'T',   12, A_SinkGib			    , &States[S_SERPENT_GIB3_1+9]),
	S_NORMAL (SSXD, 'T',	8, A_SinkGib			    , &States[S_SERPENT_GIB3_1+10]),
	S_NORMAL (SSXD, 'T',	8, A_SinkGib			    , &States[S_SERPENT_GIB3_1+11]),
	S_NORMAL (SSXD, 'T',	8, A_SinkGib			    , NULL),
};

IMPLEMENT_ACTOR (ASerpentGib3, Hexen, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)

	PROP_SpawnState (S_SERPENT_GIB3_1)
END_DEFAULTS


//============================================================================
//
// A_SerpentUnHide
//
//============================================================================

void A_SerpentUnHide (AActor *actor)
{
	actor->renderflags &= ~RF_INVISIBLE;
	actor->floorclip = 24*FRACUNIT;
}

//============================================================================
//
// A_SerpentHide
//
//============================================================================

void A_SerpentHide (AActor *actor)
{
	actor->renderflags |= RF_INVISIBLE;
	actor->floorclip = 0;
}
//============================================================================
//
// A_SerpentChase
//
//============================================================================

void A_SerpentChase (AActor *actor)
{
	A_DoChase (actor, false, actor->MeleeState, NULL, false, true, false);
}

//============================================================================
//
// A_SerpentRaiseHump
// 
// Raises the hump above the surface by raising the floorclip level
//============================================================================

void A_SerpentRaiseHump (AActor *actor)
{
	actor->floorclip -= 4*FRACUNIT;
}

//============================================================================
//
// A_SerpentLowerHump
// 
//============================================================================

void A_SerpentLowerHump (AActor *actor)
{
	actor->floorclip += 4*FRACUNIT;
}

//============================================================================
//
// A_SerpentHumpDecide
//
//		Decided whether to hump up, or if the mobj is a serpent leader, 
//			to missile attack
//============================================================================

void A_SerpentHumpDecide (AActor *actor)
{
	if (static_cast<ASerpent *>(actor)->bLeader)
	{
		if (pr_serpenthump() > 30)
		{
			return;
		}
		else if (pr_serpenthump() < 40)
		{ // Missile attack
			actor->SetState (&ASerpent::States[S_SERPENT_SURFACE1]);
			return;
		}
	}
	else if (pr_serpenthump() > 3)
	{
		return;
	}
	if (!actor->CheckMeleeRange ())
	{ // The hump shouldn't occur when within melee range
		if (static_cast<ASerpent *>(actor)->bLeader &&
			pr_serpenthump() < 128)
		{
			actor->SetState (&ASerpent::States[S_SERPENT_SURFACE1]);
		}
		else
		{	
			actor->SetState (&ASerpent::States[S_SERPENT_HUMP1]);
			S_Sound (actor, CHAN_BODY, "SerpentActive", 1, ATTN_NORM);
		}
	}
}

//============================================================================
//
// A_SerpentBirthScream
//
//============================================================================

void A_SerpentBirthScream (AActor *actor)
{
	S_Sound (actor, CHAN_VOICE, "SerpentBirth", 1, ATTN_NORM);
}

//============================================================================
//
// A_SerpentDiveSound
//
//============================================================================

void A_SerpentDiveSound (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "SerpentActive", 1, ATTN_NORM);
}

//============================================================================
//
// A_SerpentWalk
//
// Similar to A_Chase, only has a hardcoded entering of meleestate
//============================================================================

void A_SerpentWalk (AActor *actor)
{
	A_DoChase (actor, false, &ASerpent::States[S_SERPENT_ATK1], NULL, true, true, false);
}

//============================================================================
//
// A_SerpentCheckForAttack
//
//============================================================================

void A_SerpentCheckForAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (static_cast<ASerpent *>(actor)->bLeader)
	{
		if (!actor->CheckMeleeRange ())
		{
			actor->SetState (&ASerpent::States[S_SERPENT_ATK1]);
			return;
		}
	}
	if (P_CheckMeleeRange2 (actor))
	{
		actor->SetState (&ASerpent::States[S_SERPENT_WALK1]);
	}
	else if (actor->CheckMeleeRange ())
	{
		if (pr_serpentattack() < 32)
		{
			actor->SetState (&ASerpent::States[S_SERPENT_WALK1]);
		}
		else
		{
			actor->SetState (&ASerpent::States[S_SERPENT_ATK1]);
		}
	}
}

//============================================================================
//
// A_SerpentChooseAttack
//
//============================================================================

void A_SerpentChooseAttack (AActor *actor)
{
	if (!actor->target || actor->CheckMeleeRange())
	{
		return;
	}
	if (static_cast<ASerpent *>(actor)->bLeader)
	{
		actor->SetState (&ASerpent::States[S_SERPENT_MISSILE1]);
	}
}
	
//============================================================================
//
// A_SerpentMeleeAttack
//
//============================================================================

void A_SerpentMeleeAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_serpentmeattack.HitDice (5);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		S_Sound (actor, CHAN_BODY, "SerpentMeleeHit", 1, ATTN_NORM);
	}
	if (pr_serpentmeattack() < 96)
	{
		A_SerpentCheckForAttack (actor);
	}
}

//============================================================================
//
// A_SerpentMissileAttack
//
//============================================================================
	
void A_SerpentMissileAttack (AActor *actor)
{
	AActor *mo;

	if (!actor->target)
	{
		return;
	}
	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ASerpentFX));
}

//============================================================================
//
// A_SerpentHeadPop
//
//============================================================================

void A_SerpentHeadPop (AActor *actor)
{
	Spawn<ASerpentHead> (actor->x, actor->y, actor->z+45*FRACUNIT, ALLOW_REPLACE);
}

//============================================================================
//
// A_SerpentSpawnGibs
//
//============================================================================

void A_SerpentSpawnGibs (AActor *actor)
{
	AActor *mo;
	static const PClass *const GibTypes[] =
	{
		RUNTIME_CLASS(ASerpentGib3),
		RUNTIME_CLASS(ASerpentGib2),
		RUNTIME_CLASS(ASerpentGib1)
	};

	for (int i = countof(GibTypes)-1; i >= 0; --i)
	{
		mo = Spawn (GibTypes[i],
			actor->x+((pr_serpentgibs()-128)<<12), 
			actor->y+((pr_serpentgibs()-128)<<12),
			actor->floorz+FRACUNIT, ALLOW_REPLACE);
		if (mo)
		{
			mo->momx = (pr_serpentgibs()-128)<<6;
			mo->momy = (pr_serpentgibs()-128)<<6;
			mo->floorclip = 6*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_FloatGib
//
//============================================================================

void A_FloatGib (AActor *actor)
{
	actor->floorclip -= FRACUNIT;
}

//============================================================================
//
// A_SinkGib
//
//============================================================================

void A_SinkGib (AActor *actor)
{
	actor->floorclip += FRACUNIT;
}

//============================================================================
//
// A_DelayGib
//
//============================================================================

void A_DelayGib (AActor *actor)
{
	actor->tics -= pr_delaygib()>>2;
}

//============================================================================
//
// A_SerpentHeadCheck
//
//============================================================================

void A_SerpentHeadCheck (AActor *actor)
{
	if (actor->z <= actor->floorz)
	{
		if (Terrains[P_GetThingFloorType(actor)].IsLiquid)
		{
			P_HitFloor (actor);
			actor->SetState (NULL);
		}
		else
		{
			actor->SetState (actor->FindState(NAME_Death));
		}
	}
}

//============================================================================
//
// A_SerpentFXSound
//
//============================================================================

void A_SerpentFXSound (AActor *actor)
{
	if (!S_IsActorPlayingSomething (actor, CHAN_BODY, -1))
	{
		S_LoopedSound (actor, CHAN_BODY, "SerpentFXContinuous", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_StopSerpentFXSound
//
//============================================================================

void A_StopSerpentFXSound (AActor *actor)
{
	S_StopSound (actor, CHAN_BODY);
}
