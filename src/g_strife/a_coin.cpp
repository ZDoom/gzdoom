#include "a_pickups.h"
#include "a_strifeglobal.h"
#include "gstrings.h"

// Coin ---------------------------------------------------------------------

FState ACoin::States[] =
{
	S_NORMAL (COIN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACoin, Strife, 93, 0)
	PROP_StrifeType (168)
	PROP_StrifeTeaserType (161)
	PROP_StrifeTeaserType2 (165)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL|MF_DROPPED|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Inventory_MaxAmountLong (INT_MAX)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_Icon ("I_COIN")
	PROP_Tag ("coin")
	PROP_Inventory_PickupMessage("$TXT_COIN")
END_DEFAULTS

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
	AInventory *copy = Spawn<ACoin> (0,0,0, NO_REPLACE);
	copy->Amount = Amount;
	copy->BecomeItem ();
	GoAwayAndDie ();
	return copy;
}

// 10 Gold ------------------------------------------------------------------

class AGold10 : public ACoin
{
	DECLARE_ACTOR (AGold10, ACoin)
};

FState AGold10::States[] =
{
	S_NORMAL (CRED, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGold10, Strife, 138, 0)
	PROP_StrifeType (169)
	PROP_StrifeTeaserType (162)
	PROP_StrifeTeaserType2 (166)
	PROP_SpawnState (0)
	PROP_Inventory_Amount (10)
	PROP_Tag ("10_gold")
END_DEFAULTS

// 25 Gold ------------------------------------------------------------------

class AGold25 : public ACoin
{
	DECLARE_ACTOR (AGold25, ACoin)
};

FState AGold25::States[] =
{
	S_NORMAL (SACK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGold25, Strife, 139, 0)
	PROP_StrifeType (170)
	PROP_StrifeTeaserType (163)
	PROP_StrifeTeaserType2 (167)
	PROP_SpawnState (0)
	PROP_Inventory_Amount (25)
	PROP_Tag ("25_gold")
END_DEFAULTS

// 50 Gold ------------------------------------------------------------------

class AGold50 : public ACoin
{
	DECLARE_ACTOR (AGold50, ACoin)
};

FState AGold50::States[] =
{
	S_NORMAL (CHST, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGold50, Strife, 140, 0)
	PROP_StrifeType (171)
	PROP_StrifeTeaserType (164)
	PROP_StrifeTeaserType2 (168)
	PROP_SpawnState (0)
	PROP_Inventory_Amount (50)
	PROP_Tag ("50_gold")
END_DEFAULTS

// 300 Gold ------------------------------------------------------------------

class AGold300 : public ACoin
{
	DECLARE_ACTOR (AGold300, ACoin)
public:
	bool TryPickup (AActor *toucher);
};

FState AGold300::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGold300, Strife, -1, 0)
	PROP_StrifeType (172)
	PROP_SpawnState (0)
	PROP_Inventory_AmountWord (300)
	PROP_Tag ("300_gold")
END_DEFAULTS

bool AGold300::TryPickup (AActor *toucher)
{
	toucher->GiveInventoryType (QuestItemClasses[2]);
	return Super::TryPickup (toucher);
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
		tossed = Spawn<AGold50> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	}
	else if (Amount >= 25)
	{
		Amount -= 25;
		tossed = Spawn<AGold25> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	}
	else if (Amount >= 10)
	{
		Amount -= 10;
		tossed = Spawn<AGold10> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	}
	else if (Amount > 1 || (ItemFlags & IF_KEEPDEPLETED))
	{
		Amount -= 1;
		tossed = Spawn<ACoin> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
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
