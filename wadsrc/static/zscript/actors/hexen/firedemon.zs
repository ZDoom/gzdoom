
// FireDemon ----------------------------------------------------------------

class FireDemon : Actor
{
	const FIREDEMON_ATTACK_RANGE = 64*8.;
	int fdstrafecount;

	Default
	{
		Health 80;
		ReactionTime 8;
		PainChance 1;
		Speed 13;
		Radius 20;
		Height 68;
		Mass 75;
		Damage 1;
		Monster;
		+DROPOFF +NOGRAVITY +FLOAT
		+FLOORCLIP +INVULNERABLE +TELESTOMP
		SeeSound "FireDemonSpawn";
		PainSound "FireDemonPain";
		DeathSound "FireDemonDeath";
		ActiveSound "FireDemonActive";
		Obituary "$OB_FIREDEMON";
		Tag "$FN_FIREDEMON";
	}

	States
	{
	Spawn:
		FDMN X 5 Bright;
		FDMN EFG 10 Bright A_Look;
		Goto Spawn + 1;
	See:
		FDMN E 8 Bright;
		FDMN F 6 Bright;
		FDMN G 5 Bright;
		FDMN F 8 Bright;
		FDMN E 6 Bright;
		FDMN G 7 Bright A_FiredRocks;
		FDMN HI 5 Bright;
		FDMN J 5 Bright A_UnSetInvulnerable;
	Chase:
		FDMN ABC 5 Bright A_FiredChase;
		Loop;
	Pain:
		FDMN D 0 Bright A_UnSetInvulnerable;
		FDMN D 6 Bright A_Pain;
		Goto Chase;
	Missile:
		FDMN K 3 Bright A_FaceTarget;
		FDMN KKK 5 Bright A_FiredAttack;
		Goto Chase;
	Crash:
	XDeath:
		FDMN M 5 A_FaceTarget;
		FDMN N 5 A_NoBlocking;
		FDMN O 5 A_FiredSplotch;
		Stop;
	Death:
		FDMN D 4 Bright A_FaceTarget;
		FDMN L 4 Bright A_Scream;
		FDMN L 4 Bright A_NoBlocking;
		FDMN L 200 Bright;
		Stop;
	Ice:
		FDMN R 5 A_FreezeDeath;
		FDMN R 1 A_FreezeDeathChunks;
		Wait;
	}
	
	


	//============================================================================
	// Fire Demon AI
	//
	// special1			index into floatbob
	// fdstrafecount			whether strafing or not
	//============================================================================

	//============================================================================
	//
	// A_FiredSpawnRock
	//
	//============================================================================

	private void A_FiredSpawnRock ()
	{
		Actor mo;
		class<Actor> rtype;

		switch (random[FireDemonRock](0, 4))
		{
			case 0:
				rtype = "FireDemonRock1";
				break;
			case 1:
				rtype = "FireDemonRock2";
				break;
			case 2:
				rtype = "FireDemonRock3";
				break;
			case 3:
				rtype = "FireDemonRock4";
				break;
			case 4:
			default:
				rtype = "FireDemonRock5";
				break;
		}

		double xo = (random[FireDemonRock]() - 128) / 16.;
		double yo = (random[FireDemonRock]() - 128) / 16.;
		double zo = random[FireDemonRock]() / 32.;
		mo = Spawn (rtype, Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo.target = self;
			mo.Vel.X = (random[FireDemonRock]() - 128) / 64.;
			mo.Vel.Y = (random[FireDemonRock]() - 128) / 64.;
			mo.Vel.Z = (random[FireDemonRock]() / 64.);
			mo.special1 = 2;		// Number bounces
		}

		// Initialize fire demon
		fdstrafecount = 0;
		bJustAttacked = false;
	}

	//============================================================================
	//
	// A_FiredRocks
	//
	//============================================================================
	void A_FiredRocks()
	{
		A_FiredSpawnRock ();
		A_FiredSpawnRock ();
		A_FiredSpawnRock ();
		A_FiredSpawnRock ();
		A_FiredSpawnRock ();
	}

	//============================================================================
	//
	// A_FiredAttack
	//
	//============================================================================

	void A_FiredAttack()
	{
		if (target == null)	return;
		Actor mo = SpawnMissile (target, "FireDemonMissile");
		if (mo) A_StartSound ("FireDemonAttack", CHAN_BODY);
	}

	//============================================================================
	//
	// A_FiredChase
	//
	//============================================================================

	void A_FiredChase()
	{
		int weaveindex = special1;
		double ang;
		double dist;

		if (reactiontime) reactiontime--;
		if (threshold) threshold--;

		// Float up and down
		AddZ(BobSin(weaveindex));
		special1 = (weaveindex + 2) & 63;

		// Ensure it stays above certain height
		if (pos.Z < floorz + 64)
		{
			AddZ(2);
		}

		if(!target || !target.bShootable)
		{	// Invalid target
			LookForPlayers (true);
			return;
		}

		// Strafe
		if (fdstrafecount > 0)
		{
			fdstrafecount--;
		}
		else
		{
			fdstrafecount = 0;
			Vel.X = Vel.Y = 0;
			dist = Distance2D(target);
			if (dist < FIREDEMON_ATTACK_RANGE)
			{
				if (random[FiredChase]() < 30)
				{
					ang = AngleTo(target);
					if (random[FiredChase]() < 128)
						ang += 90;
					else
						ang -= 90;
					Thrust(8, ang);
					fdstrafecount = 3;		// strafe time
				}
			}
		}

		FaceMovementDirection ();

		// Normal movement
		if (!fdstrafecount)
		{
			if (--movecount<0 || !MonsterMove ())
			{
				NewChaseDir ();
			}
		}

		// Do missile attack
		if (!bJustAttacked)
		{
			if (CheckMissileRange () && (random[FiredChase]() < 20))
			{
				SetState (MissileState);
				bJustAttacked = true;
				return;
			}
		}
		else
		{
			bJustAttacked = false;
		}

		// make active sound
		if (random[FiredChase]() < 3)
		{
			PlayActiveSound ();
		}
	}

	//============================================================================
	//
	// A_FiredSplotch
	//
	//============================================================================

	void A_FiredSplotch()
	{
		Actor mo;

		mo = Spawn ("FireDemonSplotch1", Pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.Vel.X = (random[FireDemonSplotch]() - 128) / 32.;
			mo.Vel.Y = (random[FireDemonSplotch]() - 128) / 32.;
			mo.Vel.Z = (random[FireDemonSplotch]() / 64.) + 3;
		}
		mo = Spawn ("FireDemonSplotch2", Pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.Vel.X = (random[FireDemonSplotch]() - 128) / 32.;
			mo.Vel.Y = (random[FireDemonSplotch]() - 128) / 32.;
			mo.Vel.Z = (random[FireDemonSplotch]() / 64.) + 3;
		}
	}
	
}

// FireDemonSplotch1 -------------------------------------------------------

class FireDemonSplotch1 : Actor
{
	Default
	{
		ReactionTime 8;
		Radius 3;
		Height 16;
		Mass 100;
		+DROPOFF +CORPSE
		+NOTELEPORT +FLOORCLIP
	}
	States
	{
	Spawn:
		FDMN P 3;
		FDMN P 6 A_QueueCorpse;
		FDMN Y -1;
		Stop;
	}
}

// FireDemonSplotch2 -------------------------------------------------------

class FireDemonSplotch2 : FireDemonSplotch1
{
	States
	{
	Spawn:
		FDMN Q 3;
		FDMN Q 6 A_QueueCorpse;
		FDMN Z -1;
		Stop;
	}
}

// FireDemonRock1 ------------------------------------------------------------

class FireDemonRock1 : Actor
{
	Default
	{
		ReactionTime 8;
		Radius 3;
		Height 5;
		Mass 16;
		+NOBLOCKMAP +DROPOFF +MISSILE
		+NOTELEPORT
	}

	States
	{
	Spawn:
		FDMN S 4;
		Loop;
	Death:
		FDMN S 5 A_SmBounce;
	XDeath:
		FDMN S 200;
		Stop;
	}
	
	//============================================================================
	//
	// A_SmBounce
	//
	//============================================================================

	void A_SmBounce()
	{
		// give some more velocity (x,y,&z)
		SetZ(floorz + 1);
		Vel.Z = 2. + random[SMBounce]() / 64.;
		Vel.X = random[SMBounce](0, 2);
		Vel.Y = random[SMBounce](0, 2);
	}
}

// FireDemonRock2 ------------------------------------------------------------

class FireDemonRock2 : FireDemonRock1
{
	States
	{
	Spawn:
		FDMN T 4;
		Loop;
	Death:
		FDMN T 5 A_SmBounce;
	XDeath:
		FDMN T 200;
		Stop;
	}
}

// FireDemonRock3 ------------------------------------------------------------

class FireDemonRock3 : FireDemonRock1
{
	States
	{
	Spawn:
		FDMN U 4;
		Loop;
	Death:
		FDMN U 5 A_SmBounce;
	XDeath:
		FDMN U 200;
		Stop;
	}
}

// FireDemonRock4 ------------------------------------------------------------

class FireDemonRock4 : FireDemonRock1
{
	States
	{
	Spawn:
		FDMN V 4;
		Loop;
	Death:
		FDMN V 5 A_SmBounce;
	XDeath:
		FDMN V 200;
		Stop;
	}
}

// FireDemonRock5 ------------------------------------------------------------

class FireDemonRock5 : FireDemonRock1
{
	States
	{
	Spawn:
		FDMN W 4;
		Loop;
	Death:
		FDMN W 5 A_SmBounce;
	XDeath:
		FDMN W 200;
		Stop;
	}
}

// FireDemonMissile -----------------------------------------------------------

class FireDemonMissile : Actor
{
	Default
	{
		ReactionTime 8;
		Speed 10;
		Radius 10;
		Height 6;
		Mass 5;
		Damage 1;
		DamageType "Fire";
		Projectile;
		RenderStyle "Add";
		DeathSound "FireDemonMissileHit";
		+ZDOOMTRANS
	}
	States
	{
	Spawn:
		FDMB A 5 Bright;
		Loop;
	Death:
		FDMB BCDE 5 Bright;
		Stop;
	}
}
