#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"

static FRandom pr_tele ("TeleportSelf");

// Teleport (self) ----------------------------------------------------------

class AArtiTeleport : public AInventory
{
	DECLARE_ACTOR (AArtiTeleport, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiTeleport::States[] =
{
	S_NORMAL (ATLP, 'A',	4, NULL, &States[1]),
	S_NORMAL (ATLP, 'B',	4, NULL, &States[2]),
	S_NORMAL (ATLP, 'C',	4, NULL, &States[3]),
	S_NORMAL (ATLP, 'B',	4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiTeleport, Raven, 36, 18)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIATLP")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTITELEPORT")
END_DEFAULTS

bool AArtiTeleport::Use (bool pickup)
{
	fixed_t destX;
	fixed_t destY;
	angle_t destAngle;

	if (deathmatch)
	{
		unsigned int selections = deathmatchstarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = deathmatchstarts[i].x << FRACBITS;
		destY = deathmatchstarts[i].y << FRACBITS;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
	}
	else
	{
		destX = playerstarts[Owner->player - players].x << FRACBITS;
		destY = playerstarts[Owner->player - players].y << FRACBITS;
		destAngle = ANG45 * (playerstarts[Owner->player - players].angle/45);
	}
	P_Teleport (Owner, destX, destY, ONFLOORZ, destAngle, true, true, false);
	if (gameinfo.gametype == GAME_Hexen && Owner->player->morphTics)
	{ // Teleporting away will undo any morph effects (pig)
		P_UndoPlayerMorph (Owner->player);
	}
	if (gameinfo.gametype == GAME_Heretic)
	{ // Full volume laugh
		S_Sound (Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_NONE);
	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_AutoUseChaosDevice
//
//---------------------------------------------------------------------------

bool P_AutoUseChaosDevice (player_t *player)
{
	AArtiTeleport *arti = player->mo->FindInventory<AArtiTeleport> ();

	if (arti != NULL)
	{
		player->mo->UseInventory (arti);
		player->health = player->mo->health = (player->health+1)/2;
		return true;
	}
	return false;
}
