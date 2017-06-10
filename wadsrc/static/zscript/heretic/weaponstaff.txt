// Staff --------------------------------------------------------------------

class Staff : HereticWeapon
{
	Default
	{
		Weapon.SelectionOrder 3800;
		+THRUGHOST
		+WEAPON.WIMPY_WEAPON
		+WEAPON.MELEEWEAPON
		Weapon.sisterweapon "StaffPowered";
		Obituary "$OB_MPSTAFF";
		Tag "$TAG_STAFF";
	}


	States
	{
	Ready:	
		STFF A 1 A_WeaponReady;
		Loop;
	Deselect:
		STFF A 1 A_Lower;
		Loop;
	Select:
		STFF A 1 A_Raise;
		Loop;
	Fire:
		STFF B 6;
		STFF C 8 A_StaffAttack(random[StaffAttack](5, 20), "StaffPuff");
		STFF B 8 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_StaffAttackPL1
	//
	//----------------------------------------------------------------------------

	action void A_StaffAttack (int damage, class<Actor> puff)
	{
		FTranslatedLineTarget t;

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
		double ang = angle + Random2[StaffAtk]() * (5.625 / 256);
		double slope = AimLineAttack (ang, DEFMELEERANGE);
		LineAttack (ang, DEFMELEERANGE, slope, damage, 'Melee', puff, true, t);
		if (t.linetarget)
		{
			//S_StartSound(player.mo, sfx_stfhit);
			// turn to face target
			angle = t.angleFromSource;
		}
	}
}

class StaffPowered : Staff
{
	Default
	{
		Weapon.sisterweapon "Staff";
		Weapon.ReadySound "weapons/staffcrackle";
		+WEAPON.POWERED_UP
		+WEAPON.READYSNDHALF
		+WEAPON.STAFF2_KICKBACK
		Obituary "$OB_MPPSTAFF";
		Tag "$TAG_STAFFP";
	}

	States
	{
	Ready:	
		STFF DEF 4 A_WeaponReady;
		Loop;
	Deselect:
		STFF D 1 A_Lower;
		Loop;
	Select:
		STFF D 1 A_Raise;
		Loop;
	Fire:
		STFF G 6;
		STFF H 8 A_StaffAttack(random[StaffAttack](18, 81), "StaffPuff2");
		STFF G 8 A_ReFire;
		Goto Ready;
	}
}


// Staff puff ---------------------------------------------------------------

class StaffPuff : Actor
{
	Default
	{
		RenderStyle "Translucent";
		Alpha 0.4;
		VSpeed 1;
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		AttackSound "weapons/staffhit";
	}

	States
	{
	Spawn:
		PUF3 A 4 BRIGHT;
		PUF3 BCD 4;
		Stop;
	}
}

// Staff puff 2 -------------------------------------------------------------

class StaffPuff2 : Actor
{
	Default
	{
		RenderStyle "Add";
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		+ZDOOMTRANS
		AttackSound "weapons/staffpowerhit";
	}

	States
	{
	Spawn:
		PUF4 ABCDEF 4 BRIGHT;
		Stop;
	}
}	



