/*****************************************************************************
 * Copyright (C) 1993-2024 id Software LLC, a ZeniMax Media company.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ****************************************************************************/

/*****************************************************************************
 * id1 - decohack - gore
 ****************************************************************************/

//converted from DECOHACK

class ID24LargeCorpsePile : Actor
{
	Default
	{
		Radius 40;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		POL7 A -1;
		Stop;
	}
}

class ID24HumanBBQ1 : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		HBBQ ABC 5 BRIGHT;
		Loop;
	}
}

class ID24HumanBBQ2 : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		HBB2 ABC 5 BRIGHT;
		Loop;
	}
}

class ID24HangingBodyBothLegs : Actor
{
	Default
	{
		Radius 16;
		Height 80;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR6 A -1;
		Stop;
	}
}

class ID24HangingBodyBothLegsSolid : ID24HangingBodyBothLegs
{
	Default
	{
		+SOLID
	}
}

class ID24HangingBodyCrucified : Actor
{
	Default
	{
		Radius 16;
		Height 64;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR7 A -1;
		Stop;
	}
}

class ID24HangingBodyCrucifiedSolid : ID24HangingBodyCrucified
{
	Default
	{
		+SOLID
	}
}

class ID24HangingBodyArmsBound : Actor
{
	Default
	{
		Radius 16;
		Height 72;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GOR8 A -1;
		Stop;
	}
}

class ID24HangingBodyArmsBoundSolid : ID24HangingBodyArmsBound
{
	Default
	{
		+SOLID
	}
}

class ID24HangingBaronOfHell : Actor
{
	Default
	{
		Radius 16;
		Height 80;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		GORA A -1;
		Stop;
	}
}

class ID24HangingBaronOfHellSolid : ID24HangingBaronOfHell
{
	Default
	{
		+SOLID
	}
}

class ID24HangingChainedBody : Actor
{
	Default
	{
		Radius 16;
		Height 84;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB7 A -1;
		Stop;
	}
}

class ID24HangingChainedBodySolid : ID24HangingChainedBody
{
	Default
	{
		+SOLID
	}
}

class ID24HangingChainedTorso : Actor
{
	Default
	{
		Radius 16;
		Height 52;
		+NOGRAVITY
		+SPAWNCEILING
	}
	States
	{
	Spawn:
		HDB8 A -1;
		Stop;
	}
}

class ID24HangingChainedTorsoSolid : ID24HangingChainedTorso
{
	Default
	{
		+SOLID
	}
}

class ID24SkullPoleTrio : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		POLA A -1;
		Stop;
	}
}

class ID24SkullGibs : Actor
{
	Default
	{
		Radius 16;
		Height 16;
	}
	States
	{
	Spawn:
		POB6 A -1;
		Stop;
	}
}
