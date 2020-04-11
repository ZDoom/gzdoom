
// The Fighter's Axe --------------------------------------------------------

class FWeapAxe : FighterWeapon
{
	const AXERANGE = (2.25 * DEFMELEERANGE);

	Default
	{
		Weapon.SelectionOrder 1500;
		+WEAPON.AXEBLOOD +WEAPON.AMMO_OPTIONAL +WEAPON.MELEEWEAPON
		Weapon.AmmoUse1 2;
		Weapon.AmmoGive1 25;
		Weapon.KickBack 150;
		Weapon.YAdjust -12;
		Weapon.AmmoType1 "Mana1";
		Inventory.PickupMessage "$TXT_WEAPON_F2";
		Obituary "$OB_MPFWEAPAXE";
		Tag "$TAG_FWEAPAXE";
	}

	States
	{
	Spawn:
		WFAX A -1;
		Stop;
	Select:
		FAXE A 1 A_FAxeCheckUp;
		Loop;
	Deselect:
		FAXE A 1 A_Lower;
		Loop;
	Ready:
		FAXE A 1 A_FAxeCheckReady;
		Loop;
	Fire:
		FAXE B 4 Offset (15, 32) A_FAxeCheckAtk;
		FAXE C 3 Offset (15, 32);
		FAXE D 2 Offset (15, 32);
		FAXE D 1 Offset (-5, 70) A_FAxeAttack;
	EndAttack:
		FAXE D 2 Offset (-25, 90);
		FAXE E 1 Offset (15, 32);
		FAXE E 2 Offset (10, 54);
		FAXE E 7 Offset (10, 150);
		FAXE A 1 Offset (0, 60) A_ReFire;
		FAXE A 1 Offset (0, 52);
		FAXE A 1 Offset (0, 44);
		FAXE A 1 Offset (0, 36);
		FAXE A 1;
		Goto Ready;
	SelectGlow:
		FAXE L 1 A_FAxeCheckUpG;
		Loop;
	DeselectGlow:
		FAXE L 1 A_Lower;
		Loop;
	ReadyGlow:
		FAXE LLL 1 A_FAxeCheckReadyG;
		FAXE MMM 1 A_FAxeCheckReadyG;
		Loop;
	FireGlow:
		FAXE N 4 Offset (15, 32);
		FAXE O 3 Offset (15, 32);
		FAXE P 2 Offset (15, 32);
		FAXE P 1 Offset (-5, 70) A_FAxeAttack;
		FAXE P 2 Offset (-25, 90);
		FAXE Q 1 Offset (15, 32);
		FAXE Q 2 Offset (10, 54);
		FAXE Q 7 Offset (10, 150);
		FAXE A 1 Offset (0, 60) A_ReFire;
		FAXE A 1 Offset (0, 52);
		FAXE A 1 Offset (0, 44);
		FAXE A 1 Offset (0, 36);
		FAXE A 1;
		Goto ReadyGlow;
	}
	
	override State GetUpState ()
	{
		return Ammo1.Amount ? FindState ("SelectGlow") : Super.GetUpState();
	}

	override State GetDownState ()
	{
		return Ammo1.Amount ? FindState ("DeselectGlow") : Super.GetDownState();
	}

	override State GetReadyState ()
	{
		return Ammo1.Amount ? FindState ("ReadyGlow") : Super.GetReadyState();
	}

	override State GetAtkState (bool hold)
	{
		return Ammo1.Amount ? FindState ("FireGlow") :  Super.GetAtkState(hold);
	}

	
	
	//============================================================================
	//
	// A_FAxeCheckReady
	//
	//============================================================================

	action void A_FAxeCheckReady()
	{
		if (player == null)
		{
			return;
		}
		Weapon w = player.ReadyWeapon;
		if (w.Ammo1 && w.Ammo1.Amount > 0)
		{
			player.SetPsprite(PSP_WEAPON, w.FindState("ReadyGlow"));
		}
		else
		{
			A_WeaponReady();
		}
	}

	//============================================================================
	//
	// A_FAxeCheckReadyG
	//
	//============================================================================

	action void A_FAxeCheckReadyG()
	{
		if (player == null)
		{
			return;
		}
		Weapon w = player.ReadyWeapon;
		if (!w.Ammo1 || w.Ammo1.Amount <= 0)
		{
			player.SetPsprite(PSP_WEAPON, w.FindState("Ready"));
		}
		else
		{
			A_WeaponReady();
		}
	}

	//============================================================================
	//
	// A_FAxeCheckUp
	//
	//============================================================================

	action void A_FAxeCheckUp()
	{
		if (player == null)
		{
			return;
		}
		Weapon w = player.ReadyWeapon;
		if (w.Ammo1 && w.Ammo1.Amount > 0)
		{
			player.SetPsprite(PSP_WEAPON, w.FindState("SelectGlow"));
		}
		else
		{
			A_Raise();
		}
	}

	//============================================================================
	//
	// A_FAxeCheckUpG
	//
	//============================================================================

	action void A_FAxeCheckUpG()
	{
		if (player == null)
		{
			return;
		}
		Weapon w = player.ReadyWeapon;
		if (!w.Ammo1 || w.Ammo1.Amount <= 0)
		{
			player.SetPsprite(PSP_WEAPON, w.FindState("Select"));
		}
		else
		{
			A_Raise();
		}
	}

	//============================================================================
	//
	// A_FAxeCheckAtk
	//
	//============================================================================

	action void A_FAxeCheckAtk()
	{
		if (player == null)
		{
			return;
		}
		Weapon w = player.ReadyWeapon;
		if (w.Ammo1 && w.Ammo1.Amount > 0)
		{
			player.SetPsprite(PSP_WEAPON, w.FindState("FireGlow"));
		}
	}

	//============================================================================
	//
	// A_FAxeAttack
	//
	//============================================================================

	action void A_FAxeAttack()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}

		int damage = random[AxeAtk](40, 55);
		damage += random[AxeAtk](0, 7);
		int power = 0;
		Weapon weapon = player.ReadyWeapon;
		class<Actor> pufftype;
		int usemana;
		if ((usemana = (weapon.Ammo1 && weapon.Ammo1.Amount > 0)))
		{
			damage <<= 1;
			power = 6;
			pufftype = "AxePuffGlow";
		}
		else
		{
			pufftype = "AxePuff";
		}
		for (int i = 0; i < 16; i++)
		{
			for (int j = 1; j >= -1; j -= 2)
			{
				double ang = angle + j*i*(45. / 16);
				double slope = AimLineAttack(ang, AXERANGE, t, 0., ALF_CHECK3D);
				if (t.linetarget)
				{
					LineAttack(ang, AXERANGE, slope, damage, 'Melee', pufftype, true, t);
					if (t.linetarget != null)
					{
						if (t.linetarget.bIsMonster || t.linetarget.player)
						{
							t.linetarget.Thrust(power, t.attackAngleFromSource);
						}
						AdjustPlayerAngle(t);
						
						weapon.DepleteAmmo (weapon.bAltFire, false);

						if ((weapon.Ammo1 == null || weapon.Ammo1.Amount == 0) &&
							(!(weapon.bPrimary_Uses_Both) ||
							  weapon.Ammo2 == null || weapon.Ammo2.Amount == 0))
						{
							player.SetPsprite(PSP_WEAPON, weapon.FindState("EndAttack"));
						}
						return;
					}
				}
			}
		}
		// didn't find any creatures, so try to strike any walls
		self.weaponspecial = 0;

		double slope = AimLineAttack (angle, DEFMELEERANGE, null, 0., ALF_CHECK3D);
		LineAttack (angle, DEFMELEERANGE, slope, damage, 'Melee', pufftype, true);
	}
}

// Axe Puff -----------------------------------------------------------------

class AxePuff : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		+PUFFONACTORS
		RenderStyle "Translucent";
		Alpha 0.6;
		SeeSound "FighterAxeHitThing";
		AttackSound "FighterHammerHitWall";
		ActiveSound "FighterHammerMiss";
	}
	States
	{
	Spawn:
		FHFX STUVW 4;
		Stop;
	}
}

// Glowing Axe Puff ---------------------------------------------------------

class AxePuffGlow : AxePuff
{
	Default
	{
		+PUFFONACTORS
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 1;
	}
	States
	{
	Spawn:
		FAXE RSTUVWX 4 Bright;
		Stop;
	}
}
