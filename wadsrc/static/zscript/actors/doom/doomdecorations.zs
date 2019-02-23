
// Tech lamp ---------------------------------------------------------------

class TechLamp : Actor
{
	Default
	{
		Radius 16;
		Height 80;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TLMP ABCD 4 BRIGHT;
		Loop;
	}
}

// Tech lamp 2 -------------------------------------------------------------

class TechLamp2 : Actor
{
	Default
	{
		Radius 16;
		Height 60;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TLP2 ABCD 4 BRIGHT;
		Loop;
	}
}

// Column ------------------------------------------------------------------

class Column : Actor
{
	Default
	{
		Radius 16;
		Height 48;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COLU A -1 BRIGHT;
		Stop;
	}
}

// Tall green column -------------------------------------------------------

class TallGreenColumn : Actor
{
	Default
	{
		Radius 16;
		Height 52;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL1 A -1;
		Stop;
	}
}

// Short green column ------------------------------------------------------

class ShortGreenColumn : Actor
{
	Default
	{
		Radius 16;
		Height 40;;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL2 A -1;
		Stop;
	}
}

// Tall red column ---------------------------------------------------------

class TallRedColumn : Actor
{
	Default
	{
		Radius 16;
		Height 52;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL3 A -1;
		Stop;
	}
}

// Short red column --------------------------------------------------------

class ShortRedColumn : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL4 A -1;
		Stop;
	}
}

// Skull column ------------------------------------------------------------

class SkullColumn : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL6 A -1;
		Stop;
	}
}

// Heart column ------------------------------------------------------------

class HeartColumn : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		COL5 AB 14;
		Loop;
	}
}

// Evil eye ----------------------------------------------------------------

class EvilEye : Actor
{
	Default
	{
		Radius 16;
		Height 54;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		CEYE ABCB 6 BRIGHT;
		Loop;
	}
}

// Floating skull ----------------------------------------------------------

class FloatingSkull : Actor
{
	Default
	{
		Radius 16;
		Height 26;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		FSKU ABC 6 BRIGHT;
		Loop;
	}
}

// Torch tree --------------------------------------------------------------

class TorchTree : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TRE1 A -1;
		Stop;
	}
}

// Blue torch --------------------------------------------------------------

class BlueTorch : Actor
{
	Default
	{
		Radius 16;
		Height 68;;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TBLU ABCD 4 BRIGHT;
		Loop;
	}
}

// Green torch -------------------------------------------------------------

class GreenTorch : Actor
{
	Default
	{
		Radius 16;
		Height 68;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TGRN ABCD 4 BRIGHT;
		Loop;
	}
}

// Red torch ---------------------------------------------------------------

class RedTorch : Actor
{
	Default
	{
		Radius 16;
		Height 68;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TRED ABCD 4 BRIGHT;
		Loop;
	}
}

// Short blue torch --------------------------------------------------------

class ShortBlueTorch : Actor
{
	Default
	{
		Radius 16;
		Height 37;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		SMBT ABCD 4 BRIGHT;
		Loop;
	}
}

// Short green torch -------------------------------------------------------

class ShortGreenTorch : Actor
{
	Default
	{
		Radius 16;
		Height 37;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		SMGT ABCD 4 BRIGHT;
		Loop;
	}
}

// Short red torch ---------------------------------------------------------

class ShortRedTorch : Actor
{
	Default
	{
		Radius 16;
		Height 37;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		SMRT ABCD 4 BRIGHT;
		Loop;
	}
}

// Stalagtite --------------------------------------------------------------

class Stalagtite : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		SMIT A -1;
		Stop;
	}
}

// Tech pillar -------------------------------------------------------------

class TechPillar : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		ELEC A -1;
		Stop;
	}
}

// Candle stick ------------------------------------------------------------

class Candlestick : Actor
{
	Default
	{
		Radius 20;
		Height 14;
		ProjectilePassHeight -16;
	}
	States
	{
	Spawn:
		CAND A -1 BRIGHT;
		Stop;
	}
}

// Candelabra --------------------------------------------------------------

class Candelabra : Actor
{
	Default
	{
		Radius 16;
		Height 60;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		CBRA A -1 BRIGHT;
		Stop;
	}
}

// Bloody twitch -----------------------------------------------------------

class BloodyTwitch : Actor
{
	Default
	{
		Radius 16;
		Height 68;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR1 A 10;
		GOR1 B 15;
		GOR1 C 8;
		GOR1 B 6;
		Loop;
	}
}

// Meat 2 ------------------------------------------------------------------

class Meat2 : Actor
{
	Default
	{
		Radius 16;
		Height 84;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR2 A -1;
		Stop;
	}
}

// Meat 3 ------------------------------------------------------------------

class Meat3 : Actor
{
	Default
	{
		Radius 16;
		Height 84;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR3 A -1;
		Stop;
	}
}

// Meat 4 ------------------------------------------------------------------

class Meat4 : Actor
{
	Default
	{
		Radius 16;
		Height 68;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR4 A -1;
		Stop;
	}
}

// Meat 5 ------------------------------------------------------------------

class Meat5 : Actor
{
	Default
	{
		Radius 16;
		Height 52;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR5 A -1;
		Stop;
	}
}

// Nonsolid meat -----------------------------------------------------------

class NonsolidMeat2 : Meat2
{
	Default
	{
		-SOLID
		Radius 20;
	}
}

class NonsolidMeat3 : Meat3
{
	Default
	{
		-SOLID
		Radius 20;
	}
}

class NonsolidMeat4 : Meat4
{
	Default
	{
		-SOLID
		Radius 20;
	}
}

class NonsolidMeat5 : Meat5
{
	Default
	{
		-SOLID
		Radius 20;
	}
}

// Nonsolid bloody twitch --------------------------------------------------

class NonsolidTwitch : BloodyTwitch
{
	Default
	{
		-SOLID
		Radius 20;
	}
}

// Head on a stick ---------------------------------------------------------

class HeadOnAStick : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		POL4 A -1;
		Stop;
	}
}

// Heads (plural!) on a stick ----------------------------------------------

class HeadsOnAStick : Actor
{
	Default
	{
		Radius 16;
		Height 64;;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		POL2 A -1;
		Stop;
	}
}

// Head candles ------------------------------------------------------------

class HeadCandles : Actor
{
	Default
	{
		Radius 16;
		Height 42;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		POL3 AB 6 BRIGHT;
		Loop;
	}
}

// Dead on a stick ---------------------------------------------------------

class DeadStick : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		POL1 A -1;
		Stop;
	}
}

// Still alive on a stick --------------------------------------------------

class LiveStick : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		POL6 A 6;
		POL6 B 8;
		Loop;
	}
}

// Big tree ----------------------------------------------------------------

class BigTree : Actor
{
	Default
	{
		Radius 32;
		Height 108;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		TRE2 A -1;
		Stop;
	}
}

// Burning barrel ----------------------------------------------------------

class BurningBarrel : Actor
{
	Default
	{
		Radius 16;
		Height 32;
		ProjectilePassHeight -16;
		+SOLID
	}
	States
	{
	Spawn:
		FCAN ABC 4 BRIGHT;
		Loop;
	}
}

// Hanging with no guts ----------------------------------------------------

class HangNoGuts : Actor
{
	Default
	{
		Radius 16;
		Height 88;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB1 A -1;
		Stop;
	}
}

// Hanging from bottom with no brain ---------------------------------------

class HangBNoBrain : Actor
{
	Default
	{
		Radius 16;
		Height 88;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB2 A -1;
		Stop;
	}
}

// Hanging from top, looking down ------------------------------------------

class HangTLookingDown : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB3 A -1;
		Stop;
	}
}

// Hanging from top, looking up --------------------------------------------

class HangTLookingUp : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB5 A -1;
		Stop;
	}
}

// Hanging from top, skully ------------------------------------------------

class HangTSkull : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB4 A -1;
		Stop;
	}
}

// Hanging from top without a brain ----------------------------------------

class HangTNoBrain : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		+SOLID
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB6 A -1;
		Stop;
	}
}

// Colon gibs --------------------------------------------------------------

class ColonGibs : Actor
{
	Default
	{
		Radius 20;
		Height 4;
		+NOBLOCKMAP
		+MOVEWITHSECTOR
	}
	States
	{
	Spawn:
		POB1 A -1;
		Stop;
	}
}

// Small pool o' blood -----------------------------------------------------

class SmallBloodPool : Actor
{
	Default
	{
		Radius 20;
		Height 1;
		+NOBLOCKMAP
		+MOVEWITHSECTOR
	}
	States
	{
	Spawn:
		POB2 A -1;
		Stop;
	}
}

// brain stem lying on the ground ------------------------------------------

class BrainStem : Actor
{
	Default
	{
		Radius 20;
		Height 4;
		+NOBLOCKMAP
		+MOVEWITHSECTOR
	}
	States
	{
	Spawn:
		BRS1 A -1;
		Stop;
	}
}


// Grey stalagmite (unused Doom sprite, definition taken from Skulltag -----

class Stalagmite : Actor
{
	Default
	{
		Radius 16;
		Height 48;
		+SOLID
	}
	States
	{
	Spawn:
		SMT2 A -1;
		Stop;
	}
}

