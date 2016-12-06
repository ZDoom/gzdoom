Class SkullHang70 : Actor
{
	Default
	{
		Radius 20;
		Height 70;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SKH1 A -1;
		Stop;
	}
}
	
Class SkullHang60 : Actor
{
	Default
	{
		Radius 20;
		Height 60;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SKH2 A -1;
		Stop;
	}
}
	
Class SkullHang45 : Actor
{
	Default
	{
		Radius 20;
		Height 45;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SKH3 A -1;
		Stop;
	}
}
	
Class SkullHang35 : Actor
{
	Default
	{
		Radius 20;
		Height 35;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SKH4 A -1;
		Stop;
	}
}
	
Class Chandelier : Actor
{
	Default
	{
		Radius 20;
		Height 60;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		CHDL ABC 4;
		Loop;
	}
}

Class SerpentTorch : Actor
{
	Default
	{
		Radius 12;
		Height 54;
		+SOLID
	}
	States
	{
	Spawn:
		SRTC ABC 4;
		Loop;
	}
}

Class SmallPillar : Actor
{
	Default
	{
		Radius 16;
		Height 34;
		+SOLID
	}
	States
	{
	Spawn:
		SMPL A -1;
		Stop;
	}
}

Class StalagmiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		STGS A -1;
		Stop;
	}
}

Class StalagmiteLarge : Actor
{
	Default
	{
		Radius 12;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		STGL A -1;
		Stop;
	}
}

Class StalactiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 36;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		STCS A -1;
		Stop;
	}
}

Class StalactiteLarge : Actor
{
	Default
	{
		Radius 12;
		Height 68;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		STCL A -1;
		Stop;
	}
}

Class FireBrazier : Actor
{
	Default
	{
		Radius 16;
		Height 44;
		+SOLID
	}
	States
	{
	Spawn:
		KFR1 ABCDEFGH 3 Bright;
		Loop;
	}
}

Class Barrel : Actor
{
	Default
	{
		Radius 12;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		BARL A -1;
		Stop;
	}
}

Class BrownPillar : Actor
{
	Default
	{
		Radius 14;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		BRPL A -1;
		Stop;
	}
}

Class Moss1 : Actor
{
	Default
	{
		Radius 20;
		Height 23;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		MOS1 A -1;
		Stop;
	}
}

Class Moss2 : Actor
{
	Default
	{
		Radius 20;
		Height 27;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		MOS2 A -1;
		Stop;
	}
}

Class WallTorch : Actor
{
	Default
	{
		Radius 6;
		Height 16;
		+NOGRAVITY
		+FIXMAPTHINGPOS
	}
	States
	{
	Spawn:
		WTRH ABC 6 Bright;
		Loop;
	}
}

Class HangingCorpse : Actor
{
	Default
	{
		Radius 8;
		Height 104;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		HCOR A -1;
		Stop;
	}
}

