#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"

// Boost Armor Artifact (Dragonskin Bracers) --------------------------------

BASIC_ARTI (BoostArmor, arti_boostarmor, GStrings(TXT_ARTIBOOSTARMOR))
	AT_GAME_SET_FRIEND (BoostArmor)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		int count = 0;

		if (gameinfo.gametype == GAME_Hexen)
		{
			for (armortype_t i = ARMOR_ARMOR; i < NUMARMOR; i = (armortype_t)(i+1))
			{
				count += P_GiveArmor (player, i, 1); // 1 point per armor type
			}
			return count != 0;
		}
		else
		{
			player->armorpoints[0] += 50;
			if (!player->armortype)
				player->armortype = deh.GreenAC;
			return true;
		}
	}
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
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (BoostArmor)
{
	ArtiDispatch[arti_boostarmor] = AArtiBoostArmor::ActivateArti;
	ArtiPics[arti_boostarmor] = "ARTIBRAC";
}
