
// Boost Armor Artifact (Dragonskin Bracers) --------------------------------

class ArtiBoostArmor : Inventory
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.DefMaxAmount;
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.INVBAR +INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIBRAC";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIBOOSTARMOR";
		Tag "$TAG_ARTIBOOSTARMOR";
	}
	States
	{
	Spawn:
		BRAC ABCDEFGH 4 Bright;
		Loop;
	}
	
	override bool Use (bool pickup)
	{
		int count = 0;

		if (gameinfo.gametype == GAME_Hexen)
		{
			HexenArmor armor;

			for (int i = 0; i < 4; ++i)
			{
				armor = HexenArmor(Spawn(GetHexenArmorClass()));
				armor.bDropped = true;
				armor.health = i;
				armor.Amount = 1;
				if (!armor.CallTryPickup (Owner))
				{
					armor.Destroy ();
				}
				else
				{
					count++;
				}
			}
			return count != 0;
		}
		else
		{
			BasicArmorBonus armor = BasicArmorBonus(Spawn("BasicArmorBonus"));
			armor.bDropped = true;
			armor.SaveAmount = 50;
			armor.MaxSaveAmount = 300;
			if (!armor.CallTryPickup (Owner))
			{
				armor.Destroy ();
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	
}
