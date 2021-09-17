// --------------------------------------------------------------------------
//
// Fist 
//
// --------------------------------------------------------------------------

class Fist : Weapon
{
	Default
	{
		Weapon.SelectionOrder 3700;
		Weapon.Kickback 100;
		Obituary "$OB_MPFIST";
		Tag "$TAG_FIST";
		+WEAPON.WIMPY_WEAPON
		+WEAPON.MELEEWEAPON
		+WEAPON.NOAUTOSWITCHTO
	}
	States
	{
	Ready:
		PUNG A 1 A_WeaponReady;
		Loop;
	Deselect:
		PUNG A 1 A_Lower;
		Loop;
	Select:
		PUNG A 1 A_Raise;
		Loop;
	Fire:
		PUNG B 4;
		PUNG C 4 A_Punch;
		PUNG D 5;
		PUNG C 4;
		PUNG B 5 A_ReFire;
		Goto Ready;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{	
	action void A_Punch()
	{
		FTranslatedLineTarget t;

		if (player != null)
		{
			Weapon weap = player.ReadyWeapon;
			if (weap != null && !weap.bDehAmmo && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				if (!weap.DepleteAmmo (weap.bAltFire))
					return;
			}
		}

		int damage = random[Punch](1, 10) << 1;

		if (FindInventory("PowerStrength"))
			damage *= 10;

		double ang = angle + Random2[Punch]() * (5.625 / 256);
		double pitch = AimLineAttack (ang, MeleeRange + MELEEDELTA, null, 0., ALF_CHECK3D);

		LineAttack (ang, MeleeRange, pitch, damage, 'Melee', "BulletPuff", LAF_ISMELEEATTACK, t);

		// turn to face target
		if (t.linetarget)
		{
			A_StartSound ("*fist", CHAN_WEAPON);
			angle = t.angleFromSource;
		}
	}

}