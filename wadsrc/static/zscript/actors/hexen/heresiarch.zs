
// The Heresiarch him/itself ------------------------------------------------

//============================================================================
//
//	Sorcerer stuff
//
// Sorcerer Variables
//		specialf1		Angle of ball 1 (all others relative to that)
//		StopBall		which ball to stop at in stop mode (MT_???)
//		args[0]			Defense time
//		args[1]			Number of full rotations since stopping mode
//		args[2]			Target orbit speed for acceleration/deceleration
//		args[3]			Movement mode (see SORC_ macros)
//		args[4]			Current ball orbit speed
//	Sorcerer Ball Variables
//		specialf1		Previous angle of ball (for woosh)
//		special2		Countdown of rapid fire (FX4)
//============================================================================

class Heresiarch : Actor
{
	
	const SORCBALL_INITIAL_SPEED 		= 7;
	const SORCBALL_TERMINAL_SPEED		= 25;
	const SORCBALL_SPEED_ROTATIONS 		= 5;
	const SORC_DEFENSE_TIME				= 255;
	const SORC_DEFENSE_HEIGHT			= 45;
	const BOUNCE_TIME_UNIT				= (35/2);
	const SORCFX4_RAPIDFIRE_TIME		= (6*3);		// 3 seconds
	const SORCFX4_SPREAD_ANGLE			= 20;

	enum ESorc
	{
		SORC_DECELERATE,	
		SORC_ACCELERATE, 
		SORC_STOPPING,	
		SORC_FIRESPELL,	
		SORC_STOPPED,	
		SORC_NORMAL,		
		SORC_FIRING_SPELL
	}

	const BALL1_ANGLEOFFSET	= 0.;
	const BALL2_ANGLEOFFSET	= 120.;
	const BALL3_ANGLEOFFSET	= 240.;

	double BallAngle;
	class<SorcBall> StopBall;
	
	Default
	{
		Health 5000;
		Painchance 10;
		Speed 16;
		Radius 40;
		Height 110;
		Mass 500;
		Damage 9;
		Monster;
		+FLOORCLIP
		+BOSS
		+DONTMORPH
		+NOTARGET
		+NOICEDEATH
		+DEFLECT
		+NOBLOOD
		SeeSound "SorcererSight";
		PainSound "SorcererPain";
		DeathSound "SorcererDeathScream";
		ActiveSound "SorcererActive";
		Obituary "$OB_HERESIARCH";
		Tag "$FN_HERESIARCH";
	}
	
	States
	{
	Spawn:
		SORC A 3;
		SORC A 2 A_SorcSpinBalls;
	Idle:
		SORC A 10 A_Look;
		Wait;
	See:
		SORC ABCD 5 A_Chase;
		Loop;
	Pain:
		SORC G 8;
		SORC G 8 A_Pain;
		Goto See;
	Missile:
		SORC F 6 Bright A_FaceTarget;
		SORC F 6 Bright A_SpeedBalls;
		SORC F 6 Bright A_FaceTarget;
		Wait;
	Attack1:
		SORC E 6 Bright;
		SORC E 6 Bright A_SpawnFizzle;
		SORC E 5 Bright A_FaceTarget;
		Goto Attack1+1;
	Attack2:
		SORC E 2 Bright;
		SORC E 2 Bright A_SorcBossAttack;
		Goto See;
	Death:
		SORC H 5 Bright;
		SORC I 5 Bright A_FaceTarget;
		SORC J 5 Bright A_Scream;
		SORC KLMNOPQRST 5 Bright;
		SORC U 5 Bright A_NoBlocking;
		SORC VWXY 5 Bright;
		SORC Z -1 Bright;
		Stop;
	}

	override void Die (Actor source, Actor inflictor, int dmgflags, Name MeansOfDeath)
	{
		// The heresiarch just executes a script instead of a special upon death
		int script = special;
		special = 0;

		Super.Die (source, inflictor, dmgflags, MeansOfDeath);

		if (script != 0)
		{
			ACS_Execute(script, 0);
		}
	}
	
	//============================================================================
	//
	// A_StopBalls
	//
	// Instant stop when rotation gets to ball in special2
	//		self is sorcerer
	//
	//============================================================================

	void A_StopBalls()
	{
		int chance = random[Heresiarch]();
		args[3] = SORC_STOPPING;				// stopping mode
		args[1] = 0;							// Reset rotation counter

		if ((args[0] <= 0) && (chance < 200))
		{
			StopBall = "SorcBall2";	// Blue
		}
		else if((health < (SpawnHealth() >> 1)) && (chance < 200))
		{
			StopBall = "SorcBall3";	// Green
		}
		else
		{
			StopBall = "SorcBall1";	// Yellow
		}
	}

	//============================================================================
	//
	// A_SorcSpinBalls
	//
	// Spawn spinning balls above head - actor is sorcerer
	//============================================================================

	void A_SorcSpinBalls()
	{
		A_SlowBalls();
		args[0] = 0;								// Currently no defense
		args[3] = SORC_NORMAL;
		args[4] = SORCBALL_INITIAL_SPEED;		// Initial orbit speed
		BallAngle = 1.;

		Vector3 ballpos = (pos.xy, -Floorclip + Height);
		
		Actor mo = Spawn("SorcBall1", pos, NO_REPLACE);
		if (mo)
		{
			mo.target = self;
			mo.special2 = SORCFX4_RAPIDFIRE_TIME;
		}
		mo = Spawn("SorcBall2", pos, NO_REPLACE);
		if (mo) mo.target = self;
		mo = Spawn("SorcBall3", pos, NO_REPLACE);
		if (mo) mo.target = self;
	}


	//============================================================================
	//
	// A_SpeedBalls
	//
	// Set balls to speed mode - self is sorcerer
	//
	//============================================================================

	void A_SpeedBalls()
	{
		args[3] = SORC_ACCELERATE;				// speed mode
		args[2] = SORCBALL_TERMINAL_SPEED;		// target speed
	}


	//============================================================================
	//
	// A_SlowBalls
	//
	// Set balls to slow mode - actor is sorcerer
	//
	//============================================================================

	void A_SlowBalls()
	{
		args[3] = SORC_DECELERATE;				// slow mode
		args[2] = SORCBALL_INITIAL_SPEED;		// target speed
	}

	//============================================================================
	//
	// A_SorcBossAttack
	//
	// Resume ball spinning
	//
	//============================================================================

	void A_SorcBossAttack()
	{
		args[3] = SORC_ACCELERATE;
		args[2] = SORCBALL_INITIAL_SPEED;
	}

	//============================================================================
	//
	// A_SpawnFizzle
	//
	// spell cast magic fizzle
	//
	//============================================================================

	void A_SpawnFizzle()
	{
		Vector3 pos = Vec3Angle(5., Angle, -Floorclip + Height / 2. );
		for (int ix=0; ix<5; ix++)
		{
			Actor mo = Spawn("SorcSpark1", pos, ALLOW_REPLACE);
			if (mo)
			{
				double rangle = Angle + random[Heresiarch](0, 4) * (4096 / 360.);
				mo.Vel.X = random[Heresiarch](0, int(speed) - 1) * cos(rangle);
				mo.Vel.Y = random[Heresiarch](0, int(speed) - 1) * sin(rangle);
				mo.Vel.Z = 2;
			}
		}
	}

	
}

// Base class for the balls flying around the Heresiarch's head -------------

class SorcBall : Actor
{
	Default
	{
		Speed 10;
		Radius 5;
		Height 5;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+FULLVOLDEATH
		+CANBOUNCEWATER
		+NOWALLBOUNCESND
		BounceType "HexenCompat";
		SeeSound "SorcererBallBounce";
		DeathSound "SorcererBigBallExplode";
	}

	double OldAngle, AngleOffset;

	//============================================================================
	//
	// SorcBall::DoFireSpell
	//
	//============================================================================

	virtual void DoFireSpell ()
	{
		CastSorcererSpell ();
		target.args[3] = Heresiarch.SORC_STOPPED;
	}

	
	virtual void SorcUpdateBallAngle ()
	{
	}
	
	override bool SpecialBlastHandling (Actor source, double strength)
	{ // don't blast sorcerer balls
		return false;
	}
	
	//============================================================================
	//
	// ASorcBall::CastSorcererSpell
	//
	// Make noise and change the parent sorcerer's animation
	//
	//============================================================================

	virtual void CastSorcererSpell ()
	{
		target.A_StartSound ("SorcererSpellCast", CHAN_VOICE);

		// Put sorcerer into throw spell animation
		if (target.health > 0)
			target.SetStateLabel ("Attack2");
	}

	//============================================================================
	//
	// A_SorcBallOrbit
	//
	// - actor is ball
	//============================================================================

	void A_SorcBallOrbit()
	{
		// [RH] If no parent, then die instead of crashing
		if (target == null || target.health <= 0)
		{
			SetStateLabel ("Pain");
			return;
		}

		int mode = target.args[3];
		Heresiarch parent = Heresiarch(target);
		double dist = parent.radius - (radius*2);

		double prevangle = OldAngle;
		double baseangle = parent.BallAngle;
		double curangle = baseangle + AngleOffset;

		angle = curangle;

		switch (mode)
		{
		case Heresiarch.SORC_NORMAL:			// Balls rotating normally
			SorcUpdateBallAngle ();
			break;

		case Heresiarch.SORC_DECELERATE:		// Balls decelerating
			A_DecelBalls();
			SorcUpdateBallAngle ();
			break;

		case Heresiarch.SORC_ACCELERATE:		// Balls accelerating
			A_AccelBalls();
			SorcUpdateBallAngle ();
			break;

		case Heresiarch.SORC_STOPPING:			// Balls stopping
			if ((parent.StopBall == GetClass()) &&
				 (parent.args[1] > Heresiarch.SORCBALL_SPEED_ROTATIONS) &&
				 absangle(curangle, parent.angle) < 42.1875)
			{
				// Can stop now
				target.args[3] = Heresiarch.SORC_FIRESPELL;
				target.args[4] = 0;
				// Set angle so self angle == sorcerer angle
				parent.BallAngle = parent.angle - AngleOffset;
			}
			else
			{
				SorcUpdateBallAngle ();
			}
			break;

		case Heresiarch.SORC_FIRESPELL:			// Casting spell
			if (parent.StopBall == GetClass())
			{
				// Put sorcerer into special throw spell anim
				if (parent.health > 0)
					parent.SetStateLabel("Attack1");

				DoFireSpell ();
			}
			break;

		case Heresiarch.SORC_FIRING_SPELL:
			if (parent.StopBall == GetClass())
			{
				if (special2-- <= 0)
				{
					// Done rapid firing 
					parent.args[3] = Heresiarch.SORC_STOPPED;
					// Back to orbit balls
					if (parent.health > 0)
						parent.SetStateLabel("Attack2");
				}
				else
				{
					// Do rapid fire spell
					A_SorcOffense2();
				}
			}
			break;

		default:
			break;
		}

		// The comparison here depends on binary angle semantics and cannot be done in floating point.
		// It also requires very exact conversion that must be done natively.
		if (BAM(curangle) < BAM(prevangle) && (parent.args[4] == Heresiarch.SORCBALL_TERMINAL_SPEED))
		{
			parent.args[1]++;			// Bump rotation counter
			// Completed full rotation - make woosh sound
			A_StartSound ("SorcererBallWoosh", CHAN_BODY);
		}
		OldAngle = curangle;		// Set previous angle

		Vector3 pos = parent.Vec3Angle(dist, curangle, -parent.Floorclip + parent.Height);
		SetOrigin (pos, true);
		floorz = parent.floorz;
		ceilingz = parent.ceilingz;
	}

	//============================================================================
	//
	// A_SorcOffense2
	//
	// Actor is ball
	//
	//============================================================================

	void A_SorcOffense2()
	{
		Actor parent = target;
		Actor dest = parent.target;

		// [RH] If no enemy, then don't try to shoot.
		if (dest == null)
		{
			return;
		}

		int index = args[4];
		args[4] = (args[4] + 15) & 255;
		double delta = sin(index * (360 / 256.f)) * Heresiarch.SORCFX4_SPREAD_ANGLE;

		double ang1 = Angle + delta;
		Actor mo = parent.SpawnMissileAngle("SorcFX4", ang1, 0);
		if (mo)
		{
			mo.special2 = 35*5/2;		// 5 seconds
			double dist = mo.DistanceBySpeed(dest, mo.Speed);
			mo.Vel.Z = (dest.pos.z - mo.pos.z) / dist;
		}
	}

	//============================================================================
	//
	// A_AccelBalls
	//
	// Increase ball orbit speed - actor is ball
	//
	//============================================================================

	void A_AccelBalls()
	{
		Heresiarch sorc = Heresiarch(target);

		if (sorc.args[4] < sorc.args[2])
		{
			sorc.args[4]++;
		}
		else
		{
			sorc.args[3] = Heresiarch.SORC_NORMAL;
			if (sorc.args[4] >= Heresiarch.SORCBALL_TERMINAL_SPEED)
			{
				// Reached terminal velocity - stop balls
				sorc.A_StopBalls();
			}
		}
	}

	//============================================================================
	//
	// A_DecelBalls
	//
	// Decrease ball orbit speed - actor is ball
	//
	//============================================================================

	void A_DecelBalls()
	{
		Actor sorc = target;

		if (sorc.args[4] > sorc.args[2])
		{
			sorc.args[4]--;
		}
		else
		{
			sorc.args[3] = Heresiarch.SORC_NORMAL;
		}
	}


	void A_SorcBallExplode()
	{
		bNoBounceSound = true;
		A_Explode(255, 255);
	}
	
	//============================================================================
	//
	// A_SorcBallPop
	//
	// Ball death - bounce away in a random direction
	//
	//============================================================================

	void A_SorcBallPop()
	{
		A_StartSound ("SorcererBallPop", CHAN_BODY, CHANF_DEFAULT, 1., ATTN_NONE);
		bNoGravity = false;
		Gravity = 1. / 8;

		Vel.X = random[Heresiarch](-5, 4);
		Vel.Y = random[Heresiarch](-5, 4);
		Vel.Z = random[Heresiarch](2, 4);
		args[4] = Heresiarch.BOUNCE_TIME_UNIT;	// Bounce time unit
		args[3] = 5;					// Bounce time in seconds
	}
	
	//============================================================================
	//
	// A_BounceCheck
	//
	//============================================================================

	void A_BounceCheck ()
	{
		if (args[4]-- <= 0)
		{
			if (args[3]-- <= 0)
			{
				SetStateLabel("Death");
				A_StartSound ("SorcererBigBallExplode", CHAN_BODY, CHANF_DEFAULT, 1., ATTN_NONE);
			}
			else
			{
				args[4] = Heresiarch.BOUNCE_TIME_UNIT;
			}
		}
	}
	
}

// First ball (purple) - fires projectiles ----------------------------------

class SorcBall1 : SorcBall
{
	States
	{
	Spawn:
		SBMP ABCDEFGHIJKLMNOP 2 A_SorcBallOrbit;
		Loop;
	Pain:
		SBMP A 5 A_SorcBallPop;
		SBMP B 2 A_BounceCheck;
		Wait;
	Death:
		SBS4 D 5 A_SorcBallExplode;
		SBS4 E 5;
		SBS4 FGH 6;
		Stop;
	}

	override void BeginPlay ()
	{
		Super.BeginPlay ();
		AngleOffset = Heresiarch.BALL1_ANGLEOFFSET;
	}

	//============================================================================
	//
	// SorcBall1::CastSorcererSpell
	//
	// Offensive
	//
	//============================================================================

	override void CastSorcererSpell ()
	{
		Super.CastSorcererSpell ();

		Actor parent = target;

		double ang1 = Angle + 70;
		double ang2 = Angle - 70;
		Class<Actor> cls = "SorcFX1";
		Actor mo = parent.SpawnMissileAngle (cls, ang1, 0);
		if (mo)
		{
			mo.target = parent;
			mo.tracer = parent.target;
			mo.args[4] = Heresiarch.BOUNCE_TIME_UNIT;
			mo.args[3] = 15;				// Bounce time in seconds
		}
		mo = parent.SpawnMissileAngle (cls, ang2, 0);
		if (mo)
		{
			mo.target = parent;
			mo.tracer = parent.target;
			mo.args[4] = Heresiarch.BOUNCE_TIME_UNIT;
			mo.args[3] = 15;				// Bounce time in seconds
		}
	}

	
	//============================================================================
	//
	// ASorcBall1::SorcUpdateBallAngle
	//
	// Update angle if first ball
	//============================================================================

	override void SorcUpdateBallAngle ()
	{
		(Heresiarch(target)).BallAngle += target.args[4];
	}

	//============================================================================
	//
	// SorcBall1::DoFireSpell
	//
	//============================================================================

	override void DoFireSpell ()
	{
		if (random[Heresiarch]() < 200)
		{
			target.A_StartSound ("SorcererSpellCast", CHAN_VOICE, CHANF_DEFAULT, 1., ATTN_NONE);
			special2 = Heresiarch.SORCFX4_RAPIDFIRE_TIME;
			args[4] = 128;
			target.args[3] = Heresiarch.SORC_FIRING_SPELL;
		}
		else
		{
			Super.DoFireSpell ();
		}
	}

	
}


// Second ball (blue) - generates the shield --------------------------------

class SorcBall2 : SorcBall
{
	States
	{
	Spawn:
		SBMB ABCDEFGHIJKLMNOP 2 A_SorcBallOrbit;
		Loop;
	Pain:
		SBMB A 5 A_SorcBallPop;
		SBMB B 2 A_BounceCheck;
		Wait;
	Death:
		SBS3 D 5 A_SorcBallExplode;
		SBS3 E 5;
		SBS3 FGH 6;
		Stop;
	}
	
	override void BeginPlay ()
	{
		Super.BeginPlay ();
		AngleOffset = Heresiarch.BALL2_ANGLEOFFSET;
	}

	//============================================================================
	//
	// ASorcBall2::CastSorcererSpell
	//
	// Defensive
	//
	//============================================================================

	override void CastSorcererSpell ()
	{
		Super.CastSorcererSpell ();

		Actor parent = target;
		Actor mo = Spawn("SorcFX2", Pos + (0, 0, parent.Floorclip + Heresiarch.SORC_DEFENSE_HEIGHT), ALLOW_REPLACE);
		parent.bReflective = true;
		parent.bInvulnerable = true;
		parent.args[0] = Heresiarch.SORC_DEFENSE_TIME;
		if (mo) mo.target = parent;
	}

	
}

// Third ball (green) - summons Bishops -------------------------------------

class SorcBall3 : SorcBall
{
	States
	{
	Spawn:
		SBMG ABCDEFGHIJKLMNOP 2 A_SorcBallOrbit;
		Loop;
	Pain:
		SBMG A 5 A_SorcBallPop;
		SBMG B 2 A_BounceCheck;
		Wait;
	Death:
		SBS3 D 5 A_SorcBallExplode;
		SBS3 E 5;
		SBS3 FGH 6;
		Stop;
	}
	
	override void BeginPlay ()
	{
		Super.BeginPlay ();
		AngleOffset = Heresiarch.BALL3_ANGLEOFFSET;
	}

	//============================================================================
	//
	// ASorcBall3::CastSorcererSpell
	//
	// Reinforcements
	//
	//============================================================================

	override void CastSorcererSpell ()
	{
		Actor mo;
		Super.CastSorcererSpell ();
		Actor parent = target;

		double ang1 = Angle - 45;
		double ang2 = Angle + 45;
		Class<Actor> cls = "SorcFX3";
		if (health < (SpawnHealth()/3))
		{	// Spawn 2 at a time
			mo = parent.SpawnMissileAngle(cls, ang1, 4.);
			if (mo) mo.target = parent;
			mo = parent.SpawnMissileAngle(cls, ang2, 4.);
			if (mo) mo.target = parent;
		}			
		else
		{
			if (random[Heresiarch]() < 128)	ang1 = ang2;
			mo = parent.SpawnMissileAngle(cls, ang1, 4.);
			if (mo) mo.target = parent;
		}
	}

	
}


// Sorcerer spell 1 (The burning, bouncing head thing) ----------------------

class SorcFX1 : Actor
{
	Default
	{
		Speed 7;
		Radius 5;
		Height 5;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		-NOGRAVITY
		+FULLVOLDEATH
		+CANBOUNCEWATER
		+NOWALLBOUNCESND
		BounceFactor 1.0;
		BounceType "HexenCompat";
		SeeSound "SorcererBallBounce";
		DeathSound "SorcererHeadScream";
	}

	States
	{
	Spawn:
		SBS1 A 2 Bright;
		SBS1 BCD 3 Bright A_SorcFX1Seek;
		Loop;
	Death:
		FHFX S 2 Bright A_Explode(30, 128);
		FHFX SS 6 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_SorcFX1Seek
	//
	// Yellow spell - offense
	//
	//============================================================================

	void A_SorcFX1Seek()
	{
		if (args[4]-- <= 0)
		{
			if (args[3]-- <= 0)
			{
				SetStateLabel("Death");
				A_StartSound ("SorcererHeadScream", CHAN_BODY, CHANF_DEFAULT, 1., ATTN_NONE);
			}
			else
			{
				args[4] = Heresiarch.BOUNCE_TIME_UNIT;
			}
		}
		A_SeekerMissile(2, 6);
	}
	
}


// Sorcerer spell 2 (The visible part of the shield) ------------------------

class SorcFX2 : Actor
{
	Default
	{
		Speed 15;
		Radius 5;
		Height 5;
		+NOBLOCKMAP
		+NOGRAVITY
		+NOTELEPORT
	}

	states
	{
	Spawn:
		SBS2 A 3 Bright A_SorcFX2Split;
		Loop;
	Orbit:
		SBS2 A 2 Bright;
		SBS2 BCDEFGHIJKLMNOPA 2 Bright A_SorcFX2Orbit;
		Goto Orbit+1;
	Death:
		SBS2 A 10;
		Stop;
	}
	
	//============================================================================
	//
	// A_SorcFX2Split
	//
	// Blue spell - defense
	//
	//============================================================================
	//
	// FX2 Variables
	//		specialf1		current angle
	//		special2
	//		args[0]		0 = CW,  1 = CCW
	//		args[1]		
	//============================================================================

	// Split ball in two
	void A_SorcFX2Split()
	{
		Actor mo = Spawn(GetClass(), Pos, NO_REPLACE);
		if (mo)
		{
			mo.target = target;
			mo.args[0] = 0;									// CW
			mo.specialf1 = Angle;							// Set angle
			mo.SetStateLabel("Orbit");
		}
		mo = Spawn(GetClass(), Pos, NO_REPLACE);
		if (mo)
		{
			mo.target = target;
			mo.args[0] = 1;									// CCW
			mo.specialf1 = Angle;							// Set angle
			mo.SetStateLabel("Orbit");
		}
		Destroy ();
	}

	//============================================================================
	//
	// A_SorcFX2Orbit
	//
	// Orbit FX2 about sorcerer
	//
	//============================================================================

	void A_SorcFX2Orbit ()
	{
		Actor parent = target;

		// [RH] If no parent, then disappear
		if (parent == null)
		{
			Destroy();
			return;
		}

		double dist = parent.radius;

		if ((parent.health <= 0) ||		// Sorcerer is dead
			(!parent.args[0]))				// Time expired
		{
			SetStateLabel("Death");
			parent.args[0] = 0;
			parent.bReflective = false;
			parent.bInvulnerable = false;
		}

		if (args[0] && (parent.args[0]-- <= 0))		// Time expired
		{
			SetStateLabel("Death");
			parent.args[0] = 0;
			parent.bReflective = false;
		}

		Vector3 posi;
		// Move to new position based on angle
		if (args[0])		// Counter clock-wise
		{
			specialf1 += 10;
			angle = specialf1;
			posi = parent.Vec3Angle(dist, angle, parent.Floorclip + Heresiarch.SORC_DEFENSE_HEIGHT);
			posi.Z += 15 * cos(angle);
			// Spawn trailer
			Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
		}
		else							// Clock wise
		{
			specialf1 -= 10;
			angle = specialf1;
			posi = parent.Vec3Angle(dist, angle, parent.Floorclip + Heresiarch.SORC_DEFENSE_HEIGHT);
			posi.Z += 20 * sin(angle);
			// Spawn trailer
			Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
		}

		SetOrigin (posi, true);
		floorz = parent.floorz;
		ceilingz = parent.ceilingz;
	}
}

// The translucent trail behind SorcFX2 -------------------------------------

class SorcFX2T1 : SorcFX2
{
	Default
	{
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		Goto Death;
	}
}


// Sorcerer spell 3 (The Bishop spawner) ------------------------------------

class SorcFX3 : Actor
{
	Default
	{
		Speed 15;
		Radius 22;
		Height 65;
		+NOBLOCKMAP
		+MISSILE
		+NOTELEPORT
		SeeSound "SorcererBishopSpawn";
	}

	States
	{
	Spawn:
		SBS3 ABC 2 Bright;
		Loop;
	Death:
		SBS3 A 4 Bright;
		BISH P 4 A_SorcererBishopEntry;
		BISH ON 4;
		BISH MLKJIH 3;
		BISH G 3 A_SpawnBishop;
		Stop;
	}
	
	//============================================================================
	//
	// A_SorcererBishopEntry
	//
	//============================================================================

	void A_SorcererBishopEntry()
	{
		Spawn("SorcFX3Explosion", Pos, ALLOW_REPLACE);
		A_StartSound (SeeSound, CHAN_VOICE);
	}

	//============================================================================
	//
	// A_SpawnBishop
	//
	// Green spell - spawn bishops
	//
	//============================================================================

	void A_SpawnBishop()
	{
		Actor mo = Spawn("Bishop", Pos, ALLOW_REPLACE);
		if (mo)
		{
			if (!mo.TestMobjLocation())
			{
				mo.ClearCounters();
				mo.Destroy ();
			}
			else if (target != null)
			{ // [RH] Make the new bishops inherit the Heresiarch's target
				mo.CopyFriendliness (target, true);
				mo.master = target;
			}
		}
		Destroy ();
	}
}


// The Bishop spawner's explosion animation ---------------------------------

class SorcFX3Explosion : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		SBS3 DEFGH 3;
		Stop;
	}
}


// Sorcerer spell 4 (The purple projectile) ---------------------------------

class SorcFX4 : Actor
{
	Default
	{
		Speed 12;
		Radius 10;
		Height 10;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		DeathSound "SorcererBallExplode";
	}

	States
	{
	Spawn:
		SBS4 ABC 2 Bright A_SorcFX4Check;
		Loop;
	Death:
		SBS4 D 2 Bright;
		SBS4 E 2 Bright A_Explode(20, 128);
		SBS4 FGH 2 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_SorcFX4Check
	//
	// FX4 - rapid fire balls
	//
	//============================================================================

	void A_SorcFX4Check()
	{
		if (special2-- <= 0)
		{
			SetStateLabel ("Death");
		}
	}
}


// Spark that appears when shooting SorcFX4 ---------------------------------

class SorcSpark1 : Actor
{
	Default
	{
		Radius 5;
		Height 5;
		Gravity 0.125;
		+NOBLOCKMAP
		+DROPOFF
		+NOTELEPORT
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		SBFX ABCDEFG 4 Bright;
		Stop;
	}
}
