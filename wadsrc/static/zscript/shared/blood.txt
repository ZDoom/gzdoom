
// Blood sprite ------------------------------------------------------------

class Blood : Actor
{
	Default
	{
		Mass 5;
		+NOBLOCKMAP
		+NOTELEPORT
		+ALLOWPARTICLES
	}
	States
	{
	Spawn:
		BLUD CBA 8;
		Stop;
	Spray:
		SPRY ABCDEF 3;
		SPRY G 2;
		Stop;
	}
}

// Blood splatter -----------------------------------------------------------

class BloodSplatter : Actor
{
	Default
	{
		Radius 2;
		Height 4;
		+NOBLOCKMAP
		+MISSILE
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+ALLOWPARTICLES
		Mass 5;
	}
	States
	{
	Spawn:
		BLUD CBA 8;
		Stop;
	Death:
		BLUD A 6;
		Stop;
	}
}
	
// Axe Blood ----------------------------------------------------------------

class AxeBlood : Actor
{
	Default
	{
		Radius 2;
		Height 4;
		+NOBLOCKMAP
		+NOGRAVITY
		+DROPOFF
		+NOTELEPORT
		+CANNOTPUSH
		+ALLOWPARTICLES
		Mass 5;
	}
	States
	{
	Spawn:
		FAXE FGHIJ 3;
	Death:
		FAXE K 3;
		Stop;
	}
}

	