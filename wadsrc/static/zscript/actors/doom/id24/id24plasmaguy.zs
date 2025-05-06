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
 * id1 - decohack - former shocktrooper (a.k.a. "plasma guy" or "plasmadick")
 ****************************************************************************/

//converted from DECOHACK

class ID24PlasmaGuy : Actor
{
	Default
	{
		Health 100;
		Speed 10;
		Radius 20;
		Height 56;
		ReactionTime 8;
		PainChance 30;
		Mass 100;
		Monster;
		PainSound "monsters/plasmaguy/pain";
		DeathSound "monsters/plasmaguy/death";
		ActiveSound "monsters/plasmaguy/active";
		Obituary "$ID24_OB_SHOCKTROOPER";
		Tag "$ID24_CC_SHOCKTROOPER";
	}
	States
	{
	Spawn:
		PPOS AB 10 A_Look;
		Loop;
	See:
		PPOS AABBCCDD 2 Fast A_Chase;
		Loop;
	Missile:
		PPOS E 10 A_FaceTarget;
		PPOS F 2 Fast MBF21_MonsterProjectile("PlasmaBall", 0.0, 0.0, 0.0, 0.0);
		PPOS E 4 Fast;
		PPOS F 2 Fast MBF21_MonsterProjectile("PlasmaBall", 0.0, 0.0, 0.0, 0.0);
		PPOS E 4 Fast;
		PPOS F 2 Fast MBF21_MonsterProjectile("PlasmaBall", 0.0, 0.0, 0.0, 0.0);
		PPOS E 4 Fast;
		Goto See;
	Pain:
		PPOS G 5 Fast;
		PPOS G 5 Fast A_Pain;
		Goto See;
	Death:
		PPOS H 0 A_FaceTarget;
		PPOS H 5 MBF21_SpawnObject("ID24PlasmaGuyHead", 175.0, 0.0, 0.0, 40.0, 2.0, 0.0, 1.5);
		PPOS I 5 A_Scream;
		PPOS J 5 A_Fall;
		PPOS KL 5;
		PPOS M -1;
		Stop;
	XDeath:
		PPOS N 5;
		PPOS O 5 A_XScream;
		PPOS P 5 A_Fall;
		PPOS Q 0 A_FaceTarget;
		PPOS Q 5 MBF21_SpawnObject("ID24PlasmaGuyTorso", 170.0, 0.0, -8.0, 32.0, 4.0, 0.0, 2.0);
		PPOS RST 5;
		PPOS U -1;
		Stop;
	Raise:
		PPOS MLKJIH 5;
		Goto See;
	}
}

class ID24PlasmaGuyHead : Actor
{
	Default
	{
		Damage 0;
		Speed 8;
		Radius 6;
		Height 16;
		Gravity 0.125;
		+MISSILE
		+DROPOFF
		DeathSound "monsters/plasmaguy/head";
	}
	States
	{
	Spawn:
		PHED ABCDEFGHI 3;
		Loop;
	Death:
		PHED J -1;
		Loop;
	}
}

class ID24PlasmaGuyTorso : Actor
{
	Default
	{
		Damage 0;
		Speed 8;
		Radius 6;
		Height 16;
		+NOBLOCKMAP
		+MISSILE
		+DROPOFF
	}
	States
	{
	Spawn:
		PPOS V -1;
		Loop;
	Death:
		PPOS W 5;
		PPOS X -1;
		Stop;
	}
}
