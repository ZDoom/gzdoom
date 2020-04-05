
// Knight -------------------------------------------------------------------

class Knight : Actor
{
	Default
	{
		Health 200;
		Radius 24;
		Height 78;
		Mass 150;
		Speed 12;
		Painchance 100;
		Monster;
		+FLOORCLIP
		SeeSound "hknight/sight";
		AttackSound "hknight/attack";
		PainSound "hknight/pain";
		DeathSound "hknight/death";
		ActiveSound "hknight/active";
		Obituary "$OB_BONEKNIGHT";
		HitObituary "$OB_BONEKNIGHTHIT";
		Tag "$FN_BONEKNIGHT";
		DropItem "CrossbowAmmo", 84, 5;
	}
	
	States
	{
	Spawn:
		KNIG AB 10 A_Look;
		Loop;
	See:
		KNIG ABCD 4 A_Chase;
		Loop;
	Melee:
	Missile:
		KNIG E 10 A_FaceTarget;
		KNIG F 8 A_FaceTarget;
		KNIG G 8 A_KnightAttack;
		KNIG E 10 A_FaceTarget;
		KNIG F 8 A_FaceTarget;
		KNIG G 8 A_KnightAttack;
		Goto See;
	Pain:
		KNIG H 3;
		KNIG H 3 A_Pain;
		Goto See;
	Death:
		KNIG I 6;
		KNIG J 6 A_Scream;
		KNIG K 6;
		KNIG L 6 A_NoBlocking;
		KNIG MN 6;
		KNIG O -1;
		Stop;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_KnightAttack
	//
	//----------------------------------------------------------------------------

	void A_KnightAttack ()
	{
		let targ = target;
		if (!targ) return;
		if (CheckMeleeRange ())
		{
			int damage = random[KnightAttack](1, 8) * 3;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			A_StartSound ("hknight/melee", CHAN_BODY);
			return;
		}
		// Throw axe
		A_StartSound (AttackSound, CHAN_BODY);
		if (self.bShadow || random[KnightAttack]() < 40)
		{ // Red axe
			SpawnMissileZ (pos.Z + 36, targ, "RedAxe");
		}
		else
		{ // Green axe
			SpawnMissileZ (pos.Z + 36, targ, "KnightAxe");
		}
	}
}


// Knight ghost -------------------------------------------------------------

class KnightGhost : Knight
{
	Default
	{
		+SHADOW
		+GHOST
		RenderStyle "Translucent";
		Alpha 0.4;
	}
}

// Knight axe ---------------------------------------------------------------

class KnightAxe : Actor 
{
	Default
	{
		Radius 10;
		Height 8;
		Speed 9;
		FastSpeed 18;
		Damage 2;
		Projectile;
		-NOBLOCKMAP
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+WINDTHRUST
		+THRUGHOST
		DeathSound "hknight/hit";
	}

	States
	{
	Spawn:
		SPAX A 3 BRIGHT A_StartSound("hknight/axewhoosh");
		SPAX BC 3 BRIGHT;
		Loop;
	Death:
		SPAX DEF 6 BRIGHT;
		Stop;
	}
}


// Red axe ------------------------------------------------------------------

class RedAxe : KnightAxe
{
	Default
	{
		+NOBLOCKMAP
		-WINDTHRUST
		Damage 7;
	}

	States
	{
	Spawn:
		RAXE AB 5 BRIGHT A_DripBlood;
		Loop;
	Death:
		RAXE CDE 6 BRIGHT;
		Stop;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_DripBlood
	//
	//----------------------------------------------------------------------------
	
	void A_DripBlood ()
	{
		double xo = random2[DripBlood]() / 32.0;
		double yo = random2[DripBlood]() / 32.0;
		Actor mo = Spawn ("Blood", Vec3Offset(xo, yo, 0.), ALLOW_REPLACE);
		if (mo != null)
		{
			mo.Vel.X = random2[DripBlood]() / 64.0;
			mo.Vel.Y = random2[DripBlood]() / 64.0;
			mo.Gravity = 1./8;
		}
	}
}
