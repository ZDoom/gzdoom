#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "sbar.h"

IMPLEMENT_POINTY_CLASS (AFourthWeaponPiece)
 DECLARE_POINTER (TempFourthWeapon)
END_POINTERS

BEGIN_STATELESS_DEFAULTS (AFourthWeaponPiece, Hexen, -1, 0)
END_DEFAULTS

void AFourthWeaponPiece::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc.UserWriteClass (FourthWeaponClass);
	}
	else
	{
		arc.UserReadClass (FourthWeaponClass);
	}
	arc << PieceValue << TempFourthWeapon;
}

const char *AFourthWeaponPiece::PickupMessage ()
{
	if (TempFourthWeapon != NULL)
	{
		return TempFourthWeapon->PickupMessage ();
	}
	else
	{
		return PieceMessage ();
	}
}

bool AFourthWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return true;
}

const char *AFourthWeaponPiece::PieceMessage ()
{
	return "A weapon piece! This is your lucky day!";
}

void AFourthWeaponPiece::PlayPickupSound (AActor *toucher)
{
	if (TempFourthWeapon != NULL)
	{
		// Play the build-sound full volume for all players
		S_Sound (toucher, CHAN_ITEM, "WeaponBuild", 1, ATTN_SURROUND);
	}
	else
	{
		S_Sound (toucher, CHAN_PICKUP, "misc/w_pkup", 1, ATTN_NORM);
	}
}

//==========================================================================
//
// TryPickupWeaponPiece
//
//==========================================================================

bool AFourthWeaponPiece::TryPickup (AActor *toucher)
{
	bool shouldStay;
	bool checkAssembled;
	bool gaveWeapon;
	int gaveMana;

	checkAssembled = true;
	gaveWeapon = false;
	shouldStay = PrivateShouldStay ();
	if (!MatchPlayerClass (toucher))
	{ // Wrong class, but try to pick up for mana
		if (shouldStay)
		{ // Can't pick up wrong-class weapons in coop netplay
			return false;
		}
		checkAssembled = false;
		gaveMana = P_GiveAmmo (toucher->player, MANA_1, 20) + P_GiveAmmo (toucher->player, MANA_2, 20);
		if (!gaveMana)
		{ // Didn't need the mana, so don't pick it up
			return false;
		}
	}
	else if (shouldStay)
	{ // Cooperative net-game
		if (toucher->player->pieces & PieceValue)
		{ // Already has the piece
			return false;
		}
		P_GiveAmmo (toucher->player, MANA_1, 20);
		P_GiveAmmo (toucher->player, MANA_2, 20);
	}
	else
	{ // Deathmatch or singleplayer game
		gaveMana = P_GiveAmmo (toucher->player, MANA_1, 20) + P_GiveAmmo (toucher->player, MANA_2, 20);
		if (toucher->player->pieces & PieceValue)
		{ // Already has the piece, check if mana needed
			if (!gaveMana)
			{ // Didn't need the mana, so don't pick it up
				return false;
			}
			checkAssembled = false;
		}
	}

	// Check if fourth weapon assembled
	if (checkAssembled)
	{
		toucher->player->pieces |= PieceValue;
		for (int i = 0; i < 9; i += 3)
		{
			int mask = (WPIECE1|WPIECE2|WPIECE3) << i;

			if (PieceValue & mask)
			{
				if (players[consoleplayer].camera == toucher)
				{
					StatusBar->SetInteger (0, i);
				}
				if ((toucher->player->pieces & mask) == mask)
				{
					gaveWeapon = (FourthWeaponClass != NULL);
				}
				break;
			}
		}
	}

	if (gaveWeapon)
	{
		TempFourthWeapon = static_cast<AInventory *>(Spawn (FourthWeaponClass, x, y, z));
		if (TempFourthWeapon != NULL)
		{
			gaveWeapon = TempFourthWeapon->TryPickup (toucher);
		}
		else
		{
			gaveWeapon = NULL;
		}
	}
	return gaveWeapon || gaveMana || checkAssembled;
}

bool AFourthWeaponPiece::ShouldStay ()
{
	if (TempFourthWeapon != NULL)
	{
		TempFourthWeapon->Destroy ();
		TempFourthWeapon = NULL;
	}
	return PrivateShouldStay ();
}

bool AFourthWeaponPiece::PrivateShouldStay ()
{
	// We want a weapon piece to behave like a weapon, so follow the exact
	// same logic as weapons when deciding whether or not to stay.
	if (((multiplayer &&
		(!deathmatch && !alwaysapplydmflags)) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags & MF_DROPPED))
	{
		return true;
	}
	return false;
}