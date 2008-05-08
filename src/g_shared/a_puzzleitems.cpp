#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "c_console.h"

IMPLEMENT_STATELESS_ACTOR (APuzzleItem, Any, -1, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_DefMaxAmount
	PROP_UseSound ("PuzzleSuccess")
	PROP_Inventory_PickupSound ("misc/i_pkup")
END_DEFAULTS

void APuzzleItem::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PuzzleItemNumber;
}

bool APuzzleItem::HandlePickup (AInventory *item)
{
	// Can't carry more than 1 of each puzzle item in coop netplay
	if (multiplayer && !deathmatch && item->GetClass() == GetClass())
	{
		return true;
	}
	return Super::HandlePickup (item);
}

bool APuzzleItem::Use (bool pickup)
{
	if (P_UsePuzzleItem (Owner, PuzzleItemNumber))
	{
		return true;
	}
	// [RH] Always play the sound if the use fails.
	S_Sound (Owner, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	if (Owner != NULL && Owner->CheckLocalView (consoleplayer))
	{
		const char *message = GetClass()->Meta.GetMetaString (AIMETA_PuzzFailMessage);
		if (message != NULL && *message=='$') message = GStrings[message + 1];
		if (message == NULL) message = GStrings("TXT_USEPUZZLEFAILED");
		C_MidPrintBold (message);
	}
	return false;
}

bool APuzzleItem::ShouldStay ()
{
	return !!multiplayer;
}

