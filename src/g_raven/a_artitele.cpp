#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"

static FRandom pr_tele ("TeleportSelf");

// Teleport (self) ----------------------------------------------------------

class AArtiTeleport : public AInventory
{
	DECLARE_CLASS (AArtiTeleport, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiTeleport)

bool AArtiTeleport::Use (bool pickup)
{
	fixed_t destX;
	fixed_t destY;
	angle_t destAngle;

	if (deathmatch)
	{
		unsigned int selections = deathmatchstarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = deathmatchstarts[i].x;
		destY = deathmatchstarts[i].y;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
	}
	else
	{
		FPlayerStart *start = G_PickPlayerStart(int(Owner->player - players));
		destX = start->x;
		destY = start->y;
		destAngle = ANG45 * (start->angle/45);
	}
	P_Teleport (Owner, destX, destY, ONFLOORZ, destAngle, TELF_SOURCEFOG | TELF_DESTFOG);
	bool canlaugh = true;
 	if (Owner->player->morphTics && (Owner->player->MorphStyle & MORPH_UNDOBYCHAOSDEVICE))
 	{ // Teleporting away will undo any morph effects (pig)
		if (!P_UndoPlayerMorph (Owner->player, Owner->player, MORPH_UNDOBYCHAOSDEVICE)
			&& (Owner->player->MorphStyle & MORPH_FAILNOLAUGH))
		{
			canlaugh = false;
		}
 	}
	if (canlaugh)
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
	AInventory *arti = player->mo->FindInventory(PClass::FindClass("ArtiTeleport"));

	if (arti != NULL)
	{
		player->mo->UseInventory (arti);
		player->health = player->mo->health = (player->health+1)/2;
		return true;
	}
	return false;
}
