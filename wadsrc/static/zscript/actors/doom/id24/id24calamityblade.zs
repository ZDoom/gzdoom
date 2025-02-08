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
 * id1 - decohack - heatwave generator
 ****************************************************************************/

class ID24CalamityBlade : DoomWeapon // Heatwave Generator
{
	Default
	{
		Weapon.SelectionOrder 1000;
		Weapon.AmmoUse 10;
		Weapon.AmmoGive 20;
		Weapon.AmmoType "ID24Fuel";
		+Weapon.NoAutoAim;
		Inventory.PickupMessage "$ID24_GOTCALAMITYBLADE";
		Tag "$TAG_ID24CALAMITYBLADE";
	}
	
	action void A_CheckAmmo(StateLabel st, int amount)
	{
		MBF21_CheckAmmo(ResolveState(st), amount);
	}
	
	action void A_GunFlashTo(StateLabel st, int dontchangeplayer)
	{
		MBF21_GunFlashTo(ResolveState(st), dontchangeplayer);
	}
	
	action void A_RefireTo(StateLabel st, int skipcheck)
	{
		MBF21_RefireTo(ResolveState(st), skipcheck);
	}
	
	States
	{
	Spawn:
		CBLD A -1;
		Stop;
	Ready:
		HETG A 1 A_WeaponReady;
		Loop;
	Deselect:
		HETG A 1 A_Lower;
		Loop;
	Select:
		HETG A 1 A_Raise;
		Loop;
	Fire:
	Hold0:
		HETG A 0 MBF21_ConsumeAmmo(0);
		HETG A 0 A_GunFlashTo("Hold0Flash", 1);
		HETG A 20 A_StartSound("weapons/calamityblade/charge", CHAN_WEAPON);
		HETG A 0 A_CheckAmmo("Hold0FireProjectiles", 0);
		HETG A 0 A_RefireTo("Hold1", 0);
		Goto Hold0FireProjectiles;
	Hold1:
		HETG A 0 MBF21_ConsumeAmmo(0);
		HETG A 0 A_GunFlashTo("Hold1Flash", 1);
		HETG A 20 A_StartSound("weapons/calamityblade/charge", CHAN_WEAPON);
		HETG A 0 A_CheckAmmo("Hold1FireProjectiles", 0);
		HETG A 0 A_RefireTo("Hold2", 0);
		Goto Hold1FireProjectiles;
	Hold2:
		HETG A 0 MBF21_ConsumeAmmo(0);
		HETG A 0 A_GunFlashTo("Hold2Flash", 1);
		HETG A 20 A_StartSound("weapons/calamityblade/charge", CHAN_WEAPON);
		HETG A 0 A_CheckAmmo("Hold2FireProjectiles", 0);
		HETG A 0 A_RefireTo("Hold3", 0);
		Goto Hold2FireProjectiles;
	Hold3:
		HETG A 0 MBF21_ConsumeAmmo(0);
		HETG A 0 A_GunFlashTo("Hold3Flash", 1);
		HETG A 20 A_StartSound("weapons/calamityblade/charge", CHAN_WEAPON);
		HETG A 0 A_CheckAmmo("Hold3FireProjectiles", 0);
		HETG A 0 A_RefireTo("Hold4", 0);
		Goto Hold3FireProjectiles;
	Hold4:
		HETG A 0 MBF21_ConsumeAmmo(0);
		HETG A 0 A_GunFlashTo("Hold4Flash", 1);
		HETG A 20 A_StartSound("weapons/calamityblade/charge", CHAN_WEAPON);
		Goto Hold4FireProjectiles;
	Hold4FireProjectiles:
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -35, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -30, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -25, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -20, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -15, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -10, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 0, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 10, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 15, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 20, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 25, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 30, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 35, 0, 0, 0);
		Goto FireAnim;
	Hold3FireProjectiles:
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -27.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -22.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -17.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -12.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -7.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -2.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 2.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 7.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 12.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 17.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 22.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 27.5, 0, 0, 0);
		Goto FireAnim;
	Hold2FireProjectiles:
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -20, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -15, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -10, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 0, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 10, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 15, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 20, 0, 0, 0);
		Goto FireAnim;
	Hold1FireProjectiles:
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -12.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -7.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -2.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 2.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 7.5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 12.5, 0, 0, 0);
		Goto FireAnim;
	Hold0FireProjectiles:
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", -5, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 0, 0, 0, 0);
		HETF A 0 BRIGHT MBF21_WeaponProjectile("ID24IncineratorProjectile", 5, 0, 0, 0);
		Goto FireAnim;
	FireAnim:
		HETF A 0 BRIGHT A_StartSound("weapons/calamityblade/shoot", CHAN_WEAPON);
		HETF A 3 BRIGHT A_GunFlash;
		HETF B 5 BRIGHT;
		HETG D 4;
		HETG C 4;
		HETG B 4;
		HETG A 0 A_ReFire;
		Goto Ready;
	Hold0Flash:
		HETC A 6 BRIGHT;
		HETC BCD 5 BRIGHT;
		Goto LightDone;
	Hold1Flash:
		HETC E 6 BRIGHT;
		HETC FGH 5 BRIGHT;
		Goto LightDone;
	Hold2Flash:
		HETC I 6 BRIGHT;
		HETC JKL 5 BRIGHT;
		Goto LightDone;
	Hold3Flash:
		HETC M 6 BRIGHT;
		HETC NOP 5 BRIGHT;
		Goto LightDone;
	Hold4Flash:
		HETC Q 6 BRIGHT;
		HETC RST 5 BRIGHT;
		Goto LightDone;
	Flash:
		TNT1 A 3 A_Light1;
		TNT1 A 5 A_Light2;
		Goto LightDone;
	}
}

class ID24IncineratorProjectile : Actor // Heatwave Ripper 
{
	Default
	{
		Damage 10;
		Speed 20;
		Radius 16;
		Height 8;

		Projectile;
		+ZDOOMTRANS;
		+RIPPER;
		RenderStyle "Add";

		DeathSound "weapons/calamityblade/explode";
	}

	States
	{
		Spawn:
			HETB ABC 3 Bright;
			Loop;
		Death:
			HETB DEFGHI 3 Bright;
			Stop;
	}
}