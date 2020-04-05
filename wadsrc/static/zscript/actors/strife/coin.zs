
// Coin ---------------------------------------------------------------------

class Coin : Inventory
{
	Default
	{
		+DROPPED
		+NOTDMATCH
		+FLOORCLIP
		Inventory.MaxAmount 0x7fffffff;
		+INVENTORY.INVBAR
		Tag "$TAG_COIN";
		Inventory.Icon "I_COIN";
		Inventory.PickupMessage "$TXT_COIN";
	}
	States
	{
	Spawn:
		COIN A -1;
		Stop;
	}
	
	// Coin ---------------------------------------------------------------------

	override String PickupMessage ()
	{
		if (Amount == 1)
		{
			return Super.PickupMessage();
		}
		else
		{
			String msg = StringTable.Localize("$TXT_XGOLD");
			msg.Replace("%d", "" .. Amount);
			return msg;
		}
	}

	override bool HandlePickup (Inventory item)
	{
		if (item is "Coin")
		{
			if (Amount < MaxAmount)
			{
				if (MaxAmount - Amount < item.Amount)
				{
					Amount = MaxAmount;
				}
				else
				{
					Amount += item.Amount;
				}
				item.bPickupGood = true;
			}
			return true;
		}
		return false;
	}

	override Inventory CreateCopy (Actor other)
	{
		if (GetClass() == "Coin")
		{
			return Super.CreateCopy (other);
		}
		Inventory copy = Inventory(Spawn("Coin"));
		copy.Amount = Amount;
		copy.BecomeItem ();
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

	override Inventory CreateTossable (int amt)
	{
		Coin tossed;

		if (bUndroppable || Owner == NULL || Amount <= 0)
		{
			return NULL;
		}
		if (amt == -1) amt = Amount >= 50? 50 : Amount >= 25? 25 : Amount >= 10? 10 : 1;
		else if (amt > Amount) amt = Amount;
		if (amt > 25)
		{
			Amount -= amt;
			tossed = Coin(Spawn("Gold50"));
			tossed.Amount = amt;
		}
		else if (amt > 10)
		{
			Amount -= 25;
			tossed = Coin(Spawn("Gold25"));
		}
		else if (amt > 1)
		{
			Amount -= 10;
			tossed = Coin(Spawn("Gold10"));
		}
		else if (Amount > 1 || bKeepDepleted)
		{
			Amount -= 1;
			tossed = Coin(Spawn("Coin"));
		}
		else // Amount == 1 && !(ItemFlags & IF_KEEPDEPLETED)
		{
			BecomePickup ();
			tossed = self;
		}
		tossed.bSpecial = false;
		tossed.bSolid = false;
		tossed.DropTime = 30;
		if (tossed != self && Amount <= 0)
		{
			Destroy ();
		}
		return tossed;
	}
	
}


// 10 Gold ------------------------------------------------------------------

class Gold10 : Coin
{
	Default
	{
		Inventory.Amount 10;
		Tag "$TAG_10GOLD";
		Inventory.PickupMessage "$TXT_10GOLD";
	}
	States
	{
	Spawn:
		CRED A -1;
		Stop;
	}
}

// 25 Gold ------------------------------------------------------------------

class Gold25 : Coin
{
	Default
	{
		Inventory.Amount 25;
		Tag "$TAG_25GOLD";
		Inventory.PickupMessage "$TXT_25GOLD";
	}
	States
	{
	Spawn:
		SACK A -1;
		Stop;
	}
}

// 50 Gold ------------------------------------------------------------------

class Gold50 : Coin
{
	Default
	{
		Inventory.Amount 50;
		Tag "$TAG_50GOLD";
		Inventory.PickupMessage "$TXT_50GOLD";
	}
	States
	{
	Spawn:
		CHST A -1;
		Stop;
	}
}

// 300 Gold ------------------------------------------------------------------

class Gold300 : Coin
{
	Default
	{
		Inventory.Amount 300;
		Tag "$TAG_300GOLD";
		Inventory.PickupMessage "$TXT_300GOLD";
		Inventory.GiveQuest 3;
		+INVENTORY.ALWAYSPICKUP
	}
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
}

