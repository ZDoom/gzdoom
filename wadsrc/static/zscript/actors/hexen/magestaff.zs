
// Mage Weapon Piece --------------------------------------------------------

class MageWeaponPiece : WeaponPiece
{
	Default
	{
		Inventory.PickupSound "misc/w_pkup";
		Inventory.PickupMessage "$TXT_BLOODSCOURGE_PIECE";
		Inventory.ForbiddenTo "FighterPlayer", "ClericPlayer";
		WeaponPiece.Weapon "MWeapBloodscourge";
		+FLOATBOB
	}
}

// Mage Weapon Piece 1 ------------------------------------------------------

class MWeaponPiece1 : MageWeaponPiece
{
	Default
	{
		WeaponPiece.Number 1;
	}
	States
	{
	Spawn:
		WMS1 A -1 Bright;
		Stop;
	}
}

// Mage Weapon Piece 2 ------------------------------------------------------

class MWeaponPiece2 : MageWeaponPiece
{
	Default
	{
		WeaponPiece.Number 2;
	}
	States
	{
	Spawn:
		WMS2 A -1 Bright;
		Stop;
	}
}

// Mage Weapon Piece 3 ------------------------------------------------------

class MWeaponPiece3 : MageWeaponPiece
{
	Default
	{
		WeaponPiece.Number 3;
	}
	States
	{
	Spawn:
		WMS3 A -1 Bright;
		Stop;
	}
}

// Bloodscourge Drop --------------------------------------------------------

class BloodscourgeDrop : Actor
{
	States
	{
	Spawn:
		TNT1 A 1;
		TNT1 A 1 A_DropWeaponPieces("MWeaponPiece1", "MWeaponPiece2", "MWeaponPiece3");
		Stop;
	}
}

// The Mages's Staff (Bloodscourge) -----------------------------------------

class MWeapBloodscourge : MageWeapon
{
	int MStaffCount;
	
	Default
	{
		Health 3;
		Weapon.SelectionOrder 3100;
		Weapon.AmmoUse1 15;
		Weapon.AmmoUse2 15;
		Weapon.AmmoGive1 20;
		Weapon.AmmoGive2 20;
		Weapon.KickBack 150;
		Weapon.YAdjust 20;
		Weapon.AmmoType1 "Mana1";
		Weapon.AmmoType2 "Mana2";
		+WEAPON.PRIMARY_USES_BOTH;
		+Inventory.NoAttenPickupSound
		Inventory.PickupMessage "$TXT_WEAPON_M4";
		Inventory.PickupSound "WeaponBuild";
		Tag "$TAG_MWEAPBLOODSCOURGE";
	}

	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	Select:
		MSTF A 1 A_Raise;
		Loop;
	Deselect:
		MSTF A 1 A_Lower;
		Loop;
	Ready:
		MSTF AAAAAABBBBBBCCCCCCDDDDDDEEEEEEFFFFF 1 A_WeaponReady;
		Loop;
	Fire:
		MSTF G 4 Offset (0, 40);
		MSTF H 4 Bright Offset (0, 48) A_MStaffAttack;
		MSTF H 2 Bright Offset (0, 48) A_MStaffPalette;
		MSTF II 2 Offset (0, 48) A_MStaffPalette;
		MSTF I 1 Offset (0, 40);
		MSTF J 5 Offset (0, 36);
		Goto Ready;
	}
	
	//============================================================================
	//
	// 
	//
	//============================================================================

	override Color GetBlend ()
	{
		if (paletteflash & PF_HEXENWEAPONS)
		{
			if (MStaffCount == 3)
				return Color(128, 100, 73, 0);
			else if (MStaffCount == 2)
				return Color(128, 125, 92, 0);
			else if (MStaffCount == 1)
				return Color(128, 150, 110, 0);
			else
				return Color(0, 0, 0, 0);
		}
		else
		{
			return Color (MStaffCount * 128 / 3, 151, 110, 0);
		}
	}

	//============================================================================
	//
	// MStaffSpawn
	//
	//============================================================================

	private action void MStaffSpawn (double angle, Actor alttarget)
	{
		FTranslatedLineTarget t;

		Actor mo = SpawnPlayerMissile ("MageStaffFX2", angle, pLineTarget:t);
		if (mo)
		{
			mo.target = self;
			if (t.linetarget && !t.unlinked)
				mo.tracer = t.linetarget;
			else
				mo.tracer = alttarget;
		}
	}

	//============================================================================
	//
	// A_MStaffAttack
	//
	//============================================================================

	action void A_MStaffAttack()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		
		// [RH] Let's try and actually track what the player aimed at
		AimLineAttack (angle, PLAYERMISSILERANGE, t, 32.);
		if (t.linetarget == NULL)
		{
			t.linetarget = RoughMonsterSearch(10, true, true);
		}
		MStaffSpawn (angle, t.linetarget);
		MStaffSpawn (angle-5, t.linetarget);
		MStaffSpawn (angle+5, t.linetarget);
		A_StartSound ("MageStaffFire", CHAN_WEAPON);
		invoker.MStaffCount = 3;
	}

	//============================================================================
	//
	// A_MStaffPalette
	//
	//============================================================================

	action void A_MStaffPalette()
	{
		if (invoker.MStaffCount > 0) invoker.MStaffCount--;
	}
}


// Mage Staff FX2 (Bloodscourge) --------------------------------------------

class MageStaffFX2 : Actor
{
	Default
	{
		Speed 17;
		Height 8;
		Damage 4;
		DamageType "Fire";
		Projectile;
		+SEEKERMISSILE
		+SCREENSEEKER
		+EXTREMEDEATH
		DeathSound "MageStaffExplode";
		Obituary "$OB_MPMWEAPBLOODSCOURGE";
	}


	States
	{
	Spawn:
		MSP2 ABCD 2 Bright A_MStaffTrack;
		Loop;
	Death:
		MSP2 E 4 Bright A_SetTranslucent(1,1);
		MSP2 F 5 Bright A_Explode (80, 192, 0);
		MSP2 GH 5 Bright;
		MSP2 I 4 Bright;
		Stop;
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================

	override int SpecialMissileHit (Actor victim)
	{
		if (victim != target && !victim.player && !victim.bBoss)
		{
			victim.DamageMobj (self, target, 10, 'Fire');
			return 1;	// Keep going
		}
		return -1;
	}

	override bool SpecialBlastHandling (Actor source, double strength)
	{
		// Reflect to originator
		tracer = target;	
		target = source;
		return true;
	}

	//============================================================================
	//
	// A_MStaffTrack
	//
	//============================================================================

	void A_MStaffTrack()
	{
		if (tracer == null && random[MStaffTrack]() < 50)
		{
			tracer = RoughMonsterSearch (10, true);
		}
		A_SeekerMissile(2, 10);
	}
}
