// Mini-Missile Launcher ----------------------------------------------------

class MiniMissileLauncher : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 1800;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 8;
		Weapon.AmmoType1 "MiniMissiles";
		Inventory.Icon "MMSLA0";
		Tag "$TAG_MMLAUNCHER";
		Inventory.PickupMessage "$TXT_MMLAUNCHER";
		+WEAPON.EXPLOSIVE
	}

	States
	{
	Spawn:
		MMSL A -1;
		Stop;
	Ready:
		MMIS A 1 A_WeaponReady;
		Loop;
	Deselect:
		MMIS A 1 A_Lower;
		Loop;
	Select:
		MMIS A 1 A_Raise;
		Loop;
	Fire:
		MMIS A 4 A_FireMiniMissile;
		MMIS B 4 A_Light1;
		MMIS C 5 Bright;
		MMIS D 2 Bright A_Light2;
		MMIS E 2 Bright;
		MMIS F 2 Bright A_Light0;
		MMIS F 0 A_ReFire;
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_FireMiniMissile
	//
	//============================================================================

	action void A_FireMiniMissile ()
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
		
		double savedangle = angle;
		angle += Random2[MiniMissile]() * (11.25 / 256) * AccuracyFactor();
		player.mo.PlayAttacking2 ();
		SpawnPlayerMissile ("MiniMissile");
		angle = savedangle;
	}
}
	

// Rocket Trail -------------------------------------------------------------

class RocketTrail : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.25;
		SeeSound "misc/missileinflight";
	}
	States
	{
	Spawn:
		PUFY BCBCD 4;
		Stop;
	}
}

// Rocket Puff --------------------------------------------------------------

class MiniMissilePuff : StrifePuff
{
	Default
	{
		-ALLOWPARTICLES
	}
	States
	{
	Spawn:
		Goto Crash;
	}
}

// Mini Missile -------------------------------------------------------------

class MiniMissile : Actor
{
	Default
	{
		Speed 20;
		Radius 10;
		Height 14;
		Damage 10;
		Projectile;
		+STRIFEDAMAGE
		MaxStepHeight 4;
		SeeSound "weapons/minimissile";
		DeathSound "weapons/minimissilehit";
		Obituary "$OB_MPMINIMISSILELAUNCHER";
	}
	States
	{
	Spawn:
		MICR A 6 Bright A_RocketInFlight;
		Loop;
	Death:
		SMIS A 0 Bright A_SetRenderStyle(1, STYLE_Normal);
		SMIS A 5 Bright A_Explode(64, 64, alert:true);
		SMIS B 5 Bright;
		SMIS C 4 Bright;
		SMIS DEFG 2 Bright;
		Stop;
	}
}

