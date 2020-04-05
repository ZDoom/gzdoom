//===========================================================================
//
//
//
//===========================================================================

class ScoreItem : Inventory
{
	Default
	{
		Height 10;
		+COUNTITEM
		Inventory.Amount 1;
		+Inventory.ALWAYSPICKUP
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		toucher.Score += Amount;
		GoAwayAndDie();
		return true;
	}
}

//===========================================================================
//
//
//
//===========================================================================

class Key : Inventory
{
	Default
	{
		+DONTGIB;		// Don't disappear due to a crusher
		Inventory.InterHubAmount 0;
		Inventory.PickupSound "misc/k_pkup";
	}

	static native clearscope int GetKeyTypeCount();
	static native clearscope class<Key> GetKeyType(int index);
	
	override bool HandlePickup (Inventory item)
	{
		// In single player, you can pick up an infinite number of keys
		// even though you can only hold one of each.
		if (multiplayer)
		{
			return Super.HandlePickup (item);
		}
		if (GetClass() == item.GetClass())
		{
			item.bPickupGood = true;
			return true;
		}
		return false;
	}

	override bool ShouldStay ()
	{
		return !!multiplayer;
	}
}

//===========================================================================
//
// AMapRevealer
//
// A MapRevealer reveals the whole map for the player who picks it up.
// The MapRevealer doesn't actually go in your inventory. Instead, it sets
// a flag on the level.
//
//===========================================================================

class MapRevealer : Inventory
{
	override bool TryPickup (in out Actor toucher)
	{
		level.allmap = true;
		GoAwayAndDie ();
		return true;
	}
}

//===========================================================================
//
//
//
//===========================================================================

class PuzzleItem : Inventory
{
	meta int PuzzleItemNumber;
	meta String PuzzFailMessage;
	
	property Number: PuzzleItemNumber;
	property FailMessage: PuzzFailMessage;

	Default
	{
		+NOGRAVITY
		+INVENTORY.INVBAR
		Inventory.DefMaxAmount;
		Inventory.UseSound "PuzzleSuccess";
		Inventory.PickupSound "misc/i_pkup";
		PuzzleItem.FailMessage("$TXT_USEPUZZLEFAILED");
	}
	
	override bool HandlePickup (Inventory item)
	{
		// Can't carry more than 1 of each puzzle item in coop netplay
		if (multiplayer && !deathmatch && item.GetClass() == GetClass())
		{
			return true;
		}
		return Super.HandlePickup (item);
	}

	override bool Use (bool pickup)
	{
		if (Owner == NULL) return false;
		if (Owner.UsePuzzleItem (PuzzleItemNumber))
		{
			return true;
		}
		// [RH] Always play the sound if the use fails.
		Owner.A_StartSound ("*puzzfail", CHAN_VOICE);
		if (Owner.CheckLocalView())
		{
			Console.MidPrint (null, PuzzFailMessage, true);
		}
		return false;
	}

	override void UseAll(Actor user)
	{
	}
	
	override bool ShouldStay ()
	{
		return !!multiplayer;
	}

}

