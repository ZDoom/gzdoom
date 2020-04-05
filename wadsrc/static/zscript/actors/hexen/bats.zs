
// Bat Spawner --------------------------------------------------------------

class BatSpawner : SwitchableDecoration
{
	Default
	{
		+NOBLOCKMAP +NOSECTOR +NOGRAVITY
		RenderStyle "None";
	}

	States
	{
	Spawn:
	Active:
		TNT1 A 2;
		TNT1 A 2 A_BatSpawnInit;
		TNT1 A 2 A_BatSpawn;
		Wait;
	Inactive:
		TNT1 A -1;
		Stop;
	}
	
	//===========================================================================
	// Bat Spawner Variables
	//	special1	frequency counter
	//	special2	
	//	args[0]		frequency of spawn (1=fastest, 10=slowest)
	//	args[1]		spread angle (0..255)
	//	args[2]		
	//	args[3]		duration of bats (in octics)
	//	args[4]		turn amount per move (in degrees)
	//
	// Bat Variables
	//	special2	lifetime counter
	//	args[4]		turn amount per move (in degrees)
	//===========================================================================

	void A_BatSpawnInit()
	{
		special1 = 0;	// Frequency count
	}

	void A_BatSpawn()
	{
		// Countdown until next spawn
		if (special1-- > 0) return;
		special1 = args[0];		// Reset frequency count

		int delta = args[1];
		if (delta == 0) delta = 1;

		double ang = Angle + ((random[BatSpawn](0, delta-1) - (delta >> 1)) * (360 / 256.));

		Actor mo = SpawnMissileAngle ("Bat", ang, 0);
		if (mo)
		{
			mo.args[0] = random[BatSpawn](0, 63);			// floatbob index
			mo.args[4] = args[4];							// turn degrees
			mo.special2 = args[3] << 3;						// Set lifetime
			mo.target = self;
		}
	}
	
}

// Bat ----------------------------------------------------------------------

class Bat : Actor
{
	Default
	{
		Speed 5;
		Radius 3;
		Height 3;
		+NOBLOCKMAP +NOGRAVITY +MISSILE
		+NOTELEPORT +CANPASS
	}

	States
	{
	Spawn:
		ABAT ABC 2 A_BatMove;
		Loop;
	Death:
		ABAT A 2;
		Stop;
	}
	
	void A_BatMove()
	{
		if (special2 < 0)
		{
			SetStateLabel ("Death");
		}
		special2 -= 2;		// Called every 2 tics

		double newangle;
		if (random[BatMove]() < 128)
		{
			newangle = Angle + args[4];
		}
		else
		{
			newangle = Angle - args[4];
		}

		// Adjust velocity vector to new direction
		VelFromAngle(Speed, newangle);

		if (random[BatMove]() < 15)
		{
			A_StartSound ("BatScream", CHAN_VOICE, CHANF_DEFAULT, 1, ATTN_IDLE);
		}

		// Handle Z movement
		SetZ(target.pos.Z + 2 * BobSin(args[0]));
		args[0] = (args[0] + 3) & 63;	
	}
}




