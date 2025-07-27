
// General Pickups ============================================================

// Health ---------------------------------------------------------------------

class GlassOfWater : HealthBonus
{
	Default
	{
		inventory.pickupmessage "$GOTWATER";
		tag "$TAG_GLASSOFWATER";
	}
}

class BowlOfFruit : Stimpack
{
	Default
	{
		inventory.pickupmessage "$GOTFRUIT";
		tag "$TAG_BOWLOFFRUIT";
	}
}

class BowlOfVegetables : Medikit
{
	Default
	{
		inventory.pickupmessage "$GOTVEGETABLES";
		health.lowmessage 25, "$GOTVEGETABLESNEED";
		tag "$TAG_BOWLOFVEGETABLES";
	}
}

class SuperchargeBreakfast : Soulsphere
{
	Default
	{
		inventory.pickupmessage "$GOTBREAKFAST";
		tag "$TAG_BREAKFAST";
	}
}

// Armor ----------------------------------------------------------------------

class SlimeRepellent : ArmorBonus
{
	Default
	{
		inventory.pickupmessage "$GOTREPELLENT";
		tag "$TAG_REPELLENT";
	}
}

class ChexArmor : GreenArmor
{
	Default
	{
		inventory.pickupmessage "$GOTCHEXARMOR";
		tag "$TAG_CHEXARMOR";
	}
}

class SuperChexArmor : BlueArmor
{
	Default
	{
		inventory.pickupmessage "$GOTSUPERCHEXARMOR";
		tag "$TAG_SUPERCHEXARMOR";
	}
}

// Powerups ===================================================================

class ComputerAreaMap : Allmap
{
	Default
	{
		inventory.pickupmessage "$GOTCHEXMAP";
		tag "$TAG_CHEXMAP";
	}
}

class SlimeProofSuit : RadSuit
{
	Default
	{
		inventory.pickupmessage "$GOTSLIMESUIT";
		tag "$TAG_SLIMESUIT";
	}
}

class Zorchpack : Backpack
{
	Default
	{
		inventory.pickupmessage "$GOTZORCHPACK";
		tag "$TAG_ZORCHPACK";
	}
}
