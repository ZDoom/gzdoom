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
 * id1 - decohack - incinerator
 ****************************************************************************/

class ID24Incinerator : DoomWeapon // Incinerator
{
	Default
	{
		Weapon.SelectionOrder 120;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 20;
		Weapon.AmmoType "ID24Fuel";
		Inventory.PickupMessage "$ID24_GOTINCINERATOR";
		Tag "$TAG_ID24INCINERATOR";
	}
	
	States
	{
	Spawn:
		INCN A -1;
		Stop;
	Ready:
		FLMG A 1 A_WeaponReady;
		Loop;
	Deselect:
		FLMG A 1 A_Lower;
		Loop;
	Select:
		FLMG A 1 A_Raise;
		Loop;
	Fire:
		FLMF A 0 BRIGHT A_Jump(128, "FireAltSound");
		FLMF A 0 BRIGHT A_StartSound("weapons/incinerator/fire1", CHAN_WEAPON);
		Goto DoFire;
	FireAltSound:
		FLMF A 0 BRIGHT A_StartSound("weapons/incinerator/fire2", CHAN_WEAPON);
		Goto DoFire;
	DoFire:
		FLMF A 0 BRIGHT A_GunFlash;
		FLMF A 0 BRIGHT MBF21_ConsumeAmmo(1);
		FLMF A 1 BRIGHT MBF21_WeaponProjectile("ID24IncineratorFlame", 0, 0, 0, 0);
		FLMF B 1 BRIGHT;
		FLMG A 1;
		FLMG A 0 A_ReFire;
		Goto Ready;
	Flash:
		TNT1 A 2 A_Light2;
		TNT1 A 1 A_Light1;
		Goto LightDone;
	}
}

class ID24IncineratorFlame : Actor // Incinerator Flame 
{
	Default
	{
		Damage 5;
		Speed 40;
		Radius 13;
		Height 8;

		Projectile;
		+ZDOOMTRANS;
		+FORCERADIUSDMG;
		RenderStyle "Add";
		Decal "Scorch";
	}

	States
	{
	Spawn:
		TNT1 A 1 BRIGHT;
		IFLM A 2 BRIGHT;
		IFLM B 2 BRIGHT A_StartSound("weapons/incinerator/burn", CHAN_BODY);
		IFLM CDEFGH 2 BRIGHT;
		Stop;
	Death:
		IFLM A 0 BRIGHT A_Jump(128, "DeathSoundAlt");
		IFLM A 0 BRIGHT A_StartSound("weapons/incinerator/hot1", CHAN_BODY);
		Goto DeathExplosion;
	DeathSoundAlt:
		IFLM A 0 BRIGHT A_StartSound("weapons/incinerator/hot2", CHAN_BODY);
		Goto DeathExplosion;
	DeathExplosion:
		IFLM I 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM JI 2 BRIGHT;
		IFLM J 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM KJ 2 BRIGHT;
		IFLM K 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM L 2 BRIGHT;
		IFLM K 2 BRIGHT A_StartSound("weapons/incinerator/hot3", CHAN_BODY);
		IFLM L 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM ML 2 BRIGHT;
		IFLM M 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM NM 2 BRIGHT;
		IFLM N 2 BRIGHT A_RadiusDamage(5, 64);
		IFLM ONO 2 BRIGHT;
		IFLM POP 2 BRIGHT;
		Stop;
	}
}