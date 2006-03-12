#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "gi.h"
#include "doomstat.h"
#include "gstrings.h"

static FRandom pr_spawnpuffx ("SpawnPuffX");

// The barrel of green goop ------------------------------------------------

void A_BarrelDestroy (AActor *);
void A_BarrelRespawn (AActor *);

FState AExplosiveBarrel::States[] =
{
#define S_BAR 0
	S_NORMAL (BAR1, 'A',	6, NULL 						, &States[S_BAR+1]),
	S_NORMAL (BAR1, 'B',	6, NULL 						, &States[S_BAR+0]),

#define S_BEXP (S_BAR+2)
	S_BRIGHT (BEXP, 'A',	5, NULL 						, &States[S_BEXP+1]),
	S_BRIGHT (BEXP, 'B',	5, A_Scream 					, &States[S_BEXP+2]),
	S_BRIGHT (BEXP, 'C',	5, NULL 						, &States[S_BEXP+3]),
	S_BRIGHT (BEXP, 'D',   10, A_Explode					, &States[S_BEXP+4]),
	S_BRIGHT (BEXP, 'E',   10, NULL 						, &States[S_BEXP+5]),
	S_BRIGHT (BEXP, 'E', 1050, A_BarrelDestroy				, &States[S_BEXP+6]),
	S_BRIGHT (BEXP, 'E',    5, A_BarrelRespawn				, &States[S_BEXP+6])
};

IMPLEMENT_ACTOR (AExplosiveBarrel, Doom, 2035, 125)
	PROP_SpawnHealth (20)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (34)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags2 (MF2_MCROSS)
	PROP_Flags3 (MF3_DONTGIB)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_BAR)
	PROP_DeathState (S_BEXP)

	PROP_DeathSound ("world/barrelx")
END_DEFAULTS

const char *AExplosiveBarrel::GetObituary ()
{
	return GStrings("OB_BARREL");
}

void A_BarrelDestroy (AActor *actor)
{
	if ((dmflags2 & DF2_BARRELS_RESPAWN) &&
		(deathmatch || alwaysapplydmflags))
	{
		actor->height = actor->GetDefault()->height;
		actor->renderflags |= RF_INVISIBLE;
		actor->flags &= ~MF_SOLID;
	}
	else
	{
		actor->Destroy ();
	}
}

void A_BarrelRespawn (AActor *actor)
{
	fixed_t x = actor->SpawnPoint[0] << FRACBITS;
	fixed_t y = actor->SpawnPoint[1] << FRACBITS;
	sector_t *sec;

	actor->flags |= MF_SOLID;
	sec = R_PointInSubsector (x, y)->sector;
	actor->SetOrigin (x, y, sec->floorplane.ZatPoint (x, y));
	if (P_TestMobjLocation (actor))
	{
		AActor *defs = actor->GetDefault();
		actor->health = defs->health;
		actor->flags = defs->flags;
		actor->flags2 = defs->flags2;
		actor->SetState (actor->SpawnState);
		actor->renderflags &= ~RF_INVISIBLE;
		Spawn<ATeleportFog> (x, y, actor->z + TELEFOGHEIGHT);
	}
	else
	{
		actor->flags &= ~MF_SOLID;
	}
}

// Bullet puff -------------------------------------------------------------

FState ABulletPuff::States[] =
{
	S_BRIGHT (PUFF, 'A',	4, NULL 						, &States[1]),
	S_NORMAL (PUFF, 'B',	4, NULL 						, &States[2]),
	S_NORMAL (PUFF, 'C',	4, NULL 						, &States[3]),
	S_NORMAL (PUFF, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABulletPuff, Doom, -1, 131)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags4 (MF4_ALLOWPARTICLES)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC50)

	PROP_SpawnState (0)
	PROP_MeleeState (2)
END_DEFAULTS

void ABulletPuff::BeginPlay ()
{
	Super::BeginPlay ();

	momz = FRACUNIT;
	tics -= pr_spawnpuffx() & 3;
	if (tics < 1)
		tics = 1;
}

// Container for an unused state -------------------------------------------

/* Doom defined the states S_STALAG, S_DEADTORSO, and S_DEADBOTTOM but never
 * actually used them. For compatibility with DeHackEd patches, they still
 * need to be kept around. This actor serves that purpose.
 */

class ADoomUnusedStates : public AActor
{
	DECLARE_ACTOR (ADoomUnusedStates, AActor)
};

FState ADoomUnusedStates::States[] =
{
#define S_STALAG 0
	S_NORMAL (SMT2, 'A',   -1, NULL 			, NULL),

#define S_DEADTORSO (S_STALAG+1)
	S_NORMAL (PLAY, 'N',   -1, NULL 			, NULL),

#define S_DEADBOTTOM (S_DEADTORSO+1)
	S_NORMAL (PLAY, 'S',   -1, NULL 			, NULL)
};

IMPLEMENT_ACTOR (ADoomUnusedStates, Doom, -1, 0)
	PROP_DeathState (S_DEADTORSO)
END_DEFAULTS
