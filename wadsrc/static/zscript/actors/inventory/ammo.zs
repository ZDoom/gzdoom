/*
** a_ammo.cpp
** Implements ammo and backpack items.
**
**---------------------------------------------------------------------------
** Copyright 2000-2016 Randy Heit
** Copyright 2006-2017 Christoph Oelckers
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

class Ammo : Inventory
{
	int BackpackAmount;
	int BackpackMaxAmount;
	meta int DropAmount;
	
	property BackpackAmount: BackpackAmount;
	property BackpackMaxAmount: BackpackMaxAmount;
	property DropAmount: DropAmount;

	Default
	{
		+INVENTORY.KEEPDEPLETED
		Inventory.PickupSound "misc/ammo_pkup";
	}

	//===========================================================================
	//
	// AAmmo :: GetParentAmmo
	//
	// Returns the least-derived ammo type that this ammo is a descendant of.
	// That is, if this ammo is an immediate subclass of Ammo, then this ammo's
	// type is returned. If this ammo's superclass is not Ammo, then this
	// function travels up the inheritance chain until it finds a type that is
	// an immediate subclass of Ammo and returns that.
	//
	// The intent of this is that all unique ammo types will be immediate
	// subclasses of Ammo. To make different pickups with different ammo amounts,
	// you subclass the type of ammo you want a different amount for and edit
	// that.
	//
	//===========================================================================

	virtual Class<Ammo> GetParentAmmo ()
	{
		class<Object> type = GetClass();

		while (type.GetParentClass() != "Ammo" && type.GetParentClass() != NULL)
		{
			type = type.GetParentClass();
		}
		return (class<Ammo>)(type);
	}

	//===========================================================================
	//
	// AAmmo :: HandlePickup
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		let ammoitem = Ammo(item);
		if (ammoitem != null && ammoitem.GetParentAmmo() == GetClass())
		{
			if (Amount < MaxAmount || sv_unlimited_pickup)
			{
				int receiving = item.Amount;

				if (!item.bIgnoreSkill)
				{ // extra ammo in baby mode and nightmare mode
					receiving = int(receiving * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
				}
				int oldamount = Amount;

				if (Amount > 0 && Amount + receiving < 0)
				{
					Amount = 0x7fffffff;
				}
				else
				{
					Amount += receiving;
				}
				if (Amount > MaxAmount && !sv_unlimited_pickup)
				{
					Amount = MaxAmount;
				}
				item.bPickupGood = true;

				// If the player previously had this ammo but ran out, possibly switch
				// to a weapon that uses it, but only if the player doesn't already
				// have a weapon pending.

				if (oldamount == 0 && Owner != null && Owner.player != null)
				{
					PlayerPawn(Owner).CheckWeaponSwitch(GetClass());
				}
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// AAmmo :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		Inventory copy;
		int amount = Amount;

		// extra ammo in baby mode and nightmare mode
		if (!bIgnoreSkill)
		{
			amount = int(amount * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
		}

		let type = GetParentAmmo();
		if (GetClass() != type && type != null)
		{
			if (!GoAway ())
			{
				Destroy ();
			}

			copy = Inventory(Spawn (type));
			copy.Amount = amount;
			copy.BecomeItem ();
		}
		else
		{
			copy = Super.CreateCopy (other);
			copy.Amount = amount;
		}
		if (copy.Amount > copy.MaxAmount)
		{ // Don't pick up more ammo than you're supposed to be able to carry.
			copy.Amount = copy.MaxAmount;
		}
		return copy;
	}

	//===========================================================================
	//
	// AAmmo :: CreateTossable
	//
	//===========================================================================

	override Inventory CreateTossable(int amt)
	{
		Inventory copy = Super.CreateTossable(amt);
		if (copy != null)
		{ // Do not increase ammo by dropping it and picking it back up at
		  // certain skill levels.
			copy.bIgnoreSkill = true;
		}
		return copy;
	}

	//---------------------------------------------------------------------------
	//
	// Modifies the drop amount of this item according to the current skill's
	// settings (also called by ADehackedPickup::TryPickup)
	//
	//---------------------------------------------------------------------------
	
	override void ModifyDropAmount(int dropamount)
	{
		bool ignoreskill = true;
		double dropammofactor = G_SkillPropertyFloat(SKILLP_DropAmmoFactor);
		// Default drop amount is half of regular amount * regular ammo multiplication
		if (dropammofactor == -1) 
		{
			dropammofactor = 0.5;
			ignoreskill = false;
		}

		if (dropamount > 0)
		{
			if (ignoreskill)
			{
				self.Amount = int(dropamount * dropammofactor);
				bIgnoreSkill = true;
			}
			else
			{
				self.Amount = dropamount;
			}
		}
		else
		{
			// Half ammo when dropped by bad guys.
			int amount = self.DropAmount;
			if (amount <= 0)
			{
				amount = MAX(1, int(self.Amount * dropammofactor));
			}
			self.Amount = amount;
			bIgnoreSkill = ignoreskill;
		}
	}
	
}

class BackpackItem : Inventory
{
	bool bDepleted;
	
	//===========================================================================
	//
	// ABackpackItem :: CreateCopy
	//
	// A backpack is being added to a player who doesn't yet have one. Give them
	// every kind of ammo, and increase their max amounts.
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		// Find every unique type of ammoitem. Give it to the player if
		// he doesn't have it already, and double its maximum capacity.
		uint end = AllActorClasses.Size();
		for (uint i = 0; i < end; ++i)
		{
			let ammotype = (class<Ammo>)(AllActorClasses[i]);

			if (ammotype && GetDefaultByType(ammotype).GetParentAmmo() == ammotype)
			{
				let ammoitem = Ammo(other.FindInventory(ammotype));
				int amount = GetDefaultByType(ammotype).BackpackAmount;
				// extra ammo in baby mode and nightmare mode
				if (!bIgnoreSkill)
				{
					amount = int(amount * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
				}
				if (amount < 0) amount = 0;
				if (ammoitem == NULL)
				{ // The player did not have the ammoitem. Add it.
					ammoitem = Ammo(Spawn(ammotype));
					ammoitem.Amount = bDepleted ? 0 : amount;
					if (ammoitem.BackpackMaxAmount > ammoitem.MaxAmount)
					{
						ammoitem.MaxAmount = ammoitem.BackpackMaxAmount;
					}
					if (ammoitem.Amount > ammoitem.MaxAmount)
					{
						ammoitem.Amount = ammoitem.MaxAmount;
					}
					ammoitem.AttachToOwner (other);
				}
				else
				{ // The player had the ammoitem. Give some more.
					if (ammoitem.MaxAmount < ammoitem.BackpackMaxAmount)
					{
						ammoitem.MaxAmount = ammoitem.BackpackMaxAmount;
					}
					if (!bDepleted && ammoitem.Amount < ammoitem.MaxAmount)
					{
						ammoitem.Amount += amount;
						if (ammoitem.Amount > ammoitem.MaxAmount)
						{
							ammoitem.Amount = ammoitem.MaxAmount;
						}
					}
				}
			}
		}
		return Super.CreateCopy (other);
	}

	//===========================================================================
	//
	// ABackpackItem :: HandlePickup
	//
	// When the player picks up another backpack, just give them more ammoitem.
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		// Since you already have a backpack, that means you already have every
		// kind of ammo in your inventory, so we don't need to look at the
		// entire PClass list to discover what kinds of ammo exist, and we don't
		// have to alter the MaxAmount either.
		if (item is 'BackpackItem')
		{
			for (let probe = Owner.Inv; probe != NULL; probe = probe.Inv)
			{
				let ammoitem = Ammo(probe);

				if (ammoitem && ammoitem.GetParentAmmo() == ammoitem.GetClass())
				{
					if (ammoitem.Amount < ammoitem.MaxAmount || sv_unlimited_pickup)
					{
						int amount = ammoitem.Default.BackpackAmount;
						// extra ammo in baby mode and nightmare mode
						if (!bIgnoreSkill)
						{
							amount = int(amount * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
						}
						ammoitem.Amount += amount;
						if (ammoitem.Amount > ammoitem.MaxAmount && !sv_unlimited_pickup)
						{
							ammoitem.Amount = ammoitem.MaxAmount;
						}
					}
				}
			}
			// The pickup always succeeds, even if you didn't get anything
			item.bPickupGood = true;
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// ABackpackItem :: CreateTossable
	//
	// The tossed backpack must not give out any more ammo, otherwise a player
	// could cheat by dropping their backpack and picking it up for more ammoitem.
	//
	//===========================================================================

	override Inventory CreateTossable (int amount)
	{
		let pack = BackpackItem(Super.CreateTossable(-1));
		if (pack != NULL)
		{
			pack.bDepleted = true;
		}
		return pack;
	}

	//===========================================================================
	//
	// ABackpackItem :: DetachFromOwner
	//
	//===========================================================================

	override void DetachFromOwner ()
	{
		// When removing a backpack, drop the player's ammo maximums to normal

		for (let item = Owner.Inv; item != NULL; item = item.Inv)
		{
			if (item is 'Ammo' && item.MaxAmount == Ammo(item).BackpackMaxAmount)
			{
				item.MaxAmount = item.Default.MaxAmount;
				if (item.Amount > item.MaxAmount)
				{
					item.Amount = item.MaxAmount;
				}
			}
		}
	}
}

