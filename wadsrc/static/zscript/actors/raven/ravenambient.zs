

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
		TNT1 A 2 A_StartSound("world/wind", CHAN_6, CHANF_LOOPING);
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
		TNT1 A 2 A_StartSound("world/waterfall", CHAN_6, CHANF_LOOPING);
		Loop;
	}
}
