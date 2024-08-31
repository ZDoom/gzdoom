
// Bishop -------------------------------------------------------------------

class Bishop : Actor
{
	int missilecount;
	int bobstate;
	
	Default
	{
		Health 130;
		Radius 22;
		Height 65;
		Speed 10;
		PainChance 110;
		Monster;
		+FLOAT +NOGRAVITY +NOBLOOD
		+TELESTOMP
		+DONTOVERLAP
		+NOTARGETSWITCH
		SeeSound "BishopSight";
		AttackSound "BishopAttack";
		PainSound "BishopPain";
		DeathSound "BishopDeath";
		ActiveSound "BishopActiveSounds";
		Obituary"$OB_BISHOP";
		Tag "$FN_BISHOP";
	}

	States
	{
	Spawn:
		BISH A 10 A_Look;
		Loop;
	See:
		BISH A 2 A_Chase;
		BISH A 2 A_BishopChase;
		BISH A 2;
		BISH B 2 A_BishopChase;
		BISH B 2 A_Chase;
		BISH B 2 A_BishopChase;
		BISH A 1 A_BishopDecide;
		Loop;
	Blur:
		BISH A 2 A_BishopDoBlur;
		BISH A 4 A_BishopSpawnBlur;
		Wait;
	Pain:
		BISH C 6 A_Pain;
		BISH CCC 6 A_BishopPainBlur;
		BISH C 0;
		Goto See;
	Missile:
		BISH A 3 A_FaceTarget;
		BISH DE 3 A_FaceTarget;
		BISH F 3 A_BishopAttack;
		BISH F 5 A_BishopAttack2;
		Wait;
	Death:
		BISH G 6;
		BISH H 6 Bright A_Scream;
		BISH I 5 Bright A_NoBlocking;
		BISH J 5 BRIGHT A_Explode(random[BishopBoom](25,40));
		BISH K 5 Bright;
		BISH LM 4 Bright;
		BISH N 4 A_SpawnItemEx("BishopPuff", 0,0,40, 0,0,0.5);
		BISH O 4 A_QueueCorpse;
		BISH P -1;
		Stop;
	Ice:
		BISH X 5 A_FreezeDeath;
		BISH X 1 A_FreezeDeathChunks;
		Wait;
	}
	
	

	//============================================================================
	//
	// A_BishopAttack
	//
	//============================================================================

	void A_BishopAttack()
	{
		let targ = target;
		if (!targ)
		{
			return;
		}
		A_StartSound (AttackSound, CHAN_BODY);
		if (CheckMeleeRange())
		{
			int damage = random[BishopAttack](1, 8) * 4;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}
		missilecount = random[BishopAttack](5, 8);
	}

	//============================================================================
	//
	// A_BishopAttack2
	//
	//		Spawns one of a string of bishop missiles
	//============================================================================

	void A_BishopAttack2()
	{
		if (!target || !missilecount)
		{
			missilecount = 0;
			SetState (SeeState);
			return;
		}
		Actor mo = SpawnMissile (target, "BishopFX");
		if (mo != null)
		{
			mo.tracer = target;
		}
		missilecount--;
		return;
	}

	//============================================================================
	//
	// A_BishopDecide
	//
	//============================================================================

	void A_BishopDecide()
	{
		if (random[BishopDecide]() >= 220)
		{
			SetStateLabel ("Blur");
		}
	}

	//============================================================================
	//
	// A_BishopDoBlur
	//
	//============================================================================

	void A_BishopDoBlur()
	{
		missilecount = random[BishopDoBlur](3, 6); // Random number of blurs
		if (random[BishopDoBlur]() < 120)
		{
			Thrust(11, Angle + 90);
		}
		else if (random[BishopDoBlur]() > 125)
		{
			Thrust(11, Angle - 90);
		}
		else
		{ // Thrust forward
			Thrust(11);
		}
		A_StartSound ("BishopBlur", CHAN_BODY);
	}

	//============================================================================
	//
	// A_BishopSpawnBlur
	//
	//============================================================================

	void A_BishopSpawnBlur()
	{
		if (!--missilecount)
		{
			Vel.XY = (0,0);// = Vel.Y = 0;
			if (random[BishopSpawnBlur]() > 96)
			{
				SetState (SeeState);
			}
			else
			{
				SetState (MissileState);
			}
		}
		Actor mo = Spawn ("BishopBlur", Pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.angle = angle;
		}
	}

	//============================================================================
	//
	// A_BishopChase
	//
	//============================================================================

	void A_BishopChase()
	{
		double newz = pos.z - BobSin(bobstate) / 2.;
		bobstate = (bobstate + 4) & 63;
		newz += BobSin(bobstate) / 2.;
		SetZ(newz);
	}

	//============================================================================
	//
	// A_BishopPainBlur
	//
	//============================================================================

	void A_BishopPainBlur()
	{
		if (random[BishopPainBlur]() < 64)
		{
			SetStateLabel ("Blur");
			return;
		}
		double xo = random2[BishopPainBlur]() / 16.;
		double yo = random2[BishopPainBlue]() / 16.;
		double zo = random2[BishopPainBlue]() / 32.;
		Actor mo = Spawn ("BishopPainBlur", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo.angle = angle;
		}
	}
	
}

extend class Actor
{
	//============================================================================
	//
	// A_BishopMissileWeave (this function must be in Actor)
	//
	//============================================================================

	void A_BishopMissileWeave()
	{
		A_Weave(2, 2, 2., 1.);
	}
}

// Bishop puff --------------------------------------------------------------

class BishopPuff : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.6;
	}
	States
	{
	Spawn:
		BISH QRST 5;
		BISH UV 6;
		BISH W 5;
		Stop;
	}
}

// Bishop pain blur ---------------------------------------------------------

class BishopPainBlur : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.6;
	}
	States
	{
	Spawn:
		BISH C 8;
		Stop;
	}
}

// Bishop FX ----------------------------------------------------------------

class BishopFX : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 10;
		Damage 1;
		Projectile;
		+SEEKERMISSILE
		-ACTIVATEIMPACT -ACTIVATEPCROSS
		+STRIFEDAMAGE +ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "BishopMissileExplode";
	}
	States
	{
	Spawn:
		BPFX ABAB 1 Bright A_BishopMissileWeave;
		BPFX B 0 Bright A_SeekerMissile(2,3);
		Loop;
	Death:
		BPFX CDEF 4 Bright;
		BPFX GH 3 Bright;
		Stop;
	}
}

// Bishop blur --------------------------------------------------------------

class BishopBlur : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.6;
	}
	States
	{
	Spawn:
		BISH A 16;
		BISH A 8 A_SetTranslucent(0.4);
		Stop;
	}
}

