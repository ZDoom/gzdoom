// Dragon -------------------------------------------------------------------
class Dragon : Actor
{
	Default
	{
		Health 640;
		PainChance 128;
		Speed 10;
		Height 65;
		Mass 0x7fffffff;
		Monster;
		+NOGRAVITY +FLOAT +NOBLOOD
		+BOSS
		+DONTMORPH +NOTARGET
		+NOICEDEATH
		SeeSound "DragonSight";
		AttackSound "DragonAttack";
		PainSound "DragonPain";
		DeathSound "DragonDeath";
		ActiveSound "DragonActive";
		Obituary "$OB_DRAGON";
		Tag "$FN_DRAGON";
	}

	States
	{
	Spawn:
		DRAG D 10 A_Look;
		Loop;
	See:
		DRAG CB 5;
		DRAG A 5 A_DragonInitFlight;
		DRAG B 3 A_DragonFlap;
		DRAG BCCDDCCBBAA 3 A_DragonFlight;
		Goto See + 3;
	Pain:
		DRAG F 10 A_DragonPain;
		Goto See + 3;
	Missile:
		DRAG E 8 A_DragonAttack;
		Goto See + 3;
	Death:
		DRAG G 5 A_Scream;
		DRAG H 4 A_NoBlocking;
		DRAG I 4;
		DRAG J 4 A_DragonCheckCrash;
		Wait;
	Crash:
		DRAG KL 5;
		DRAG M -1;
		Stop;
	}
	
	//============================================================================
	//
	// DragonSeek
	//
	//============================================================================

	private void DragonSeek (double thresh, double turnMax)
	{
		double dist;
		double delta;
		Actor targ;
		int i;
		double bestAngle;
		double angleToSpot, angleToTarget;
		Actor mo;

		targ = tracer;
		if(targ == null)
		{
			return;
		}

		double diff = deltaangle(angle, AngleTo(targ));
		delta = abs(diff);

		if (delta > thresh)
		{
			delta /= 2;
			if (delta > turnMax)
			{
				delta = turnMax;
			}
		}
		if (diff > 0)
		{ // Turn clockwise
			angle = angle + delta;
		}
		else
		{ // Turn counter clockwise
			angle = angle - delta;
		}
		VelFromAngle();

		dist = DistanceBySpeed(targ, Speed);
		if (pos.z + height < targ.pos.z || targ.pos.z + targ.height < pos.z)
		{
			Vel.Z = (targ.pos.z - pos.z) / dist;
		}
		if (targ.bShootable && random[DragonSeek]() < 64)
		{ // attack the destination mobj if it's attackable
			Actor oldTarget;
			
			if (absangle(angle, AngleTo(targ)) < 22.5)
			{
				oldTarget = target;
				target = targ;
				if (CheckMeleeRange ())
				{
					int damage = random[DragonSeek](1, 8) * 10;
					int newdam = targ.DamageMobj (self, self, damage, 'Melee');
					targ.TraceBleed (newdam > 0 ? newdam : damage, self);
					A_StartSound (AttackSound, CHAN_WEAPON);
				}
				else if (random[DragonSeek]() < 128 && CheckMissileRange())
				{
					SpawnMissile(targ, "DragonFireball");		
					A_StartSound (AttackSound, CHAN_WEAPON);
				}
				target = oldTarget;
			}
		}
		if (dist < 4)
		{ // Hit the target thing
			if (target && random[DragonSeek]() < 200)
			{
				Actor bestActor = null;
				bestAngle = 360.;
				angleToTarget = AngleTo(target);
				for (i = 0; i < 5; i++)
				{
					if (!targ.args[i])
					{
						continue;
					}
					ActorIterator iter = Level.CreateActorIterator(targ.args[i]);
					mo = iter.Next ();
					if (mo == null)
					{
						continue;
					}
					angleToSpot = AngleTo(mo);
					double diff = absangle(angleToSpot, angleToTarget);
					if (diff < bestAngle)
					{
						bestAngle = diff;
						bestActor = mo;
					}
				}
				if (bestActor != null)
				{
					tracer = bestActor;
				}
			}
			else
			{
				// [RH] Don't lock up if the dragon doesn't have any
				// targs defined
				for (i = 0; i < 5; ++i)
				{
					if (targ.args[i] != 0)
					{
						break;
					}
				}
				if (i < 5)
				{
					do
					{
						i = (random[DragonSeek]() >> 2) % 5;
					} while(!targ.args[i]);
					ActorIterator iter = Level.CreateActorIterator(targ.args[i]);
					tracer = iter.Next ();
				}
			}
		}
	}

	//============================================================================
	//
	// A_DragonInitFlight
	//
	//============================================================================

	void A_DragonInitFlight()
	{
		ActorIterator iter = Level.CreateActorIterator(tid);

		do
		{ // find the first tid identical to the dragon's tid
			tracer = iter.Next ();
			if (tracer == null)
			{
				SetState (SpawnState);
				return;
			}
		} while (tracer == self);
		RemoveFromHash ();
	}

	//============================================================================
	//
	// A_DragonFlight
	//
	//============================================================================

	void A_DragonFlight()
	{
		double ang;

		DragonSeek (4., 8.);
		let targ = target;
		if (targ)
		{
			if(!target.bShootable)
			{ // target died
				target = null;
				return;
			}
			ang = absangle(angle, AngleTo(target));
			if (ang <22.5 && CheckMeleeRange())
			{
				int damage = random[DragonFlight](1, 8) * 8;
				int newdam = targ.DamageMobj (self, self, damage, 'Melee');
				targ.TraceBleed (newdam > 0 ? newdam : damage, self);
				A_StartSound (AttackSound, CHAN_WEAPON);
			}
			else if (ang <= 20)
			{
				SetState (MissileState);
				A_StartSound (AttackSound, CHAN_WEAPON);
			}
		}
		else
		{
			LookForPlayers (true);
		}
	}

	//============================================================================
	//
	// A_DragonFlap
	//
	//============================================================================

	void A_DragonFlap()
	{
		A_DragonFlight();
		if (random[DragonFlight]() < 240)
		{
			A_StartSound ("DragonWingflap", CHAN_BODY);
		}
		else
		{
			PlayActiveSound ();
		}
	}

	//============================================================================
	//
	// A_DragonAttack
	//
	//============================================================================

	void A_DragonAttack()
	{
		SpawnMissile (target, "DragonFireball");
	}


	//============================================================================
	//
	// A_DragonPain
	//
	//============================================================================

	void A_DragonPain()
	{
		A_Pain();
		if (!tracer)
		{ // no destination spot yet
			SetState (SeeState);
		}
	}

	//============================================================================
	//
	// A_DragonCheckCrash
	//
	//============================================================================

	void A_DragonCheckCrash()
	{
		if (pos.z <= floorz)
		{
			SetStateLabel ("Crash");
		}
	}
}

// Dragon Fireball ----------------------------------------------------------

class DragonFireball : Actor
{
	Default
	{
		Speed 24;
		Radius 12;
		Height 10;
		Damage 6;
		DamageType "Fire";
		Projectile;
		-ACTIVATEIMPACT -ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "DragonFireballExplode";
	}

	States
	{
	Spawn:
		DRFX ABCDEF 4 Bright;
		Loop;
	Death:
		DRFX GHI 4 Bright;
		DRFX J 4 Bright A_DragonFX2;
		DRFX KL 3 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_DragonFX2
	//
	//============================================================================

	void A_DragonFX2()
	{
		int delay = 16+(random[DragonFX2]()>>3);
		for (int i = random[DragonFX2](1, 4); i; i--)
		{
			double xo = (random[DragonFX2]() - 128) / 4.;
			double yo = (random[DragonFX2]() - 128) / 4.;
			double zo = (random[DragonFX2]() - 128) / 16.;

			Actor mo = Spawn ("DragonExplosion", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
			if (mo)
			{
				mo.tics = delay + (random[DragonFX2](0, 3)) * i*2;
				mo.target = target;
			}
		}
	}
}

// Dragon Fireball Secondary Explosion --------------------------------------

class DragonExplosion : Actor
{
	Default
	{
		Radius 8;
		Height 8;
		DamageType "Fire";
		+NOBLOCKMAP
		+NOTELEPORT
		+INVISIBLE
		+ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "DragonFireballExplode";
	}
	States
	{
	Spawn:
		CFCF Q 1 Bright;
		CFCF Q 4 Bright A_UnHideThing;
		CFCF R 3 Bright A_Scream;
		CFCF S 4 Bright;
		CFCF T 3 Bright A_Explode (80, 128, 0);
		CFCF U 4 Bright;
		CFCF V 3 Bright;
		CFCF W 4 Bright;
		CFCF X 3 Bright;
		CFCF Y 4 Bright;
		CFCF Z 3 Bright;
		Stop;
	}
}
