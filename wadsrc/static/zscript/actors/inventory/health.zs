/*
** a_health.cpp
** All health items
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

class Health : Inventory
{
	transient int PrevHealth;
	meta int LowHealth;
	meta String LowHealthMessage;
	
	property LowMessage: LowHealth, LowHealthMessage;
	
	Default
	{
		+INVENTORY.ISHEALTH
		Inventory.Amount 1;
		Inventory.MaxAmount 0;
		Inventory.PickupSound "misc/health_pkup";
	}
	
	//===========================================================================
	//
	// AHealth :: PickupMessage
	//
	//===========================================================================
	override String PickupMessage ()
	{
		if (PrevHealth < LowHealth)
		{
			String message = LowHealthMessage;
			if (message.Length() != 0)
			{
				return message;
			}
		}
		return Super.PickupMessage();
	}

	//===========================================================================
	//
	// TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor other)
	{
		PrevHealth = other.player != NULL ? other.player.health : other.health;

		// P_GiveBody adds one new feature, applied only if it is possible to pick up negative health:
		// Negative values are treated as positive percentages, ie Amount -100 means 100% health, ignoring max amount.
		if (other.GiveBody(Amount, MaxAmount))
		{
			GoAwayAndDie();
			return true;
		}
		return false;
	}
}

class MaxHealth : Health
{
	//===========================================================================
	//
	// TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor other)
	{
		bool success = false;
		let player = PlayerPawn(other);
		if (player)
		{
			if (player.BonusHealth < MaxAmount)
			{
				player.BonusHealth = min(player.BonusHealth + Amount, MaxAmount);
				success = true;
			}
		}
		success |= Super.TryPickup(other);
		if (success) GoAwayAndDie();
		return success;
	}
}

class HealthPickup : Inventory
{
	int autousemode;

	property AutoUse: autousemode;

	Default
	{
		Inventory.DefMaxAmount;
		+INVENTORY.INVBAR
		+INVENTORY.ISHEALTH
	}
	
	//===========================================================================
	//
	// CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		Inventory copy = Super.CreateCopy (other);
		copy.health = health;
		return copy;
	}

	//===========================================================================
	//
	// CreateTossable
	//
	//===========================================================================

	override Inventory CreateTossable (int amount)
	{
		Inventory copy = Super.CreateTossable (-1);
		if (copy != NULL)
		{
			copy.health = health;
		}
		return copy;
	}

	//===========================================================================
	//
	// HandlePickup
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		// HealthPickups that are the same type but have different health amounts
		// do not count as the same item.
		if (item.health == health)
		{
			return Super.HandlePickup (item);
		}
		return false;
	}

	//===========================================================================
	//
	// Use
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		return Owner.GiveBody (health, 0);
	}

	
}
