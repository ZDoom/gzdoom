// --------------------------------------------------------------------------
//
// Doom weap base class
//
// --------------------------------------------------------------------------

class DoomWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 100;
	}
}



extend class StateProvider
{

	//
	// [RH] A_FireRailgun
	// [TP] This now takes a puff type to retain Skulltag's railgun's ability to pierce armor.
	//
	action void A_FireRailgun(class<Actor> puffType = "BulletPuff", int offset_xy = 0)
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
				player.SetSafeFlash(weap, flash, random[FireRail](0, 1));
			}
			
		}

		int damage = deathmatch ? 100 : 150;
		A_RailAttack(damage, offset_xy, false, pufftype: puffType);	// note that this function handles ammo depletion itself for Dehacked compatibility purposes.
	}

	action void A_FireRailgunLeft()
	{
		A_FireRailgun(offset_xy: -10);
	}

	action void A_FireRailgunRight()
	{
		A_FireRailgun(offset_xy: 10);
	}

	action void A_RailWait() 
	{
		// only here to satisfy old Dehacked patches.
	}

}
