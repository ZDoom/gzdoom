#include "actor.h"
#include "info.h"
#include "gi.h"
#include "a_sharedglobal.h"

// Default actor for unregistered doomednums -------------------------------

FState AUnknown::States[] =
{
	S_NORMAL (UNKN, 'A',   -1, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AUnknown, Any, -1, 0)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (56)
	PROP_Flags (MF_NOGRAVITY|MF_NOBLOCKMAP)

	PROP_SpawnState (0)
END_DEFAULTS

// Route node for monster patrols -------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APatrolPoint, Any, 9024, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Mass (10)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

// A special to execute when a monster reaches a matching patrol point ------

IMPLEMENT_STATELESS_ACTOR (APatrolSpecial, Any, 9047, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Mass (10)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

// Blood sprite - adjusts itself for each game -----------------------------

FState ABlood::States[] =
{
#define S_DBLOOD 0
	S_NORMAL (BLUD, 'C',	8, NULL 						, &States[S_DBLOOD+1]),
	S_NORMAL (BLUD, 'B',	8, NULL 						, &States[S_DBLOOD+2]),
	S_NORMAL (BLUD, 'A',	8, NULL 						, NULL),

#define S_HBLOOD (S_DBLOOD+3)
	S_NORMAL (BLOD, 'C',	8, NULL							, &States[S_HBLOOD+1]),
	S_NORMAL (BLOD, 'B',	8, NULL							, &States[S_HBLOOD+2]),
	S_NORMAL (BLOD, 'A',	8, NULL							, NULL)
};

IMPLEMENT_ACTOR (ABlood, Any, -1, 130)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Mass (5)
END_DEFAULTS

AT_GAME_SET (Blood)
{
	ABlood *def = GetDefault<ABlood>();

	def->SpawnState = &ABlood::States[gameinfo.gametype == GAME_Doom ? S_DBLOOD : S_HBLOOD];
}

// Map spot ----------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AMapSpot, Any, 9001, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

// Map spot with gravity ---------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AMapSpotGravity, Any, 9013, 0)
	PROP_Flags (0)
END_DEFAULTS

// Bloody gibs -------------------------------------------------------------

FState ARealGibs::States[] =
{
	S_NORMAL (POL5, 'A', -1, NULL, NULL),
	S_NORMAL (GIBS, 'A', -1, NULL, NULL),
};

IMPLEMENT_ACTOR (ARealGibs, Any, -1, 0)
	PROP_Flags (MF_DROPOFF|MF_CORPSE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags3 (MF3_DONTGIB)
END_DEFAULTS

AT_GAME_SET (RealGibs)
{
	ARealGibs *def = GetDefault<ARealGibs>();

	if (gameinfo.gametype == GAME_Doom)
	{
		def->SpawnState = &ARealGibs::States[0];
	}
	else if (gameinfo.gametype & GAME_Raven)
	{
		def->SpawnState = &ARealGibs::States[1];
	}
}

// Gibs that can be placed on a map. ---------------------------------------
//
// These need to be a separate class from the above, in case someone uses
// a deh patch to change the gibs, since ZDoom actually creates a gib actor
// for actors that get crushed instead of changing their state as Doom did.

class AGibs : public ARealGibs
{
	DECLARE_STATELESS_ACTOR (AGibs, ARealGibs)
};

IMPLEMENT_STATELESS_ACTOR (AGibs, Doom, 24, 146)
	PROP_SpawnState (0)
END_DEFAULTS
