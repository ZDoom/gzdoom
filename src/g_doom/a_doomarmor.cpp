#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "gstrings.h"
#include "gi.h"

// Armor bonus --------------------------------------------------------------

class AArmorBonus : public ABasicArmorBonus
{
	DECLARE_ACTOR (AArmorBonus, ABasicArmorBonus)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTARMBONUS);
	}
};

FState AArmorBonus::States[] =
{
	S_NORMAL (BON2, 'A',	6, NULL 				, &States[1]),
	S_NORMAL (BON2, 'B',	6, NULL 				, &States[2]),
	S_NORMAL (BON2, 'C',	6, NULL 				, &States[3]),
	S_NORMAL (BON2, 'D',	6, NULL 				, &States[4]),
	S_NORMAL (BON2, 'C',	6, NULL 				, &States[5]),
	S_NORMAL (BON2, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AArmorBonus, Doom, 2015, 22)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)

	PROP_BasicArmorBonus_SavePercent (FRACUNIT/3)
	PROP_BasicArmorBonus_SaveAmount (1)
	PROP_BasicArmorBonus_MaxSaveAmount (200)	// deh.MaxArmor
	PROP_Inventory_FlagsSet (IF_ALWAYSPICKUP)

	PROP_SpawnState (0)
	PROP_Inventory_Icon ("ARM1A0")
END_DEFAULTS

// Green armor --------------------------------------------------------------

class AGreenArmor : public ABasicArmorPickup
{
	DECLARE_ACTOR (AGreenArmor, ABasicArmorPickup)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTARMOR);
	}
};

FState AGreenArmor::States[] =
{
	S_NORMAL (ARM1, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (ARM1, 'B',	7, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AGreenArmor, Doom, 2018, 68)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_BasicArmorPickup_SavePercent (FRACUNIT/3)
	PROP_BasicArmorPickup_SaveAmount (100)		// 100*deh.GreenAC

	PROP_SpawnState (0)
	PROP_Inventory_Icon ("ARM1A0")
END_DEFAULTS

// Blue armor ---------------------------------------------------------------

class ABlueArmor : public ABasicArmorPickup
{
	DECLARE_ACTOR (ABlueArmor, ABasicArmorPickup)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTMEGA);
	}
};

FState ABlueArmor::States[] =
{
	S_NORMAL (ARM2, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (ARM2, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlueArmor, Doom, 2019, 69)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_BasicArmorPickup_SavePercent (FRACUNIT/2)
	PROP_BasicArmorPickup_SaveAmount (200)		// 100*deh.BlueAC

	PROP_SpawnState (0)
	PROP_Inventory_Icon ("ARM2A0")
END_DEFAULTS
