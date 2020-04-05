// Strife's Crossbow --------------------------------------------------------

class StrifeCrossbow : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 1200;
		+WEAPON.NOALERT
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 8;
		Weapon.AmmoType1 "ElectricBolts";
		Weapon.SisterWeapon "StrifeCrossbow2";
		Inventory.PickupMessage "$TXT_STRIFECROSSBOW";
		Tag "$TAG_STRIFECROSSBOW1";
		Inventory.Icon "CBOWA0";
	}
	
	States
	{
	Spawn:
		CBOW A -1;
		Stop;
	Ready:
		XBOW A 0 A_ShowElectricFlash;
		XBOW A 1 A_WeaponReady;
		Wait;
	Deselect:
		XBOW A 1 A_Lower;
		Loop;
	Select:
		XBOW A 1 A_Raise;
		Loop;
	Fire:
		XBOW A 3 A_ClearFlash;
		XBOW B 6 A_FireArrow("ElectricBolt");
		XBOW C 4;
		XBOW D 6;
		XBOW E 3;
		XBOW F 5;
		XBOW G 0 A_ShowElectricFlash;
		XBOW G 5 A_CheckReload;
		Goto Ready+1;
	Flash:
		XBOW KLM 5;
		Loop;
	}
	
	//============================================================================
	//
	// A_ClearFlash
	//
	//============================================================================

	action void A_ClearFlash ()
	{
		if (player == null)
			return;

		player.SetPsprite (PSP_FLASH, null);
	}

	//============================================================================
	//
	// A_ShowElectricFlash
	//
	//============================================================================

	action void A_ShowElectricFlash ()
	{
		if (player != null)
		{
			player.SetPsprite (PSP_FLASH, player.ReadyWeapon.FindState('Flash'));
		}
	}

	//============================================================================
	//
	// A_FireElectric
	//
	//============================================================================

	action void A_FireArrow (class<Actor> proj)
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
		if (proj) 
		{
			double savedangle = angle;
			angle += Random2[Electric]() * (5.625/256) * AccuracyFactor();
			player.mo.PlayAttacking2 ();
			SpawnPlayerMissile (proj);
			angle = savedangle;
			A_StartSound ("weapons/xbowshoot", CHAN_WEAPON);
		}
	}
}


class StrifeCrossbow2 : StrifeCrossbow
{
	Default
	{
		Weapon.SelectionOrder 2700;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 0;
		Weapon.AmmoType1 "PoisonBolts";
		Weapon.SisterWeapon "StrifeCrossbow";
		Tag "$TAG_STRIFECROSSBOW2";
	}
	States
	{
	Ready:
		XBOW H 1 A_WeaponReady;
		Loop;
	Deselect:
		XBOW H 1 A_Lower;
		Loop;
	Select:
		XBOW H 1 A_Raise;
		Loop;
	Fire:
		XBOW H 3;
		XBOW B 6 A_FireArrow("PoisonBolt");
		XBOW C 4;
		XBOW D 6;
		XBOW E 3;
		XBOW I 5;
		XBOW J 5 A_CheckReload;
		Goto Ready;
	Flash:
		Stop;
	}
}

// Electric Bolt ------------------------------------------------------------

class ElectricBolt : Actor
{
	Default
	{
		Speed 30;
		Radius 10;
		Height 10;
		Damage 10;
		Projectile;
		+STRIFEDAMAGE
		+NOBLOCKMAP
		+NOGRAVITY
		+DROPOFF
		MaxStepHeight 4;
		SeeSound "misc/swish";
		ActiveSound "misc/swish";
		DeathSound "weapons/xbowhit";
		Obituary "$OB_MPELECTRICBOLT";
	}
	States
	{
	Spawn:
		AROW A 10 A_LoopActiveSound;
		Loop;
	Death:
		ZAP1 A 3 A_AlertMonsters;
		ZAP1 BCDEFE 3;
		ZAP1 DCB 2;
		ZAP1 A 1;
		Stop;
	}
}


// Poison Bolt --------------------------------------------------------------

class PoisonBolt : Actor
{
	Default
	{
		Speed 30;
		Radius 10;
		Height 10;
		Damage 500;
		Projectile;
		+STRIFEDAMAGE
		MaxStepHeight 4;
		SeeSound "misc/swish";
		ActiveSound "misc/swish";
		Obituary "$OB_MPPOISONBOLT";
	}
	States
	{
	Spawn:
		ARWP A 10 A_LoopActiveSound;
		Loop;
	Death:
		AROW A 1;
		Stop;
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.bNoBlood)
		{
			return -1;
		}
		if (target.health < 1000000)
		{
			if (!target.bBoss)			
				return target.health + 10;
			else
				return 50;
		}
		return 1;
	}

	
}


