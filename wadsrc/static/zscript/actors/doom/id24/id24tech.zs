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
 * id1 - decohack - tech
 ****************************************************************************/

//converted from DECOHACK

class ID24OfficeChair : Actor
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
		CHR1 A -1;
		Stop;
	}
}

class ID24OfficeLamp : Actor
{
	Default
	{
		Radius 12;
		Height 52;
		Health 20;
		Mass 65536;
		DeathSound "world/officelamp";
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+DONTGIB
		+NOICEDEATH
	}
	States
	{
	Spawn:
		LAMP A -1 BRIGHT;
		Stop;
	Death:
		LAMP B 3 BRIGHT A_Scream;
		LAMP B 3 BRIGHT A_Fall;
	DeathLoop:
		LAMP CD 5 BRIGHT;
		Loop;
	}
}

class ID24CeilingLamp : Actor
{
	Default
	{
		Radius 12;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		TLP6 A -1 BRIGHT;
		Stop;
	}
}

class ID24CandelabraShort : Actor
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
		CBR2 A -1 BRIGHT;
		Stop;
	}
}
