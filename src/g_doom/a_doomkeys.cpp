#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "a_keys.h"
#include "gstrings.h"

IMPLEMENT_STATELESS_ACTOR (ADoomKey, Doom, -1, 0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
END_DEFAULTS

// Blue key card ------------------------------------------------------------

class ABlueCard : public ADoomKey
{
	DECLARE_ACTOR (ABlueCard, ADoomKey)
public:
	const char *PickupMessage ();
};

FState ABlueCard::States[] =
{
	S_NORMAL (BKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (BKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlueCard, Doom, 5, 85)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS0")
END_DEFAULTS

const char *ABlueCard::PickupMessage ()
{
	return GStrings("GOTBLUECARD");
}

// Yellow key card ----------------------------------------------------------

class AYellowCard : public ADoomKey
{
	DECLARE_ACTOR (AYellowCard, ADoomKey)
public:
	const char *PickupMessage ();
};

FState AYellowCard::States[] =
{
	S_NORMAL (YKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (YKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AYellowCard, Doom, 6, 87)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS1")
END_DEFAULTS

const char *AYellowCard::PickupMessage ()
{
	return GStrings("GOTYELWCARD");
}

// Red key card -------------------------------------------------------------

class ARedCard : public ADoomKey
{
	DECLARE_ACTOR (ARedCard, ADoomKey)
public:
	const char *PickupMessage ();
};

FState ARedCard::States[] =
{
	S_NORMAL (RKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (RKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ARedCard, Doom, 13, 86)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS2")
END_DEFAULTS

const char *ARedCard::PickupMessage ()
{
	return GStrings("GOTREDCARD");
}

// Blue skull key -----------------------------------------------------------

class ABlueSkull : public ADoomKey
{
	DECLARE_ACTOR (ABlueSkull, ADoomKey)
public:
	const char *PickupMessage ();
};

FState ABlueSkull::States[] =
{
	S_NORMAL (BSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (BSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlueSkull, Doom, 40, 90)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS3")
END_DEFAULTS

const char *ABlueSkull::PickupMessage ()
{
	return GStrings("GOTBLUESKUL");
}

// Yellow skull key ---------------------------------------------------------

class AYellowSkull : public ADoomKey
{
	DECLARE_ACTOR (AYellowSkull, ADoomKey)
public:
	const char *PickupMessage ();
};

FState AYellowSkull::States[] =
{
	S_NORMAL (YSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (YSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AYellowSkull, Doom, 39, 88)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS4")
END_DEFAULTS

const char *AYellowSkull::PickupMessage ()
{
	return GStrings("GOTYELWSKUL");
}

// Red skull key ------------------------------------------------------------

class ARedSkull : public ADoomKey
{
	DECLARE_ACTOR (ARedSkull, ADoomKey)
public:
	const char *PickupMessage ();
};

FState ARedSkull::States[] =
{
	S_NORMAL (RSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (RSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ARedSkull, Doom, 38, 89)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("STKEYS5")
END_DEFAULTS

const char *ARedSkull::PickupMessage ()
{
	return GStrings("GOTREDSKUL");
}

