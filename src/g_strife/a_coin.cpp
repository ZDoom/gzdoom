/*
#include "a_pickups.h"
#include "a_strifeglobal.h"
#include "gstrings.h"
*/

// Coin ---------------------------------------------------------------------

IMPLEMENT_CLASS (ACoin)

const char *ACoin::PickupMessage ()
{
	if (Amount == 1)
	{
		return Super::PickupMessage();
	}
	else
	{
		static char msg[64];

		mysnprintf (msg, countof(msg), GStrings("TXT_XGOLD"), Amount);
		return msg;
	}
}

bool ACoin::HandlePickup (AInventory *item)
{
	if (item->IsKindOf (RUNTIME_CLASS(ACoin)))
	{
		if (Amount < MaxAmount)
		{
			if (MaxAmount - Amount < item->Amount)
			{
				Amount = MaxAmount;
			}
			else
			{
				Amount += item->Amount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

AInventory *ACoin::CreateCopy (AActor *other)
{
	if (GetClass() == RUNTIME_CLASS(ACoin))
	{
		return Super::CreateCopy (other);
	}
	AInventory *copy = Spawn<ACoin> ();
	copy->Amount = Amount;
	copy->BecomeItem ();
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// ACoin :: CreateTossable
//
// Gold drops in increments of 50 if you have that much, less if you don't.
//
//===========================================================================

AInventory *ACoin::CreateTossable ()
{
	ACoin *tossed;

	if ((ItemFlags & IF_UNDROPPABLE) || Owner == NULL || Amount <= 0)
	{
		return NULL;
	}
	if (Amount >= 50)
	{
		Amount -= 50;
		tossed = static_cast<ACoin*>(Spawn("Gold50", Owner->Pos(), NO_REPLACE));
	}
	else if (Amount >= 25)
	{
		Amount -= 25;
		tossed = static_cast<ACoin*>(Spawn("Gold25", Owner->Pos(), NO_REPLACE));
	}
	else if (Amount >= 10)
	{
		Amount -= 10;
		tossed = static_cast<ACoin*>(Spawn("Gold10", Owner->Pos(), NO_REPLACE));
	}
	else if (Amount > 1 || (ItemFlags & IF_KEEPDEPLETED))
	{
		Amount -= 1;
		tossed = static_cast<ACoin*>(Spawn("Coin", Owner->Pos(), NO_REPLACE));
	}
	else // Amount == 1 && !(ItemFlags & IF_KEEPDEPLETED)
	{
		BecomePickup ();
		tossed = this;
	}
	tossed->flags &= ~(MF_SPECIAL|MF_SOLID);
	tossed->DropTime = 30;
	if (tossed != this && Amount <= 0)
	{
		Destroy ();
	}
	return tossed;
}
