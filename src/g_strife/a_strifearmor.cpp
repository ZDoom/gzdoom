#include "a_pickups.h"

// Metal Armor --------------------------------------------------------------

class AMetalArmor : public ABasicArmorPickup
{
	DECLARE_ACTOR (AMetalArmor, ABasicArmorPickup)
public:
	virtual const char *PickupMessage ()
	{
		return "You picked up the Metal Armor.";
	}
};

FState AMetalArmor::States[] =
{
	S_NORMAL (ARM3, 'A', -1, NULL, NULL),
};

IMPLEMENT_ACTOR (AMetalArmor, Strife, 2019, 69)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Inventory_MaxAmount (3)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_INVBAR)
	PROP_StrifeType (129)
	PROP_StrifeTeaserType (125)
	PROP_StrifeTeaserType2 (128)
	PROP_BasicArmorPickup_SaveAmount (200)
	PROP_BasicArmorPickup_SavePercent (FRACUNIT/2)
	PROP_Inventory_Icon ("I_ARM1")
	PROP_Tag ("Metal_Armor")
END_DEFAULTS

// Leather Armor ------------------------------------------------------------

class ALeatherArmor : public ABasicArmorPickup
{
	DECLARE_ACTOR (ALeatherArmor, ABasicArmorPickup)
public:
	virtual const char *PickupMessage ()
	{
		return "You picked up the Leather Armor.";
	}
};

FState ALeatherArmor::States[] =
{
	S_NORMAL (ARM4, 'A', -1, NULL, NULL),
};

IMPLEMENT_ACTOR (ALeatherArmor, Strife, 2018, 68)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Inventory_MaxAmount (5)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_INVBAR)
	PROP_StrifeType (130)
	PROP_StrifeTeaserType (126)
	PROP_StrifeTeaserType2 (129)
	PROP_BasicArmorPickup_SaveAmount (100)
	PROP_BasicArmorPickup_SavePercent (FRACUNIT/3)
	PROP_Inventory_Icon ("I_ARM2")
	PROP_Tag ("Leather_Armor")
END_DEFAULTS
