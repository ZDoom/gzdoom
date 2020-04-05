
class StrifeWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 100;
	}
}

// Same as the bullet puff for Doom -----------------------------------------

class StrifePuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+ALLOWPARTICLES
		RenderStyle "Translucent";
		Alpha 0.25;
	}

	States
	{
	Spawn:
		POW3 ABCDEFGH 3;
		Stop;
	Crash:
		PUFY A 4 Bright;
		PUFY BCD 4;
		Stop;
	}
}
	

// A spark when you hit something that doesn't bleed ------------------------
// Only used by the dagger.

class StrifeSpark : StrifePuff
{
	Default
	{
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Crash:
		POW2 ABCD 4;
		Stop;
	}
}

