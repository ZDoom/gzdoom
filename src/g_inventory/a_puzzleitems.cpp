/*
** a_puzzleitems.cpp
** Implements Hexen's puzzle items.
**
**---------------------------------------------------------------------------
** Copyright 2002-2016 Randy Heit
** Copyright 2006-2016 Cheistoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "c_console.h"
#include "doomstat.h"
#include "v_font.h"
#include "a_keys.h"


IMPLEMENT_CLASS(PClassPuzzleItem, false, false)
IMPLEMENT_CLASS(APuzzleItem, false, false)

DEFINE_FIELD(APuzzleItem, PuzzleItemNumber)

//===========================================================================
//
//
//
//===========================================================================

void PClassPuzzleItem::DeriveData(PClass *newclass)
{
	Super::DeriveData(newclass);
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassPuzzleItem)));
	static_cast<PClassPuzzleItem *>(newclass)->PuzzFailMessage = PuzzFailMessage;
}


//===========================================================================
//
//
//
//===========================================================================

bool APuzzleItem::HandlePickup (AInventory *item)
{
	// Can't carry more than 1 of each puzzle item in coop netplay
	if (multiplayer && !deathmatch && item->GetClass() == GetClass())
	{
		return true;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
//
//
//===========================================================================

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

//===========================================================================
//
//
//
//===========================================================================

bool APuzzleItem::ShouldStay ()
{
	return !!multiplayer;
}

