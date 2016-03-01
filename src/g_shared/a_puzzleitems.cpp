#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "c_console.h"
#include "doomstat.h"
#include "v_font.h"
#include "farchive.h"

IMPLEMENT_CLASS(PClassPuzzleItem)

void PClassPuzzleItem::DeriveData(PClass *newclass)
{
	Super::DeriveData(newclass);
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassPuzzleItem)));
	static_cast<PClassPuzzleItem *>(newclass)->PuzzFailMessage = PuzzFailMessage;
}

IMPLEMENT_CLASS(APuzzleItem)

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
		FString message = GetClass()->PuzzFailMessage;
		if (message.IsNotEmpty() && message[0] == '$') message = GStrings[&message[1]];
		if (message.IsEmpty()) message = GStrings("TXT_USEPUZZLEFAILED");
		C_MidPrintBold (SmallFont, message);
	}
	return false;
}

bool APuzzleItem::ShouldStay ()
{
	return !!multiplayer;
}

