

// Wind ---------------------------------------------------------------------

class SoundWind : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+DONTSPLASH
	}
	States
	{
	Spawn:
		TNT1 A 2 A_PlaySound("world/wind", CHAN_6, 1, true);
		Loop;
	}
}

class SoundWindHexen : SoundWind
{
}


// Waterfall ----------------------------------------------------------------

class SoundWaterfall : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+DONTSPLASH
	}
	States
	{
	Spawn:
		TNT1 A 2 A_PlaySound("world/waterfall", CHAN_6, 1, true);
		Loop;
	}
}
