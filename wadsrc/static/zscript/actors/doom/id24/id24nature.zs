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
 * id1 - decohack - nature
 ****************************************************************************/

//converted from DECOHACK

class ID24GrayStalagmite : Actor
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
		SMT2 A -1;
		Stop;
	}
}

class ID24BushShort : Actor
{
	Default
	{
		Radius 12;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		BSH1 A -1;
		Stop;
	}
}

class ID24BushShortBurned1 : Actor
{
	Default
	{
		Radius 12;
		Height 16;
	}
	States
	{
	Spawn:
		BSH1 B -1;
		Stop;
	}
}

class ID24BushShortBurned2 : Actor
{
	Default
	{
		Radius 12;
		Height 16;
	}
	States
	{
	Spawn:
		BSH1 C -1;
		Stop;
	}
}

class ID24BushTall : Actor
{
	Default
	{
		Radius 12;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		BSH2 A -1;
		Stop;
	}
}

class ID24BushTallBurned1 : Actor
{
	Default
	{
		Radius 12;
		Height 16;
	}
	States
	{
	Spawn:
		BSH2 B -1;
		Stop;
	}
}

class ID24BushTallBurned2 : Actor
{
	Default
	{
		Radius 12;
		Height 16;
	}
	States
	{
	Spawn:
		BSH2 C -1;
		Stop;
	}
}

class ID24CaveRockColumn : Actor
{
	Default
	{
		Radius 24;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		STMI A -1;
		Stop;
	}
}

class ID24CaveStalagmiteLarge : Actor
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
		STG1 A -1;
		Stop;
	}
}

class ID24CaveStalagmiteMedium : Actor
{
	Default
	{
		Radius 12;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		STG2 A -1;
		Stop;
	}
}

class ID24CaveStalagmiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		STG3 A -1;
		Stop;
	}
}

class ID24CaveStalactiteLarge : Actor
{
	Default
	{
		Radius 16;
		Height 112;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		STC1 A -1;
		Stop;
	}
}

class ID24CaveStalactiteLargeSolid : ID24CaveStalactiteLarge
{
	Default
	{
		+SOLID
	}
}

class ID24CaveStalactiteMedium : Actor
{
	Default
	{
		Radius 12;
		Height 64;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		STC2 A -1;
		Stop;
	}
}

class ID24CaveStalactiteMediumSolid : ID24CaveStalactiteMedium
{
	Default
	{
		+SOLID
	}
}

class ID24CaveStalactiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		STC3 A -1;
		Stop;
	}
}

class ID24CaveStalactiteSmallSolid : ID24CaveStalactiteSmall
{
	Default
	{
		+SOLID
	}
}
