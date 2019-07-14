// Flame Thrower ------------------------------------------------------------

class FlameThrower : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 2100;
		Weapon.Kickback 0;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 100;
		Weapon.UpSound "weapons/flameidle";
		Weapon.ReadySound "weapons/flameidle";
		Weapon.AmmoType1 "EnergyPod";
		Inventory.Icon "FLAMA0";
		Tag "$TAG_FLAMER";
		Inventory.PickupMessage "$TXT_FLAMER";
	}
	
	States
	{
	Spawn:
		FLAM A -1;
		Stop;
	Ready:
		FLMT AB 3 A_WeaponReady;
		Loop;
	Deselect:
		FLMT A 1 A_Lower;
		Loop;
	Select:
		FLMT A 1 A_Raise;
		Loop;
	Fire:
		FLMF A 2 A_FireFlamer;
		FLMF B 3 A_ReFire;
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_FireFlamer
	//
	//============================================================================

	action void A_FireFlamer ()
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
		
		player.mo.PlayAttacking2 ();

		Angle += Random2[Flamethrower]() * (5.625/256.);
		Actor mo = SpawnPlayerMissile ("FlameMissile");
		if (mo != NULL)
		{
			mo.Vel.Z += 5;
		}
	}
}	


// Flame Thrower Projectile -------------------------------------------------

class FlameMissile : Actor
{
	Default
	{
		Speed 15;
		Height 11;
		Radius 8;
		Mass 10;
		Damage 4;
		DamageType "Fire";
		ReactionTime 8;
		Projectile;
		-NOGRAVITY
		+STRIFEDAMAGE
		+ZDOOMTRANS
		MaxStepHeight 4;
		RenderStyle "Add";
		SeeSound "weapons/flamethrower";
		Obituary "$OB_MPFLAMETHROWER";
	}

	States
	{
	Spawn:
		FRBL AB 3 Bright;
		FRBL C 3 Bright A_Countdown;
		Loop;
	Death:
		FRBL D 5 Bright A_FlameDie;
		FRBL EFGHI 5 Bright;
		Stop;
	}

	//============================================================================
	//
	// A_FlameDie
	//
	//============================================================================

	void A_FlameDie ()
	{
		bNoGravity = true;
		Vel.Z = random[FlameDie](0, 3);
	}
}

