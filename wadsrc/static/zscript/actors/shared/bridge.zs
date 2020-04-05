/*
	args[0]: Bridge radius, in mapunits
	args[1]: Bridge height, in mapunits
	args[2]: Amount of bridge balls (if 0: Doom bridge)
	args[3]: Rotation speed of bridge balls, in byte angle per seconds, sorta:
		Since an arg is only a byte, it can only go from 0 to 255, while ZDoom's
		BAM go from 0 to 65535. Plus, it needs to be able to go either way. So,
		up to 128, it goes counterclockwise; 129-255 is clockwise, substracting
		256 from it to get the angle. A few example values:
		  0: Hexen default
	     11:  15° / seconds
		 21:  30° / seconds
		 32:  45° / seconds
		 64:  90° / seconds
		128: 180° / seconds
		192: -90° / seconds
		223: -45° / seconds
		233: -30° / seconds
		244: -15° / seconds
		This value only matters if args[2] is not zero.
	args[4]: Rotation radius of bridge balls, in bridge radius %.
		If 0, use Hexen default: ORBIT_RADIUS, regardless of bridge radius.
		This value only matters if args[2] is not zero.
*/

// Bridge ball -------------------------------------------------------------

class BridgeBall : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOTELEPORT
		+NOGRAVITY
	}

	States
	{
	Spawn:
		TLGL A 2 Bright;
		TLGL A 1 Bright A_BridgeOrbit;
		Wait;
	}

	void A_BridgeOrbit()
	{
		if (target == NULL)
		{ // Don't crash if somebody spawned this into the world
		  // independently of a Bridge actor.
			return;
		}
		// Set default values
		// Every five tics, Hexen moved the ball 3/256th of a revolution.
		double rotationspeed = 45. / 32 * 3 / 5;
		double rotationradius = CustomBridge.ORBIT_RADIUS;
		// If the bridge is custom, set non-default values if any.

		// Set angular speed; 1--128: counterclockwise rotation ~=1--180°; 129--255: clockwise rotation ~= 180--1°
		if (target.args[3] > 128) rotationspeed = 45. / 32 * (target.args[3] - 256) / TICRATE;
		else if (target.args[3] > 0) rotationspeed = 45. / 32 * (target.args[3]) / TICRATE;
		// Set rotation radius
		if (target.args[4]) rotationradius = ((target.args[4] * target.radius) / 100);

		Angle += rotationspeed;
		SetOrigin(target.Vec3Angle(rotationradius, Angle, 0), true);
		floorz = target.floorz;
		ceilingz = target.ceilingz;
	}
}

// The bridge itself -------------------------------------------------------

class CustomBridge : Actor
{
	const ORBIT_RADIUS = 15;

	default
	{
		+SOLID
		+NOGRAVITY
		+NOLIFTDROP
		+ACTLIKEBRIDGE
		Radius 32;
		Height 2;
		RenderStyle "None";
	}

	states
	{
	Spawn:
		TLGL ABCDE 3 Bright;
		Loop;
	See:
		TLGL A 2;
		TLGL A 2 A_BridgeInit;
		TLGL A -1;
		Stop;
	Death:
		TLGL A 2;
		TLGL A 300;
		Stop;
	}
	
	override void BeginPlay ()
	{
		if (args[2]) // Hexen bridge if there are balls
		{
			SetState(SeeState);
			A_SetSize(args[0] ? args[0] : 32, args[1] ? args[1] : 2);
		}
		else // No balls? Then a Doom bridge.
		{
			A_SetSize(args[0] ? args[0] : 36, args[1] ? args[1] : 4);
			A_SetRenderStyle(1., STYLE_Normal);
		}
	}
	
	override void OnDestroy()
	{
		// Hexen originally just set a flag to make the bridge balls remove themselves in A_BridgeOrbit.
		// But this is not safe with custom bridge balls that do not necessarily call that function.
		// So the best course of action is to look for all bridge balls here and destroy them ourselves.
		
		let it = ThinkerIterator.Create("Actor");
		Actor thing;
		
		while ((thing = Actor(it.Next())))
		{
			if (thing.target == self)
			{
				thing.Destroy();
			}
		}
		Super.OnDestroy();
	}
	

	void A_BridgeInit(class<Actor> balltype = "BridgeBall")
	{
		if (balltype == NULL)
		{
			balltype = "BridgeBall";
		}

		double startangle = random[orbit]() * (360./256.);

		// Spawn triad into world -- may be more than a triad now.
		int ballcount = args[2]==0 ? 3 : args[2];

		for (int i = 0; i < ballcount; i++)
		{
			Actor ball = Spawn(balltype, Pos, ALLOW_REPLACE);
			ball.Angle = startangle + (45./32) * (256/ballcount) * i;
			ball.target = self;
			
			double rotationradius = ORBIT_RADIUS;
			if (args[4]) rotationradius = (args[4] * radius) / 100;

			ball.SetOrigin(Vec3Angle(rotationradius, ball.Angle, 0), true);
			ball.floorz = floorz;
			ball.ceilingz = ceilingz;
		}
	}
}

// The Hexen bridge -------------------------------------------------------

class Bridge : CustomBridge
{
	default
	{
		RenderStyle "None";
		Args 32, 2, 3, 0;
	}
}

// The ZDoom bridge -------------------------------------------------------

class ZBridge : CustomBridge
{
	default
	{
		Args 36, 4, 0, 0;
	}
}


// Invisible bridge --------------------------------------------------------

class InvisibleBridge : Actor
{
	default
	{
		RenderStyle "None";
		Radius 32;
		RenderRadius -1;
		Height 4;
		+SOLID
		+NOGRAVITY
		+NOLIFTDROP
		+ACTLIKEBRIDGE
	}
	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	}
	
	override void BeginPlay ()
	{
		Super.BeginPlay ();
		if (args[0] || args[1])
		{
			A_SetSize(args[0]? args[0] : radius, args[1]? args[1] : height);
		}
	}
	
}

// And some invisible bridges from Skull Tag -------------------------------

class InvisibleBridge32 : InvisibleBridge
{
	default
	{
		Radius 32;
		Height 8;
	}
}

class InvisibleBridge16 : InvisibleBridge
{
	default
	{
		Radius 16;
		Height 8;
	}
}

class InvisibleBridge8 : InvisibleBridge
{
	default
	{
		Radius 8;
		Height 8;
	}
}
