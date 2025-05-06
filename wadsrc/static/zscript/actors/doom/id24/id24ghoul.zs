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
 * id1 - decohack - ghoul
 ****************************************************************************/

//converted from DECOHACK

class ID24Ghoul : Actor
{
	Default
	{
		Health 50;
		Speed 12;
		Radius 16;
		Height 40;
		ReactionTime 8;
		PainChance 128;
		Mass 50;
		Monster;
		+FLOAT
		+NOGRAVITY
		SeeSound "monsters/ghoul/sight";
		PainSound "monsters/ghoul/pain";
		DeathSound "monsters/ghoul/death";
		ActiveSound "monsters/ghoul/active";
		Obituary "$ID24_OB_GHOUL";
		Tag "$ID24_CC_GHOUL";
	}
	States
	{
	Spawn:
		GHUL AB 10 A_Look;
		Loop;
	See:
		GHUL AABBCCBB 3 A_Chase;
		Loop;
	Missile:
		GHUL DE 4 BRIGHT A_FaceTarget;
		GHUL F 4 BRIGHT MBF21_MonsterProjectile("ID24GhoulBall", 0.0, 0.0, 0.0, -8.0);
		GHUL G 4 BRIGHT;
		Goto See;
	Pain:
		GHUL I 3 BRIGHT;
		GHUL K 3 BRIGHT A_Pain;
		Goto See;
	Death:
		GHUL L 5 BRIGHT;
		GHUL M 5 BRIGHT A_Scream;
		GHUL NO 5 BRIGHT;
		GHUL P 5 BRIGHT A_Fall;
		GHUL QR 5 BRIGHT;
		GHUL S -1;
		Stop;
	}
}

class ID24GhoulBall : Actor
{
	Default
	{
		Damage 3;
		Speed 15;
		FastSpeed 20;
		Radius 6;
		Height 8;
		Projectile;
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "monsters/ghoul/attack";
		DeathSound "monsters/ghoul/shotx";
	}
	States
	{
	Spawn:
		GBAL AB 4 BRIGHT;
		Loop;
	Death:
		GBAL C 5 BRIGHT;
		APBX BCDE 5 BRIGHT;
		Stop;
	}
}
