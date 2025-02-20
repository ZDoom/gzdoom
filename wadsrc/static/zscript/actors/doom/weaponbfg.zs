// --------------------------------------------------------------------------
//
// BFG 9000
//
// --------------------------------------------------------------------------

class BFG9000 : DoomWeapon
{
	Default
	{
		Height 20;
		Weapon.SelectionOrder 2800;
		Weapon.AmmoUse 40;
		Weapon.AmmoGive 40;
		Weapon.AmmoType "Cell";
		+WEAPON.NOAUTOFIRE;
		+WEAPON.BFG;
		Inventory.PickupMessage "$GOTBFG9000";
		Tag "$TAG_BFG9000";
	}
	States
	{
	Ready:
		BFGG A 1 A_WeaponReady;
		Loop;
	Deselect:
		BFGG A 1 A_Lower;
		Loop;
	Select:
		BFGG A 1 A_Raise;
		Loop;
	Fire:
		BFGG A 20 A_BFGsound;
		BFGG B 10 A_GunFlash;
		BFGG B 10 A_FireBFG;
		BFGG B 20 A_ReFire;
		Goto Ready;
	Flash:
		BFGF A 11 Bright A_Light1;
		BFGF B 6 Bright A_Light2;
		Goto LightDone;
	Spawn:
		BFUG A -1;
		Stop;
	OldFire:
		BFGG A 10 A_BFGsound;
		BFGG BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB 1 A_FireOldBFG;
		BFGG B 0 A_Light0;
		BFGG B 20 A_ReFire;
		Goto Ready;
	}
}

//===========================================================================
//
// Weapon code (must be attached to StateProvider)
//
//===========================================================================

extend class StateProvider
{
	action void A_BFGsound() 
	{ 
		A_StartSound("weapons/bfgf", CHAN_WEAPON); 
	}
	

	//
	// A_FireBFG
	//

	action void A_FireBFG()
	{
		if (player == null)
		{
			return;
		}
		Weapon weap = player.ReadyWeapon;
		if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weap.DepleteAmmo (weap.bAltFire, true, deh.BFGCells))
				return;
		}

		SpawnPlayerMissile("BFGBall", angle, nofreeaim:sv_nobfgaim);
	}


	//
	// A_FireOldBFG
	//
	// This function emulates Doom's Pre-Beta BFG
	// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
	//
	// This code may not be used in other mods without appropriate credit given.
	// Code leeches will be telefragged.

	action void A_FireOldBFG()
	{
		bool doesautoaim = false;

		if (player == null)
		{
			return;
		}
		Weapon weap = player.ReadyWeapon;

		if (invoker != weap || stateinfo == null || stateinfo.mStateType != STATE_Psprite) weap = null;
		if (weap != null)
		{
			if (!weap.DepleteAmmo (weap.bAltFire, true))
				return;

			doesautoaim = weap.bNoAutoaim;
			weap.bNoAutoaim = true;
		}
		player.extralight = 2;

		// Save values temporarily
		double SavedPlayerAngle = angle;
		double SavedPlayerPitch = pitch;
		for (int i = 0; i < 2; i++) // Spawn two plasma balls in sequence
		{
			angle += random[OldBFG](-64, 63) * (90./768);
			pitch += random[OldBFG](-64, 63) * (90./640);
			SpawnPlayerMissile (i == 0? (class<Actor>)("PlasmaBall1") : (class<Actor>)("PlasmaBall2"));
			// Restore saved values
			angle = SavedPlayerAngle;
			pitch = SavedPlayerPitch;
		}
		// Restore autoaim setting
		if (weap != null) weap.bNoAutoaim = doesautoaim;
	}
}

class BFGBall : Actor
{
	Default
	{
		Radius 13;
		Height 8;
		Speed 25;
		Damage 100;
		Projectile;
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 0.75;
		DeathSound "weapons/bfgx";
		Obituary "$OB_MPBFG_BOOM";
	}
	States
	{
	Spawn:
		BFS1 AB 4 Bright;
		Loop;
	Death:
		BFE1 AB 8 Bright;
		BFE1 C 8 Bright A_BFGSpray;
		BFE1 DEF 8 Bright;
		Stop;
	}
}
		
class BFGExtra : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 0.75;
		DamageType "BFGSplash";
	}
	States
	{
	Spawn:
		BFE2 ABCD 8 Bright;
		Stop;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	//
	// A_BFGSpray
	// Spawn a BFG explosion on every monster in view
	//
	void A_BFGSpray(class<Actor> spraytype = "BFGExtra", int numrays = 40, int damagecnt = 15, double ang = 90, double distance = 16*64, double vrange = 32, int defdamage = 0, int flags = 0)
	{
		int damage;
		FTranslatedLineTarget t;

		// validate parameters
		if (spraytype == null) spraytype = "BFGExtra";
		if (numrays <= 0) numrays = 40;
		if (damagecnt <= 0) damagecnt = 15;
		if (ang == 0) ang = 90.;
		if (distance <= 0) distance = 16 * 64;
		if (vrange == 0) vrange = 32.;

		// [RH] Don't crash if no target
		if (!target) return;

		// [XA] Set the originator of the rays to the projectile (self) if
		//      the new flag is set, else set it to the player (target)
		Actor originator = (flags & BFGF_MISSILEORIGIN) ? self : target;

		// offset angles from its attack ang
		for (int i = 0; i < numrays; i++)
		{
			double an = angle - ang / 2 + ang / numrays*i;

			originator.AimLineAttack(an, distance, t, vrange);

			if (t.linetarget != null)
			{
				Actor spray = Spawn(spraytype, t.linetarget.pos + (0, 0, t.linetarget.Height / 4), ALLOW_REPLACE);

				int dmgFlags = 0;
				Name dmgType = 'BFGSplash';

				if (spray != null)
				{
					if ((spray.bMThruSpecies && target.GetSpecies() == t.linetarget.GetSpecies()) || 
						(!(flags & BFGF_HURTSOURCE) && target == t.linetarget)) // [XA] Don't hit oneself unless we say so.
					{
						spray.Destroy(); // [MC] Remove it because technically, the spray isn't trying to "hit" them.
						continue;
					}
					if (spray.bPuffGetsOwner) spray.target = target;
					if (spray.bFoilInvul) dmgFlags |= DMG_FOILINVUL;
					if (spray.bFoilBuddha) dmgFlags |= DMG_FOILBUDDHA;
					dmgType = spray.DamageType;
				}

				if (defdamage == 0)
				{
					damage = 0;
					for (int j = 0; j < damagecnt; ++j)
						damage += Random[BFGSpray](1, 8);
				}
				else
				{
					// if this is used, damagecnt will be ignored
					damage = defdamage;
				}

				int newdam = t.linetarget.DamageMobj(originator, target, damage, dmgType, dmgFlags|DMG_USEANGLE, t.angleFromSource);
				t.TraceBleed(newdam > 0 ? newdam : damage, self);
			}
		}
	}
}
