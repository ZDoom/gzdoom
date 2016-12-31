

// Base class for the rebels ------------------------------------------------

class Rebel : StrifeHumanoid
{
	Default
	{
		Health 60;
		Painchance 250;
		Speed 8;
		Radius 20;
		Height 56;
		Monster;
		+FRIENDLY
		-COUNTKILL
		+NOSPLASHALERT
		MinMissileChance 150;
		Tag "$TAG_REBEL";
		SeeSound "rebel/sight";
		PainSound "rebel/pain";
		DeathSound "rebel/death";
		ActiveSound "rebel/active";
		Obituary "$OB_REBEL";
	}
	States
	{
	Spawn:
		HMN1 P 5 A_Look2;
		Loop;
		HMN1 Q 8;
		Loop;
		HMN1 R 8;
		Loop;
		HMN1 ABCDABCD 6 A_Wander;
		Loop;
	See:
		HMN1 AABBCCDD 3 A_Chase;
		Loop;
	Missile:
		HMN1 E 10 A_FaceTarget;
		HMN1 F 10 BRIGHT A_ShootGun;
		HMN1 E 10 A_ShootGun;
		Goto See;
	Pain:
		HMN1 O 3;
		HMN1 O 3 A_Pain;
		Goto See;
	Death:
		HMN1 G 5;
		HMN1 H 5 A_Scream;
		HMN1 I 3 A_NoBlocking;
		HMN1 J 4;
		HMN1 KLM 3;
		HMN1 N -1;
		Stop;
	XDeath:
		RGIB A 4 A_TossGib;
		RGIB B 4 A_XScream;
		RGIB C 3 A_NoBlocking;
		RGIB DEF 3 A_TossGib;
		RGIB G 3;
		RGIB H 1400;
		Stop;
	}
}

// Rebel 1 ------------------------------------------------------------------

class Rebel1 : Rebel
{
	Default
	{
		DropItem "ClipOfBullets";
	}
}

// Rebel 2 ------------------------------------------------------------------

class Rebel2 : Rebel
{
}

// Rebel 3 ------------------------------------------------------------------

class Rebel3 : Rebel
{
}

// Rebel 4 ------------------------------------------------------------------

class Rebel4 : Rebel
{
}

// Rebel 5 ------------------------------------------------------------------

class Rebel5 : Rebel
{
}

// Rebel 6 ------------------------------------------------------------------

class Rebel6 : Rebel
{
}

// Teleporter Beacon --------------------------------------------------------

class TeleporterBeacon : Inventory
{
	Default
	{
		Health 5;
		Radius 16;
		Height 16;
		Inventory.MaxAmount 3;
		+DROPPED
		+INVENTORY.INVBAR
		Inventory.Icon "I_BEAC";
		Tag "$TAG_TELEPORTERBEACON";
		Inventory.PickupMessage "$TXT_BEACON";
	}

	States
	{
	Spawn:
		BEAC A -1;
		Stop;
	Drop:
		BEAC A 30;
		BEAC A 160 A_Beacon;
		Wait;
	Death:
		BEAC A 1 A_FadeOut(0.015);
		Loop;
	}
	
	// Teleporter Beacon --------------------------------------------------------

	override bool Use (bool pickup)
	{
		// Increase the amount by one so that when DropInventory decrements it,
		// the actor will have the same number of beacons that he started with.
		// When we return to UseInventory, it will take care of decrementing
		// Amount again and disposing of self item if there are no more.
		Amount++;
		Inventory drop = Owner.DropInventory (self);
		if (drop == null)
		{
			Amount--;
			return false;
		}
		else
		{
			drop.SetStateLabel("Drop");
			drop.target = Owner;
			return true;
		}
	}

	void A_Beacon()
	{
		Actor owner = target;
		Actor rebel = Spawn("Rebel1", (pos.xy, floorz), ALLOW_REPLACE);
		if (rebel == null)
		{
			return;
		}
		if (!rebel.TryMove (rebel.Pos.xy, true))
		{
			rebel.Destroy ();
			return;
		}
		// Once the rebels start teleporting in, you can't pick up the beacon anymore.
		bSpecial = false;
		Inventory(self).DropTime = 0;
		// Set up the new rebel.
		rebel.threshold = rebel.DefThreshold;
		rebel.target = null;
		rebel.bInCombat = true;
		rebel.LastHeard = owner;	// Make sure the rebels look for targets
		if (deathmatch)
		{
			rebel.health *= 2;
		}
		if (owner != null)
		{
			// Rebels are the same color as their owner (but only in multiplayer)
			if (multiplayer)
			{
				rebel.Translation = owner.Translation;
			}
			rebel.SetFriendPlayer(owner.player);
			// Set the rebel's target to whatever last hurt the player, so long as it's not
			// one of the player's other rebels.
			if (owner.target != null && !rebel.IsFriend (owner.target))
			{
				rebel.target = owner.target;
			}
		}

		rebel.SetState (rebel.SeeState);
		rebel.Angle = Angle;
		rebel.SpawnTeleportFog(rebel.Vec3Angle(20., Angle, 0), false, true);
		if (--health < 0)
		{
			SetStateLabel("Death");
		}
	}
}