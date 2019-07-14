// Skull (Horn) Rod ---------------------------------------------------------

class SkullRod : HereticWeapon
{
	Default
	{
		Weapon.SelectionOrder 200;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 50;
		Weapon.YAdjust 15;
		Weapon.AmmoType1 "SkullRodAmmo";
		Weapon.SisterWeapon "SkullRodPowered";
		Inventory.PickupMessage "$TXT_WPNSKULLROD";
		Tag "$TAG_SKULLROD";
	}

	States
	{
	Spawn:
		WSKL A -1;
		Stop;
	Ready:
		HROD A 1 A_WeaponReady;
		Loop;
	Deselect:
		HROD A 1 A_Lower;
		Loop;
	Select:
		HROD A 1 A_Raise;
		Loop;
	Fire:
		HROD AB 4 A_FireSkullRodPL1;
		HROD B 0 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireSkullRodPL1
	//
	//----------------------------------------------------------------------------

	action void A_FireSkullRodPL1()
	{
		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		Actor mo = SpawnPlayerMissile ("HornRodFX1");
		// Randomize the first frame
		if (mo && random[FireSkullRod]() > 128)
		{
			mo.SetState (mo.CurState.NextState);
		}
	}

	
}

class SkullRodPowered : SkullRod
{
	Default
	{
		+WEAPON.POWERED_UP
		Weapon.AmmoUse1 5;
		Weapon.AmmoGive1 0;
		Weapon.SisterWeapon "SkullRod";
		Tag "$TAG_SKULLRODP";
	}

	States
	{
	Fire:
		HROD C 2;
		HROD D 3;
		HROD E 2;
		HROD F 3;
		HROD G 4 A_FireSkullRodPL2;
		HROD F 2;
		HROD E 3;
		HROD D 2;
		HROD C 2 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireSkullRodPL2
	//
	// The special2 field holds the player number that shot the rain missile.
	// The special1 field holds the id of the rain sound.
	//
	//----------------------------------------------------------------------------

	action void A_FireSkullRodPL2()
	{
		FTranslatedLineTarget t;
		
		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		// Use MissileActor instead of the first return value from P_SpawnPlayerMissile 
		// because we need to give info to it, even if it exploded immediately.
		Actor mo, MissileActor;
		[mo, MissileActor] = SpawnPlayerMissile ("HornRodFX2", angle, pLineTarget: t);
		if (MissileActor != null)
		{
			if (t.linetarget && !t.unlinked)
			{
				MissileActor.tracer = t.linetarget;
			}
			MissileActor.A_PlaySound ("weapons/hornrodpowshoot", CHAN_WEAPON);
		}
	}

	
}

// Horn Rod FX 1 ------------------------------------------------------------

class HornRodFX1 : Actor
{
	Default
	{
		Radius 12;
		Height 8;
		Speed 22;
		Damage 3;
		Projectile;
		+WINDTHRUST
		+ZDOOMTRANS
		-NOBLOCKMAP
		RenderStyle "Add";
		SeeSound "weapons/hornrodshoot";
		DeathSound "weapons/hornrodhit";
		Obituary "$OB_MPSKULLROD";
	}

	States
	{
	Spawn:
		FX00 AB 6 BRIGHT;
		Loop;
	Death:
		FX00 HI 5 BRIGHT;
		FX00 JK 4 BRIGHT;
		FX00 LM 3 BRIGHT;
		Stop;
	}
}


// Horn Rod FX 2 ------------------------------------------------------------

class HornRodFX2 : Actor
{
	Default
	{
		Radius 12;
		Height 8;
		Speed 22;
		Damage 10;
		Health 140;
		Projectile;
		RenderStyle "Add";
		+ZDOOMTRANS
		SeeSound "weapons/hornrodpowshoot";
		DeathSound "weapons/hornrodpowhit";
		Obituary "$OB_MPPSKULLROD";
	}

	States
	{
	Spawn:	
		FX00 C 3 BRIGHT;
		FX00 D 3 BRIGHT A_SeekerMissile(10, 30);
		FX00 E 3 BRIGHT;
		FX00 F 3 BRIGHT A_SeekerMissile(10, 30);
		Loop;
	Death:
		FX00 H 5 BRIGHT A_AddPlayerRain;
		FX00 I 5 BRIGHT;
		FX00 J 4 BRIGHT;
		FX00 KLM 3 BRIGHT;
		FX00 G 1 A_HideInCeiling;
		FX00 G 1 A_SkullRodStorm;
		Wait;
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		Sorcerer2 s2 = Sorcerer2(target);
		if (s2 != null && random[HornRodFX2]() < 96)
		{ // D'Sparil teleports away
			s2.DSparilTeleport ();
			return -1;
		}
		return damage;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_AddPlayerRain
	//
	//----------------------------------------------------------------------------

	void A_AddPlayerRain()
	{
		RainTracker tracker;

		if (target == null || target.health <= 0)
		{ // Shooter is dead or nonexistent
			return;
		}

		tracker = RainTracker(target.FindInventory("RainTracker"));

		// They player is only allowed two rainstorms at a time. Shooting more
		// than that will cause the oldest one to terminate.
		if (tracker != null)
		{
			if (tracker.Rain1 && tracker.Rain2)
			{ // Terminate an active rain
				if (tracker.Rain1.health < tracker.Rain2.health)
				{
					if (tracker.Rain1.health > 16)
					{
						tracker.Rain1.health = 16;
					}
					tracker.Rain1 = null;
				}
				else
				{
					if (tracker.Rain2.health > 16)
					{
						tracker.Rain2.health = 16;
					}
					tracker.Rain2 = null;
				}
			}
		}
		else
		{
			tracker = RainTracker(target.GiveInventoryType("RainTracker"));
		}
		// Add rain mobj to list
		if (tracker.Rain1)
		{
			tracker.Rain2 = self;
		}
		else
		{
			tracker.Rain1 = self;
		}
		ActiveSound = "misc/rain";
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_HideInCeiling
	//
	//----------------------------------------------------------------------------

	void A_HideInCeiling()
	{
		// This no longer hides in the ceiling. It just makes the actor invisible and keeps it in place.
		// We need its actual position to determine the correct ceiling height in A_SkullRodStorm.
		bInvisible = true;
		bSolid = false;
		bMissile = false;
		Vel = (0,0,0);
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_SkullRodStorm
	//
	//----------------------------------------------------------------------------

	void A_SkullRodStorm()
	{
		static const Name translations[] =
		{
			"RainPillar1", "RainPillar2", "RainPillar3", "RainPillar4",
			"RainPillar5", "RainPillar6", "RainPillar7", "RainPillar8"
		};
		
		if (health-- == 0)
		{
			A_StopSound (CHAN_BODY);
			if (target == null)
			{ // Player left the game
				Destroy ();
				return;
			}
			RainTracker tracker = RainTracker(target.FindInventory("RainTracker"));
			if (tracker != null)
			{
				if (tracker.Rain1 == self)
				{
					tracker.Rain1 = null;
				}
				else if (tracker.Rain2 == self)
				{
					tracker.Rain2 = null;
				}
			}
			Destroy ();
			return;
		}
		if (Random[SkullRodStorm]() < 25)
		{ // Fudge rain frequency
			return;
		}
		double xo = Random[SkullRodStorm](-64, 63);
		double yo = Random[SkullRodStorm](-64, 63);
		Vector3 spawnpos = Vec2OffsetZ(xo, yo, pos.z);
		Actor mo = Spawn("RainPillar", spawnpos, ALLOW_REPLACE);
		if (!mo) return;
		
		// Find the ceiling above the spawn location. This may come from 3D floors but will not reach through portals.
		// (should probably be fixed for portals, too.)
		double newz = mo.CurSector.NextHighestCeilingAt(mo.pos.x, mo.pos.y, mo.pos.z, mo.pos.z, FFCF_NOPORTALS) - mo.height;
		mo.SetZ(newz);

		if (multiplayer && target.player)
		{
			mo.A_SetTranslation(translations[target.PlayerNumber()]);
		}
		mo.target = target;
		mo.Vel.X = MinVel; // Force collision detection
		mo.Vel.Z = -mo.Speed;
		mo.CheckMissileSpawn (radius);
		if (ActiveSound > 0) A_PlaySound(ActiveSound, CHAN_BODY, 1, true);
	}

	
}

// Rain pillar 1 ------------------------------------------------------------

class RainPillar : Actor
{
	Default
	{
		Radius 5;
		Height 12;
		Speed 12;
		Damage 5;
		Mass 5;
		Projectile;
		-ACTIVATEPCROSS
		-ACTIVATEIMPACT
		+ZDOOMTRANS
		RenderStyle "Add";
		Obituary "$OB_MPPSKULLROD";
	}

	States
	{
	Spawn:
		FX22 A -1 BRIGHT;
		Stop;
	Death:
		FX22 B 4 BRIGHT A_RainImpact;
		FX22 CDEF 4 BRIGHT;
		Stop;
	NotFloor:
		FX22 GHI 4 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_RainImpact
	//
	//----------------------------------------------------------------------------

	void A_RainImpact()
	{
		if (pos.z > floorz)
		{
			SetStateLabel("NotFloor");
		}
		else if (random[RainImpact]() < 40)
		{
			HitFloor ();
		}
	}

	// Rain pillar 1 ------------------------------------------------------------

	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.bBoss)
		{ // Decrease damage for bosses
			damage = random[RainDamage](1, 8);
		}
		return damage;
	}
}

// Rain tracker "inventory" item --------------------------------------------

class RainTracker : Inventory
{
	Actor Rain1, Rain2;
	
	Default
	{
		+INVENTORY.UNDROPPABLE
	}
}
