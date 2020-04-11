
// Fighter Weapon Piece -----------------------------------------------------

class FighterWeaponPiece : WeaponPiece
{
	Default
	{
		Inventory.PickupSound "misc/w_pkup";
		Inventory.PickupMessage "$TXT_QUIETUS_PIECE";
		Inventory.ForbiddenTo "ClericPlayer", "MagePlayer";
		WeaponPiece.Weapon "FWeapQuietus";
		+FLOATBOB
	}
}

// Fighter Weapon Piece 1 ---------------------------------------------------

class FWeaponPiece1 : FighterWeaponPiece
{
	Default
	{
		WeaponPiece.Number 1;
	}
	States
	{
	Spawn:
		WFR1 A -1 Bright;
		Stop;
	}
}

// Fighter Weapon Piece 2 ---------------------------------------------------

class FWeaponPiece2 : FighterWeaponPiece
{
	Default
	{
		WeaponPiece.Number 2;
	}
	States
	{
	Spawn:
		WFR2 A -1 Bright;
		Stop;
	}
}

// Fighter Weapon Piece 3 ---------------------------------------------------

class FWeaponPiece3 : FighterWeaponPiece
{
	Default
	{
		WeaponPiece.Number 3;
	}
	States
	{
	Spawn:
		WFR3 A -1 Bright;
		Stop;
	}
}

// Quietus Drop -------------------------------------------------------------

class QuietusDrop : Actor
{
	States
	{
	Spawn:
		TNT1 A 1;
		TNT1 A 1 A_DropWeaponPieces("FWeaponPiece1", "FWeaponPiece2", "FWeaponPiece3");
		Stop;
	}
}

// The Fighter's Sword (Quietus) --------------------------------------------

class FWeapQuietus : FighterWeapon
{
	Default
	{
		Health 3;
		Weapon.SelectionOrder 2900;
		+WEAPON.PRIMARY_USES_BOTH;
		+Inventory.NoAttenPickupSound
		Weapon.AmmoUse1 14;
		Weapon.AmmoUse2 14;
		Weapon.AmmoGive1 20;
		Weapon.AmmoGive2 20;
		Weapon.KickBack 150;
		Weapon.YAdjust 10;
		Weapon.AmmoType1 "Mana1";
		Weapon.AmmoType2 "Mana2";
		Inventory.PickupMessage "$TXT_WEAPON_F4";
		Inventory.PickupSound "WeaponBuild";
		Tag "$TAG_FWEAPQUIETUS";
	}

	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	Select:
		FSRD A 1 Bright A_Raise;
		Loop;
	Deselect:
		FSRD A 1 Bright A_Lower;
		Loop;
	Ready:
		FSRD AAAABBBBCCCC 1 Bright A_WeaponReady;
		Loop;
	Fire:
		FSRD DE 3 Bright Offset (5, 36);
		FSRD F 2 Bright Offset (5, 36);
		FSRD G 3 Bright Offset (5, 36) A_FSwordAttack;
		FSRD H 2 Bright Offset (5, 36);
		FSRD I 2 Bright Offset (5, 36);
		FSRD I 10 Bright Offset (5, 150);
		FSRD A 1 Bright Offset (5, 60);
		FSRD B 1 Bright Offset (5, 55);
		FSRD C 1 Bright Offset (5, 50);
		FSRD A 1 Bright Offset (5, 45);
		FSRD B 1 Bright Offset (5, 40);
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_FSwordAttack
	//
	//============================================================================

	action void A_FSwordAttack()
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
		SpawnPlayerMissile ("FSwordMissile", Angle + (45./4),0, 0, -10);
		SpawnPlayerMissile ("FSwordMissile", Angle + (45./8),0, 0,  -5);
		SpawnPlayerMissile ("FSwordMissile", Angle          ,0, 0,   0);
		SpawnPlayerMissile ("FSwordMissile", Angle - (45./8),0, 0,   5);
		SpawnPlayerMissile ("FSwordMissile", Angle - (45./4),0, 0,  10);
		A_StartSound ("FighterSwordFire", CHAN_WEAPON);
	}

	
}

// Fighter Sword Missile ----------------------------------------------------

class FSwordMissile : Actor
{
	Default
	{
		Speed 30;
		Radius 16;
		Height 8;
		Damage 8;
		Projectile;
		+EXTREMEDEATH
		+ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "FighterSwordExplode";
		Obituary "$OB_MPFWEAPQUIETUS";
	}

	States
	{
	Spawn:
		FSFX ABC 3 Bright;
		Loop;
	Death:
		FSFX D 4 Bright;
		FSFX E 3 Bright A_FSwordFlames;
		FSFX F 4 Bright A_Explode (64, 128, 0);
		FSFX G 3 Bright;
		FSFX H 4 Bright;
		FSFX I 3 Bright;
		FSFX J 4 Bright;
		FSFX KLM 3 Bright;
		Stop;
	}
	
	override int DoSpecialDamage(Actor victim, int damage, Name damagetype)
	{
		if (victim.player)
		{
			damage -= damage >> 2;
		}
		return damage;
	}
		
	//============================================================================
	//
	// A_FSwordFlames
	//
	//============================================================================

	void A_FSwordFlames()
	{
		for (int i = random[FSwordFlame](1, 4); i; i--)
		{
			double xo = (random[FSwordFlame]() - 128) / 16.;
			double yo = (random[FSwordFlame]() - 128) / 16.;
			double zo = (random[FSwordFlame]() - 128) / 8.;
			Spawn ("FSwordFlame", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		}
	}
}

// Fighter Sword Flame ------------------------------------------------------

class FSwordFlame : Actor
{
	Default
	{
	+NOBLOCKMAP +NOGRAVITY
	RenderStyle "Translucent";
	Alpha 0.6;
	}
	States
	{
	Spawn:
		FSFX NOPQRSTUVW 3 Bright;
		Stop;
	}
}
