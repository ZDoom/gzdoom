
// Silver Shield (Shield1) --------------------------------------------------

Class SilverShield : BasicArmorPickup
{
	Default
	{
		+FLOATBOB
		Inventory.Pickupmessage "$TXT_ITEMSHIELD1";
		Tag "$TAG_ITEMSHIELD1";
		Inventory.Icon "SHLDA0";
		Armor.Savepercent 50;
		Armor.Saveamount 100;
	}
	States
	{
	Spawn:
		SHLD A -1;
		stop;
	}
}

// Enchanted shield (Shield2) -----------------------------------------------

Class EnchantedShield : BasicArmorPickup
{
	Default
	{
		+FLOATBOB
		Inventory.Pickupmessage "$TXT_ITEMSHIELD2";
		Tag "$TAG_ITEMSHIELD2";
		Inventory.Icon "SHD2A0";
		Armor.Savepercent 75;
		Armor.Saveamount 200;
	}
	States
	{
	Spawn:
		SHD2 A -1;
		stop;
	}
}

