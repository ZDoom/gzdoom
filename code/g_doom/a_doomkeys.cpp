#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"

// Blue key card ------------------------------------------------------------

class ABlueCard : public AKey
{
	DECLARE_ACTOR (ABlueCard, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_bluecard;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTBLUECARD);
	}
};

FState ABlueCard::States[] =
{
	S_NORMAL (BKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (BKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlueCard, Doom, 5, 85)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS

// Yellow key card ----------------------------------------------------------

class AYellowCard : public AKey
{
	DECLARE_ACTOR (AYellowCard, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_yellowcard;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTYELWCARD);
	}
};

FState AYellowCard::States[] =
{
	S_NORMAL (YKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (YKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AYellowCard, Doom, 6, 87)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS

// Red key card -------------------------------------------------------------

class ARedCard : public AKey
{
	DECLARE_ACTOR (ARedCard, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_redcard;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTREDCARD);
	}
};

FState ARedCard::States[] =
{
	S_NORMAL (RKEY, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (RKEY, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ARedCard, Doom, 13, 86)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS

// Blue skull key -----------------------------------------------------------

class ABlueSkull : public AKey
{
	DECLARE_ACTOR (ABlueSkull, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_blueskull;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTBLUESKUL);
	}
};

FState ABlueSkull::States[] =
{
	S_NORMAL (BSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (BSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlueSkull, Doom, 40, 90)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS

// Yellow skull key ---------------------------------------------------------

class AYellowSkull : public AKey
{
	DECLARE_ACTOR (AYellowSkull, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_yellowskull;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTYELWSKUL);
	}
};

FState AYellowSkull::States[] =
{
	S_NORMAL (YSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (YSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AYellowSkull, Doom, 39, 88)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS

// Red skull key ------------------------------------------------------------

class ARedSkull : public AKey
{
	DECLARE_ACTOR (ARedSkull, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return it_redskull;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTREDSKUL);
	}
};

FState ARedSkull::States[] =
{
	S_NORMAL (RSKU, 'A',   10, NULL 				, &States[1]),
	S_BRIGHT (RSKU, 'B',   10, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ARedSkull, Doom, 38, 89)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)

	PROP_SpawnState (0)
END_DEFAULTS
