#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"

// Health -------------------------------------------------------------------

class AArtiHealth : public AHealthPickup
{
	DECLARE_ACTOR (AArtiHealth, AHealthPickup)
public:
	const char *PickupMessage ();
};

FState AArtiHealth::States[] =
{
	S_NORMAL (PTN2, 'A',	4, NULL, &States[1]),
	S_NORMAL (PTN2, 'B',	4, NULL, &States[2]),
	S_NORMAL (PTN2, 'C',	4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiHealth, Raven, 82, 24)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnHealth (25)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("ARTIPTN2")
END_DEFAULTS

const char *AArtiHealth::PickupMessage ()
{
	return GStrings(TXT_ARTIHEALTH);
}

// Super health -------------------------------------------------------------

class AArtiSuperHealth : public AHealthPickup
{
	DECLARE_ACTOR (AArtiSuperHealth, AHealthPickup)
public:
	const char *PickupMessage ();
};

FState AArtiSuperHealth::States[] =
{
	S_NORMAL (SPHL, 'A',  350, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiSuperHealth, Raven, 32, 25)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_SpawnHealth (100)
	PROP_Inventory_Icon ("ARTISPHL")
END_DEFAULTS

const char *AArtiSuperHealth::PickupMessage ()
{
	return GStrings(TXT_ARTISUPERHEALTH);
}

// Flight -------------------------------------------------------------------

class AArtiFly : public APowerupGiver
{
	DECLARE_ACTOR (AArtiFly, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AArtiFly::States[] =
{
	S_NORMAL (SOAR, 'A',	5, NULL, &States[1]),
	S_NORMAL (SOAR, 'B',	5, NULL, &States[2]),
	S_NORMAL (SOAR, 'C',	5, NULL, &States[3]),
	S_NORMAL (SOAR, 'B',	5, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiFly, Raven, 83, 15)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_RespawnTics (30+4200)
	PROP_Inventory_Flags (IF_INTERHUBSTRIP)
	PROP_Inventory_Icon ("ARTISOAR")
	PROP_PowerupGiver_Powerup ("PowerFlight")
END_DEFAULTS

const char *AArtiFly::PickupMessage ()
{
	return GStrings(TXT_ARTIFLY);
}

// Invulnerability ----------------------------------------------------------

class AArtiInvulnerability : public APowerupGiver
{
	DECLARE_ACTOR (AArtiInvulnerability, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AArtiInvulnerability::States[] =
{
	S_NORMAL (INVU, 'A',	3, NULL, &States[1]),
	S_NORMAL (INVU, 'B',	3, NULL, &States[2]),
	S_NORMAL (INVU, 'C',	3, NULL, &States[3]),
	S_NORMAL (INVU, 'D',	3, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiInvulnerability, Raven, 84, 133)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_RespawnTics (30+4200)
	PROP_Inventory_Icon ("ARTIINVU")
	PROP_PowerupGiver_Powerup ("PowerInvulnerable")
END_DEFAULTS

const char *AArtiInvulnerability::PickupMessage ()
{
	if (gameinfo.gametype == GAME_Hexen)
	{
		return GStrings (TXT_ARTIINVULNERABILITY2);
	}
	else
	{
		return GStrings (TXT_ARTIINVULNERABILITY);
	}
}

// Torch --------------------------------------------------------------------

class AArtiTorch : public APowerupGiver
{
	DECLARE_ACTOR (AArtiTorch, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AArtiTorch::States[] =
{
	S_BRIGHT (TRCH, 'A',	3, NULL, &States[1]),
	S_BRIGHT (TRCH, 'B',	3, NULL, &States[2]),
	S_BRIGHT (TRCH, 'C',	3, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiTorch, Raven, 33, 73)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("ARTITRCH")
	PROP_PowerupGiver_Powerup ("PowerTorch")
END_DEFAULTS

const char *AArtiTorch::PickupMessage ()
{
	return GStrings(TXT_ARTITORCH);
}
