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
 * id1 - decohack - vassago
 ****************************************************************************/

//converted from DECOHACK

class ID24Vassago : Actor
{
	Default
	{
		Health 1000;
		Speed 8;
		Radius 24;
		Height 64;
		ReactionTime 8;
		PainChance 100;
		Mass 1000;
		SplashGroup 1;
		Monster;
		SeeSound "monsters/vassago/sight";
		PainSound "monsters/vassago/pain";
		DeathSound "monsters/vassago/death";
		ActiveSound "monsters/vassago/active";
		Obituary "$ID24_OB_VASSAGO";
		Tag "$ID24_CC_VASSAGO";
	}
	States
	{
	Spawn:
		VASS AB 10 A_Look;
		Loop;
	See:
		VASS AABBCCDD 3 A_Chase;
		Loop;
	Melee:
	Missile:
		VASS E 0 BRIGHT A_StartSound("monsters/vassago/attack");
		VASS E 8 BRIGHT A_FaceTarget;
		VASS FG 4 BRIGHT A_FaceTarget;
		VASS H 8 BRIGHT MBF21_MonsterProjectile("ID24VassagoFlame", 0.0, 0.0, 0.0, 0.0);
		Goto See;
	Pain:
		VASS I 2;
		VASS I 2 A_Pain;
		Goto See;
	Death:
		VASS J 8 BRIGHT;
		VASS K 8 BRIGHT A_Scream;
		VASS L 7 BRIGHT;
		VASS M 6 BRIGHT A_Fall;
		VASS NO 6 BRIGHT;
		VASS P 7 BRIGHT;
		VASS Q -1 A_BossDeath;
		Stop;
	Raise:
		VASS P 8;
		VASS ONMLKJ 8;
		Goto See;
	}
}

class ID24VassagoFlame : Actor
{
	Default
	{
		Damage 5;
		Speed 15;
		FastSpeed 20;
		SplashGroup 1;
		Radius 6;
		Height 16;
		Projectile;
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "monsters/vassago/burn";
		DeathSound "monsters/vassago/shotx";
	}
	States
	{
	Spawn:
		VFLM AB 4 BRIGHT;
		Loop;
	Death:
		VFLM C 0 BRIGHT { bNoGravity = false; }
		VFLM C 0 BRIGHT A_ChangeLinkFlags(0, 0);
		VFLM C 0 BRIGHT A_Jump(128, "DeathSound2");
		VFLM C 0 BRIGHT A_StartSound("monsters/vassago/hot1");
		goto Burninate;
	DeathSound2:
		VFLM C 0 BRIGHT A_StartSound("monsters/vassago/hot2");
		Goto Burninate;
	Burninate:
		VFLM C 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM D 4 BRIGHT;
		VFLM E 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot3");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot2");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot3");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot1");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot2");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot1");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM F 0 BRIGHT A_StartSound("monsters/vassago/hot2");
		VFLM F 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM G 4 BRIGHT;
		VFLM H 4 BRIGHT;
		VFLM I 0 BRIGHT A_StartSound("monsters/vassago/hot3");
		VFLM I 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM J 4 BRIGHT;
		VFLM K 4 BRIGHT;
		VFLM L 4 BRIGHT A_RadiusDamage(10, 128);
		VFLM MNOPQ 4 BRIGHT;
		Stop;
	}
}
