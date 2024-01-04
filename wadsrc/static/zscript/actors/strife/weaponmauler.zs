// Mauler -------------------------------------------------------------------
// The scatter version

class Mauler : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 300;
		Weapon.AmmoUse1 20;
		Weapon.AmmoGive1 40;
		Weapon.AmmoType1 "EnergyPod";
		Weapon.SisterWeapon "Mauler2";
		Inventory.Icon "TRPDA0";
		Tag "$TAG_MAULER1";
		Inventory.PickupMessage "$TXT_MAULER";
		Obituary "$OB_MPMAULER1";
	}

	States
	{
	Ready:
		MAUL FGHA 6 A_WeaponReady;
		Loop;
	Deselect:
		MAUL A 1 A_Lower;
		Loop;
	Select:
		MAUL A 1 A_Raise;
		Loop;
	Fire:
		BLSF A 5 Bright A_FireMauler1;
		MAUL B 3 Bright A_Light1;
		MAUL C 2 A_Light2;
		MAUL DE 2;
		MAUL A 7 A_Light0;
		MAUL H 7;
		MAUL G 7 A_CheckReload;
		Goto Ready;
	Spawn:
		TRPD A -1;
		Stop;
	}
		
	//============================================================================
	//
	// A_FireMauler1
	//
	// Hey! This is exactly the same as a super shotgun except for the sound
	// and the bullet puffs and the disintegration death.
	//
	//============================================================================

	action void A_FireMauler1()
	{
		if (player == null)
		{
			return;
		}

		A_StartSound ("weapons/mauler1", CHAN_WEAPON);
		Weapon weap = player.ReadyWeapon;
		if (weap != null)
		{
			if (!weap.DepleteAmmo (weap.bAltFire, true))
				return;
			
		}
		player.mo.PlayAttacking2 ();

		double pitch = BulletSlope ();
			
		for (int i = 0 ; i < 20 ; i++)
		{
			int damage = 5 * random[Mauler1](1, 3);
			double ang = angle + Random2[Mauler1]() * (11.25 / 256);

			// Strife used a range of 2112 units for the mauler to signal that
			// it should use a different puff. ZDoom's default range is longer
			// than this, so let's not handicap it by being too faithful to the
			// original.

			LineAttack (ang, PLAYERMISSILERANGE, pitch + Random2[Mauler1]() * (7.097 / 256), damage, 'Hitscan', "MaulerPuff");
		}
	}
}


// Mauler Torpedo version ---------------------------------------------------

class Mauler2 : Mauler
{
	Default
	{
		Weapon.SelectionOrder 3300;
		Weapon.AmmoUse1 30;
		Weapon.AmmoGive1 0;
		Weapon.AmmoType1 "EnergyPod";
		Weapon.SisterWeapon "Mauler";
		Tag "$TAG_MAULER2";
	}

	States
	{
	Ready:
		MAUL IJKL 7 A_WeaponReady;
		Loop;
	Deselect:
		MAUL I 1 A_Lower;
		Loop;
	Select:
		MAUL I 1 A_Raise;
		Loop;
	Fire:
		MAUL I 20 A_FireMauler2Pre;
		MAUL J 10 A_Light1;
		BLSF A 10 Bright A_FireMauler2;
		MAUL B 10 Bright A_Light2;
		MAUL C 2;
		MAUL D 2 A_Light0;
		MAUL E 2 A_ReFire;
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_FireMauler2Pre
	//
	// Makes some noise and moves the psprite.
	//
	//============================================================================

	action void A_FireMauler2Pre ()
	{
		A_StartSound ("weapons/mauler2charge", CHAN_WEAPON);

		if (player != null)
		{
			PSprite psp = player.GetPSprite(PSP_WEAPON);
			if (psp)
			{
				psp.x += Random2[Mauler2]() / 64.;
				psp.y += Random2[Mauler2]() / 64.;
			}
		}
	}

	//============================================================================
	//
	// A_FireMauler2Pre
	//
	// Fires the torpedo.
	//
	//============================================================================

	action void A_FireMauler2 ()
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
		
		SpawnPlayerMissile ("MaulerTorpedo");
		DamageMobj (self, null, 20, 'Disintegrate');
		Thrust(7.8125, Angle+180.);
	}
}


// Mauler "Bullet" Puff -----------------------------------------------------

class MaulerPuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		+ZDOOMTRANS
		RenderStyle "Add";
		DamageType "Disintegrate";
	}
	States
	{
	Spawn:
		MPUF AB 5;
		POW1 ABCDE 4;
		Stop;
	}
}

// The Mauler's Torpedo -----------------------------------------------------

class MaulerTorpedo : Actor
{
	Default
	{
		Speed 20;
		Height 8;
		Radius 13;
		Damage 1;
		DamageType "Disintegrate";
		Projectile;
		+STRIFEDAMAGE
		+ZDOOMTRANS
		MaxStepHeight 4;
		RenderStyle "Add";
		SeeSound "weapons/mauler2fire";
		DeathSound "weapons/mauler2hit";
		Obituary "$OB_MPMAULER";
	}

	States
	{
	Spawn:
		TORP ABCD 4 Bright;
		Loop;
	Death:
		THIT AB 8 Bright;
		THIT C 8 Bright A_MaulerTorpedoWave;
		THIT DE 8 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_MaulerTorpedoWave
	//
	// Launches lots of balls when the torpedo hits something.
	//
	//============================================================================

	action void A_MaulerTorpedoWave()
	{
		if (target == null) return;
		readonly<Actor> wavedef = GetDefaultByType("MaulerTorpedoWave");
		double savedz = pos.z;
		angle += 180.;

		// If the torpedo hit the ceiling, it should still spawn the wave
		if (wavedef && ceilingz < pos.z + wavedef.Height)
		{
			SetZ(ceilingz - wavedef.Height);
		}

		for (int i = 0; i < 80; ++i)
		{
			Angle += 4.5;
			SpawnSubMissile ("MaulerTorpedoWave", target);
		}
		SetZ(savedz);
	}
}


// The mini torpedoes shot by the big torpedo --------------------------------

class MaulerTorpedoWave : Actor
{
	Default
	{
		Speed 35;
		Radius 13;
		Height 13;
		Damage 10;
		DamageType "Disintegrate";
		Projectile;
		+STRIFEDAMAGE
		+ZDOOMTRANS
		MaxStepHeight 4;
		RenderStyle "Add";
		Obituary "$OB_MPMAULER";
	}
	States
	{
	Spawn:
		TWAV AB 9 Bright;
	Death:
		TWAV C 9 Bright;
		Stop;
	}

}


