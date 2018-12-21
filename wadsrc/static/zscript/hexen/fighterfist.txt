
// Fist (first weapon) ------------------------------------------------------

class FWeapFist : FighterWeapon
{
	Default
	{
		+BLOODSPLATTER
		Weapon.SelectionOrder 3400;
		+WEAPON.MELEEWEAPON
		Weapon.KickBack 150;
		Obituary "$OB_MPFWEAPFIST";
		Tag "$TAG_FWEAPFIST";
	}

	States
	{
	Select:
		FPCH A 1 A_Raise;
		Loop;
	Deselect:
		FPCH A 1 A_Lower;
		Loop;
	Ready:
		FPCH A 1 A_WeaponReady;
		Loop;
	Fire:
		FPCH B 5 Offset (5, 40);
		FPCH C 4 Offset (5, 40);
		FPCH D 4 Offset (5, 40) A_FPunchAttack;
		FPCH C 4 Offset (5, 40);
		FPCH B 5 Offset (5, 40) A_ReFire;
		Goto Ready;
	Fire2:
		FPCH DE 4 Offset (5, 40);
		FPCH E 1 Offset (15, 50);
		FPCH E 1 Offset (25, 60);
		FPCH E 1 Offset (35, 70);
		FPCH E 1 Offset (45, 80);
		FPCH E 1 Offset (55, 90);
		FPCH E 1 Offset (65, 90);
		FPCH E 10 Offset (0, 150);
		Goto Ready;
	}
	
	//============================================================================
	//
	// TryPunch
	//
	// Returns true if an actor was punched, false if not.
	//
	//============================================================================

	private action bool TryPunch(double angle, int damage, int power)
	{
		Class<Actor> pufftype;
		FTranslatedLineTarget t;

		double slope = AimLineAttack (angle, 2*DEFMELEERANGE, t, 0., ALF_CHECK3D);
		if (t.linetarget != null)
		{
			if (++weaponspecial >= 3)
			{
				damage <<= 1;
				power *= 3;
				pufftype = "HammerPuff";
			}
			else
			{
				pufftype = "PunchPuff";
			}
			LineAttack (angle, 2*DEFMELEERANGE, slope, damage, 'Melee', pufftype, true, t);
			if (t.linetarget != null)
			{
				// The mass threshold has been changed to CommanderKeen's value which has been used most often for 'unmovable' stuff.
				if (t.linetarget.player != null || 
					(t.linetarget.Mass < 10000000 && (t.linetarget.bIsMonster)))
				{
					if (!t.linetarget.bDontThrust)
						t.linetarget.Thrust(power, t.attackAngleFromSource);
				}
				AdjustPlayerAngle(t);
				return true;
			}
		}
		return false;
	}

	//============================================================================
	//
	// A_FPunchAttack
	//
	//============================================================================

	action void A_FPunchAttack()
	{
		if (player == null)
		{
			return;
		}

		int damage = random[FighterAtk](40, 55);
		for (int i = 0; i < 16; i++)
		{
			if (TryPunch(angle + i*(45./16), damage, 2) ||
				TryPunch(angle - i*(45./16), damage, 2))
			{ // hit something
				if (weaponspecial >= 3)
				{
					weaponspecial = 0;
					player.SetPsprite(PSP_WEAPON, player.ReadyWeapon.FindState("Fire2"));
					A_PlaySound ("*fistgrunt", CHAN_VOICE);
				}
				return;
			}
		}
		// didn't find any creatures, so try to strike any walls
		weaponspecial = 0;

		double slope = AimLineAttack (angle, DEFMELEERANGE, null, 0., ALF_CHECK3D);
		LineAttack (angle, DEFMELEERANGE, slope, damage, 'Melee', "PunchPuff", true);
	}
	
}

// Punch puff ---------------------------------------------------------------

class PunchPuff : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		+PUFFONACTORS
		RenderStyle "Translucent";
		Alpha 0.6;
		SeeSound "FighterPunchHitThing";
		AttackSound "FighterPunchHitWall";
		ActiveSound "FighterPunchMiss";
		VSpeed 1;
	}
	States
	{
	Spawn:
		FHFX STUVW 4;
		Stop;
	}
}
