// --------------------------------------------------------------------------
//
// Plasma rifle
//
// --------------------------------------------------------------------------

class PlasmaRifle : DoomWeapon
{
	Default
	{
		Weapon.SelectionOrder 100;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 40;
		Weapon.AmmoType "Cell";
		Inventory.PickupMessage "$GOTPLASMA";
		Tag "$TAG_PLASMARIFLE";
	}
	States
	{
	Ready:
		PLSG A 1 A_WeaponReady;
		Loop;
	Deselect:
		PLSG A 1 A_Lower;
		Loop;
	Select:
		PLSG A 1 A_Raise;
		Loop;
	Fire:
		PLSG A 3 A_FirePlasma;
		PLSG B 20 A_ReFire;
		Goto Ready;
	Flash:
		PLSF A 4 Bright A_Light1;
		Goto LightDone;
		PLSF B 4 Bright A_Light1;
		Goto LightDone;
	Spawn:
		PLAS A -1;
		Stop;
	}
}

class PlasmaBall : Actor
{
	Default
	{
		Radius 13;
		Height 8;
		Speed 25;
		Damage 5;
		Projectile;
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 0.75;
		SeeSound "weapons/plasmaf";
		DeathSound "weapons/plasmax";
		Obituary "$OB_MPPLASMARIFLE";
	}
	States
	{
	Spawn:
		PLSS AB 6 Bright;
		Loop;
	Death:
		PLSE ABCDE 4 Bright;
		Stop;
	}
}

// --------------------------------------------------------------------------
//
// BFG 2704
//
// --------------------------------------------------------------------------

class PlasmaBall1 : PlasmaBall
{
	Default
	{
		Damage 4;
		BounceType "Classic";
		BounceFactor 1.0;
		Obituary "$OB_MPBFG_MBF";
	}
	States
	{
	Spawn:
		PLS1 AB 6 Bright;
		Loop;
	Death:
		PLS1 CDEFG 4 Bright;
		Stop;
	}
}
	
class PlasmaBall2 : PlasmaBall1
{
	States
	{
	Spawn:
		PLS2 AB 6 Bright;
		Loop;
	Death:
		PLS2 CDE 4 Bright;
		Stop;
	}
}


//===========================================================================
//
// Code (must be attached to StateProvider)
//
//===========================================================================

extend class StateProvider
{

	//===========================================================================
	//
	// A_FirePlasma
	//
	//===========================================================================

	action void A_FirePlasma()
	{
		if (player == null)
		{
			return;
		}
		Weapon weap = player.ReadyWeapon;
		if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weap.DepleteAmmo (weap.bAltFire, true, 1))
				return;
			
			State flash = weap.FindState('Flash');
			if (flash != null)
			{
				player.SetSafeFlash(weap, flash, random[FirePlasma](0, 1));
			}
			
		}
		
		SpawnPlayerMissile ("PlasmaBall");
	}
}
