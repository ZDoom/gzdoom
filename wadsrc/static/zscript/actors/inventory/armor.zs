/*
** armor.txt
** Implements all variations of armor objects
**
**---------------------------------------------------------------------------
** Copyright 2002-2016 Randy Heit
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

class Armor : Inventory
{
	Default
	{
		Inventory.PickupSound "misc/armor_pkup";
		+INVENTORY.ISARMOR
	}
}

//===========================================================================
//
//
// BasicArmor
//
// Basic armor absorbs a specific percent of the damage. You should
// never pickup a BasicArmor. Instead, you pickup a BasicArmorPickup
// or BasicArmorBonus and those gives you BasicArmor when it activates.
//
//
//===========================================================================

class BasicArmor : Armor
{
	
	int AbsorbCount;
	double SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int BonusCount;
	Name ArmorType;
	int ActualSaveAmount;
	
	Default
	{
		Inventory.Amount 0;
		+Inventory.KEEPDEPLETED
	}

	//===========================================================================
	//
	// ABasicArmor :: Tick
	//
	// If BasicArmor is given to the player by means other than a
	// BasicArmorPickup, then it may not have an icon set. Fix that here.
	//
	//===========================================================================

	override void Tick ()
	{
		Super.Tick ();
		AbsorbCount = 0;
		if (!Icon.isValid())
		{
			String icontex = gameinfo.ArmorIcon1;

			if (SavePercent >= gameinfo.Armor2Percent && gameinfo.ArmorIcon2.Length() != 0)
				icontex = gameinfo.ArmorIcon2;

			if (icontex.Length() != 0)
				Icon = TexMan.CheckForTexture (icontex, TexMan.TYPE_Any);
		}
	}

	//===========================================================================
	//
	// ABasicArmor :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		// BasicArmor that is in use is stored in the inventory as BasicArmor.
		// BasicArmor that is in reserve is not.
		let copy = BasicArmor(Spawn(GetBasicArmorClass()));
		copy.SavePercent = SavePercent != 0 ? SavePercent : 0.33335;	// slightly more than 1/3 to avoid roundoff errors.
		copy.Amount = Amount;
		copy.MaxAmount = MaxAmount;
		copy.Icon = Icon;
		copy.BonusCount = BonusCount;
		copy.ArmorType = ArmorType;
		copy.ActualSaveAmount = ActualSaveAmount;
		GoAwayAndDie ();
		return copy;
	}


	//===========================================================================
	//
	// ABasicArmor :: HandlePickup
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item is "BasicArmor")
		{
			// You shouldn't be picking up BasicArmor anyway.
			return true;
		}
		return false;
	}
	
	//===========================================================================
	//
	// ABasicArmor :: AbsorbDamage
	//
	//===========================================================================

	override void AbsorbDamage (int damage, Name damageType, out int newdamage, Actor inflictor, Actor source, int flags)
	{
		int saved;

		if (!DamageTypeDefinition.IgnoreArmor(damageType))
		{
			int full = MAX(0, MaxFullAbsorb - AbsorbCount);
			
			if (damage < full)
			{
				saved = damage;
			}
			else
			{
				saved = full + int((damage - full) * SavePercent);
				if (MaxAbsorb > 0 && saved + AbsorbCount > MaxAbsorb) 
				{
					saved = MAX(0,  MaxAbsorb - AbsorbCount);
				}
			}

			if (Amount < saved)
			{
				saved = Amount;
			}
			newdamage -= saved;
			Amount -= saved;
			AbsorbCount += saved;
			if (Amount == 0)
			{
				// The armor has become useless
				SavePercent = 0;
				ArmorType = 'None'; // Not NAME_BasicArmor.
				// Now see if the player has some more armor in their inventory
				// and use it if so. As in Strife, the best armor is used up first.
				BasicArmorPickup best = null;
				Inventory probe = Owner.Inv;
				while (probe != null)
				{
					let inInv = BasicArmorPickup(probe);
					if (inInv != null)
					{
						if (best == null || best.SavePercent < inInv.SavePercent)
						{
							best = inInv;
						}
					}
					probe = probe.Inv;
				}
				if (best != null)
				{
					Owner.UseInventory (best);
				}
			}
			damage = newdamage;
		}

		// Once the armor has absorbed its part of the damage, then apply its damage factor, if any, to the player
		if ((damage > 0) && (ArmorType != 'None')) // BasicArmor is not going to have any damage factor, so skip it.
		{
			newdamage = ApplyDamageFactors(ArmorType, damageType, damage, damage);
		}
	}
}

//===========================================================================
//
//
// BasicArmorBonus
//
//
//===========================================================================

class BasicArmorBonus : Armor 
{
	double SavePercent;	// The default, for when you don't already have armor
	int MaxSaveAmount;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;
	int BonusCount;
	int BonusMax;
	
	property prefix: Armor;
	property MaxSaveAmount: MaxSaveAmount;
	property SaveAmount : SaveAmount;
	property SavePercent: SavePercent;
	property MaxAbsorb: MaxAbsorb;
	property MaxFullAbsorb: MaxFullAbsorb;
	property MaxBonus: BonusCount;
	property MaxBonusMax: BonusMax;

	Default
	{
		+Inventory.AUTOACTIVATE
		+Inventory.ALWAYSPICKUP
		Inventory.MaxAmount 0;
		Armor.SavePercent 33.335;
	}
	
	//===========================================================================
	//
	// ABasicArmorBonus :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		let copy = BasicArmorBonus(Super.CreateCopy (other));
		copy.SavePercent = SavePercent;
		copy.SaveAmount = SaveAmount;
		copy.MaxSaveAmount = MaxSaveAmount;
		copy.BonusCount = BonusCount;
		copy.BonusMax = BonusMax;
		copy.MaxAbsorb = MaxAbsorb;
		copy.MaxFullAbsorb = MaxFullAbsorb;

		return copy;
	}

	//===========================================================================
	//
	// ABasicArmorBonus :: Use
	//
	// Tries to add to the amount of BasicArmor a player has.
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		let armor = BasicArmor(Owner.FindInventory("BasicArmor", true));
		bool result = false;

		// This should really never happen but let's be prepared for a broken inventory.
		if (armor == null)
		{
			armor = BasicArmor(Spawn(GetBasicArmorClass()));
			armor.BecomeItem ();
			armor.Amount = 0;
			armor.MaxAmount = MaxSaveAmount;
			Owner.AddInventory (armor);
		}

		if (BonusCount > 0 && armor.BonusCount < BonusMax)
		{
			armor.BonusCount = min(armor.BonusCount + BonusCount, BonusMax);
			result = true;
		}

		int saveAmount = min(GetSaveAmount(), MaxSaveAmount);

		if (saveAmount <= 0)
		{ // If it can't give you anything, it's as good as used.
			return BonusCount > 0 ? result : true;
		}

		// If you already have more armor than this item can give you, you can't
		// use it.
		if (armor.Amount >= MaxSaveAmount + armor.BonusCount)
		{
			return result;
		}

		if (armor.Amount <= 0)
		{ // Should never be less than 0, but might as well check anyway
			armor.Amount = 0;
			armor.Icon = Icon;
			armor.SavePercent = clamp(SavePercent, 0, 100) / 100;
			armor.MaxAbsorb = MaxAbsorb;
			armor.ArmorType = GetClassName();
			armor.MaxFullAbsorb = MaxFullAbsorb;
			armor.ActualSaveAmount = MaxSaveAmount;
		}

		armor.Amount = min(armor.Amount + saveAmount, MaxSaveAmount + armor.BonusCount);
		armor.MaxAmount = max(armor.MaxAmount, MaxSaveAmount);
		return true;
	}

	
	override void SetGiveAmount(Actor receiver, int amount, bool bycheat)
	{
		SaveAmount *= amount;
	}

	int GetSaveAmount ()
	{
		return !bIgnoreSkill ? int(SaveAmount * G_SkillPropertyFloat(SKILLP_ArmorFactor)) : SaveAmount;
	}
}

//===========================================================================
//
//
// BasicArmorPickup
//
//
//===========================================================================

class BasicArmorPickup : Armor
{

	double SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;

	property prefix: Armor;
	property SaveAmount : SaveAmount;
	property SavePercent: SavePercent;
	property MaxAbsorb: MaxAbsorb;
	property MaxFullAbsorb: MaxFullAbsorb;

	Default
	{
		+Inventory.AUTOACTIVATE;
		Inventory.MaxAmount 0;
	}
	
	//===========================================================================
	//
	// ABasicArmorPickup :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		let copy = BasicArmorPickup(Super.CreateCopy (other));
		copy.SavePercent = SavePercent;
		copy.SaveAmount = SaveAmount;
		copy.MaxAbsorb = MaxAbsorb;
		copy.MaxFullAbsorb = MaxFullAbsorb;

		return copy;
	}
	
	//===========================================================================
	//
	// ABasicArmorPickup :: Use
	//
	// Either gives you new armor or replaces the armor you already have (if
	// the SaveAmount is greater than the amount of armor you own). When the
	// item is auto-activated, it will only be activated if its max amount is 0
	// or if you have no armor active already.
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		int SaveAmount = GetSaveAmount();
		let armor = BasicArmor(Owner.FindInventory("BasicArmor", true));

		// This should really never happen but let's be prepared for a broken inventory.
		if (armor == null)
		{
			armor = BasicArmor(Spawn(GetBasicArmorClass()));
			armor.BecomeItem ();
			Owner.AddInventory (armor);
		}
		else
		{
			// If you already have more armor than this item gives you, you can't
			// use it.
			if (armor.Amount >= SaveAmount + armor.BonusCount)
			{
				return false;
			}
			// Don't use it if you're picking it up and already have some.
			if (pickup && armor.Amount > 0 && MaxAmount > 0)
			{
				return false;
			}
		}
		
		armor.SavePercent = clamp(SavePercent, 0, 100) / 100;
		armor.Amount = SaveAmount + armor.BonusCount;
		armor.MaxAmount = SaveAmount;
		armor.Icon = Icon;
		armor.MaxAbsorb = MaxAbsorb;
		armor.MaxFullAbsorb = MaxFullAbsorb;
		armor.ArmorType = GetClassName();
		armor.ActualSaveAmount = SaveAmount;
		return true;
	}
	
	override void SetGiveAmount(Actor receiver, int amount, bool bycheat)
	{
		SaveAmount *= amount;
	}
	
	int GetSaveAmount ()
	{
		return !bIgnoreSkill ? int(SaveAmount * G_SkillPropertyFloat(SKILLP_ArmorFactor)) : SaveAmount;
	}
}

//===========================================================================
//
//
// HexenArmor
//
// Hexen armor consists of four separate armor types plus a conceptual armor
// type (the player himself) that work together as a single armor.
//
//
//===========================================================================

class HexenArmor : Armor
{
	
	double Slots[5];
	double SlotsIncrement[4];
	
	Default
	{
		+Inventory.KEEPDEPLETED
		+Inventory.UNTOSSABLE
	}
	
	//===========================================================================
	//
	// AHexenArmor :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		// Like BasicArmor, HexenArmor is used in the inventory but not the map.
		// health is the slot this armor occupies.
		// Amount is the quantity to give (0 = normal max).
		let copy = HexenArmor(Spawn(GetHexenArmorClass()));
		copy.AddArmorToSlot (health, Amount);
		GoAwayAndDie ();
		return copy;
	}

	//===========================================================================
	//
	// AHexenArmor :: CreateTossable
	//
	// Since this isn't really a single item, you can't drop it. Ever.
	//
	//===========================================================================

	override Inventory CreateTossable (int amount)
	{
		return NULL;
	}

	//===========================================================================
	//
	// AHexenArmor :: HandlePickup
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item is "HexenArmor")
		{
			if (AddArmorToSlot (item.health, item.Amount))
			{
				item.bPickupGood = true;
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// AHexenArmor :: AddArmorToSlot
	//
	//===========================================================================

	protected bool AddArmorToSlot (int slot, int amount)
	{
		double hits;

		if (slot < 0 || slot > 3)
		{
			return false;
		}
		if (amount <= 0)
		{
			hits = SlotsIncrement[slot];
			if (Slots[slot] < hits)
			{
				Slots[slot] = hits;
				return true;
			}
		}
		else
		{
			hits = amount * 5;
			let total = Slots[0] + Slots[1] + Slots[2] + Slots[3] + Slots[4];
			let max = SlotsIncrement[0] + SlotsIncrement[1] + SlotsIncrement[2] + SlotsIncrement[3] + Slots[4] + 4 * 5;
			if (total < max)
			{
				Slots[slot] += hits;
				return true;
			}
		}
		return false;
	}

	//===========================================================================
	//
	// AHexenArmor :: AbsorbDamage
	//
	//===========================================================================

	override void AbsorbDamage (int damage, Name damageType, out int newdamage, Actor inflictor, Actor source, int flags)
	{
		if (!DamageTypeDefinition.IgnoreArmor(damageType))
		{
			double savedPercent = Slots[0] + Slots[1] + Slots[2] + Slots[3] + Slots[4];

			if (savedPercent)
			{ // armor absorbed some damage
				if (savedPercent > 100)
				{
					savedPercent = 100;
				}
				for (int i = 0; i < 4; i++)
				{
					if (Slots[i])
					{
						// 300 damage always wipes out the armor unless some was added
						// with the dragon skin bracers.
						if (damage < 10000)
						{
							Slots[i] -= damage * SlotsIncrement[i] / 300.;
							if (Slots[i] < 2)
							{
								Slots[i] = 0;
							}
						}
						else
						{
							Slots[i] = 0;
						}
					}
				}
				int saved = int(damage * savedPercent / 100.);
				if (saved > savedPercent*2)
				{	
					saved = int(savedPercent*2);
				}
				newdamage -= saved;
				damage = newdamage;
			}
		}
	}

	//===========================================================================
	//
	// AHexenArmor :: DepleteOrDestroy
	//
	//===========================================================================

	override void DepleteOrDestroy()
	{
		for (int i = 0; i < 4; i++)
		{
			Slots[i] = 0;
		}
	}	
}

