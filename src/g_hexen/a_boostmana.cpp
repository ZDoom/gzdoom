#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"

// Boost Mana Artifact (Krater of Might) ------------------------------------

BASIC_ARTI (BoostMana, arti_boostmana, GStrings(TXT_ARTIBOOSTMANA))
	AT_GAME_SET_FRIEND (BoostMana)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		if (!P_GiveAmmo (player, MANA_1, MAX_MANA))
		{
			if (!P_GiveAmmo (player, MANA_2, MAX_MANA))
			{
				return false;
			}
		}
		else 
		{
			P_GiveAmmo (player, MANA_2, MAX_MANA);
		}
		return true;
	}
};

FState AArtiBoostMana::States[] =
{
	S_BRIGHT (BMAN, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AArtiBoostMana, Hexen, 8003, 26)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (BoostMana)
{
	ArtiDispatch[arti_boostmana] = AArtiBoostMana::ActivateArti;
	ArtiPics[arti_boostmana] = "ARTIBMAN";
}
