
// Wimpy ammo ---------------------------------------------------------------

Class GoldWandAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOGOLDWAND1";
		Inventory.Amount 10;
		Inventory.MaxAmount 100;
		Ammo.BackpackAmount 10;
		Ammo.BackpackMaxAmount 200;
		Inventory.Icon "INAMGLD";
		Tag "$AMMO_GOLDWAND";
	}
	States
	{
	Spawn:
		AMG1 A -1;
		Stop;
	}
}

// Hefty ammo ---------------------------------------------------------------

Class GoldWandHefty : GoldWandAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOGOLDWAND2";
		Inventory.Amount 50;
	}
	States
	{
	Spawn:
		AMG2 ABC 4;
		Loop;
	}
}
// Wimpy ammo ---------------------------------------------------------------

Class CrossbowAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOCROSSBOW1";
		Inventory.Amount 5;
		Inventory.MaxAmount 50;
		Ammo.BackpackAmount 5;
		Ammo.BackpackMaxAmount 100;
		Inventory.Icon "INAMBOW";
		Tag "$AMMO_CROSSBOW";
	}
	States
	{
	Spawn:
		AMC1 A -1;
		Stop;
	}
}

// Hefty ammo ---------------------------------------------------------------

Class CrossbowHefty : CrossbowAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOCROSSBOW2";
		Inventory.Amount 20;
	}
	States
	{
	Spawn:
		AMC2 ABC 5;
		Loop;
	}
}
// Wimpy ammo ---------------------------------------------------------------

Class MaceAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOMACE1";
		Inventory.Amount 20;
		Inventory.MaxAmount 150;
		Ammo.BackpackAmount 0;
		Ammo.BackpackMaxAmount 300;
		Inventory.Icon "INAMLOB";
		Tag "$AMMO_MACE";
	}
	States
	{
	Spawn:
		AMM1 A -1;
		Stop;
	}
}

// Hefty ammo ---------------------------------------------------------------

Class MaceHefty : MaceAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOMACE2";
		Inventory.Amount 100;
	}
	States
	{
	Spawn:
		AMM2 A -1;
		Stop;
	}
}

// Wimpy ammo ---------------------------------------------------------------

Class BlasterAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOBLASTER1";
		Inventory.Amount 10;
		Inventory.MaxAmount 200;
		Ammo.BackpackAmount 10;
		Ammo.BackpackMaxAmount 400;
		Inventory.Icon "INAMBST";
		Tag "$AMMO_BLASTER";
	}
	States
	{
	Spawn:
		AMB1 ABC 4;
		Loop;
	}
}

// Hefty ammo ---------------------------------------------------------------

Class BlasterHefty : BlasterAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOBLASTER2";
		Inventory.Amount 25;
	}
	States
	{
	Spawn:
		AMB2 ABC 4;
		Loop;
	}
}

// Wimpy ammo ---------------------------------------------------------------

Class SkullRodAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOSKULLROD1";
		Inventory.Amount 20;
		Inventory.MaxAmount 200;
		Ammo.BackpackAmount 20;
		Ammo.BackpackMaxAmount 400;
		Inventory.Icon "INAMRAM";
		Tag "$AMMO_SKULLROD";
	}
	States
	{
	Spawn:
		AMS1 AB 5;
		Loop;
	}
}

// Hefty ammo ---------------------------------------------------------------

Class SkullRodHefty : SkullRodAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOSKULLROD2";
		Inventory.Amount 100;
	}
	States
	{
	Spawn:
		AMS2 AB 5;
		Loop;
	}
}

// Wimpy ammo ---------------------------------------------------------------

Class PhoenixRodAmmo : Ammo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOPHOENIXROD1";
		Inventory.Amount 1;
		Inventory.MaxAmount 20;
		Ammo.BackpackAmount 1;
		Ammo.BackpackMaxAmount 40;
		Inventory.Icon "INAMPNX";
		Tag "$AMMO_PHOENIXROD";
	}
	States
	{
	Spawn:
		AMP1 ABC 4;
		Loop;
	}
}
// Hefty ammo ---------------------------------------------------------------

Class PhoenixRodHefty : PhoenixRodAmmo
{
	Default
	{
		Inventory.PickupMessage "$TXT_AMMOPHOENIXROD2";
		Inventory.Amount 10;
	}
	States
	{
	Spawn:
		AMP2 ABC 4;
		Loop;
	}
}

// --- Bag of holding -------------------------------------------------------

Class BagOfHolding : BackpackItem
{
	Default
	{
		Inventory.PickupMessage "$TXT_ITEMBAGOFHOLDING";
		+COUNTITEM
		+FLOATBOB
	}
	States
	{
	Spawn:
		BAGH A -1;
		Stop;
	}
}
