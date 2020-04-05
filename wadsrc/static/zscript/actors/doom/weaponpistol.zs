// --------------------------------------------------------------------------
//
// Pistol 
//
// --------------------------------------------------------------------------

class Pistol : DoomWeapon
{
	Default
	{
		Weapon.SelectionOrder 1900;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 20;
		Weapon.AmmoType "Clip";
		Obituary "$OB_MPPISTOL";
		+WEAPON.WIMPY_WEAPON
		Inventory.Pickupmessage "$PICKUP_PISTOL_DROPPED";
		Tag "$TAG_PISTOL";
	}
	States
	{
	Ready:
		PISG A 1 A_WeaponReady;
		Loop;
	Deselect:
		PISG A 1 A_Lower;
		Loop;
	Select:
		PISG A 1 A_Raise;
		Loop;
	Fire:
		PISG A 4;
		PISG B 6 A_FirePistol;
		PISG C 4;
		PISG B 5 A_ReFire;
		Goto Ready;
	Flash:
		PISF A 7 Bright A_Light1;
		Goto LightDone;
		PISF A 7 Bright A_Light1;
		Goto LightDone;
 	Spawn:
		PIST A -1;
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
	// This is also used by the shotgun and chaingun
	//===========================================================================
	
	protected action void GunShot(bool accurate, Class<Actor> pufftype, double pitch)
	{
		int damage = 5 * random[GunShot](1, 3);
		double ang = angle;

		if (!accurate)
		{
			ang += Random2[GunShot]() * (5.625 / 256);
		}

		LineAttack(ang, PLAYERMISSILERANGE, pitch, damage, 'Hitscan', pufftype);
	}
	
	//===========================================================================
	action void A_FirePistol()
	{
		bool accurate;

		if (player != null)
		{
			Weapon weap = player.ReadyWeapon;
			if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				if (!weap.DepleteAmmo (weap.bAltFire, true, 1))
					return;

				player.SetPsprite(PSP_FLASH, weap.FindState('Flash'), true);
			}
			player.mo.PlayAttacking2 ();

			accurate = !player.refire;
		}
		else
		{
			accurate = true;
		}

		A_StartSound ("weapons/pistol", CHAN_WEAPON);
		GunShot (accurate, "BulletPuff", BulletSlope ());
	}
}