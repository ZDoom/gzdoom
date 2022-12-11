// Renaming and changing messages

// Mini Zorch -----------------------------------------------------------------

class MiniZorchRecharge : Clip
{
	Default
	{
		inventory.pickupmessage "$GOTZORCHRECHARGE";
	}
}

class MiniZorchPack : Clip
{
	Default
	{
		Inventory.PickupMessage "$GOTMINIZORCHPACK";
		Inventory.Amount 50;
	}
	States
	{
		Spawn:
			AMMO A -1;
			Stop;
	}
}

// Large Zorch ----------------------------------------------------------------

class LargeZorchRecharge : Shell
{
	Default
	{
		inventory.pickupmessage "$GOTLARGEZORCHERRECHARGE";
	}
}

class LargeZorchPack : Shell
{
	Default
	{
		Inventory.PickupMessage "$GOTLARGEZORCHERPACK";
		Inventory.Amount 20;
	}
	States
	{
		Spawn:
			SBOX A -1;
			Stop;
	}
}

// Zorch Propulsor ------------------------------------------------------------

class PropulsorZorch : RocketAmmo
{
	Default
	{
		inventory.pickupmessage "$GOTPROPULSORRECHARGE";
	}
}

class PropulsorZorchPack : RocketAmmo
{
	Default
	{
		Inventory.PickupMessage "$GOTPROPULSORPACK";
		Inventory.Amount 5;
	}
	States
	{
		Spawn:
			BROK A -1;
			Stop;
	}
}

// Phasing Zorch --------------------------------------------------------------

class PhasingZorch : Cell
{
	Default
	{
		inventory.pickupmessage "$GOTPHASINGZORCHERRECHARGE";
	}
}

class PhasingZorchPack : Cell
{
	Default
	{
		Inventory.PickupMessage "$GOTPHASINGZORCHERPACK";
		Inventory.Amount 100;
	}
	States
	{
		Spawn:
			CELP A -1;
			Stop;
	}
}
