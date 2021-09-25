class LightProbe : Actor native
{
	Default
	{
		+NOINTERACTION
	}

	States
	{
	Spawn:
		LPRO A -1;
		Stop;
	}
}
