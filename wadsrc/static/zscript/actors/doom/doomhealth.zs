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
		Tag "$TAG_HEALTHBONUS";
	}
	States
	{
	Spawn:
		BON1 ABCDCB 6;
		Loop;
	}

	//===========================================================================
	//
	// TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor other)
	{
		PrevHealth = other.player != NULL ? other.player.health : other.health;

		// Dehacked max health is compatibility dependent because Boom interpreted this value wrong.
		let maxamt = MaxAmount;
		if (maxamt < 0)
		{
			maxamt = deh.MaxHealth;
			if (!(Level.compatflags & COMPATF_DEHHEALTH)) maxamt *= 2;
		}

		if (other.GiveBody(Amount, maxamt))
		{
			GoAwayAndDie();
			return true;
		}
		return false;
	}

}
	
// Stimpack -----------------------------------------------------------------

class Stimpack : Health
{
	Default
	{
		Inventory.Amount 10;
		Inventory.PickupMessage "$GOTSTIM";
		Tag "$TAG_STIMPACK";
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
		Tag "$TAG_MEDIKIT";
	}
	States
	{
	Spawn:
		MEDI A -1;
		Stop;
	}
}
