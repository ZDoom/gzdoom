#include "a_pickups.h"
#include "a_hexenglobal.h"
#include "p_local.h"
#include "gstrings.h"
#include "s_sound.h"

// Blue mana ----------------------------------------------------------------

FState AMana1::States[] =
{
	S_BRIGHT (MAN1, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (MAN1, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (MAN1, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (MAN1, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (MAN1, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (MAN1, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (MAN1, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (MAN1, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (MAN1, 'I',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AMana1, Hexen, 122, 11)
	PROP_Inventory_Amount (15)
	PROP_Inventory_MaxAmount (MAX_MANA)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("MAN1I0")
END_DEFAULTS

const char *AMana1::PickupMessage ()
{
	return GStrings(TXT_MANA_1);
}

// Green mana ---------------------------------------------------------------

FState AMana2::States[] =
{
	S_BRIGHT (MAN2, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (MAN2, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (MAN2, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (MAN2, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (MAN2, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (MAN2, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (MAN2, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (MAN2, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (MAN2, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (MAN2, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (MAN2, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (MAN2, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (MAN2, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (MAN2, 'N',	4, NULL					    , &States[14]),
	S_BRIGHT (MAN2, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (MAN2, 'P',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AMana2, Hexen, 124, 12)
	PROP_Inventory_Amount (15)
	PROP_Inventory_MaxAmount (MAX_MANA)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("MAN2G0")
END_DEFAULTS

const char *AMana2::PickupMessage ()
{
	return GStrings(TXT_MANA_2);
}

// Combined mana ------------------------------------------------------------

class AMana3 : public AAmmo
{
	DECLARE_ACTOR (AMana3, AAmmo)
public:
	bool TryPickup (AActor *other)
	{
		bool gave;

		gave = GiveMana (other, RUNTIME_CLASS(AMana1));
		gave |= GiveMana (other, RUNTIME_CLASS(AMana2));
		if (gave)
		{
			GoAwayAndDie ();
		}
		return gave;
	}
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_MANA_BOTH);
	}
	bool GiveMana (AActor *other, const TypeInfo *type)
	{
		AInventory *mana = other->FindInventory (type);
		if (mana == NULL)
		{
			mana = static_cast<AInventory *>(Spawn (type, 0, 0, 0));
			mana->BecomeItem ();
			mana->Amount = Amount;
			other->AddInventory (mana);
			return true;
		}
		else if (mana->Amount < mana->MaxAmount)
		{
			mana->Amount += Amount;
			if (mana->Amount > mana->MaxAmount)
			{
				mana->Amount = mana->MaxAmount;
			}
			return true;
		}
		return false;
	}
};

FState AMana3::States[] =
{
#define S_MANA3 0
	S_BRIGHT (MAN3, 'A',	4, NULL					    , &States[S_MANA3+1]),
	S_BRIGHT (MAN3, 'B',	4, NULL					    , &States[S_MANA3+2]),
	S_BRIGHT (MAN3, 'C',	4, NULL					    , &States[S_MANA3+3]),
	S_BRIGHT (MAN3, 'D',	4, NULL					    , &States[S_MANA3+4]),
	S_BRIGHT (MAN3, 'E',	4, NULL					    , &States[S_MANA3+5]),
	S_BRIGHT (MAN3, 'F',	4, NULL					    , &States[S_MANA3+6]),
	S_BRIGHT (MAN3, 'G',	4, NULL					    , &States[S_MANA3+7]),
	S_BRIGHT (MAN3, 'H',	4, NULL					    , &States[S_MANA3+8]),
	S_BRIGHT (MAN3, 'I',	4, NULL					    , &States[S_MANA3+9]),
	S_BRIGHT (MAN3, 'J',	4, NULL					    , &States[S_MANA3+10]),
	S_BRIGHT (MAN3, 'K',	4, NULL					    , &States[S_MANA3+11]),
	S_BRIGHT (MAN3, 'L',	4, NULL					    , &States[S_MANA3+12]),
	S_BRIGHT (MAN3, 'M',	4, NULL					    , &States[S_MANA3+13]),
	S_BRIGHT (MAN3, 'N',	4, NULL					    , &States[S_MANA3+14]),
	S_BRIGHT (MAN3, 'O',	4, NULL					    , &States[S_MANA3+15]),
	S_BRIGHT (MAN3, 'P',	4, NULL					    , &States[S_MANA3+0]),
};

IMPLEMENT_ACTOR (AMana3, Hexen, 8004, 75)
	PROP_Inventory_Amount (20)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (S_MANA3)
	PROP_Inventory_Icon ("MAN3L0")
END_DEFAULTS

// Boost Mana Artifact (Krater of Might) ------------------------------------

class AArtiBoostMana : public AInventory
{
	DECLARE_ACTOR (AArtiBoostMana, AInventory)
public:
	bool Use ();
	const char *PickupMessage ();
	void PlayPickupSound (AActor *toucher);
protected:
	bool FillMana (const TypeInfo *type);
};

FState AArtiBoostMana::States[] =
{
	S_BRIGHT (BMAN, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AArtiBoostMana, Hexen, 8003, 26)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_PICKUPFLASH)
	PROP_Inventory_Icon ("ARTIBMAN")
END_DEFAULTS

void AArtiBoostMana::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/p_pkup", 1,
		toucher == NULL || toucher == players[consoleplayer].camera
		? ATTN_SURROUND : ATTN_NORM);
}

bool AArtiBoostMana::Use ()
{
	bool success;

	success = FillMana (RUNTIME_CLASS(AMana1));
	success |= FillMana (RUNTIME_CLASS(AMana2));
	return success;
}

bool AArtiBoostMana::FillMana (const TypeInfo *type)
{
	AInventory *mana = Owner->FindInventory (type);
	if (mana == NULL)
	{
		mana = static_cast<AInventory *>(Spawn (type, 0, 0, 0));
		mana->BecomeItem ();
		mana->Amount = mana->MaxAmount;
		Owner->AddInventory (mana);
		return true;
	}
	else if (mana->Amount < mana->MaxAmount)
	{
		mana->Amount = mana->MaxAmount;
		return true;
	}
	return false;
}

const char *AArtiBoostMana::PickupMessage ()
{
	return GStrings(TXT_ARTIBOOSTMANA);
}
