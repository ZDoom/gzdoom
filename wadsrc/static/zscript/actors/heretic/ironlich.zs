
// Ironlich -----------------------------------------------------------------

class Ironlich : Actor
{
	Default
	{
		Health 700;
		Radius 40;
		Height 72;
		Mass 325;
		Speed 6;
		Painchance 32;
		Monster;
		+NOBLOOD
		+DONTMORPH
		+DONTSQUASH
		+BOSSDEATH
		SeeSound "ironlich/sight";
		AttackSound "ironlich/attack";
		PainSound "ironlich/pain";
		DeathSound "ironlich/death";
		ActiveSound "ironlich/active";
		Obituary "$OB_IRONLICH";
		HitObituary "$OB_IRONLICHHIT";
		Tag "$FN_IRONLICH";
		DropItem "BlasterAmmo", 84, 10;
		DropItem "ArtiEgg", 51, 0;
	}

	
	States
	{
	Spawn:
		LICH A 10 A_Look;
		Loop;
	See:
		LICH A 4 A_Chase;
		Loop;
	Missile:
		LICH A 5 A_FaceTarget;
		LICH B 20 A_LichAttack;
		Goto See;
	Pain:
		LICH A 4;
		LICH A 4 A_Pain;
		Goto See;
	Death:
		LICH C 7;
		LICH D 7 A_Scream;
		LICH EF 7;
		LICH G 7 A_NoBlocking;
		LICH H 7;
		LICH I -1 A_BossDeath;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_LichAttack
	//
	//----------------------------------------------------------------------------

	void A_LichAttack ()
	{
		static const int atkResolve1[] = { 50, 150 };
		static const int atkResolve2[] = { 150, 200 };

		// Ice ball		(close 20% : far 60%)
		// Fire column	(close 40% : far 20%)
		// Whirlwind	(close 40% : far 20%)
		// Distance threshold = 8 cells

		let targ = target;
		if (targ == null)
		{
			return;
		}
		A_FaceTarget ();
		if (CheckMeleeRange ())
		{
			int damage = random[LichAttack](1, 8) * 6;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}
		int dist = Distance2D(targ) > 8 * 64;
		int randAttack = random[LichAttack]();
		if (randAttack < atkResolve1[dist])
		{ // Ice ball
			SpawnMissile (targ, "HeadFX1");
			A_StartSound ("ironlich/attack2", CHAN_BODY);
		}
		else if (randAttack < atkResolve2[dist])
		{ // Fire column
			Actor baseFire = SpawnMissile (targ, "HeadFX3");
			if (baseFire != null)
			{
				baseFire.SetStateLabel("NoGrow");
				for (int i = 0; i < 5; i++)
				{
					Actor fire = Spawn("HeadFX3", baseFire.Pos, ALLOW_REPLACE);
					if (i == 0)
					{
						A_StartSound ("ironlich/attack1", CHAN_BODY);
					}
					if (fire != null)
					{
						fire.target = baseFire.target;
						fire.angle = baseFire.angle;
						fire.Vel = baseFire.Vel;
						fire.SetDamage(0);
						fire.health = (i+1) * 2;
						fire.CheckMissileSpawn (radius);
					}
				}
			}
		}
		else
		{ // Whirlwind
			Actor mo = SpawnMissile (targ, "Whirlwind");
			if (mo != null)
			{
				mo.AddZ(-32);
				mo.tracer = targ;
				mo.health = 20*TICRATE; // Duration
				A_StartSound ("ironlich/attack3", CHAN_BODY);
			}
		}
	}
	
}

// Head FX 1 ----------------------------------------------------------------

class HeadFX1 : Actor
{
	Default
	{
		Radius 12;
		Height 6;
		Speed 13;
		FastSpeed 20;
		Damage 1;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+THRUGHOST
		+ZDOOMTRANS
		RenderStyle "Add";
	}


	States
	{
	Spawn:
		FX05 ABC 6 BRIGHT;
		Loop;
	Death:
		FX05 D 5 BRIGHT A_LichIceImpact;
		FX05 EFG 5 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_LichIceImpact
	//
	//----------------------------------------------------------------------------

	void A_LichIceImpact()
	{
		for (int i = 0; i < 8; i++)
		{
			Actor shard = Spawn("HeadFX2", Pos, ALLOW_REPLACE);
			if (shard != null)
			{
				shard.target = target;
				shard.angle = i*45.;
				shard.VelFromAngle();
				shard.Vel.Z = -.6;
				shard.CheckMissileSpawn (radius);
			}
		}
	}
}

// Head FX 2 ----------------------------------------------------------------

class HeadFX2 : Actor
{
	Default
	{
		Radius 12;
		Height 6;
		Speed 8;
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
		FX05 HIJ 6 BRIGHT;
		Loop;
	Death:
		FX05 DEFG 5 BRIGHT;
		Stop;
	}
}


// Head FX 3 ----------------------------------------------------------------

class HeadFX3 : Actor
{
	Default
	{
		Radius 14;
		Height 12;
		Speed 10;
		FastSpeed 18;
		Damage 5;
		Projectile;
		+WINDTHRUST	
		+ZDOOMTRANS
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		-NOBLOCKMAP
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX06 ABC 4 BRIGHT A_LichFireGrow;
		Loop;
	NoGrow:
		FX06 ABC 5 BRIGHT;
		Loop;
	Death:
		FX06 DEFG 5 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_LichFireGrow
	//
	//----------------------------------------------------------------------------

	void A_LichFireGrow ()
	{
		health--;
		AddZ(9.);
		if (health == 0)
		{
			RestoreDamage();
			SetStateLabel("NoGrow");
		}
	}
}


// Whirlwind ----------------------------------------------------------------

class Whirlwind : Actor
{
	Default
	{
		Radius 16;
		Height 74;
		Speed 10;
		Damage 1;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEMCROSS
		+SEEKERMISSILE
		+EXPLOCOUNT
		+StepMissile
		RenderStyle "Translucent";
		DefThreshold 60;
		Threshold 50;
		Alpha 0.4;
	}

	States
	{
	Spawn:
		FX07 DEFG 3;
		FX07 ABC 3 A_WhirlwindSeek;
		Goto Spawn+4;
	Death:
		FX07 GFED 4;
		Stop;
	}
	
	override int DoSpecialDamage (Actor victim, int damage, Name damagetype)
	{
		int randVal;

		if (!victim.bDontThrust)
		{
			victim.angle += Random2[WhirlwindDamage]() * (360 / 4096.);
			victim.Vel.X += Random2[WhirlwindDamage]() / 64.;
			victim.Vel.Y += Random2[WhirlwindDamage]() / 64.;
		}

		if ((Level.maptime & 16) && !victim.bBoss && !victim.bDontThrust)
		{
			randVal = min(160, random[WhirlwindSeek]());
			victim.Vel.Z += randVal / 32.;
			if (victim.Vel.Z > 12)
			{
				victim.Vel.Z = 12;
			}
		}
		if (!(Level.maptime & 7))
		{
			victim.DamageMobj (null, target, 3, 'Melee');
		}
		return -1;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_WhirlwindSeek
	//
	//----------------------------------------------------------------------------

	void A_WhirlwindSeek()
	{

		health -= 3;
		if (health < 0)
		{
			Vel = (0,0,0);
			SetStateLabel("Death");
			bMissile = false;
			return;
		}
		if ((threshold -= 3) < 0)
		{
			threshold = random[WhirlwindSeek](58, 89);
			A_StartSound("ironlich/attack3", CHAN_BODY);
		}
		if (tracer && tracer.bShadow)
		{
			return;
		}
		A_SeekerMissile(10, 30);
	}
	
}



