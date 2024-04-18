class TimeBombWeapon : Weapon replaces BFG9000
{
	default
	{
		scale 0.30;
		weapon.kickback 100;
		weapon.selectionorder 2800;
		weapon.ammouse 1;
		weapon.ammogive 2;
		weapon.ammotype "TimeBombAmmo";
		inventory.pickupmessage "$GOTBFG9000";
		inventory.icon "BFUGA0";
		+WEAPON.NOAUTOFIRE;
		+WEAPON.NOAUTOAIM;
	}

	states
	{
		Select:
			BFGG A 1 A_Raise;
			loop;
		Deselect:
			BFGG A 1 A_Lower;
			loop;
		Ready:
			BFGG A 1 A_WeaponReady;
			loop;
		Fire:
			SHT2 A 3;
			SHT2 A 7 A_CheckReload;
			SHT2 B 1 A_BFGSound;
			SHT2 B 7;
			SHT2 C 6;
			SHT2 D 7 A_FireGrenade;
			SHT2 E 7;
			SHT2 F 7 A_Light0;
			SHT2 H 6 A_Light0;
			SHT2 A 5 A_Refire;
			goto Ready;
		Flash:
			BFGF A 11 bright A_Light1;
			BFGF B 6 bright A_Light2;
			SHTG E 0 A_Light0;
			stop;
		Spawn:
			BFUG A -1;
			stop;
	}

	action void A_FireGrenade()
	{
		if (player == null)
		{
			return;
		}
		Weapon weap = player.ReadyWeapon;
		if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weap.DepleteAmmo (weap.bAltFire, true, weap.ammouse1))
				return;
		}

		SpawnPlayerMissile("BFGBall", angle, nofreeaim:sv_nobfgaim);
	}
}
