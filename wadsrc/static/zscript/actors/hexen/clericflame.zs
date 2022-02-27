
// The Cleric's Flame Strike ------------------------------------------------

class CWeapFlame : ClericWeapon
{
	Default
	{
		+NOGRAVITY
		Weapon.SelectionOrder 1000;
		Weapon.AmmoUse 4;
		Weapon.AmmoGive 25;
		Weapon.KickBack 150;
		Weapon.YAdjust 10;
		Weapon.AmmoType1 "Mana2";
		Inventory.PickupMessage "$TXT_WEAPON_C3";
		Tag "$TAG_CWEAPFLAME";
	}

	States
	{
	Spawn:
		WCFM ABCDEFGH 4 Bright;
		Loop;
	Select:
		CFLM A 1 A_Raise;
		Loop;
	Deselect:
		CFLM A 1 A_Lower;
		Loop;
	Ready:
		CFLM AAAABBBBCCCC 1 A_WeaponReady;
		Loop;
	Fire:
		CFLM A 2 Offset (0, 40);
		CFLM D 2 Offset (0, 50);
		CFLM D 2 Offset (0, 36);
		CFLM E 4 Bright;
		CFLM F 4 Bright A_CFlameAttack;
		CFLM E 4 Bright;
		CFLM G 2 Offset (0, 40);
		CFLM G 2;
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_CFlameAttack
	//
	//============================================================================

	action void A_CFlameAttack()
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
		SpawnPlayerMissile ("CFlameMissile");
		A_StartSound ("ClericFlameFire", CHAN_WEAPON);
	}
}

// Floor Flame --------------------------------------------------------------

class CFlameFloor : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY +ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		CFFX N 5 Bright;
		CFFX O 4 Bright;
		CFFX P 3 Bright;
		Stop;
	}
}

// Flame Puff ---------------------------------------------------------------

class FlamePuff : Actor
{
	Default
	{
		Radius 1;
		Height 1;
		+NOBLOCKMAP +NOGRAVITY +ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "ClericFlameExplode";
		AttackSound "ClericFlameExplode";
	}
	States
	{
	Spawn:
		CFFX ABC 3 Bright;
		CFFX D 4 Bright;
		CFFX E 3 Bright;
		CFFX F 4 Bright;
		CFFX G 3 Bright;
		CFFX H 4 Bright;
		CFFX I 3 Bright;
		CFFX J 4 Bright;
		CFFX K 3 Bright;
		CFFX L 4 Bright;
		CFFX M 3 Bright;
		Stop;
	}
}

// Flame Puff 2 -------------------------------------------------------------

class FlamePuff2 : FlamePuff
{
	States
	{
	Spawn:
		CFFX ABC 3 Bright;
		CFFX D 4 Bright;
		CFFX E 3 Bright;
		CFFX F 4 Bright;
		CFFX G 3 Bright;
		CFFX H 4 Bright;
		CFFX IC 3 Bright;
		CFFX D 4 Bright;
		CFFX E 3 Bright;
		CFFX F 4 Bright;
		CFFX G 3 Bright;
		CFFX H 4 Bright;
		CFFX I 3 Bright;
		CFFX J 4 Bright;
		CFFX K 3 Bright;
		CFFX L 4 Bright;
		CFFX M 3 Bright;
		Stop;
	}
}

// Circle Flame -------------------------------------------------------------

class CircleFlame : Actor
{
	const FLAMESPEED = 0.45;
	const FLAMEROTSPEED = 2.;
	
	Default
	{
		Radius 6;
		Damage 2;
		DamageType "Fire";
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "ClericFlameCircle";
		Obituary "$OB_MPCWEAPFLAME";
	}

	States
	{
	Spawn:
		CFCF A 4 Bright;
		CFCF B 2 Bright A_CFlameRotate;
		CFCF C 2 Bright;
		CFCF D 1 Bright;
		CFCF E 2 Bright;
		CFCF F 2 Bright A_CFlameRotate;
		CFCF G 1 Bright;
		CFCF HI 2 Bright;
		CFCF J 1 Bright A_CFlameRotate;
		CFCF K 2 Bright;
		CFCF LM 3 Bright;
		CFCF N 2 Bright A_CFlameRotate;
		CFCF O 3 Bright;
		CFCF P 2 Bright;
		Stop;
	Death:
		CFCF QR 3 Bright;
		CFCF S 3 Bright A_Explode(20, 128, 0);
		CFCF TUVWXYZ 3 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_CFlameRotate
	//
	//============================================================================

	void A_CFlameRotate()
	{
		double an = Angle + 90.;
		VelFromAngle(FLAMEROTSPEED, an);
		Vel.XY += (specialf1, specialf2);
		Angle += 6;
	}
}

// Flame Missile ------------------------------------------------------------

class CFlameMissile : FastProjectile
{
	Default
	{
		Speed 200;
		Radius 14;
		Height 8;
		Damage 8;
		DamageType "Fire";
		+INVISIBLE
		+ZDOOMTRANS
		RenderStyle "Add";
		Obituary "$OB_MPCWEAPFLAME";
	}

	States
	{
	Spawn:
		CFFX A 4 Bright;
		CFFX A 1 A_CFlamePuff;
		Goto Death + 1;
	Death:
		CFFX A 1 Bright A_CFlameMissile;
		CFFX ABC 3 Bright;
		CFFX D 4 Bright;
		CFFX E 3 Bright;
		CFFX F 4 Bright;
		CFFX G 3 Bright;
		CFFX H 4 Bright;
		CFFX I 3 Bright;
		CFFX J 4 Bright;
		CFFX K 3 Bright;
		CFFX L 4 Bright;
		CFFX M 3 Bright;
		Stop;
	}
	
	override void BeginPlay ()
	{
		special1 = 2;
	}

	override void Effect ()
	{
		if (!--special1)
		{
			special1 = 4;
			double newz = pos.z - 12;
			if (newz < floorz)
			{
				newz = floorz;
			}
			Actor mo = Spawn ("CFlameFloor", (pos.xy, newz), ALLOW_REPLACE);
			if (mo)
			{
				mo.angle = angle;
			}
		}
	}
	
	//============================================================================
	//
	// A_CFlamePuff
	//
	//============================================================================

	void A_CFlamePuff()
	{
		bInvisible = false;
		bMissile = false;
		Vel = (0,0,0);
		A_StartSound ("ClericFlameExplode", CHAN_BODY);
	}

	//============================================================================
	//
	// A_CFlameMissile
	//
	//============================================================================

	void A_CFlameMissile()
	{
		bInvisible = false;
		A_StartSound ("ClericFlameExplode", CHAN_BODY);
		if (BlockingMobj && BlockingMobj.bShootable)
		{ // Hit something, so spawn the flame circle around the thing
			double dist = BlockingMobj.radius + 18;
			for (int i = 0; i < 4; i++)
			{
				double an = i*45.;
				Actor mo = Spawn ("CircleFlame", BlockingMobj.Vec3Angle(dist, an, 5), ALLOW_REPLACE);
				if (mo)
				{
					mo.angle = an;
					mo.target = target;
					mo.VelFromAngle(CircleFlame.FLAMESPEED);
					mo.specialf1 = mo.Vel.X;
					mo.specialf2 = mo.Vel.Y;
					mo.tics -= random[FlameMissile](0, 3);
				}
				an += 180;
				mo = Spawn("CircleFlame", BlockingMobj.Vec3Angle(dist, an, 5), ALLOW_REPLACE);
				if(mo)
				{
					mo.angle = an;
					mo.target = target;
					mo.VelFromAngle(-CircleFlame.FLAMESPEED);
					mo.specialf1 = mo.Vel.X;
					mo.specialf2 = mo.Vel.Y;
					mo.tics -= random[FlameMissile](0, 3);
				}
			}
			SetState (SpawnState);
		}
	}
}
