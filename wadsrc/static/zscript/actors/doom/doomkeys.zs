
class DoomKey : Key
{
	Default
	{
		Radius 20;
		Height 16;
		+NOTDMATCH
	}
}

// Blue key card ------------------------------------------------------------

class BlueCard : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTBLUECARD";
		Inventory.Icon "STKEYS0";
	}
	States
	{
	Spawn:
		BKEY A 10;
		BKEY B 10 bright;
		loop;
	}
}

// Yellow key card ----------------------------------------------------------

class YellowCard : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTYELWCARD";
		Inventory.Icon "STKEYS1";
	}
	States
	{
	Spawn:
		YKEY A 10;
		YKEY B 10 bright;
		loop;
	}
}

// Red key card -------------------------------------------------------------

class RedCard : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTREDCARD";
		Inventory.Icon "STKEYS2";
	}
	States
	{
	Spawn:
		RKEY A 10;
		RKEY B 10 bright;
		loop;
	}
}

// Blue skull key -----------------------------------------------------------

class BlueSkull : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTBLUESKUL";
		Inventory.Icon "STKEYS3";
	}
	States
	{
	Spawn:
		BSKU A 10;
		BSKU B 10 bright;
		loop;
	}
}

// Yellow skull key ---------------------------------------------------------

class YellowSkull : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTYELWSKUL";
		Inventory.Icon "STKEYS4";
	}
	States
	{
	Spawn:
		YSKU A 10;
		YSKU B 10 bright;
		loop;
	}
}

// Red skull key ------------------------------------------------------------

class RedSkull : DoomKey
{
	Default
	{
		Inventory.Pickupmessage "$GOTREDSKUL";
		Inventory.Icon "STKEYS5";
	}
	States
	{
	Spawn:
		RSKU A 10;
		RSKU B 10 bright;
		loop;
	}
}

