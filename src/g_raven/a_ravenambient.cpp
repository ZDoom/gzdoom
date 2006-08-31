#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "gi.h"

void A_WindSound (AActor *);
void A_WaterfallSound (AActor *);

// Wind ---------------------------------------------------------------------

class ASoundWind : public AActor
{
	DECLARE_ACTOR (ASoundWind, AActor)
};

FState ASoundWind::States[] =
{
	S_NORMAL (TNT1, 'A',   2, A_WindSound, &States[0])
};

IMPLEMENT_ACTOR (ASoundWind, Raven, -1, 110)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
END_DEFAULTS

AT_GAME_SET (SoundWind)
{
	if (gameinfo.gametype == GAME_Heretic)
	{
		DOOMEDNUMOF(ASoundWind) = 42;
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		DOOMEDNUMOF(ASoundWind) = 1410;
	}
}

// Waterfall ----------------------------------------------------------------

class ASoundWaterfall : public AActor
{
	DECLARE_ACTOR (ASoundWaterfall, AActor)
};

FState ASoundWaterfall::States[] =
{
	S_NORMAL (TNT1, 'A',   2, A_WaterfallSound, &States[0])
};

// Interestingly, MT_SOUNDWATERFALL is present in both Heretic and Hexen
// with doomednum 41. However, in Hexen, ZShroomLarge3 also has ednum 41
// and comes before the waterfall, so it will never be possible to place
// it in a map.

IMPLEMENT_ACTOR (ASoundWaterfall, Heretic, 41, 111)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
	PROP_SpawnState (0)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_WindSound
//
//----------------------------------------------------------------------------

void A_WindSound (AActor *self)
{
	if (!S_IsActorPlayingSomething (self, 6, -1))
	{
		S_LoopedSound (self, 6, "world/wind", 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_WaterfallSound
//
//----------------------------------------------------------------------------

void A_WaterfallSound (AActor *self)
{
	if (!S_IsActorPlayingSomething (self, 6, -1))
	{
		S_LoopedSound (self, 6, "world/waterfall", 1, ATTN_NORM);
	}
}
