class ColorSetter : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "None";
	}
	
	override void PostBeginPlay()
	{
		Super.PostBeginPlay();
		CurSector.SetColor(color(args[0], args[1], args[2]), args[3]);
		Destroy();
	}
	
}


class FadeSetter : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "None";
	}
	
	override void PostBeginPlay()
	{
		Super.PostBeginPlay();
		CurSector.SetFade(color(args[0], args[1], args[2]));
		Destroy();
	}

	
}
