#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"

// Speed Boots Artifact -----------------------------------------------------

BASIC_ARTI (SpeedBoots, arti_speed, GStrings(TXT_ARTISPEED))
	AT_GAME_SET_FRIEND (SpeedBoots)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GivePower (player, pw_speed);
	}
};

FState AArtiSpeedBoots::States[] =
{
	S_BRIGHT (SPED, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (SPED, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (SPED, 'C',	3, NULL					    , &States[3]),
	S_BRIGHT (SPED, 'D',	3, NULL					    , &States[4]),
	S_BRIGHT (SPED, 'E',	3, NULL					    , &States[5]),
	S_BRIGHT (SPED, 'F',	3, NULL					    , &States[6]),
	S_BRIGHT (SPED, 'G',	3, NULL					    , &States[7]),
	S_BRIGHT (SPED, 'H',	3, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiSpeedBoots, Hexen, 8002, 13)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (SpeedBoots)
{
	ArtiDispatch[arti_speed] = AArtiSpeedBoots::ActivateArti;
	ArtiPics[arti_speed] = "ARTISPED";
}
