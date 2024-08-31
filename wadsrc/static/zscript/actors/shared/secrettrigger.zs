
class SecretTrigger : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+NOGRAVITY
		+DONTSPLASH
		+NOTONAUTOMAP
	}

	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		level.total_secrets++;
	}

	override void Activate (Actor activator)
	{
		Level.GiveSecret(activator, args[0] <= 1, (args[0] == 0 || args[0] == 2));
		Destroy ();
	}

	
}

