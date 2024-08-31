
class SoundEnvironment : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		+NOTONAUTOMAP
	}
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		if (!bDormant)
		{
			Activate (self);
		}
	}

	override void Activate (Actor activator)
	{
		CurSector.SetEnvironmentID((args[0]<<8) | (args[1]));
	}

	// Deactivate just exists so that you can flag the thing as dormant in an editor
	// and not have it take effect. This is so you can use multiple environments in
	// a single zone, with only one set not-dormant, so you know which one will take
	// effect at the start.
	override void Deactivate (Actor deactivator)
	{
		bDormant = true;
	}
	
}
