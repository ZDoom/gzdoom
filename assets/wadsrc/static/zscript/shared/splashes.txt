
// Water --------------------------------------------------------------------

class WaterSplash : Actor
{
	default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP 
		+MISSILE 
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+DONTSPLASH
		+DONTBLAST
	}
	States
	{
	Spawn:
		SPSH ABC 8;
		SPSH D 16;
		Stop;
	Death:
		SPSH D 10;
		Stop;
	}
}


class WaterSplashBase : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
		+DONTBLAST
	}
	States
	{
	Spawn:
		SPSH EFGHIJK 5;
		Stop;
	}
}

// Lava ---------------------------------------------------------------------

class LavaSplash : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
		+DONTBLAST
	}
	States
	{
	Spawn:
		LVAS ABCDEF 5 Bright;
		Stop;
	}
}

class LavaSmoke : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "Translucent";
		DefaultAlpha;
	}
	States
	{
	Spawn:
		LVAS GHIJK 5 Bright;
		Stop;
	}
}

// Sludge -------------------------------------------------------------------

class SludgeChunk : Actor
{
	default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP 
		+MISSILE 
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+DONTSPLASH
	}
	States
	{
	Spawn:
		SLDG ABCD 8;
		Stop;
	Death:
		SLDG D 6;
		Stop;
	}
}

class SludgeSplash : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
	}
	States
	{
	Spawn:
		SLDG EFGH 6;
		Stop;
	}
}

/*
 * These next four classes are not used by me anywhere.
 * They are for people who want to use them in a TERRAIN lump.
 */

// Blood (water with a different sprite) ------------------------------------

class BloodSplash : Actor
{
	default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP 
		+MISSILE 
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+DONTSPLASH
		+DONTBLAST
	}
	States
	{
	Spawn:
		BSPH ABC 8;
		BSPH D 16;
		Stop;
	Death:
		BSPH D 10;
		Stop;
	}
}


class BloodSplashBase : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
		+DONTBLAST
	}
	States
	{
	Spawn:
		BSPH EFGHIJK 5;
		Stop;
	}
}

// Slime (sludge with a different sprite) -----------------------------------

class SlimeChunk : Actor
{
	default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP 
		+MISSILE 
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+DONTSPLASH
	}
	States
	{
	Spawn:
		SLIM ABCD 8;
		Stop;
	Death:
		SLIM D 6;
		Stop;
	}
}

class SlimeSplash : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY
		+DONTSPLASH
	}
	States
	{
	Spawn:
		SLIM EFGH 6;
		Stop;
	}
}

// Smoke trail for rocket -----------------------------------

class RocketSmokeTrail : Actor
{
	default
	{
		RenderStyle "Translucent";
		Alpha 0.4;
		VSpeed 1;
		+NOBLOCKMAP
		+NOCLIP
		+NOGRAVITY 
		+DONTSPLASH
		+NOTELEPORT
	}
	States
	{
	Spawn:
		RSMK ABCDE 5;
		Stop;
	}
}

class GrenadeSmokeTrail : Actor
{
	default
	{
		RenderStyle "Translucent";
		Alpha 0.4;
		+NOBLOCKMAP 
		+NOCLIP 
		+DONTSPLASH
		+NOTELEPORT
		Gravity 0.1;
		VSpeed 0.5;
		Scale 0.6;
	}
	States
	{
	Spawn:
		RSMK ABCDE 4;
		Stop;
	}
}
