// Phoenix Rod --------------------------------------------------------------

class PhoenixRod : Weapon
{
	Default
	{
		+WEAPON.NOAUTOFIRE
		Weapon.SelectionOrder 2600;
		Weapon.Kickback 150;
		Weapon.YAdjust 15;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 2;
		Weapon.AmmoType "PhoenixRodAmmo";
		Weapon.Sisterweapon "PhoenixRodPowered";
		Inventory.PickupMessage "$TXT_WPNPHOENIXROD";
		Tag "$TAG_PHOENIXROD";
	}

	States
	{
	Spawn:
		WPHX A -1;
		Stop;
	Ready:
		PHNX A 1 A_WeaponReady;
		Loop;
	Deselect:
		PHNX A 1 A_Lower;
		Loop;
	Select:
		PHNX A 1 A_Raise;
		Loop;
	Fire:
		PHNX B 5;
		PHNX C 7 A_FirePhoenixPL1;
		PHNX DB 4;
		PHNX B 0 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FirePhoenixPL1
	//
	//----------------------------------------------------------------------------

	action void A_FirePhoenixPL1()
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
		SpawnPlayerMissile ("PhoenixFX1");
		Thrust(4, angle + 180);
	}

	
}

class PhoenixRodPowered : PhoenixRod
{
	const FLAME_THROWER_TICS = (10*TICRATE);
	
	private int FlameCount;		// for flamethrower duration
	
	Default
	{
		+WEAPON.POWERED_UP
		Weapon.SisterWeapon "PhoenixRod";
		Weapon.AmmoGive 0;
		Tag "$TAG_PHOENIXRODP";
	}

	States
	{
	Fire:
		PHNX B 3 A_InitPhoenixPL2;
	Hold:
		PHNX C 1 A_FirePhoenixPL2;
		PHNX B 4 A_ReFire;
	Powerdown:
		PHNX B 4 A_ShutdownPhoenixPL2;
		Goto Ready;
	}
	

	override void EndPowerup ()
	{
		DepleteAmmo (bAltFire);
		Owner.player.refire = 0;
		Owner.A_StopSound (CHAN_WEAPON);
		Owner.player.ReadyWeapon = SisterWeapon;
		Owner.player.SetPsprite(PSP_WEAPON, SisterWeapon.GetReadyState());
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_InitPhoenixPL2
	//
	//----------------------------------------------------------------------------

	action void A_InitPhoenixPL2()
	{
		if (player != null)
		{
			PhoenixRodPowered flamethrower = PhoenixRodPowered(player.ReadyWeapon);
			if (flamethrower != null)
			{
				flamethrower.FlameCount = FLAME_THROWER_TICS;
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_FirePhoenixPL2
	//
	// Flame thrower effect.
	//
	//----------------------------------------------------------------------------

	action void A_FirePhoenixPL2()
	{
		if (player == null)
		{
			return;
		}

		PhoenixRodPowered flamethrower = PhoenixRodPowered(player.ReadyWeapon);
		
		if (flamethrower == null || --flamethrower.FlameCount == 0)
		{ // Out of flame
			player.SetPsprite(PSP_WEAPON, flamethrower.FindState("Powerdown"));
			player.refire = 0;
			A_StopSound (CHAN_WEAPON);
			return;
		}

		double slope = -clamp(tan(pitch), -5, 5);
		double xo = Random2[FirePhoenixPL2]() / 128.;
		double yo = Random2[FirePhoenixPL2]() / 128.;
		Vector3 spawnpos = Vec3Offset(xo, yo, 26 + slope - Floorclip);

		slope += 0.1;
		Actor mo = Spawn("PhoenixFX2", spawnpos, ALLOW_REPLACE);
		if (mo != null)
		{
			mo.target = self;
			mo.Angle = Angle;
			mo.VelFromAngle();
			mo.Vel.XY += Vel.XY;
			mo.Vel.Z = mo.Speed * slope;
			mo.CheckMissileSpawn (radius);
		}
		if (!player.refire)
		{
			A_PlaySound("weapons/phoenixpowshoot", CHAN_WEAPON, 1, true);
		}	
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_ShutdownPhoenixPL2
	//
	//----------------------------------------------------------------------------

	action void A_ShutdownPhoenixPL2()
	{
		if (player == null)
		{
			return;
		}
		A_StopSound (CHAN_WEAPON);
		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			weapon.DepleteAmmo (weapon.bAltFire);
		}
	}

	
}

// Phoenix FX 1 -------------------------------------------------------------

class PhoenixFX1 : Actor
{
	Default
	{
		Radius 11;
		Height 8;
		Speed 20;
		Damage 20;
		DamageType "Fire";
		Projectile;
		+THRUGHOST
		+SPECIALFIREDAMAGE
		SeeSound "weapons/phoenixshoot";
		DeathSound "weapons/phoenixhit";
		Obituary "$OB_MPPHOENIXROD";
	}

	States
	{
	Spawn:
		FX04 A 4 BRIGHT A_PhoenixPuff;
		Loop;
	Death:
		FX08 A 6 BRIGHT A_Explode;
		FX08 BC 5 BRIGHT;
		FX08 DEFGH 4 BRIGHT;
		Stop;
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
	// PROC A_PhoenixPuff
	//
	//----------------------------------------------------------------------------

	void A_PhoenixPuff()
	{
		//[RH] Heretic never sets the target for seeking
		//P_SeekerMissile (self, 5, 10);
		Actor puff = Spawn("PhoenixPuff", Pos, ALLOW_REPLACE);
		if (puff != null)
		{
			puff.Vel.XY = AngleToVector(Angle + 90, 1.3);
		}

		puff = Spawn("PhoenixPuff", Pos, ALLOW_REPLACE);
		if (puff != null)
		{
			puff.Vel.XY = AngleToVector(Angle - 90, 1.3);
		}
	}

	
}

// Phoenix puff -------------------------------------------------------------

class PhoenixPuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+NOTELEPORT
		+CANNOTPUSH
		RenderStyle "Translucent";
		Alpha 0.4;
	}

	States
	{
	Spawn:
		FX04 BCDEF 4;
		Stop;
	}
}

// Phoenix FX 2 -------------------------------------------------------------

class PhoenixFX2 : Actor
{
	Default
	{
		Radius 6;
		Height 8;
		Speed 10;
		Damage 2;
		DamageType "Fire";
		Projectile;
		RenderStyle "Add";
		+ZDOOMTRANS
		Obituary "$OB_MPPPHOENIXROD";
	}

	States
	{
	Spawn:
		FX09 ABABA 2 BRIGHT;
		FX09 B 2 BRIGHT A_FlameEnd;
		FX09 CDEF 2 BRIGHT;
		Stop;
	Death:
		FX09 G 3 BRIGHT;
		FX09 H 3 BRIGHT A_FloatPuff;
		FX09 I 4 BRIGHT;
		FX09 JK 5 BRIGHT;
		Stop;
	}
	

	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.player && Random[PhoenixFX2]() < 128)
		{ // Freeze player for a bit
			target.reactiontime += 4;
		}
		return damage;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FlameEnd
	//
	//----------------------------------------------------------------------------

	void A_FlameEnd()
	{
		Vel.Z += 1.5;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_FloatPuff
	//
	//----------------------------------------------------------------------------

	void A_FloatPuff()
	{
		Vel.Z += 1.8;
	}

	
}
