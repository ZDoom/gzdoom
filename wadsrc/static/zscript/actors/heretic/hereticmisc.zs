
class HereticWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 150;
	}
}

// Pod ----------------------------------------------------------------------

class Pod : Actor
{
	Default
	{
		Health 45;
		Radius 16;
		Height 54;
		Painchance 255;
		+SOLID +NOBLOOD +SHOOTABLE +DROPOFF
		+WINDTHRUST +PUSHABLE +SLIDESONWALLS
		+CANPASS +TELESTOMP +DONTMORPH
		+NOBLOCKMONST +DONTGIB +OLDRADIUSDMG
		DeathSound "world/podexplode";
		PushFactor 0.5;
	}

	States
	{
	Spawn:
		PPOD A 10;
		Loop;
	Pain:
		PPOD B 14 A_PodPain;
		Goto Spawn;
	Death:
		PPOD C 5 BRIGHT A_RemovePod;
		PPOD D 5 BRIGHT A_Scream;
		PPOD E 5 BRIGHT A_Explode;
		PPOD F 10 BRIGHT;
		Stop;
	Grow:
		PPOD IJKLMNOP 3;
		Goto Spawn;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_PodPain
	//
	//----------------------------------------------------------------------------

	void A_PodPain (class<Actor> gootype = "PodGoo")
	{
		int chance = Random[PodPain]();
		if (chance < 128)
		{
			return;
		}
		for (int count = chance > 240 ? 2 : 1; count; count--)
		{
			Actor goo = Spawn(gootype, pos + (0, 0, 48), ALLOW_REPLACE);
			if (goo != null)
			{
				goo.target = self;
				goo.Vel.X = Random2[PodPain]() / 128.;
				goo.Vel.Y = Random2[PodPain]() / 128.;
				goo.Vel.Z = 0.5 + random[PodPain]() / 128.;
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_RemovePod
	//
	//----------------------------------------------------------------------------

	void A_RemovePod ()
	{
		if (master && master.special1 > 0)
		{
			master.special1--;
		}
	}

}


// Pod goo (falls from pod when damaged) ------------------------------------

class PodGoo : Actor
{
	Default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP +MISSILE +DROPOFF
		+NOTELEPORT +CANNOTPUSH
	}
	States
	{
	Spawn:
		PPOD GH 8;
		Loop;
	Death:
		PPOD G 10;
		Stop;
	}
}

// Pod generator ------------------------------------------------------------

class PodGenerator : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+DONTSPLASH
		AttackSound "world/podgrow";
	}


	States
	{
	Spawn:
		TNT1 A 35 A_MakePod;
		Loop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_MakePod
	//
	//----------------------------------------------------------------------------

	const MAX_GEN_PODS = 16;

	void A_MakePod (class<Actor> podtype = "Pod")
	{
		if (special1 >= MAX_GEN_PODS)
		{ // Too many generated pods
			return;
		}
		Actor mo = Spawn(podtype, (pos.xy, ONFLOORZ), ALLOW_REPLACE);
		if (!mo) return;
		if (!mo.CheckPosition (mo.Pos.xy))
		{ // Didn't fit
			mo.Destroy ();
			return;
		}
		mo.SetStateLabel("Grow");
		mo.Thrust(4.5, random[MakePod]() * (360. / 256));
		A_StartSound (AttackSound, CHAN_BODY);
		special1++; // Increment generated pod count
		mo.master = self; // Link the generator to the pod
	}
}


// Teleglitter generator 1 --------------------------------------------------

class TeleGlitterGenerator1 : Actor
{
	Default
	{
		+NOBLOCKMAP 
		+NOGRAVITY
		+DONTSPLASH
		+MOVEWITHSECTOR
	}
	States
	{
	Spawn:
		TNT1 A 8 A_SpawnItemEx("TeleGlitter1", random[TeleGlitter](0,31)-16, random[TeleGlitter](0,31)-16, 0, 0,0,0.25);
		Loop;
	}
}

// Teleglitter generator 2 --------------------------------------------------

class TeleGlitterGenerator2 : Actor
{
	Default
	{
		+NOBLOCKMAP 
		+NOGRAVITY
		+DONTSPLASH
		+MOVEWITHSECTOR
	}
	States
	{
	Spawn:
		TNT1 A 8 A_SpawnItemEx("TeleGlitter2", random[TeleGlitter2](0,31)-16, random[TeleGlitter2](0,31)-16, 0, 0,0,0.25);
		Loop;
	}
}


// Teleglitter 1 ------------------------------------------------------------

class TeleGlitter1 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY +MISSILE +ZDOOMTRANS
		RenderStyle "Add";
		Damage 0;
	}


	States
	{
	Spawn:
		TGLT A 2 BRIGHT;
		TGLT B 2 BRIGHT A_AccTeleGlitter;
		TGLT C 2 BRIGHT;
		TGLT D 2 BRIGHT A_AccTeleGlitter;
		TGLT E 2 BRIGHT;
		Loop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_AccTeleGlitter
	//
	//----------------------------------------------------------------------------

	void A_AccTeleGlitter ()
	{
		if (++health > 35)
		{
			Vel.Z *= 1.5;
		}
	}
}

// Teleglitter 2 ------------------------------------------------------------

class TeleGlitter2 : TeleGlitter1
{
	States
	{
	Spawn:
		TGLT F 2 BRIGHT;
		TGLT G 2 BRIGHT A_AccTeleGlitter;
		TGLT H 2 BRIGHT;
		TGLT I 2 BRIGHT A_AccTeleGlitter;
		TGLT J 2 BRIGHT;
		Loop;
	}
}


// --- Volcano --------------------------------------------------------------

class Volcano : Actor
{
	Default
	{
		Radius 12;
		Height 20;
		+SOLID
	}
	
	States
	{
	Spawn:
		VLCO A 350;
		VLCO A 35 A_VolcanoSet;
		VLCO BCDBCD 3;
		VLCO E 10 A_VolcanoBlast;
		Goto Spawn+1;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_VolcanoSet
	//
	//----------------------------------------------------------------------------

	void A_VolcanoSet ()
	{
		tics = random[VolcanoSet](105, 232);
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_VolcanoBlast
	//
	//----------------------------------------------------------------------------

	void A_VolcanoBlast ()
	{
		int count = random[VolcanoBlast](1,3);
		for (int i = 0; i < count; i++)
		{
			Actor blast = Spawn("VolcanoBlast", pos + (0, 0, 44), ALLOW_REPLACE);
			if (blast != null)
			{
				blast.target = self;
				blast.Angle = random[VolcanoBlast]() * (360 / 256.);
				blast.VelFromAngle(1.);
				blast.Vel.Z = 2.5 + random[VolcanoBlast]() / 64.;
				blast.A_StartSound ("world/volcano/shoot", CHAN_BODY);
				blast.CheckMissileSpawn (radius);
			}
		}
	}
}

// Volcano blast ------------------------------------------------------------

class VolcanoBlast : Actor
{
	Default
	{
		Radius 8;
		Height 8;
		Speed 2;
		Damage 2;
		DamageType "Fire";
		Gravity 0.125;
		+NOBLOCKMAP +MISSILE +DROPOFF
		+NOTELEPORT
		DeathSound "world/volcano/blast";
	}

	States
	{
	Spawn:
		VFBL AB 4 BRIGHT A_SpawnItemEx("Puffy", random2[BeastPuff]()*0.015625, random2[BeastPuff]()*0.015625, random2[BeastPuff]()*0.015625, 
									0,0,0,0,SXF_ABSOLUTEPOSITION, 64);
		Loop;

	Death:
		XPL1 A 4 BRIGHT A_VolcBallImpact;
		XPL1 BCDEF 4 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_VolcBallImpact
	//
	//----------------------------------------------------------------------------

	void A_VolcBallImpact ()
	{
		if (pos.Z <= floorz)
		{
			bNoGravity = true;
			Gravity = 1;
			AddZ(28);
		}
		A_Explode(25, 25, XF_NOSPLASH|XF_HURTSOURCE, false, 0, 0, 0, "BulletPuff", 'Fire');
		for (int i = 0; i < 4; i++)
		{
			Actor tiny = Spawn("VolcanoTBlast", Pos, ALLOW_REPLACE);
			if (tiny)
			{
				tiny.target = self;
				tiny.Angle = 90.*i;
				tiny.VelFromAngle(0.7);
				tiny.Vel.Z = 1. + random[VolcBallImpact]() / 128.;
				tiny.CheckMissileSpawn (radius);
			}
		}
	}
}

// Volcano T Blast ----------------------------------------------------------

class VolcanoTBlast : Actor
{
	Default
	{
		Radius 8;
		Height 6;
		Speed 2;
		Damage 1;
		DamageType "Fire";
		Gravity 0.125;
		+NOBLOCKMAP +MISSILE +DROPOFF
		+NOTELEPORT
	}
	States
	{
	Spawn:
		VTFB AB 4 BRIGHT;
		Loop;
	Death:
		SFFI CBABCDE 4 BRIGHT;
		Stop;
	}
}

