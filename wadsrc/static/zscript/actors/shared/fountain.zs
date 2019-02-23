class ParticleFountain : Actor
{
	enum EColor
	{
		REDFOUNTAIN		= 1,
		GREENFOUNTAIN	= 2,
		BLUEFOUNTAIN	= 3,
		YELLOWFOUNTAIN	= 4,
		PURPLEFOUNTAIN	= 5,
		BLACKFOUNTAIN	= 6,
		WHITEFOUNTAIN	= 7
	}

	default
	{
		Height 0;
		+NOBLOCKMAP
		+NOGRAVITY
		+INVISIBLE
	}
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		if (!(SpawnFlags & MTF_DORMANT))
			Activate (null);
	}

	override void Activate (Actor activator)
	{
		Super.Activate (activator);
		fountaincolor = health;
	}

	override void Deactivate (Actor activator)
	{
		Super.Deactivate (activator);
		fountaincolor = 0;
	}
}

class RedParticleFountain : ParticleFountain
{
	default
	{
		Health REDFOUNTAIN;
	}
}

class GreenParticleFountain : ParticleFountain
{
	default
	{
		Health GREENFOUNTAIN;
	}
}

class BlueParticleFountain : ParticleFountain
{
	default
	{
		Health BLUEFOUNTAIN;
	}
}

class YellowParticleFountain : ParticleFountain
{
	default
	{
		Health YELLOWFOUNTAIN;
	}
}

class PurpleParticleFountain : ParticleFountain
{
	default
	{
		Health PURPLEFOUNTAIN;
	}
}

class BlackParticleFountain : ParticleFountain
{
	default
	{
		Health BLACKFOUNTAIN;
	}
}

class WhiteParticleFountain : ParticleFountain
{
	default
	{
		Health WHITEFOUNTAIN;
	}
}
