#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "gi.h"

#define HEAL_RADIUS_DIST	255*FRACUNIT

// Healing Radius Artifact --------------------------------------------------

BASIC_ARTI (HealingRadius, arti_healingradius, GStrings(TXT_ARTIHEALINGRADIUS))
	AT_GAME_SET_FRIEND (HealingRadius)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		bool effective = false;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] &&
				players[i].mo != NULL &&
				players[i].mo->health > 0 &&
				P_AproxDistance (players[i].mo->x - player->mo->x, players[i].mo->y - player->mo->y) <= HEAL_RADIUS_DIST)
			{
				effective |= player->mo->DoHealingRadius (players[i].mo);
			}
		}
		return effective;
	}
};

FState AArtiHealingRadius::States[] =
{
	S_BRIGHT (HRAD, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (HRAD, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (HRAD, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (HRAD, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (HRAD, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (HRAD, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (HRAD, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (HRAD, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (HRAD, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (HRAD, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (HRAD, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (HRAD, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (HRAD, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (HRAD, 'N',	4, NULL					    , &States[14]),
	S_BRIGHT (HRAD, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (HRAD, 'P',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiHealingRadius, Hexen, 10120, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (HealingRadius)
{
	ArtiDispatch[arti_healingradius] = AArtiHealingRadius::ActivateArti;
	ArtiPics[arti_healingradius] = "ARTIHRAD";
}
