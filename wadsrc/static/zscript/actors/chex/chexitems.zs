
// General Pickups ============================================================

// Health ---------------------------------------------------------------------

class GlassOfWater : HealthBonus
{
	Default
	{
		inventory.pickupmessage "$GOTWATER";
	}
}

class BowlOfFruit : Stimpack
{
	Default
	{
		inventory.pickupmessage "$GOTFRUIT";
	}
}

class BowlOfVegetables : Medikit
{
	Default
	{
		inventory.pickupmessage "$GOTVEGETABLES";
		health.lowmessage 25, "$GOTVEGETABLESNEED";
	}
}

class SuperchargeBreakfast : Soulsphere
{
	Default
	{
		inventory.pickupmessage "$GOTBREAKFAST";
	}
}

// Armor ----------------------------------------------------------------------

class SlimeRepellent : ArmorBonus
{
	Default
	{
		inventory.pickupmessage "$GOTREPELLENT";
	}
}

class ChexArmor : GreenArmor
{
	Default
	{
		inventory.pickupmessage "$GOTCHEXARMOR";
	}
}

class SuperChexArmor : BlueArmor
{
	Default
	{
		inventory.pickupmessage "$GOTSUPERCHEXARMOR";
	}
}

// Powerups ===================================================================

class ComputerAreaMap : Allmap
{
	Default
	{
		inventory.pickupmessage "$GOTCHEXMAP";
	}
}

class SlimeProofSuit : RadSuit
{
	Default
	{
		inventory.pickupmessage "$GOTSLIMESUIT";
	}
}

class Zorchpack : Backpack
{
	Default
	{
		inventory.pickupmessage "$GOTZORCHPACK";
	}
}
