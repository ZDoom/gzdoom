#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "gstrings.h"
#include "gi.h"

// Armor bonus --------------------------------------------------------------

class AArmorBonus : public AArmor
{
	DECLARE_ACTOR (AArmorBonus, AArmor)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		player_t *player = toucher->player;
		if (player->armorpoints[0] < deh.MaxArmor)
			player->armorpoints[0]++;		// can go over 100%
		if (!player->armortype)
			player->armortype = deh.GreenAC;
		return true;
	}
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

	PROP_SpawnState (0)
END_DEFAULTS

// Green armor --------------------------------------------------------------

class AGreenArmor : public AArmor
{
	DECLARE_ACTOR (AGreenArmor, AArmor)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveArmor (toucher->player, (armortype_t)-deh.GreenAC, 100*deh.GreenAC);
	}
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

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (GreenArmor)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		ArmorPics[0] = "ARM1A0";
	}
}

// Blue armor ---------------------------------------------------------------

class ABlueArmor : public AArmor
{
	DECLARE_ACTOR (ABlueArmor, AArmor)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveArmor (toucher->player, (armortype_t)-deh.BlueAC, 100*deh.BlueAC);
	}
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

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (BlueArmor)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		ArmorPics[1] = "ARM2A0";
	}
}
