// Clip --------------------------------------------------------------------

class Clip : Ammo
{
	Default
	{
		Inventory.PickupMessage "$GOTCLIP";
		Inventory.Amount 10;
		Inventory.MaxAmount 200;
		Ammo.BackpackAmount 10;
		Ammo.BackpackMaxAmount 400;
		Inventory.Icon "CLIPA0";
		Tag "$AMMO_CLIP";
	}
	States
	{
	Spawn:
		CLIP A -1;
		Stop;
	}
}

// Clip box ----------------------------------------------------------------

class ClipBox : Clip
{
	Default
	{
		Inventory.PickupMessage "$GOTCLIPBOX";
		Inventory.Amount 50;
	}
	States
	{
	Spawn:
		AMMO A -1;
		Stop;
	}
}

// Rocket ------------------------------------------------------------------

class RocketAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$GOTROCKET";
		Inventory.Amount 1;
		Inventory.MaxAmount 50;
		Ammo.BackpackAmount 1;
		Ammo.BackpackMaxAmount 100;
		Inventory.Icon "ROCKA0";
		Tag "$AMMO_ROCKETS";
	}
	States
	{
	Spawn:
		ROCK A -1;
		Stop;
	}
}

// Rocket box --------------------------------------------------------------

class RocketBox : RocketAmmo
{
	Default
	{
		Inventory.PickupMessage "$GOTROCKBOX";
		Inventory.Amount 5;
	}
	States
	{
	Spawn:
		BROK A -1;
		Stop;
	}
}

// Cell --------------------------------------------------------------------

class Cell : Ammo
{
	Default
	{
		Inventory.PickupMessage "$GOTCELL";
		Inventory.Amount 20;
		Inventory.MaxAmount 300;
		Ammo.BackpackAmount 20;
		Ammo.BackpackMaxAmount 600;
		Inventory.Icon "CELLA0";
		Tag "$AMMO_CELLS";
	}
	States
	{
	Spawn:
		CELL A -1;
		Stop;
	}
}

// Cell pack ---------------------------------------------------------------

class CellPack : Cell
{
	Default
	{
		Inventory.PickupMessage "$GOTCELLBOX";
		Inventory.Amount 100;
	}
	States
	{
	Spawn:
		CELP A -1;
		Stop;
	}
}

// Shells ------------------------------------------------------------------

class Shell : Ammo
{
	Default
	{
		Inventory.PickupMessage "$GOTSHELLS";
		Inventory.Amount 4;
		Inventory.MaxAmount 50;
		Ammo.BackpackAmount 4;
		Ammo.BackpackMaxAmount 100;
		Inventory.Icon "SHELA0";
		Tag "$AMMO_SHELLS";
	}
	States
	{
	Spawn:
		SHEL A -1;
		Stop;
	}
}

// Shell box ---------------------------------------------------------------

class ShellBox : Shell
{
	Default
	{
		Inventory.PickupMessage "$GOTSHELLBOX";
		Inventory.Amount 20;
	}
	States
	{
	Spawn:
		SBOX A -1;
		Stop;
	}
}

// Backpack ---------------------------------------------------------------

class Backpack : BackpackItem
{
	Default
	{
		Height 26;
		Inventory.PickupMessage "$GOTBACKPACK";
	}
	States
	{
	Spawn:
		BPAK A -1;
		Stop;
	}
}

