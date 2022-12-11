// Health bonus -------------------------------------------------------------

class HealthBonus : Health
{
	Default
	{
		+COUNTITEM
		+INVENTORY.ALWAYSPICKUP
		Inventory.Amount 1;
		Inventory.MaxAmount 200;
		Inventory.PickupMessage "$GOTHTHBONUS";
	}
	States
	{
	Spawn:
		BON1 ABCDCB 6;
		Loop;
	}
}
	
// Stimpack -----------------------------------------------------------------

class Stimpack : Health
{
	Default
	{
		Inventory.Amount 10;
		Inventory.PickupMessage "$GOTSTIM";
	}
	States
	{
	Spawn:
		STIM A -1;
		Stop;
	}
}

// Medikit -----------------------------------------------------------------

class Medikit : Health
{
	Default
	{
		Inventory.Amount 25;
		Inventory.PickupMessage "$GOTMEDIKIT";
		Health.LowMessage 25, "$GOTMEDINEED";
	}
	States
	{
	Spawn:
		MEDI A -1;
		Stop;
	}
}
