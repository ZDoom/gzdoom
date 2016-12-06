
Class HereticKey : Key
{
	Default
	{
		+NOTDMATCH
		Radius 20;
		Height 16;
	}
}

// Green key ------------------------------------------------------------

Class KeyGreen : HereticKey
{
	Default
	{
		Inventory.PickupMessage "$TXT_GOTGREENKEY";
		Inventory.Icon "GKEYICON";
	}
	States
	{
	Spawn:
		AKYY ABCDEFGHIJ 3 Bright;
		Loop;
	}
}

// Blue key -----------------------------------------------------------------

Class KeyBlue : HereticKey
{
	Default
	{
		Inventory.PickupMessage "$TXT_GOTBLUEKEY";
		Inventory.Icon "BKEYICON";
	}
	States
	{
	Spawn:
		BKYY ABCDEFGHIJ 3 Bright;
		Loop;
	}
}

// Yellow key ---------------------------------------------------------------

Class KeyYellow : HereticKey
{
	Default
	{
		Inventory.PickupMessage "$TXT_GOTYELLOWKEY";
		Inventory.Icon "YKEYICON";
	}
	States
	{
	Spawn:
		CKYY ABCDEFGHI 3 Bright;
		Loop;
	}
}

 
// --- Blue Key gizmo -----------------------------------------------------------

Class KeyGizmoBlue : Actor
{
	Default
	{
		Radius 16;
		Height 50;
		+SOLID
	}
	States
	{
	Spawn:
		KGZ1 A 1;
		KGZ1 A 1 A_SpawnItemEx("KeyGizmoFloatBlue", 0, 0, 60);
		KGZ1 A -1;
		Stop;
	}
}

Class KeyGizmoFloatBlue : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
		+NOGRAVITY
	}
	States
	{
	Spawn:
		KGZB A -1 Bright;
		Stop;
	}
}

// --- Green Key gizmo -----------------------------------------------------------

Class KeyGizmoGreen : Actor
{
	Default
	{
		Radius 16;
		Height 50;
		+SOLID
	}
	States
	{
	Spawn:
		KGZ1 A 1;
		KGZ1 A 1 A_SpawnItemEx("KeyGizmoFloatGreen", 0, 0, 60);
		KGZ1 A -1;
		Stop;
	}
}

Class KeyGizmoFloatGreen : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
		+NOGRAVITY
	}
	States
	{
	Spawn:
		KGZG A -1 Bright;
		Stop;
	}
}

// --- Yellow Key gizmo -----------------------------------------------------------

Class KeyGizmoYellow : Actor
{
	Default
	{
		Radius 16;
		Height 50;
		+SOLID
	}
	States
	{
	Spawn:
		KGZ1 A 1;
		KGZ1 A 1 A_SpawnItemEx("KeyGizmoFloatYellow", 0, 0, 60);
		KGZ1 A -1;
		Stop;
	}
}

Class KeyGizmoFloatYellow : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
		+NOGRAVITY
	}
	States
	{
	Spawn:
		KGZY A -1 Bright;
		Stop;
	}
}

