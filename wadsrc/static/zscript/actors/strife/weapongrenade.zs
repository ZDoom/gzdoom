// High-Explosive Grenade Launcher ------------------------------------------

class StrifeGrenadeLauncher : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 2400;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 12;
		Weapon.AmmoType1 "HEGrenadeRounds";
		Weapon.SisterWeapon "StrifeGrenadeLauncher2";
		Inventory.Icon "GRNDA0";
		Tag "$TAG_GLAUNCHER1";
		Inventory.PickupMessage "$TXT_GLAUNCHER";
	}

	States
	{
	Spawn:
		GRND A -1;
		Stop;
	Ready:
		GREN A 1 A_WeaponReady;
		Loop;
	Deselect:
		GREN A 1 A_Lower;
		Loop;
	Select:
		GREN A 1 A_Raise;
		Loop;
	Fire:
		GREN A 5 A_FireGrenade("HEGrenade", -90, "Flash");
		GREN B 10;
		GREN A 5 A_FireGrenade("HEGrenade",  90, "Flash2");
		GREN C 10;
		GREN A 0 A_ReFire;
		Goto Ready;
	Flash:
		GREF A 5 Bright A_Light1;
		Goto LightDone;
	Flash2:
		GREF B 5 Bright A_Light2;
		Goto LightDone;
	}
	
	//============================================================================
	//
	// A_FireGrenade
	//
	//============================================================================

	action void A_FireGrenade (class<Actor> grenadetype, double angleofs, statelabel flash)
	{
		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;

			player.SetPsprite (PSP_FLASH, weapon.FindState(flash), true);
		}

		if (grenadetype != null)
		{
			AddZ(32);
			Actor grenade = SpawnSubMissile (grenadetype, self);
			AddZ(-32);
			if (grenade == null)
				return;

			if (grenade.SeeSound != 0)
			{
				grenade.A_PlaySound (grenade.SeeSound, CHAN_VOICE);
			}

			grenade.Vel.Z = (-clamp(tan(Pitch), -5, 5)) * grenade.Speed + 8;

			Vector2 offset = AngleToVector(angle, radius + grenade.radius);
			double an = Angle + angleofs;
			offset += AngleToVector(an, 15);
			grenade.SetOrigin(grenade.Vec3Offset(offset.X, offset.Y, 0.), false);
		}
	}
}

// White Phosphorous Grenade Launcher ---------------------------------------

class StrifeGrenadeLauncher2 : StrifeGrenadeLauncher
{
	Default
	{
		Weapon.SelectionOrder 3200;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 0;
		Weapon.AmmoType1 "PhosphorusGrenadeRounds";
		Weapon.SisterWeapon "StrifeGrenadeLauncher";
		Tag "$TAG_GLAUNCHER2";
	}
	States
	{
	Ready:
		GREN D 1 A_WeaponReady;
		Loop;
	Deselect:
		GREN D 1 A_Lower;
		Loop;
	Select:
		GREN D 1 A_Raise;
		Loop;
	Fire:
		GREN D 5 A_FireGrenade("PhosphorousGrenade", -90, "Flash");
		GREN E 10;
		GREN D 5 A_FireGrenade("PhosphorousGrenade",  90, "Flash2");
		GREN F 10;
		GREN A 0 A_ReFire;
		Goto Ready;
	Flash:
		GREF C 5 Bright A_Light1;
		Goto LightDone;
	Flash2:
		GREF D 5 Bright A_Light2;
		Goto LightDone;
	}
}

// High-Explosive Grenade ---------------------------------------------------

class HEGrenade : Actor
{
	Default
	{
		Speed 15;
		Radius 13;
		Height 13;
		Mass 20;
		Damage 1;
		Reactiontime 30;
		Projectile;
		-NOGRAVITY
		+STRIFEDAMAGE
		+BOUNCEONACTORS
		+EXPLODEONWATER
		MaxStepHeight 4;
		BounceType "Doom";
		BounceFactor 0.5;
		BounceCount 2;
		SeeSound "weapons/hegrenadeshoot";
		DeathSound "weapons/hegrenadebang";
		Obituary "$OB_MPSTRIFEGRENADE";
	}
	States
	{
	Spawn:
		GRAP AB 3 A_Countdown;
		Loop;
	Death:
		BNG4 A 0 Bright A_NoGravity;
		BNG4 A 0 Bright A_SetRenderStyle(1, STYLE_Normal);
		BNG4 A 2 Bright A_Explode(192, 192, alert:true);
		BNG4 BCDEFGHIJKLMN 3 Bright;
		Stop;
	}
}

// White Phosphorous Grenade ------------------------------------------------

class PhosphorousGrenade : Actor
{
	Default
	{
		Speed 15;
		Radius 13;
		Height 13;
		Mass 20;
		Damage 1;
		Reactiontime 40;
		Projectile;
		-NOGRAVITY
		+STRIFEDAMAGE
		+BOUNCEONACTORS
		+EXPLODEONWATER
		BounceType "Doom";
		MaxStepHeight 4;
		BounceFactor 0.5;
		BounceCount 2;
		SeeSound "weapons/phgrenadeshoot";
		DeathSound "weapons/phgrenadebang";
		Obituary "$OB_MPPHOSPHOROUSGRENADE";
	}
	States
	{
	Spawn:
		GRIN AB 3 A_Countdown;
		Loop;
	Death:
		BNG3 A 2 A_SpawnItemEx("PhosphorousFire");
		Stop;
	}
}

// Fire from the Phosphorous Grenade ----------------------------------------

class PhosphorousFire : Actor
{
	Default
	{
	Reactiontime 120;
	DamageType "Fire";
	+NOBLOCKMAP
	+FLOORCLIP
	+NOTELEPORT
	+NODAMAGETHRUST
	+DONTSPLASH
	+ZDOOMTRANS
	RenderStyle "Add";
	Obituary "$OB_MPPHOSPHOROUSGRENADE";
	}

	States
	{
	Spawn:
		BNG3 B 2 Bright A_Burnarea;
		BNG3 C 2 Bright A_Countdown;
		FLBE A 2 Bright A_Burnination;
		FLBE B 2 Bright A_Countdown;
		FLBE C 2 Bright A_Burnarea;
		FLBE D 3 Bright A_Countdown;
		FLBE E 3 Bright A_Burnarea;
		FLBE F 3 Bright A_Countdown;
		FLBE G 3 Bright A_Burnination;
		Goto Spawn+5;
	Death:
		FLBE H 2 Bright;
		FLBE I 2 Bright A_Burnination;
		FLBE JK 2 Bright;
		Stop;
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		// This may look a bit weird but is the same as in SVE:
		// For the bosses, only their regular 0.5 damage factor for fire applies.
		let firedamage = target.ApplyDamageFactor('Fire', damage);
		if (firedamage != damage) return damage;	// if the target has a factor, do nothing here. The factor will be applied elsewhere.

		// For everything else damage is halved, for robots quartered.
		damage >>= 1;
		if (target.bNoBlood)
		{
			damage >>= 1;
		}
		return damage;
	}
	
	// This function is mostly redundant and only kept in case some mod references it.
	void A_Burnarea ()
	{
		A_Explode(128, 128);
	}

	void A_Burnination ()
	{
		Vel.Z -= 8;
		Vel.X += (random2[PHBurn] (3));
		Vel.Y += (random2[PHBurn] (3));
		A_PlaySound ("world/largefire", CHAN_VOICE);

		// Only the main fire spawns more.
		if (!bDropped)
		{
			// Original x and y offsets seemed to be like this:
			//		x + (((pr_phburn() + 12) & 31) << F.RACBITS);
			//
			// But that creates a lop-sided burn because it won't use negative offsets.
			int xofs, xrand = random[PHBurn]();
			int yofs, yrand = random[PHBurn]();

			// Adding 12 is pointless if you're going to mask it afterward.
			xofs = xrand & 31;
			if (xrand & 128)
			{
				xofs = -xofs;
			}

			yofs = yrand & 31;
			if (yrand & 128)
			{
				yofs = -yofs;
			}

			Vector2 newpos = Vec2Offset(xofs, yofs);
			
			Sector sec = Level.PointInSector(newpos);
			// Consider portals and 3D floors instead of just using the current sector's z.
			double floorh = sec.NextLowestFloorAt(newpos.x, newpos.y, pos.z+4, 0, MaxStepHeight);

			// The sector's floor is too high so spawn the flame elsewhere.
			if (floorh + MaxStepHeight)
			{
				newpos = Pos.xy;
			}

			Actor drop = Spawn("PhosphorousFire", (newpos, pos.z + 4.), ALLOW_REPLACE);
			if (drop != NULL)
			{
				drop.Vel.X = Vel.X + random2[PHBurn] (7);
				drop.Vel.Y = Vel.Y + random2[PHBurn] (7);
				drop.Vel.Z = Vel.Z - 1;
				drop.reactiontime = random[PHBurn](2, 5);
				drop.bDropped = true;
			}
		}
	}

	
}

