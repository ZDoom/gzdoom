/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
*/

// Boost Armor Artifact (Dragonskin Bracers) --------------------------------

class AArtiBoostArmor : public AInventory
{
	DECLARE_CLASS (AArtiBoostArmor, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiBoostArmor)

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
			if (!armor->CallTryPickup (Owner))
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
		if (!armor->CallTryPickup (Owner))
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

