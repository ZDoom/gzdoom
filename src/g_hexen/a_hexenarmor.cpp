#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"

#define PROP_HexenArmor_Class	PROP_Inventory_Amount

enum
{
	ARMOR_ARMOR,
	ARMOR_SHIELD,
	ARMOR_HELMET,
	ARMOR_AMULET
};

// Mesh Armor (1) -----------------------------------------------------------

class AMeshArmor : public AHexenArmor
{
	DECLARE_ACTOR (AMeshArmor, AArmor)
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_ARMOR1);
	}
};

FState AMeshArmor::States[] =
{
	S_NORMAL (AR_1, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMeshArmor, Hexen, 8005, 68)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_HexenArmor_Class (ARMOR_ARMOR)
	PROP_SpawnState (0)
END_DEFAULTS

// Falcon Shield (2) --------------------------------------------------------

class AFalconShield : public AHexenArmor
{
	DECLARE_ACTOR (AFalconShield, AHexenArmor)
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_ARMOR2);
	}
};

FState AFalconShield::States[] =
{
	S_NORMAL (AR_2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFalconShield, Hexen, 8006, 69)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_HexenArmor_Class (ARMOR_SHIELD)
	PROP_SpawnState (0)
END_DEFAULTS

// Platinum Helm (3) --------------------------------------------------------

class APlatinumHelm : public AHexenArmor
{
	DECLARE_ACTOR (APlatinumHelm, AHexenArmor)
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_ARMOR3);
	}
};

FState APlatinumHelm::States[] =
{
	S_NORMAL (AR_3, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APlatinumHelm, Hexen, 8007, 70)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_HexenArmor_Class (ARMOR_HELMET)
	PROP_SpawnState (0)
END_DEFAULTS

// Amulet of Warding (4) ----------------------------------------------------

class AAmuletOfWarding : public AHexenArmor
{
	DECLARE_ACTOR (AAmuletOfWarding, AHexenArmor)
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_ARMOR4);
	}
};

FState AAmuletOfWarding::States[] =
{
	S_NORMAL (AR_4, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AAmuletOfWarding, Hexen, 8008, 71)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_HexenArmor_Class (ARMOR_AMULET)
	PROP_SpawnState (0)
END_DEFAULTS
