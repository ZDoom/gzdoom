/*
** a_pickups.cpp
** Inventory base class implementation
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Cheistoph Oelckers
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

#include <assert.h>

#include "gstrings.h"
#include "sbar.h"
#include "doomstat.h"
#include "d_player.h"
#include "vm.h"
#include "g_levellocals.h"
#include "gi.h"

EXTERN_CVAR(Bool, sv_unlimited_pickup)


//===========================================================================
//
// This is only native so it can have some static storage for comparison.
//
//===========================================================================
static int StaticLastMessageTic;
static FString StaticLastMessage;

void PrintPickupMessage(bool localview, const FString &str)
{
	// [MK] merge identical messages on same tic unless disabled in gameinfo
	if (str.IsNotEmpty() && localview && (gameinfo.nomergepickupmsg || StaticLastMessageTic != gametic || StaticLastMessage.Compare(str)))
	{
		StaticLastMessageTic = gametic;
		StaticLastMessage = str;
		const char *pstr = str.GetChars();
		
		if (pstr[0] == '$')	pstr = GStrings(pstr + 1);
		if (pstr[0] != 0) Printf(PRINT_LOW, "%s\n", pstr);
		StatusBar->FlashCrosshair();
	}
}

//===========================================================================
//
// AInventory :: DepleteOrDestroy
//
// If the item is depleted, just change its amount to 0, otherwise it's destroyed.
//
//===========================================================================

void DepleteOrDestroy (AActor *item)
{
	IFVIRTUALPTRNAME(item, NAME_Inventory, DepleteOrDestroy)
	{
		VMValue params[1] = { item };
		VMCall(func, params, 1, nullptr, 0);
	}
}

//===========================================================================
//
// AInventory :: CallTryPickup
//
//===========================================================================

bool CallTryPickup(AActor *item, AActor *toucher, AActor **toucher_return)
{
	static VMFunction *func = nullptr;
	if (func == nullptr) PClass::FindFunction(&func, NAME_Inventory, NAME_CallTryPickup);
	VMValue params[2] = { (DObject*)item, toucher };
	VMReturn ret[2];
	int res;
	AActor *tret;
	ret[0].IntAt(&res);
	ret[1].PointerAt((void**)&tret);
	VMCall(func, params, 2, ret, 2);
	if (toucher_return) *toucher_return = tret;
	return !!res;
}

