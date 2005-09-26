#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"

// Silver Shield (Shield1) --------------------------------------------------

class ASilverShield : public ABasicArmorPickup
{
	DECLARE_ACTOR (ASilverShield, ABasicArmorPickup)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("TXT_ITEMSHIELD1");
	}
};

FState ASilverShield::States[] =
{
	S_NORMAL (SHLD, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASilverShield, Heretic, 85, 68)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_BasicArmorPickup_SavePercent (FRACUNIT/2)
	PROP_BasicArmorPickup_SaveAmount (100)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("SHLDA0")
END_DEFAULTS

// Enchanted shield (Shield2) -----------------------------------------------

class AEnchantedShield : public ABasicArmorPickup
{
	DECLARE_ACTOR (AEnchantedShield, ABasicArmorPickup)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("TXT_ITEMSHIELD2");
	}
};

FState AEnchantedShield::States[] =
{
	S_NORMAL (SHD2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AEnchantedShield, Heretic, 31, 69)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_BasicArmorPickup_SavePercent (FRACUNIT*3/4)
	PROP_BasicArmorPickup_SaveAmount (200)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("SHD2A0")
END_DEFAULTS
