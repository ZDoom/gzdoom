//===========================================================================
//
// Revenant
//
//===========================================================================
class Revenant : Actor
{
	Default
	{
		Health 300;
		Radius 20;
		Height 56;
		Mass 500;
		Speed 10;
		PainChance 100;
		Monster;
		MeleeThreshold 196;
		MissileChanceMult 0.5;
		+FLOORCLIP
		SeeSound "skeleton/sight";
		PainSound "skeleton/pain";
		DeathSound "skeleton/death";
		ActiveSound "skeleton/active";
		MeleeSound "skeleton/melee";
		HitObituary "$OB_UNDEADHIT";
		Obituary "$OB_UNDEAD";
		Tag "$FN_REVEN";
	}
	States
	{
	Spawn:
		SKEL AB 10 A_Look;
		Loop;
	See:
		SKEL AABBCCDDEEFF 2 A_Chase;
		Loop;
	Melee:
		SKEL G 0 A_FaceTarget;
		SKEL G 6 A_SkelWhoosh;
		SKEL H 6 A_FaceTarget;
		SKEL I 6 A_SkelFist;
		Goto See;
	Missile:
		SKEL J 0 BRIGHT A_FaceTarget;
		SKEL J 10 BRIGHT A_FaceTarget;
		SKEL K 10 A_SkelMissile;
		SKEL K 10 A_FaceTarget;
		Goto See;
	Pain:
		SKEL L 5;
		SKEL L 5 A_Pain;
		Goto See;
	Death:
		SKEL LM 7;
		SKEL N 7 A_Scream;
		SKEL O 7 A_NoBlocking;
		SKEL P 7;
		SKEL Q -1;
		Stop;
	Raise:
		SKEL Q 5;
		SKEL PONML 5;
		Goto See;
	}
}


//===========================================================================
//
// Revenant Tracer
//
//===========================================================================
class RevenantTracer : Actor
{
	Default
	{
		Radius 11;
		Height 8;
		Speed 10;
		Damage 10;
		Projectile;
		+SEEKERMISSILE 
		+RANDOMIZE
		+ZDOOMTRANS
		SeeSound "skeleton/attack";
		DeathSound "skeleton/tracex";
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		FATB AB 2 BRIGHT A_Tracer;
		Loop;
	Death:
		FBXP A 8 BRIGHT;
		FBXP B 6 BRIGHT;
		FBXP C 4 BRIGHT;
		Stop;
	}
}


//===========================================================================
//
// Revenant Tracer Smoke
//
//===========================================================================
class RevenantTracerSmoke : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+NOTELEPORT
		+ZDOOMTRANS
		RenderStyle "Translucent";
		Alpha 0.5;
	}
	States
	{	
	Spawn:
		PUFF ABABC 4;
		Stop;
	}
}

//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	void A_SkelMissile()
	{
		if (target == null) return;
		A_FaceTarget();
		AddZ(16);
		Actor missile = SpawnMissile(target, "RevenantTracer");
		AddZ(-16);
		if (missile != null)
		{
			missile.SetOrigin(missile.Vec3Offset(missile.Vel.X, missile.Vel.Y, 0.), false);
			missile.tracer = target;
		}
	}

	void A_SkelWhoosh()
	{
		if (target == null) return;
		A_FaceTarget();
		A_StartSound("skeleton/swing", CHAN_WEAPON);
	}

	void A_SkelFist()
	{
		let targ = target;
		if (targ == null) return;
		A_FaceTarget();
		
		if (CheckMeleeRange ())
		{
			int damage = random[SkelFist](1, 10) * 6;
			A_StartSound("skeleton/melee", CHAN_WEAPON);
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
		}
	}

	void A_Tracer2(double traceang = 19.6875)
	{
		double dist;
		double slope;
		Actor dest;
				
		// adjust direction
		dest = tracer;
		
		if (!dest || dest.health <= 0 || Speed == 0 || !CanSeek(dest))
			return;
	
		// change angle 	
		double exact = AngleTo(dest);
		double diff = deltaangle(angle, exact);

		if (diff < 0)
		{
			angle -= traceang;
			if (deltaangle(angle, exact) > 0)
				angle = exact;
		}
		else if (diff > 0)
		{
			angle += traceang;
			if (deltaangle(angle, exact) < 0.)
				angle = exact;
		}

		VelFromAngle();

		if (!bFloorHugger && !bCeilingHugger)
		{
			// change slope
			dist = DistanceBySpeed(dest, Speed);

			if (dest.Height >= 56.)
			{
				slope = (dest.pos.z + 40. - pos.z) / dist;
			}
			else
			{
				slope = (dest.pos.z + Height*(2./3) - pos.z) / dist;
			}

			if (slope < Vel.Z)
				Vel.Z -= 1. / 8;
			else
				Vel.Z += 1. / 8;
		}
	}
	
	void A_Tracer()
	{
		// killough 1/18/98: this is why some missiles do not have smoke
		// and some do. Also, internal demos start at random gametics, thus
		// the bug in which revenants cause internal demos to go out of sync.

		if (level.maptime & 3)	return;
	
		// spawn a puff of smoke behind the rocket
		SpawnPuff ("BulletPuff", pos, angle, angle, 3);
		Actor smoke = Spawn ("RevenantTracerSmoke", Vec3Offset(-Vel.X, -Vel.Y, 0.), ALLOW_REPLACE);
	
		if (smoke != null)
		{
			smoke.Vel.Z = 1.;
			smoke.tics -= random[Tracer](0, 3);
			if (smoke.tics < 1)
				smoke.tics = 1;
		}

		// The rest of this function was identical with Strife's version, except for the angle being used.
		A_Tracer2(16.875);
	}
}

