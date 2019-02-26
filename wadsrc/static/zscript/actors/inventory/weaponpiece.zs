/*
** a_weaponpieces.cpp
** Implements generic weapon pieces
**
**---------------------------------------------------------------------------
** Copyright 2006-2016 Christoph Oelckers
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

class WeaponHolder : Inventory
{
	int PieceMask;
	Class<Weapon> PieceWeapon;
	
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+INVENTORY.UNDROPPABLE
	}
}

class WeaponPiece : Inventory
{
	Default
	{
		+WEAPONSPAWN;
	}
	
	int PieceValue;
	Class<Weapon> WeaponClass;
	Weapon FullWeapon;
	
	property number: PieceValue;
	property weapon: WeaponClass;
	
	//==========================================================================
	//
	// TryPickupWeaponPiece
	//
	//==========================================================================

	override bool TryPickupRestricted (in out Actor toucher)
	{
		// Wrong class, but try to pick up for ammo
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		let Defaults = GetDefaultByType(WeaponClass);

		bool gaveSome = !!(toucher.GiveAmmo (Defaults.AmmoType1, Defaults.AmmoGive1) +
						   toucher.GiveAmmo (Defaults.AmmoType2, Defaults.AmmoGive2));

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

	override bool TryPickup (in out Actor toucher)
	{
		Inventory item;
		WeaponHolder hold = NULL;
		bool shouldStay = ShouldStay ();
		int gaveAmmo;
		let Defaults = GetDefaultByType(WeaponClass);

		FullWeapon = NULL;
		for(item=toucher.Inv; item; item=item.Inv)
		{
			hold = WeaponHolder(item);
			if (hold != null)
			{
				if (hold.PieceWeapon == WeaponClass) 
				{
					break;
				}
				hold = NULL;
			}
		}
		if (!hold)
		{
			hold = WeaponHolder(Spawn("WeaponHolder"));
			hold.BecomeItem();
			hold.AttachToOwner(toucher);
			hold.PieceMask = 0;
			hold.PieceWeapon = WeaponClass;
		}

		int pieceval = 1 << (PieceValue - 1);
		if (shouldStay)
		{ 
			// Cooperative net-game
			if (hold.PieceMask & pieceval)
			{ 
				// Already has the piece
				return false;
			}
			toucher.GiveAmmo (Defaults.AmmoType1, Defaults.AmmoGive1);
			toucher.GiveAmmo (Defaults.AmmoType2, Defaults.AmmoGive2);
		}
		else
		{ // Deathmatch or singleplayer game
			gaveAmmo = toucher.GiveAmmo (Defaults.AmmoType1, Defaults.AmmoGive1) +
						toucher.GiveAmmo (Defaults.AmmoType2, Defaults.AmmoGive2);
			
			if (hold.PieceMask & pieceval)
			{ 
				// Already has the piece, check if mana needed
				if (!gaveAmmo) return false;
				GoAwayAndDie();
				return true;
			}
		}

		hold.PieceMask |= pieceval;

		// Check if  weapon assembled
		if (hold.PieceMask == (1 << Defaults.health) - 1)
		{
			if (!toucher.FindInventory (WeaponClass))
			{
				FullWeapon= Weapon(Spawn(WeaponClass));
				
				// The weapon itself should not give more ammo to the player.
				FullWeapon.AmmoGive1 = 0;
				FullWeapon.AmmoGive2 = 0;
				FullWeapon.AttachToOwner(toucher);
				FullWeapon.AmmoGive1 = Defaults.AmmoGive1;
				FullWeapon.AmmoGive2 = Defaults.AmmoGive2;
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

	override bool ShouldStay ()
	{
		// We want a weapon piece to behave like a weapon, so follow the exact
		// same logic as weapons when deciding whether or not to stay.
		return (((multiplayer &&
			(!deathmatch && !alwaysapplydmflags)) || sv_weaponstay) && !bDropped);
	}

	//===========================================================================
	//
	// PickupMessage
	//
	// Returns the message to print when this actor is picked up.
	//
	//===========================================================================

	override String PickupMessage ()
	{
		if (FullWeapon) 
		{
			return FullWeapon.PickupMessage();
		}
		else
		{
			return Super.PickupMessage();
		}
	}

	//===========================================================================
	//
	// DoPlayPickupSound
	//
	// Plays a sound when this actor is picked up.
	//
	//===========================================================================

	override void PlayPickupSound (Actor toucher)
	{
		if (FullWeapon)
		{
			FullWeapon.PlayPickupSound(toucher);
		}
		else
		{
			Super.PlayPickupSound(toucher);
		}
	}
}
