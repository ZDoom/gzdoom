//===========================================================================
//
// Arch Vile
//
//===========================================================================

class Archvile : Actor
{
	Default
	{
		Health 700;
		Radius 20;
		Height 56;
		Mass 500;
		Speed 15;
		PainChance 10;
		Monster;
		MaxTargetRange 896;
		+QUICKTORETALIATE 
		+FLOORCLIP 
		+NOTARGET
		SeeSound "vile/sight";
		PainSound "vile/pain";
		DeathSound "vile/death";
		ActiveSound "vile/active";
		MeleeSound "vile/stop";
		Obituary "$OB_VILE";
		Tag "$FN_ARCH";
	}
	States
	{
	Spawn:
		VILE AB 10 A_Look;
		Loop;
	See:
		VILE AABBCCDDEEFF 2 A_VileChase;
		Loop;
	Missile:
		VILE G 0 BRIGHT A_VileStart;
		VILE G 10 BRIGHT A_FaceTarget;
		VILE H 8 BRIGHT A_VileTarget;
		VILE IJKLMN 8 BRIGHT A_FaceTarget;
		VILE O 8 BRIGHT A_VileAttack;
		VILE P 20 BRIGHT;
		Goto See;
	Heal:
		VILE [\] 10 BRIGHT;
		Goto See;
	Pain:
		VILE Q 5;
		VILE Q 5 A_Pain;
		Goto See;
	Death:
		VILE Q 7;
		VILE R 7 A_Scream;
		VILE S 7 A_NoBlocking;
		VILE TUVWXY 7;
		VILE Z -1;
		Stop;
	}
}


//===========================================================================
//
// Arch Vile Fire
//
//===========================================================================

class ArchvileFire : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY +ZDOOMTRANS
		RenderStyle "Add";
		Alpha 1;
	}
	States
	{
	Spawn:
		FIRE A 2 BRIGHT  A_StartFire;
		FIRE BAB 2 BRIGHT  A_Fire;
		FIRE C 2 BRIGHT  A_FireCrackle;
		FIRE BCBCDCDCDEDED 2 BRIGHT  A_Fire;
		FIRE E 2 BRIGHT  A_FireCrackle;
		FIRE FEFEFGHGHGH 2 BRIGHT  A_Fire;
		Stop;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

// A_VileAttack flags
//#define VAF_DMGTYPEAPPLYTODIRECT 1

extend class Actor
{

	void A_VileStart()
	{
		A_StartSound ("vile/start", CHAN_VOICE);
	}
	
	//
	// A_VileTarget
	// Spawn the hellfire
	//
	void A_VileTarget(class<Actor> fire = "ArchvileFire")
	{
		if (target)
		{
			A_FaceTarget ();

			Actor fog = Spawn (fire, target.Pos, ALLOW_REPLACE);
			if (fog != null)
			{
				tracer = fog;
				fog.target = self;
				fog.tracer = self.target;
				fog.A_Fire(0);
			}
		}
	}
	
	void A_VileAttack(sound snd = "vile/stop", int initialdmg = 20, int blastdmg = 70, int blastradius = 70, double thrust = 1.0, name damagetype = "Fire", int flags = 0)
	{
		Actor targ = target;
		if (targ)
		{
			A_FaceTarget();
			if (!CheckSight(targ, 0)) return;
			A_StartSound(snd, CHAN_WEAPON);
			int newdam = targ.DamageMobj (self, self, initialdmg, (flags & VAF_DMGTYPEAPPLYTODIRECT)? damagetype : 'none');

			targ.TraceBleed (newdam > 0 ? newdam : initialdmg, self);
			
			Actor fire = tracer;
			if (fire)
			{
				// move the fire between the vile and the player
				fire.SetOrigin(targ.Vec3Angle(-24., angle, 0), true);
				fire.A_Explode(blastdmg, blastradius, XF_NOSPLASH, false, 0, 0, 0, "BulletPuff", damagetype);
			}
			if (!targ.bDontThrust)
			{
				targ.Vel.z = thrust * 1000 / max(1, targ.Mass);
			}
		}
	}
	
	void A_StartFire()
	{
		A_StartSound ("vile/firestrt", CHAN_BODY);
		A_Fire();
	}
	
	//
	// A_Fire
	// Keep fire in front of player unless out of sight
	//
	void A_Fire(double spawnheight = 0)
	{
		Actor dest = tracer;
		if (!dest || !target) return;
				
		// don't move it if the vile lost sight
		if (!target.CheckSight (dest, 0) ) return;

		SetOrigin(dest.Vec3Angle(24, dest.angle, spawnheight), true);
	}
	
	void A_FireCrackle()
	{
		A_StartSound ("vile/firecrkl", CHAN_BODY);
		A_Fire();
	}
}
