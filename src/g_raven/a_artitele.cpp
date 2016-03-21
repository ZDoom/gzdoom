#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_spec.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"
#include "d_player.h"
#include "a_morph.h"

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
	DVector3 dest;
	int destAngle;

	if (deathmatch)
	{
		unsigned int selections = deathmatchstarts.Size ();
		unsigned int i = pr_tele() % selections;
		dest = deathmatchstarts[i].pos;
		destAngle = deathmatchstarts[i].angle;
	}
	else
	{
		FPlayerStart *start = G_PickPlayerStart(int(Owner->player - players));
		dest = start->pos;
		destAngle = start->angle;
	}
	dest.Z = ONFLOORZ;
	P_Teleport (Owner, dest, (double)destAngle, TELF_SOURCEFOG | TELF_DESTFOG);
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
	AInventory *arti = player->mo->FindInventory(PClass::FindActor("ArtiTeleport"));

	if (arti != NULL)
	{
		player->mo->UseInventory (arti);
		player->health = player->mo->health = (player->health+1)/2;
		return true;
	}
	return false;
}
