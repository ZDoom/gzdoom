#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"

// Boost Armor Artifact (Dragonskin Bracers) --------------------------------

class AArtiBoostArmor : public AInventory
{
	DECLARE_ACTOR (AArtiBoostArmor, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiBoostArmor::States[] =
{
#define S_ARTI_ARMOR1 0
	S_BRIGHT (BRAC, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (BRAC, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (BRAC, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (BRAC, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (BRAC, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (BRAC, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (BRAC, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (BRAC, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiBoostArmor, Hexen, 8041, 22)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIBRAC")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIBOOSTARMOR")
END_DEFAULTS

bool AArtiBoostArmor::Use (bool pickup)
{
	int count = 0;

	if (gameinfo.gametype == GAME_Hexen)
	{
		AHexenArmor *armor;

		for (int i = 0; i < 4; ++i)
		{
			armor = Spawn<AHexenArmor> (0,0,0, NO_REPLACE);
			armor->flags |= MF_DROPPED;
			armor->health = i;
			armor->Amount = 1;
			if (!armor->TryPickup (Owner))
			{
				armor->Destroy ();
			}
			else
			{
				count++;
			}
		}
		return count != 0;
	}
	else
	{
		ABasicArmorBonus *armor = Spawn<ABasicArmorBonus> (0,0,0, NO_REPLACE);
		armor->flags |= MF_DROPPED;
		armor->SaveAmount = 50;
		armor->MaxSaveAmount = 300;
		if (!armor->TryPickup (Owner))
		{
			armor->Destroy ();
			return false;
		}
		else
		{
			return true;
		}
	}
}

