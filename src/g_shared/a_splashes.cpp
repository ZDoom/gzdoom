#include "actor.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "gi.h"

// Water --------------------------------------------------------------------

FState AWaterSplash::States[] =
{
#define S_SPLASH 0
	S_NORMAL (SPSH, 'A',	8, NULL, &States[S_SPLASH+1]),
	S_NORMAL (SPSH, 'B',	8, NULL, &States[S_SPLASH+2]),
	S_NORMAL (SPSH, 'C',	8, NULL, &States[S_SPLASH+3]),
	S_NORMAL (SPSH, 'D',   16, NULL, NULL),

#define S_SPLASHX (S_SPLASH+4)
	S_NORMAL (SPSH, 'D',   10, NULL, NULL)
};

IMPLEMENT_ACTOR (AWaterSplash, Any, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_LOGRAV|MF2_CANNOTPUSH)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (S_SPLASH)
	PROP_DeathState (S_SPLASHX)
END_DEFAULTS

FState AWaterSplashBase::States[] =
{
	S_NORMAL (SPSH, 'E',	5, NULL, &States[1]),
	S_NORMAL (SPSH, 'F',	5, NULL, &States[2]),
	S_NORMAL (SPSH, 'G',	5, NULL, &States[3]),
	S_NORMAL (SPSH, 'H',	5, NULL, &States[4]),
	S_NORMAL (SPSH, 'I',	5, NULL, &States[5]),
	S_NORMAL (SPSH, 'J',	5, NULL, &States[6]),
	S_NORMAL (SPSH, 'K',	5, NULL, NULL)
};

IMPLEMENT_ACTOR (AWaterSplashBase, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (0)
END_DEFAULTS

// Lava ---------------------------------------------------------------------

FState ALavaSplash::States[] =
{
	S_BRIGHT (LVAS, 'A',	5, NULL, &States[1]),
	S_BRIGHT (LVAS, 'B',	5, NULL, &States[2]),
	S_BRIGHT (LVAS, 'C',	5, NULL, &States[3]),
	S_BRIGHT (LVAS, 'D',	5, NULL, &States[4]),
	S_BRIGHT (LVAS, 'E',	5, NULL, &States[5]),
	S_BRIGHT (LVAS, 'F',	5, NULL, NULL)
};

IMPLEMENT_ACTOR (ALavaSplash, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (0)
END_DEFAULTS

FState ALavaSmoke::States[] =
{
	S_BRIGHT (LVAS, 'G',	5, NULL, &States[1]),
	S_BRIGHT (LVAS, 'H',	5, NULL, &States[2]),
	S_BRIGHT (LVAS, 'I',	5, NULL, &States[3]),
	S_BRIGHT (LVAS, 'J',	5, NULL, &States[4]),
	S_BRIGHT (LVAS, 'K',	5, NULL, NULL)
};

IMPLEMENT_ACTOR (ALavaSmoke, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (gameinfo.gametype==GAME_Heretic?HR_SHADOW:HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Sludge -------------------------------------------------------------------

FState ASludgeChunk::States[] =
{
#define S_SLUDGECHUNK 0
	S_NORMAL (SLDG, 'A',	8, NULL, &States[S_SLUDGECHUNK+1]),
	S_NORMAL (SLDG, 'B',	8, NULL, &States[S_SLUDGECHUNK+2]),
	S_NORMAL (SLDG, 'C',	8, NULL, &States[S_SLUDGECHUNK+3]),
	S_NORMAL (SLDG, 'D',	8, NULL, NULL),

#define S_SLUDGECHUNKX (S_SLUDGECHUNK+4)
	S_NORMAL (SLDG, 'D',	6, NULL, NULL)
};

IMPLEMENT_ACTOR (ASludgeChunk, Any, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_LOGRAV|MF2_CANNOTPUSH)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (S_SLUDGECHUNK)
	PROP_DeathState (S_SLUDGECHUNKX)
END_DEFAULTS

FState ASludgeSplash::States[] =
{
	S_NORMAL (SLDG, 'E',	6, NULL, &States[1]),
	S_NORMAL (SLDG, 'F',	6, NULL, &States[2]),
	S_NORMAL (SLDG, 'G',	6, NULL, &States[3]),
	S_NORMAL (SLDG, 'H',	6, NULL, NULL)
};

IMPLEMENT_ACTOR (ASludgeSplash, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (0)
END_DEFAULTS
