#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"

// Health -------------------------------------------------------------------

BASIC_ARTI (Health, arti_health, GStrings(TXT_ARTIHEALTH))
	AT_GAME_SET_FRIEND (ArtiHealth)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GiveBody (player, 25);
	}
};

FState AArtiHealth::States[] =
{
	S_NORMAL (PTN2, 'A',	4, NULL, &States[1]),
	S_NORMAL (PTN2, 'B',	4, NULL, &States[2]),
	S_NORMAL (PTN2, 'C',	4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiHealth, Raven, 82, 24)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (ArtiHealth)
{
	ArtiDispatch[arti_health] = AArtiHealth::ActivateArti;
	ArtiPics[arti_health] = "ARTIPTN2";
}

// Super health -------------------------------------------------------------

BASIC_ARTI (SuperHealth, arti_superhealth, GStrings(TXT_ARTISUPERHEALTH))
	AT_GAME_SET_FRIEND (ArtiSuperHealth)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GiveBody (player, player->mo->GetDefault()->health);
	}
};

FState AArtiSuperHealth::States[] =
{
	S_NORMAL (SPHL, 'A',  350, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiSuperHealth, Raven, 32, 25)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (ArtiSuperHealth)
{
	ArtiDispatch[arti_superhealth] = AArtiSuperHealth::ActivateArti;
	ArtiPics[arti_superhealth] = "ARTISPHL";
}

// Flight -------------------------------------------------------------------

BASIC_ARTI (Fly, arti_fly, GStrings(TXT_ARTIFLY))
	AT_GAME_SET_FRIEND (ArtiFly)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		if (P_GivePower (player, pw_flight))
		{
			if (player->mo->momz <= -35*FRACUNIT)
			{ // stop falling scream
				S_StopSound (player->mo, CHAN_VOICE);
			}
			return true;
		}
		return false;
	}
};

FState AArtiFly::States[] =
{
	S_NORMAL (SOAR, 'A',	5, NULL, &States[1]),
	S_NORMAL (SOAR, 'B',	5, NULL, &States[2]),
	S_NORMAL (SOAR, 'C',	5, NULL, &States[3]),
	S_NORMAL (SOAR, 'B',	5, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiFly, Raven, 83, 15)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (ArtiFly)
{
	ArtiDispatch[arti_fly] = AArtiFly::ActivateArti;
	ArtiPics[arti_fly] = "ARTISOAR";
}

// Invulnerability ----------------------------------------------------------

BASIC_ARTI (Invulnerability, arti_invulnerability, GStrings(TXT_ARTIINVULNERABILITY))
	AT_GAME_SET_FRIEND (ArtiInvul)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GivePower (player, pw_invulnerability);
	}
};

FState AArtiInvulnerability::States[] =
{
	S_NORMAL (INVU, 'A',	3, NULL, &States[1]),
	S_NORMAL (INVU, 'B',	3, NULL, &States[2]),
	S_NORMAL (INVU, 'C',	3, NULL, &States[3]),
	S_NORMAL (INVU, 'D',	3, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiInvulnerability, Raven, 84, 133)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (ArtiInvul)
{
	ArtiDispatch[arti_invulnerability] = AArtiInvulnerability::ActivateArti;
	ArtiPics[arti_invulnerability] = "ARTIINVU";
}

// Torch --------------------------------------------------------------------

BASIC_ARTI (Torch, arti_torch, GStrings(TXT_ARTITORCH))
	AT_GAME_SET_FRIEND (ArtiTorch)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GivePower (player, pw_infrared);
	}
};

FState AArtiTorch::States[] =
{
	S_BRIGHT (TRCH, 'A',	3, NULL, &States[1]),
	S_BRIGHT (TRCH, 'B',	3, NULL, &States[2]),
	S_BRIGHT (TRCH, 'C',	3, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiTorch, Raven, 33, 73)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (ArtiTorch)
{
	ArtiDispatch[arti_torch] = AArtiTorch::ActivateArti;
	ArtiPics[arti_torch] = "ARTITRCH";
}
