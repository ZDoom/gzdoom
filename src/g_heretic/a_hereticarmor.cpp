#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"

// Silver Shield (Shield1) --------------------------------------------------

class ASilverShield : public AArmor
{
	DECLARE_ACTOR (ASilverShield, AArmor)
public:
	bool TryPickup (AActor *toucher)
	{
		return P_GiveArmor (toucher->player, (armortype_t)-1, 100);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_ITEMSHIELD1);
	}
};

FState ASilverShield::States[] =
{
	S_NORMAL (SHLD, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASilverShield, Heretic, 85, 68)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (SilverShield)
{
	if (gameinfo.gametype == GAME_Heretic)
	{
		ArmorPics[0] = "SHLDA0";
	}
}

// Enchanted shield (Shield2) -----------------------------------------------

class AEnchantedShield : public AArmor
{
	DECLARE_ACTOR (AEnchantedShield, AArmor)
public:
	bool TryPickup (AActor *toucher)
	{
		return P_GiveArmor (toucher->player, (armortype_t)-2, 200);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_ITEMSHIELD2);
	}
};

FState AEnchantedShield::States[] =
{
	S_NORMAL (SHD2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AEnchantedShield, Heretic, 31, 69)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (EnchantedShield)
{
	if (gameinfo.gametype == GAME_Heretic)
	{
		ArmorPics[1] = "SHD2A0";
	}
}
