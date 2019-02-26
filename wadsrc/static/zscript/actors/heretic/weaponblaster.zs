// Blaster ------------------------------------------------------------------

class Blaster : HereticWeapon
{
	Default
	{
		+BLOODSPLATTER
		Weapon.SelectionOrder 500;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 30;
		Weapon.YAdjust 15;
		Weapon.AmmoType "BlasterAmmo";
		Weapon.SisterWeapon "BlasterPowered";
		Inventory.PickupMessage "$TXT_WPNBLASTER";
		Tag "$TAG_BLASTER";
		Obituary "$OB_MPBLASTER";
	}

	States
	{
	Spawn:
		WBLS A -1;
		Stop;
	Ready:
		BLSR A 1 A_WeaponReady;
		Loop;
	Deselect:
		BLSR A 1 A_Lower;
		Loop;
	Select:
		BLSR A 1 A_Raise;
		Loop;
	Fire:
		BLSR BC 3;
	Hold:
		BLSR D 2 A_FireBlasterPL1;
		BLSR CB 2;
		BLSR A 0 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireBlasterPL1
	//
	//----------------------------------------------------------------------------

	action void A_FireBlasterPL1()
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
		int damage = random[FireBlaster](1, 8) * 4;
		double ang = angle;
		if (player.refire)
		{
			ang += Random2[FireBlaster]() * (5.625 / 256);
		}
		LineAttack (ang, PLAYERMISSILERANGE, pitch, damage, 'Hitscan', "BlasterPuff");
		A_PlaySound ("weapons/blastershoot", CHAN_WEAPON);
	}
}

class BlasterPowered : Blaster
{
	Default
	{
		+WEAPON.POWERED_UP
		Weapon.AmmoUse 5;
		Weapon.AmmoGive 0;
		Weapon.SisterWeapon "Blaster";
		Tag "$TAG_BLASTERP";
	}

	States
	{
	Fire:
		BLSR BC 0;
	Hold:
		BLSR D 3 A_FireProjectile("BlasterFX1");
		BLSR CB 4;
		BLSR A 0 A_ReFire;
		Goto Ready;
	}
}

// Blaster FX 1 -------------------------------------------------------------

class BlasterFX1 : FastProjectile
{
	Default
	{
		Radius 12;
		Height 8;
		Speed 184;
		Damage 2;
		SeeSound "weapons/blastershoot";
		DeathSound "weapons/blasterhit";
		+SPAWNSOUNDSOURCE
		Obituary "$OB_MPPBLASTER";
	}

	States
	{
	Spawn:
		ACLO E 200;
		Loop;
	Death:
		FX18 A 3 BRIGHT A_SpawnRippers;
		FX18 B 3 BRIGHT;
		FX18 CDEFG 4 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target is "Ironlich")
		{ // Less damage to Ironlich bosses
			damage = random[BlasterFX](0, 1);
			if (!damage)
			{
				return -1;
			}
		}
		return damage;
	}

	override void Effect ()
	{
		if (random[BlasterFX]() < 64)
		{
			Spawn("BlasterSmoke", (pos.xy, max(pos.z - 8, floorz)), ALLOW_REPLACE);
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_SpawnRippers
	//
	//----------------------------------------------------------------------------

	void A_SpawnRippers()
	{
		for(int i = 0; i < 8; i++)
		{
			Actor ripper = Spawn("Ripper", pos, ALLOW_REPLACE);
			if (ripper != null)
			{
				ripper.target = target;
				ripper.angle = i*45;
				ripper.VelFromAngle();
				ripper.CheckMissileSpawn (radius);
			}
		}
	}
}

// Blaster smoke ------------------------------------------------------------

class BlasterSmoke : Actor
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
		FX18 HIJKL 4;
		Stop;
	}
}

// Ripper -------------------------------------------------------------------

class Ripper : Actor
{
	Default
	{
		Radius 8;
		Height 6;
		Speed 14;
		Damage 1;
		Projectile;
		+RIPPER
		DeathSound "weapons/blasterpowhit";
		Obituary "$OB_MPPBLASTER";
	}

	States
	{
	Spawn:
		FX18 M 4;
		FX18 N 5;
		Loop;
	Death:
		FX18 OPQRS 4 BRIGHT;
		Stop;
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target is "Ironlich")
		{ // Less damage to Ironlich bosses
			damage = random[Ripper](0, 1);
			if (!damage)
			{
				return -1;
			}
		}
		return damage;
	}
	
}

// Blaster Puff -------------------------------------------------------------

class BlasterPuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "weapons/blasterhit";
	}

	States
	{
	Crash:
		FX17 ABCDE 4 BRIGHT;
		Stop;
	Spawn:
		FX17 FG 3 BRIGHT;
		FX17 HIJKL 4 BRIGHT;
		Stop;
	}
}

