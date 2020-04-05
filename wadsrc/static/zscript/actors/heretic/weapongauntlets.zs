// Gauntlets ----------------------------------------------------------------

class Gauntlets : Weapon
{
	Default
	{
		+BLOODSPLATTER
		Weapon.SelectionOrder 2300;
		+WEAPON.WIMPY_WEAPON
		+WEAPON.MELEEWEAPON
		Weapon.Kickback 0;
		Weapon.YAdjust 15;
		Weapon.UpSound "weapons/gauntletsactivate";
		Weapon.SisterWeapon "GauntletsPowered";
		Inventory.PickupMessage "$TXT_WPNGAUNTLETS";
		Tag "$TAG_GAUNTLETS";
		Obituary "$OB_MPGAUNTLETS";
	}

	States
	{
	Spawn:
		WGNT A -1;
		Stop;
	Ready:
		GAUN A 1 A_WeaponReady;
		Loop;
	Deselect:
		GAUN A 1 A_Lower;
		Loop;
	Select:
		GAUN A 1 A_Raise;
		Loop;
	Fire:
		GAUN B 4 A_StartSound("weapons/gauntletsuse", CHAN_WEAPON);
		GAUN C 4;
	Hold:
		GAUN DEF 4 BRIGHT A_GauntletAttack(0);
		GAUN C 4 A_ReFire;
		GAUN B 4 A_Light0;
		Goto Ready;
	}
	
	//---------------------------------------------------------------------------
	//
	// PROC A_GauntletAttack
	//
	//---------------------------------------------------------------------------

	action void A_GauntletAttack (int power)
	{
		int damage;
		double dist;
		Class<Actor> pufftype;
		FTranslatedLineTarget t;
		int actualdamage = 0;
		Actor puff;

		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
			
			let psp = player.GetPSprite(PSP_WEAPON);
			if (psp)
			{
				psp.x = ((random[GauntletAtk](0, 3)) - 2);
				psp.y = WEAPONTOP + (random[GauntletAtk](0, 3));
			}
		}
		double ang = angle;
		if (power)
		{
			damage = random[GauntletAtk](1, 8) * 2;
			dist = 4*DEFMELEERANGE;
			ang += random2[GauntletAtk]() * (2.8125 / 256);
			pufftype = "GauntletPuff2";
		}
		else
		{
			damage = random[GauntletAtk](1, 8) * 2;
			dist = SAWRANGE;
			ang += random2[GauntletAtk]() * (5.625 / 256);
			pufftype = "GauntletPuff1";
		}
		double slope = AimLineAttack (ang, dist);
		[puff, actualdamage] = LineAttack (ang, dist, slope, damage, 'Melee', pufftype, false, t);
		if (!t.linetarget)
		{
			if (random[GauntletAtk]() > 64)
			{
				player.extralight = !player.extralight;
			}
			A_StartSound ("weapons/gauntletson", CHAN_AUTO);
			return;
		}
		int randVal = random[GauntletAtk]();
		if (randVal < 64)
		{
			player.extralight = 0;
		}
		else if (randVal < 160)
		{
			player.extralight = 1;
		}
		else
		{
			player.extralight = 2;
		}
		if (power)
		{
			if (!t.linetarget.bDontDrain) GiveBody (actualdamage >> 1);
			A_StartSound ("weapons/gauntletspowhit", CHAN_AUTO);
		}
		else
		{
			A_StartSound ("weapons/gauntletshit", CHAN_AUTO);
		}
		// turn to face target
		ang = t.angleFromSource;
		double anglediff = deltaangle(angle, ang);

		if (anglediff < 0.0)
		{
			if (anglediff < -4.5)
				angle = ang + 90.0 / 21;
			else
				angle -= 4.5;
		}
		else
		{
			if (anglediff > 4.5)
				angle = ang - 90.0 / 21;
			else
				angle += 4.5;
		}
		bJustAttacked = true;
	}
}


class GauntletsPowered : Gauntlets
{
	Default
	{
		+WEAPON.POWERED_UP
		Tag "$TAG_GAUNTLETSP";
		Obituary "$OB_MPPGAUNTLETS";
		Weapon.SisterWeapon "Gauntlets";
	}

	States
	{
	Ready:
		GAUN GHI 4 A_WeaponReady;
		Loop;
	Deselect:
		GAUN G 1 A_Lower;
		Loop;
	Select:
		GAUN G 1 A_Raise;
		Loop;
	Fire:
		GAUN J 4 A_StartSound("weapons/gauntletsuse", CHAN_WEAPON);
		GAUN K 4;
	Hold:
		GAUN LMN 4 BRIGHT A_GauntletAttack(1);
		GAUN K 4 A_ReFire;
		GAUN J 4 A_Light0;
		Goto Ready;
	}
}


// Gauntlet puff 1 ----------------------------------------------------------

class GauntletPuff1 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+PUFFONACTORS
		RenderStyle "Translucent";
		Alpha 0.4;
		VSpeed 0.8;
	}

	States
	{
	Spawn:
		PUF1 ABCD 4 BRIGHT;
		Stop;
	}
}

// Gauntlet puff 2 ---------------------------------------------------------

class GauntletPuff2 : GauntletPuff1
{
	States
	{
	Spawn:
		PUF1 EFGH 4 BRIGHT;
		Stop;
	}
}

