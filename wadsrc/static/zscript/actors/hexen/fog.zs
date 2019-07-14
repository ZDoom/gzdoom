//==========================================================================
// Fog Variables:
//
//		args[0]		Speed (0..10) of fog
//		args[1]		Angle of spread (0..128)
//		args[2]		Frequency of spawn (1..10)
//		args[3]		Lifetime countdown
//		args[4]		Boolean: fog moving?
//		special1	Internal:  Counter for spawn frequency
//		WeaveIndexZ	Internal:  Index into floatbob table
//
//==========================================================================

// Fog Spawner --------------------------------------------------------------

class FogSpawner : Actor
{
	Default
	{
		+NOSECTOR +NOBLOCKMAP
		+FLOATBOB
		+NOGRAVITY
		+INVISIBLE
	}


	States
	{
	Spawn:
		TNT1 A 20 A_FogSpawn;
		Loop;
	}
	
	//==========================================================================
	//
	// A_FogSpawn
	//
	//==========================================================================

	void A_FogSpawn()
	{
		if (special1-- > 0)
		{
			return;
		}
		special1 = args[2];		// Reset frequency count

		class<Actor> fog;
		switch (random[FogSpawn](0,2))
		{
			default:
			case 0: fog = "FogPatchSmall"; break;
			case 1: fog = "FogPatchMedium"; break;
			case 2: fog = "FogPatchLarge"; break;
		}
		Actor mo = Spawn (fog, Pos, ALLOW_REPLACE);

		if (mo)
		{
			int delta = args[1];
			if (delta == 0) delta = 1;
			mo.angle = angle + ((random[FogSpawn](0, delta-1) - (delta >> 1)) * (360 / 256.));
			mo.target = self;
			if (args[0] < 1) args[0] = 1;
			mo.args[0] = random[FogSpawn](1, args[0]);	// Random speed
			mo.args[3] = args[3];						// Set lifetime
			mo.args[4] = 1;									// Set to moving
			mo.WeaveIndexZ = random[FogSpawn](0, 63);
		}
	}
	
}

// Small Fog Patch ----------------------------------------------------------

class FogPatchSmall : Actor
{
	Default
	{
		Speed 1;
		+NOBLOCKMAP +NOGRAVITY +NOCLIP +FLOAT
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.6;
	}


	States
	{
	Spawn:
		FOGS ABCDE 7 A_FogMove;
		Loop;
	Death:
		FOGS E 5;
		Stop;
	}
	
	//==========================================================================
	//
	// A_FogMove
	//
	//==========================================================================

	void A_FogMove()
	{
		double speed = args[0];
		int weaveindex;

		if (!args[4])
		{
			return;
		}

		if (args[3]-- <= 0)
		{
			SetStateLabel ('Death', true);
			return;
		}

		if ((args[3] % 4) == 0)
		{
			weaveindex = WeaveIndexZ;
			AddZ(BobSin(weaveindex) / 2);
			WeaveIndexZ = (weaveindex + 1) & 63;
		}
		VelFromAngle(speed);
	}
}

// Medium Fog Patch ---------------------------------------------------------

class FogPatchMedium : FogPatchSmall
{
	States
	{
	Spawn:
		FOGM ABCDE 7 A_FogMove;
		Loop;
	Death:
		FOGS ABCDE 5;
		Goto Super::Death;
	}
}

// Large Fog Patch ----------------------------------------------------------

class FogPatchLarge : FogPatchMedium
{
	States
	{
	Spawn:
		FOGL ABCDE 7 A_FogMove;
		Loop;
	Death:
		FOGM ABCDEF 4;
		Goto Super::Death;
	}
}

