//===========================================================================
//
// Mancubus
//
//===========================================================================
class Fatso : Actor
{
	Default
	{
		Health 600;
		Radius 48;
		Height 64;
		Mass 1000;
		Speed 8;
		PainChance 80;
		Monster;
		+FLOORCLIP
		+BOSSDEATH
		+MAP07BOSS1
		SeeSound "fatso/sight";
		PainSound "fatso/pain";
		DeathSound "fatso/death";
		ActiveSound "fatso/active";
		Obituary "$OB_FATSO";
		Tag "$FN_MANCU";
	}
	States
	{
	Spawn:
		FATT AB 15 A_Look;
		Loop;
	See:
		FATT AABBCCDDEEFF 4 A_Chase;
		Loop;
	Missile:
		FATT G 20 A_FatRaise;
		FATT H 10 BRIGHT A_FatAttack1;
		FATT IG 5 A_FaceTarget;
		FATT H 10 BRIGHT A_FatAttack2;
		FATT IG 5 A_FaceTarget;
		FATT H 10 BRIGHT A_FatAttack3;
		FATT IG 5 A_FaceTarget;
		Goto See;
	Pain:
		FATT J 3;
		FATT J 3 A_Pain;
		Goto See;
    Death:
		FATT K 6;
		FATT L 6 A_Scream;
		FATT M 6 A_NoBlocking;
		FATT NOPQRS 6;
		FATT T -1 A_BossDeath;
	    Stop;
	 Raise:
		FATT R 5;
		FATT QPONMLK 5;
		Goto See;
	}
}



//===========================================================================
//
// Mancubus fireball
//
//===========================================================================
class FatShot : Actor
{
	Default
	{
		Radius 6;
		Height 8;
		Speed 20;
		Damage 8;
		Projectile;
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 1;
		SeeSound "fatso/attack";
		DeathSound "fatso/shotx";
	}
	States
	{
	Spawn:
		MANF AB 4 BRIGHT;
		Loop;
	Death:
		MISL B 8 BRIGHT;
		MISL C 6 BRIGHT;
		MISL D 4 BRIGHT;
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
	const FATSPREAD = 90./8;
	
	void A_FatRaise()
	{
		A_FaceTarget();
		A_StartSound("fatso/raiseguns", CHAN_WEAPON);
	}

	//
	// Mancubus attack,
	// firing three missiles in three different directions?
	// Doesn't look like it.
	//
	
	void A_FatAttack1(class<Actor> spawntype = "FatShot")
	{
		if (target)
		{
			A_FaceTarget ();
			// Change direction  to ...
			Angle += FATSPREAD;
			SpawnMissile (target, spawntype);
			Actor missile = SpawnMissile (target, spawntype);
			if (missile)
			{
				missile.Angle += FATSPREAD;
				missile.VelFromAngle();
			}
		}
	}
	
	void A_FatAttack2(class<Actor> spawntype = "FatShot")
	{
		if (target)
		{
			A_FaceTarget ();
			// Now here choose opposite deviation.
			Angle -= FATSPREAD;
			SpawnMissile (target, spawntype);
			Actor missile = SpawnMissile (target, spawntype);
			if (missile)
			{
				missile.Angle -= FATSPREAD;
				missile.VelFromAngle();
			}
		}
	}
	
	void A_FatAttack3(class<Actor> spawntype = "FatShot")
	{
		if (target)
		{
			A_FaceTarget ();
			Actor missile = SpawnMissile (target, spawntype);
			if (missile)
			{
				missile.Angle -= FATSPREAD/2;
				missile.VelFromAngle();
			}
			missile = SpawnMissile (target, spawntype);
			if (missile)
			{
				missile.Angle += FATSPREAD/2;
				missile.VelFromAngle();
			}
		}
	}
	
	//
	// killough 9/98: a mushroom explosion effect, sorta :)
	// Original idea: Linguica
	//
	
	void A_Mushroom(class<Actor> spawntype = "FatShot", int numspawns = 0, int flags = 0, double vrange = 4.0, double hrange = 0.5)
	{
		int i, j;

		if (numspawns == 0)
		{
			numspawns = GetMissileDamage(0, 1);
		}

		A_Explode(128, 128, (flags & MSF_DontHurt) ? 0 : XF_HURTSOURCE);

		// Now launch mushroom cloud
		Actor aimtarget = Spawn("Mapspot", pos, NO_REPLACE);	// We need something to aim at.
		if (aimtarget == null) return;
		Actor owner = (flags & MSF_DontHurt) ? target : self;
		aimtarget.Height = Height;
		
		bool shootmode = ((flags & MSF_Classic) || // Flag explicitly set, or no flags and compat options
					(flags == 0 && CurState.bDehacked && (Level.compatflags & COMPATF_MUSHROOM)));

		for (i = -numspawns; i <= numspawns; i += 8)
		{
			for (j = -numspawns; j <= numspawns; j += 8)
			{
				Actor mo;
				aimtarget.SetXYZ(pos + (i, j, (i, j).Length() * vrange)); // Aim up fairly high
				if (shootmode)
				{	// Use old function for MBF compatibility
					mo = OldSpawnMissile(aimtarget, spawntype, owner);
				}
				else // Use normal function
				{
					mo = SpawnMissile(aimtarget, spawntype, owner);
				}
				if (mo)
				{	// Slow it down a bit
					mo.Vel *= hrange;
					mo.bNoGravity = false;   // Make debris fall under gravity
				}
			}
		}
		aimtarget.Destroy();
	}
	
}

