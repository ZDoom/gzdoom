// Gold wand ----------------------------------------------------------------

class GoldWand : HereticWeapon
{
	Default
	{
		+BLOODSPLATTER
		Weapon.SelectionOrder 2000;
		Weapon.AmmoGive 25;
		Weapon.AmmoUse 1;
		Weapon.AmmoType "GoldWandAmmo";
		Weapon.SisterWeapon "GoldWandPowered";
		Weapon.YAdjust 5;
		Inventory.PickupMessage "$TXT_WPNGOLDWAND";
		Obituary "$OB_MPGOLDWAND";
		Tag "$TAG_GOLDWAND";
	}

	States
	{
	Spawn:
		GWAN A -1;
		Stop;
	Ready:
		GWND A 1 A_WeaponReady;
		Loop;
	Deselect:
		GWND A 1 A_Lower;
		Loop;
	Select:
		GWND A 1 A_Raise;
		Loop;
	Fire:
		GWND B 3;
		GWND C 5 A_FireGoldWandPL1;
		GWND D 3;
		GWND D 0 A_ReFire;
		Goto Ready;
	}
	

	//----------------------------------------------------------------------------
	//
	// PROC A_FireGoldWandPL1
	//
	//----------------------------------------------------------------------------

	action void A_FireGoldWandPL1 ()
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
		double pitch = BulletSlope();
		int damage = random[FireGoldWand](7, 14);
		double ang = angle;
		if (player.refire)
		{
			ang += Random2[FireGoldWand]() * (5.625 / 256);
		}
		LineAttack(ang, PLAYERMISSILERANGE, pitch, damage, 'Hitscan', "GoldWandPuff1");
		A_StartSound("weapons/wandhit", CHAN_WEAPON);
	}
	
}

class GoldWandPowered : GoldWand
{
	Default
	{
		+WEAPON.POWERED_UP
		Weapon.AmmoGive 0;
		Weapon.SisterWeapon "GoldWand";
		Obituary "$OB_MPPGOLDWAND";
		Tag "$TAG_GOLDWANDP";
	}

	States
	{
	Fire:
		GWND B 3;
		GWND C 4 A_FireGoldWandPL2;
		GWND D 3;
		GWND D 0 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireGoldWandPL2
	//
	//----------------------------------------------------------------------------

	action void A_FireGoldWandPL2 ()
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
		double pitch = BulletSlope();

		double vz = -GetDefaultByType("GoldWandFX2").Speed * clamp(tan(pitch), -5, 5);
		SpawnMissileAngle("GoldWandFX2", angle - (45. / 8), vz);
		SpawnMissileAngle("GoldWandFX2", angle + (45. / 8), vz);
		double ang = angle - (45. / 8);
		for(int i = 0; i < 5; i++)
		{
			int damage = random[FireGoldWand](1, 8);
			LineAttack (ang, PLAYERMISSILERANGE, pitch, damage, 'Hitscan', "GoldWandPuff2");
			ang += ((45. / 8) * 2) / 4;
		}
		A_StartSound("weapons/wandhit", CHAN_WEAPON);
	}

	
}
	

// Gold wand FX1 ------------------------------------------------------------

class GoldWandFX1 : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 22;
		Damage 2;
		Projectile;
		RenderStyle "Add";
		+ZDOOMTRANS
		DeathSound "weapons/wandhit";
		Obituary "$OB_MPPGOLDWAND";
	}

	States
	{
	Spawn:
		FX01 AB 6 BRIGHT;
		Loop;
	Death:
		FX01 EFGH 3 BRIGHT;
		Stop;
	}
}

// Gold wand FX2 ------------------------------------------------------------

class GoldWandFX2 : GoldWandFX1
{
	Default
	{
		Speed 18;
		Damage 1;
		DeathSound "";
	}

	States
	{
	Spawn:
		FX01 CD 6 BRIGHT;
		Loop;
	}
}

// Gold wand puff 1 ---------------------------------------------------------

class GoldWandPuff1 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		PUF2 ABCDE 3 BRIGHT;
		Stop;
	}
}

// Gold wand puff 2 ---------------------------------------------------------

class GoldWandPuff2 : GoldWandFX1
{
	Default
	{
		Skip_Super;
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
	}

	States
	{
	Spawn:
		Goto Super::Death;
	}
}

