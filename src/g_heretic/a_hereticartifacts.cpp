#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

// Invisibility -------------------------------------------------------------

BASIC_ARTI (Invisibility, arti_invisibility, GStrings(TXT_ARTIINVISIBILITY))
	AT_GAME_SET_FRIEND (Invisibility)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		return P_GivePower (player, pw_invisibility);
	}
};

FState AArtiInvisibility::States[] =
{
	S_BRIGHT (INVS, 'A',  350, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiInvisibility, Heretic, 75, 135)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Invisibility)
{
	ArtiDispatch[arti_invisibility] = AArtiInvisibility::ActivateArti;
	ArtiPics[arti_invisibility] = "ARTIINVS";
}

// Tome of power ------------------------------------------------------------

BASIC_ARTI (TomeOfPower, arti_tomeofpower, GStrings(TXT_ARTITOMEOFPOWER))
	AT_GAME_SET_FRIEND (Tome)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		if (player->morphTics)
		{ // Attempt to undo chicken
			if (P_UndoPlayerMorph (player) == false)
			{ // Failed
				P_DamageMobj (player->mo, NULL, NULL, 10000);
			}
			else
			{ // Succeeded
				player->morphTics = 0;
				S_Sound (player->mo, CHAN_VOICE, "*evillaugh", 1, ATTN_IDLE);
			}
		}
		else
		{
			if (!P_GivePower (player, pw_weaponlevel2))
			{
				return false;
			}
			if (wpnlev1info[player->readyweapon]->readystate !=
				wpnlev2info[player->readyweapon]->readystate)
			{
				P_SetPsprite (player, ps_weapon,
					wpnlev2info[player->readyweapon]->readystate);
			}
		}
		return true;
	}
};

FState AArtiTomeOfPower::States[] =
{
	S_NORMAL (PWBK, 'A',  350, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiTomeOfPower, Heretic, 86, 134)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Tome)
{
	ArtiDispatch[arti_tomeofpower] = AArtiTomeOfPower::ActivateArti;
	ArtiPics[arti_tomeofpower] = "ARTIPWBK";
}

// Time bomb ----------------------------------------------------------------

class AActivatedTimeBomb : public AActor
{
	DECLARE_ACTOR (AActivatedTimeBomb, AActor)
public:
	void PreExplode ()
	{
		z += 32*FRACUNIT;
		alpha = OPAQUE;
	}
};

FState AActivatedTimeBomb::States[] =
{
	S_NORMAL (FBMB, 'A',   10, NULL 	, &States[1]),
	S_NORMAL (FBMB, 'B',   10, NULL 	, &States[2]),
	S_NORMAL (FBMB, 'C',   10, NULL 	, &States[3]),
	S_NORMAL (FBMB, 'D',   10, NULL 	, &States[4]),
	S_NORMAL (FBMB, 'E',	6, A_Scream , &States[5]),
	S_BRIGHT (XPL1, 'A',	4, A_Explode, &States[6]),
	S_BRIGHT (XPL1, 'B',	4, NULL 	, &States[7]),
	S_BRIGHT (XPL1, 'C',	4, NULL 	, &States[8]),
	S_BRIGHT (XPL1, 'D',	4, NULL 	, &States[9]),
	S_BRIGHT (XPL1, 'E',	4, NULL 	, &States[10]),
	S_BRIGHT (XPL1, 'F',	4, NULL 	, NULL)
};

IMPLEMENT_ACTOR (AActivatedTimeBomb, Heretic, -1, 72)
	PROP_Flags (MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (0)

	PROP_DeathSound ("misc/timebomb")
END_DEFAULTS

BASIC_ARTI (TimeBomb, arti_firebomb, GStrings(TXT_ARTIFIREBOMB))
	AT_GAME_SET_FRIEND (TimeBomb)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		angle_t angle = player->mo->angle >> ANGLETOFINESHIFT;
		AActor *mo = Spawn<AActivatedTimeBomb> (
			player->mo->x + 24*finecosine[angle],
			player->mo->y + 24*finesine[angle],
			player->mo->z - player->mo->floorclip);
		mo->target = player->mo;
		return true;
	}
};

FState AArtiTimeBomb::States[] =
{
	S_NORMAL (FBMB, 'E',  350, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AArtiTimeBomb, Heretic, 34, 72)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (TimeBomb)
{
	ArtiDispatch[arti_firebomb] = &AArtiTimeBomb::ActivateArti;
	ArtiPics[arti_firebomb] = "ARTIFBMB";
}
