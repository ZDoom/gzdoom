
// Wizard --------------------------------------------------------

class Wizard : Actor
{
	Default
	{
		Health 180;
		Radius 16;
		Height 68;
		Mass 100;
		Speed 12;
		Painchance 64;
		Monster;
		+FLOAT
		+NOGRAVITY
		+DONTOVERLAP
		SeeSound "wizard/sight";
		AttackSound "wizard/attack";
		PainSound "wizard/pain";
		DeathSound "wizard/death";
		ActiveSound "wizard/active";
		Obituary "$OB_WIZARD";
		HitObituary "$OB_WIZARDHIT";
		Tag "$FN_WIZARD";
		DropItem "BlasterAmmo", 84, 10;
		DropItem "ArtiTomeOfPower", 4, 0;
	}

	States
	{
	Spawn:
		WZRD AB 10 A_Look;
		Loop;
	See:
		WZRD A 3 A_Chase;
		WZRD A 4 A_Chase;
		WZRD A 3 A_Chase;
		WZRD A 4 A_Chase;
		WZRD B 3 A_Chase;
		WZRD B 4 A_Chase;
		WZRD B 3 A_Chase;
		WZRD B 4 A_Chase;
		Loop;
	Missile:
		WZRD C 4 A_WizAtk1;
		WZRD C 4 A_WizAtk2;
		WZRD C 4 A_WizAtk1;
		WZRD C 4 A_WizAtk2;
		WZRD C 4 A_WizAtk1;
		WZRD C 4 A_WizAtk2;
		WZRD C 4 A_WizAtk1;
		WZRD C 4 A_WizAtk2;
		WZRD D 12 A_WizAtk3;
		Goto See;
	Pain:
		WZRD E 3 A_GhostOff;
		WZRD E 3 A_Pain;
		Goto See;
	Death:
		WZRD F 6 A_GhostOff;
		WZRD G 6 A_Scream;
		WZRD HI 6;
		WZRD J 6 A_NoBlocking;
		WZRD KL 6;
		WZRD M -1 A_SetFloorClip;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_GhostOff
	//
	//----------------------------------------------------------------------------

	void A_GhostOff ()
	{
		A_SetRenderStyle(1.0, STYLE_Normal);
		bGhost = false;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_WizAtk1
	//
	//----------------------------------------------------------------------------

	void A_WizAtk1 ()
	{
		A_FaceTarget ();
		A_GhostOff();
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_WizAtk2
	//
	//----------------------------------------------------------------------------

	void A_WizAtk2 ()
	{
		A_FaceTarget ();
		A_SetRenderStyle(HR_SHADOW, STYLE_Translucent);
		bGhost = true;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_WizAtk3
	//
	//----------------------------------------------------------------------------

	void A_WizAtk3 ()
	{
		A_GhostOff();
		let targ = target;
		if (!targ) return;
		A_PlaySound (AttackSound, CHAN_WEAPON);
		if (CheckMeleeRange())
		{
			int damage = random[WizAtk3](1, 8) * 4;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}
		Actor mo = SpawnMissile (targ, "WizardFX1");
		if (mo != null)
		{
			SpawnMissileAngle("WizardFX1", mo.Angle - 45. / 8, mo.Vel.Z);
			SpawnMissileAngle("WizardFX1", mo.Angle + 45. / 8, mo.Vel.Z);
		}
	}
	
}

// Projectile --------------------------------------------------------

class WizardFX1 : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 18;
		FastSpeed 24;
		Damage 3;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX11 AB 6 BRIGHT;
		Loop;
	Death:
		FX11 CDEFG 5 BRIGHT;
		Stop;
	}
}


