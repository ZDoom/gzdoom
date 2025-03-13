
class HexenKey : Key
{
	Default
	{
		Radius 8;
		Height 20;
	}
}

class KeySteel : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT1";
		Inventory.PickupMessage "$TXT_KEY_STEEL";
		Tag "$TAG_KEY_STEEL";
	}
	States
	{
	Spawn:
		KEY1 A -1;
		Stop;
	}
}

class KeyCave : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT2";
		Inventory.PickupMessage "$TXT_KEY_CAVE";
		Tag "$TAG_KEY_CAVE";
	}
	States
	{
	Spawn:
		KEY2 A -1;
		Stop;
	}
}

class KeyAxe : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT3";
		Inventory.PickupMessage "$TXT_KEY_AXE";
		Tag "$TAG_KEY_AXE";
	}
	States
	{
	Spawn:
		KEY3 A -1;
		Stop;
	}
}

class KeyFire : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT4";
		Inventory.PickupMessage "$TXT_KEY_FIRE";
		Tag "$TAG_KEY_FIRE";
	}
	States
	{
	Spawn:
		KEY4 A -1;
		Stop;
	}
}

class KeyEmerald : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT5";
		Inventory.PickupMessage "$TXT_KEY_EMERALD";
		Tag "$TAG_KEY_EMERALD";
	}
	States
	{
	Spawn:
		KEY5 A -1;
		Stop;
	}
}

class KeyDungeon : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT6";
		Inventory.PickupMessage "$TXT_KEY_DUNGEON";
		Tag "$TAG_KEY_DUNGEON";
	}
	States
	{
	Spawn:
		KEY6 A -1;
		Stop;
	}
}

class KeySilver : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT7";
		Inventory.PickupMessage "$TXT_KEY_SILVER";
		Tag "$TAG_KEY_SILVER";
	}
	States
	{
	Spawn:
		KEY7 A -1;
		Stop;
	}
}

class KeyRusted : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT8";
		Inventory.PickupMessage "$TXT_KEY_RUSTED";
		Tag "$TAG_KEY_RUSTED";
	}
	States
	{
	Spawn:
		KEY8 A -1;
		Stop;
	}
}

class KeyHorn : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOT9";
		Inventory.PickupMessage "$TXT_KEY_HORN";
		Tag "$TAG_KEY_HORN";
	}
	States
	{
	Spawn:
		KEY9 A -1;
		Stop;
	}
}

class KeySwamp : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOTA";
		Inventory.PickupMessage "$TXT_KEY_SWAMP";
		Tag "$TAG_KEY_SWAMP";
	}
	States
	{
	Spawn:
		KEYA A -1;
		Stop;
	}
}

class KeyCastle : HexenKey
{
	Default
	{
		Inventory.Icon "KEYSLOTB";
		Inventory.PickupMessage "$TXT_KEY_CASTLE";
		Tag "$TAG_KEY_CASTLE";
	}
	States
	{
	Spawn:
		KEYB A -1;
		Stop;
	}
}

