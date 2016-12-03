/*
** a_weaponpieces.cpp
** Implements generic weapon pieces
**
**---------------------------------------------------------------------------
** Copyright 2006-2016 Cheistoph Oelckers
** Copyright 2006-2016 Randy Heit
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

#include "a_pickups.h"
#include "a_weaponpiece.h"
#include "doomstat.h"
#include "serializer.h"

IMPLEMENT_CLASS(AWeaponHolder, false, false)

DEFINE_FIELD(AWeaponHolder, PieceMask);
DEFINE_FIELD(AWeaponHolder, PieceWeapon);

//===========================================================================
//
//
//
//===========================================================================

void AWeaponHolder::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("piecemask", PieceMask)
		("pieceweapon", PieceWeapon);
}

IMPLEMENT_CLASS(AWeaponPiece, false, true)

IMPLEMENT_POINTERS_START(AWeaponPiece)
	IMPLEMENT_POINTER(FullWeapon)
	IMPLEMENT_POINTER(WeaponClass)
IMPLEMENT_POINTERS_END

//===========================================================================
//
//
//
//===========================================================================

void AWeaponPiece::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AWeaponPiece*)GetDefault();
	arc("weaponclass", WeaponClass, def->WeaponClass)
		("fullweapon", FullWeapon)
		("piecevalue", PieceValue, def->PieceValue);
}

//==========================================================================
//
// TryPickupWeaponPiece
//
//==========================================================================

bool AWeaponPiece::TryPickupRestricted (AActor *&toucher)
{
	// Wrong class, but try to pick up for ammo
	if (CallShouldStay())
	{ // Can't pick up weapons for other classes in coop netplay
		return false;
	}

	AWeapon * Defaults=(AWeapon*)GetDefaultByType(WeaponClass);

	bool gaveSome = !!(toucher->GiveAmmo (Defaults->AmmoType1, Defaults->AmmoGive1) +
					   toucher->GiveAmmo (Defaults->AmmoType2, Defaults->AmmoGive2));

	if (gaveSome)
	{
		GoAwayAndDie ();
	}
	return gaveSome;
}


//==========================================================================
//
// TryPickupWeaponPiece
//
//==========================================================================

bool AWeaponPiece::TryPickup (AActor *&toucher)
{
	AInventory * inv;
	AWeaponHolder * hold=NULL;
	bool shouldStay = PrivateShouldStay ();
	int gaveAmmo;
	AWeapon * Defaults=(AWeapon*)GetDefaultByType(WeaponClass);

	FullWeapon=NULL;
	for(inv=toucher->Inventory;inv;inv=inv->Inventory)
	{
		if (inv->IsKindOf(RUNTIME_CLASS(AWeaponHolder)))
		{
			hold=static_cast<AWeaponHolder*>(inv);

			if (hold->PieceWeapon==WeaponClass) break;
			hold=NULL;
		}
	}
	if (!hold)
	{
		hold=static_cast<AWeaponHolder*>(Spawn(RUNTIME_CLASS(AWeaponHolder)));
		hold->BecomeItem();
		hold->AttachToOwner(toucher);
		hold->PieceMask=0;
		hold->PieceWeapon=WeaponClass;
	}


	if (shouldStay)
	{ 
		// Cooperative net-game
		if (hold->PieceMask & PieceValue)
		{ 
			// Already has the piece
			return false;
		}
		toucher->GiveAmmo (Defaults->AmmoType1, Defaults->AmmoGive1);
		toucher->GiveAmmo (Defaults->AmmoType2, Defaults->AmmoGive2);
	}
	else
	{ // Deathmatch or singleplayer game
		gaveAmmo = toucher->GiveAmmo (Defaults->AmmoType1, Defaults->AmmoGive1) +
					toucher->GiveAmmo (Defaults->AmmoType2, Defaults->AmmoGive2);
		
		if (hold->PieceMask & PieceValue)
		{ 
			// Already has the piece, check if mana needed
			if (!gaveAmmo) return false;
			GoAwayAndDie();
			return true;
		}
	}

	hold->PieceMask |= PieceValue;

	// Check if  weapon assembled
	if (hold->PieceMask== (1<<Defaults->health)-1)
	{
		if (!toucher->FindInventory (WeaponClass))
		{
			FullWeapon= static_cast<AWeapon*>(Spawn(WeaponClass));
			
			// The weapon itself should not give more ammo to the player!
			FullWeapon->AmmoGive1=0;
			FullWeapon->AmmoGive2=0;
			FullWeapon->AttachToOwner(toucher);
			FullWeapon->AmmoGive1=Defaults->AmmoGive1;
			FullWeapon->AmmoGive2=Defaults->AmmoGive2;
		}
	}
	GoAwayAndDie();
	return true;
}

//===========================================================================
//
//
//
//===========================================================================

bool AWeaponPiece::ShouldStay ()
{
	return PrivateShouldStay ();
}

//===========================================================================
//
//
//
//===========================================================================

bool AWeaponPiece::PrivateShouldStay ()
{
	// We want a weapon piece to behave like a weapon, so follow the exact
	// same logic as weapons when deciding whether or not to stay.
	if (((multiplayer &&
		(!deathmatch && !alwaysapplydmflags)) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags&MF_DROPPED))
	{
		return true;
	}
	return false;
}

//===========================================================================
//
// PickupMessage
//
// Returns the message to print when this actor is picked up.
//
//===========================================================================

FString AWeaponPiece::PickupMessage ()
{
	if (FullWeapon) 
	{
		return FullWeapon->PickupMessage();
	}
	else
	{
		return Super::PickupMessage();
	}
}

//===========================================================================
//
// DoPlayPickupSound
//
// Plays a sound when this actor is picked up.
//
//===========================================================================

void AWeaponPiece::PlayPickupSound (AActor *toucher)
{
	if (FullWeapon)
	{
		FullWeapon->PlayPickupSound(toucher);
	}
	else
	{
		Super::PlayPickupSound(toucher);
	}
}

