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

#include "info.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "sbar.h"
#include "statnums.h"
#include "c_dispatch.h"
#include "gstrings.h"
#include "a_morph.h"
#include "a_specialspot.h"
#include "g_game.h"
#include "doomstat.h"
#include "d_player.h"
#include "serializer.h"
#include "vm.h"
#include "c_functions.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, sv_unlimited_pickup)

IMPLEMENT_CLASS(AInventory, false, true)

IMPLEMENT_POINTERS_START(AInventory)
IMPLEMENT_POINTER(Owner)
IMPLEMENT_POINTERS_END

DEFINE_FIELD_BIT(AInventory, ItemFlags, bPickupGood, IF_PICKUPGOOD)
DEFINE_FIELD_BIT(AInventory, ItemFlags, bCreateCopyMoved, IF_CREATECOPYMOVED)
DEFINE_FIELD_BIT(AInventory, ItemFlags, bInitEffectFailed, IF_INITEFFECTFAILED)
DEFINE_FIELD(AInventory, Owner)
DEFINE_FIELD(AInventory, Amount)
DEFINE_FIELD(AInventory, MaxAmount)
DEFINE_FIELD(AInventory, InterHubAmount)
DEFINE_FIELD(AInventory, RespawnTics)
DEFINE_FIELD(AInventory, Icon)
DEFINE_FIELD(AInventory, AltHUDIcon)
DEFINE_FIELD(AInventory, DropTime)
DEFINE_FIELD(AInventory, SpawnPointClass)
DEFINE_FIELD(AInventory, PickupFlash)
DEFINE_FIELD(AInventory, PickupSound)

//===========================================================================
//
// AInventory :: Tick
//
//===========================================================================

void AInventory::Tick ()
{
	if (Owner == NULL)
	{
		// AActor::Tick is only handling interaction with the world
		// and we don't want that for owned inventory items.
		Super::Tick ();
	}
	else if (tics != -1)	// ... but at least we have to advance the states
	{
		tics--;
				
		// you can cycle through multiple states in a tic
		// [RH] Use <= 0 instead of == 0 so that spawnstates
		// of 0 tics work as expected.
		if (tics <= 0)
		{
			assert (state != NULL);
			if (state == NULL)
			{
				Destroy();
				return;
			}
			if (!SetState (state->GetNextState()))
				return; 		// freed itself
		}
	}
	if (DropTime)
	{
		if (--DropTime == 0)
		{
			flags |= GetDefault()->flags & (MF_SPECIAL|MF_SOLID);
		}
	}
}

//===========================================================================
//
// AInventory :: Serialize
//
//===========================================================================

void AInventory::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);

	auto def = (AInventory*)GetDefault();
	arc("owner", Owner)
		("amount", Amount, def->Amount)
		("maxamount", MaxAmount, def->MaxAmount)
		("interhubamount", InterHubAmount, def->InterHubAmount)
		("respawntics", RespawnTics, def->RespawnTics)
		("itemflags", ItemFlags, def->ItemFlags)
		("icon", Icon, def->Icon)
		("althudicon", AltHUDIcon, def->AltHUDIcon)
		("pickupsound", PickupSound, def->PickupSound)
		("spawnpointclass", SpawnPointClass, def->SpawnPointClass)
		("droptime", DropTime, def->DropTime);
}

//===========================================================================
//
//
//===========================================================================
static int StaticLastMessageTic;
static FString StaticLastMessage;

DEFINE_ACTION_FUNCTION(AInventory, PrintPickupMessage)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(localview);
	PARAM_STRING(str);
	if (str.IsNotEmpty() && localview && (StaticLastMessageTic != gametic || StaticLastMessage.Compare(str)))
	{
		StaticLastMessageTic = gametic;
		StaticLastMessage = str;
		const char *pstr = str.GetChars();

		if (pstr[0] == '$')	pstr = GStrings(pstr + 1);
		if (pstr[0] != 0) Printf(PRINT_LOW, "%s\n", pstr);
		StatusBar->FlashCrosshair();
	}
	return 0;
}

//===========================================================================
//
// AInventory :: DepleteOrDestroy
//
// If the item is depleted, just change its amount to 0, otherwise it's destroyed.
//
//===========================================================================

void AInventory::DepleteOrDestroy ()
{
	IFVIRTUAL(AInventory, DepleteOrDestroy)
	{
		VMValue params[1] = { (DObject*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

//===========================================================================
//
// AInventory :: GetBlend
//
// Returns a color to blend to the player's view as long as they possess this
// item.
//
//===========================================================================

PalEntry AInventory::CallGetBlend()
{
	IFVIRTUAL(AInventory, GetBlend)
	{
		VMValue params[1] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, 1, &ret, 1);
		return retval;
	}
	else return 0;
}

//===========================================================================
//
// AInventory :: PrevInv
//
// Returns the previous item with IF_INVBAR set.
//
//===========================================================================

AInventory *AInventory::PrevInv ()
{
	AInventory *lastgood = NULL;
	AInventory *item = Owner->Inventory;

	while (item != NULL && item != this)
	{
		if (item->ItemFlags & IF_INVBAR)
		{
			lastgood = item;
		}
		item = item->Inventory;
	}
	return lastgood;
}
//===========================================================================
//
// AInventory :: NextInv
//
// Returns the next item with IF_INVBAR set.
//
//===========================================================================

AInventory *AInventory::NextInv ()
{
	AInventory *item = Inventory;

	while (item != NULL && !(item->ItemFlags & IF_INVBAR))
	{
		item = item->Inventory;
	}
	return item;
}

//===========================================================================
//
// AInventory :: DoRespawn
//
//===========================================================================

bool AInventory::DoRespawn ()
{
	if (SpawnPointClass != NULL)
	{
		AActor *spot = NULL;
		DSpotState *state = DSpotState::GetSpotState();

		if (state != NULL) spot = state->GetRandomSpot(SpawnPointClass);
		if (spot != NULL) 
		{
			SetOrigin (spot->Pos(), false);
			SetZ(floorz);
		}
	}
	return true;
}

DEFINE_ACTION_FUNCTION(AInventory, DoRespawn)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_BOOL(self->DoRespawn());
}

//===========================================================================
//
// AInventory :: CallTryPickup
//
//===========================================================================

bool AInventory::CallTryPickup(AActor *toucher, AActor **toucher_return)
{
	static VMFunction *func = nullptr;
	if (func == nullptr) PClass::FindFunction(&func, NAME_Inventory, NAME_CallTryPickup);
	VMValue params[2] = { (DObject*)this, toucher };
	VMReturn ret[2];
	int res;
	AActor *tret;
	ret[0].IntAt(&res);
	ret[1].PointerAt((void**)&tret);
	VMCall(func, params, 2, ret, 2);
	if (toucher_return) *toucher_return = tret;
	return !!res;
}

//===========================================================================
//
// CCMD printinv
//
// Prints the console player's current inventory.
//
//===========================================================================

CCMD (printinv)
{
	int pnum = consoleplayer;

#ifdef _DEBUG
	// Only allow peeking on other players' inventory in debug builds.
	if (argv.argc() > 1)
	{
		pnum = atoi (argv[1]);
		if (pnum < 0 || pnum >= MAXPLAYERS)
		{
			return;
		}
	}
#endif
	C_PrintInv(players[pnum].mo);
}

CCMD (targetinv)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL)
		return;

	C_AimLine(&t, true);

	if (t.linetarget)
	{
		C_PrintInv(t.linetarget);
	}
	else Printf("No target found. Targetinv cannot find actors that have "
				"the NOBLOCKMAP flag or have height/radius of 0.\n");
}

