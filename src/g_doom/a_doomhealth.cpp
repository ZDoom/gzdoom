#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"

// Health bonus -------------------------------------------------------------

class AHealthBonus : public AHealth
{
	DECLARE_ACTOR (AHealthBonus, AHealth)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		player_t *player = toucher->player;
		player->health++;		// can go over 100%
		if (player->health > deh.MaxSoulsphere)
			player->health = deh.MaxSoulsphere;
		player->mo->health = player->health;
		return true;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTHTHBONUS);
	}
};

FState AHealthBonus::States[] =
{
	S_NORMAL (BON1, 'A',	6, NULL 				, &States[1]),
	S_NORMAL (BON1, 'B',	6, NULL 				, &States[2]),
	S_NORMAL (BON1, 'C',	6, NULL 				, &States[3]),
	S_NORMAL (BON1, 'D',	6, NULL 				, &States[4]),
	S_NORMAL (BON1, 'C',	6, NULL 				, &States[5]),
	S_NORMAL (BON1, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AHealthBonus, Doom, 2014, 152)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)

	PROP_SpawnState (0)
END_DEFAULTS

// Stimpack -----------------------------------------------------------------

class AStimpack : public AHealth
{
	DECLARE_ACTOR (AStimpack, AHealth)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveBody (toucher->player, 10);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTSTIM);
	}
};

FState AStimpack::States[] =
{
	S_NORMAL (STIM, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AStimpack, Doom, 2011, 23)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

// Medikit ------------------------------------------------------------------

class AMedikit : public AHealth
{
	DECLARE_ACTOR (AMedikit, AHealth)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		PrevHealth = toucher->player->health;
		return P_GiveBody (toucher->player, 25);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings((PrevHealth < 25) ? GOTMEDINEED : GOTMEDIKIT);
	}
	int PrevHealth;
};

FState AMedikit::States[] =
{
	S_NORMAL (MEDI, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AMedikit, Doom, 2012, 24)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS
