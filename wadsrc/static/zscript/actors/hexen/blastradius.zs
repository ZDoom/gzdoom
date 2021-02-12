
class ArtiBlastRadius : CustomInventory
{
	Default
	{
		+FLOATBOB
		Inventory.DefMaxAmount;
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.INVBAR +INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIBLST";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIBLASTRADIUS";
		Tag "$TAG_ARTIBLASTRADIUS";
	}
	States
	{
	Spawn:
		BLST ABCDEFGH 4 Bright;
		Loop;
	Use:
		TNT1 A 0 A_Blast;
	}
	
}

//==========================================================================
//
// A_Blast is public to Actor
//
//==========================================================================

extend class Actor
{
	/* For reference, the default values:
	#define BLAST_RADIUS_DIST	255.0
	#define BLAST_SPEED			20.0
	#define BLAST_FULLSTRENGTH	255
	*/

	//==========================================================================
	//
	// AArtiBlastRadius :: BlastActor
	//
	//==========================================================================

	private void BlastActor (Actor victim, double strength, double speed, Class<Actor> blasteffect, bool dontdamage)
	{
		if (!victim.SpecialBlastHandling (self, strength))
		{
			return;
		}

		double ang = AngleTo(victim);
		Vector2 move = AngleToVector(ang, speed);
		victim.Vel.XY = move;

		// Spawn blast puff
		ang -= 180.;
		Vector3 spawnpos = victim.Vec3Offset(
			(victim.radius + 1) * cos(ang),
			(victim.radius + 1) * sin(ang),
			(victim.Height / 2) - victim.Floorclip);
		Actor mo = blasteffect? Spawn (blasteffect, spawnpos, ALLOW_REPLACE) : null;
		if (mo)
		{
			mo.Vel.XY = victim.Vel.XY;
		}
		if (victim.bMissile)
		{
			// [RH] Floor and ceiling huggers should not be blasted vertically.
			if (!victim.bFloorHugger && !victim.bCeilingHugger)
			{
				victim.Vel.Z = 8;
				if (mo != null) mo.Vel.Z = 8;
			}
		}
		else
		{
			victim.Vel.Z = 1000. / victim.Mass;
		}
		if (victim.player)
		{
			// Players handled automatically
		}
		else if (!dontdamage)
		{
			victim.bBlasted = true;
		}
		if (victim.bTouchy)
		{ // Touchy objects die when blasted
			victim.bArmed = false; // Disarm
			victim.DamageMobj(self, self, victim.health, 'Melee', DMG_FORCED|DMG_EXPLOSION);
		}
	}
	
	//==========================================================================
	//
	// AArtiBlastRadius :: Activate
	//
	// Blast all actors away
	//
	//==========================================================================

	action void A_Blast(int blastflags = 0, double strength = 255, double radius = 255, double speed = 20, class<Actor> blasteffect = "BlastEffect", sound blastsound = "BlastRadius")
	{

		if (player && (blastflags & BF_USEAMMO) && invoker == player.ReadyWeapon && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			Weapon weapon = player.ReadyWeapon;
			if (weapon != null && !weapon.DepleteAmmo(weapon.bAltFire))
			{
				return;
			}
		}

		A_StartSound (blastsound, CHAN_AUTO);

		if (!(blastflags & BF_DONTWARN))
		{
			SoundAlert (self);
		}
		ThinkerIterator it = ThinkerIterator.Create("Actor");
		Actor mo;
		while ( (mo = Actor(it.Next ())) )
		{
			if (mo == self || (mo.bBoss && !(blastflags & BF_AFFECTBOSSES)) || mo.bDormant || mo.bDontBlast)
			{ // Not a valid monster: originator, boss, dormant, or otherwise protected
				continue;
			}
			if (mo.bIceCorpse || mo.bCanBlast)
			{
				// Let these special cases go
			}
			else if (mo.bIsMonster && mo.health <= 0)
			{
				continue;
			}
			else if (!mo.player && !mo.bMissile && !mo.bIsMonster && !mo.bCanBlast && !mo.bTouchy && !mo.bVulnerable)
			{	// Must be monster, player, missile, touchy or vulnerable
				continue;
			}
			if (Distance2D(mo) > radius)
			{ // Out of range
				continue;
			}
			if (mo.CurSector.PortalGroup != CurSector.PortalGroup && !CheckSight(mo))
			{
				// in another region and cannot be seen.
				continue;
			}
			BlastActor (mo, strength, speed, blasteffect, !!(blastflags & BF_NOIMPACTDAMAGE));
		}
	}
}

// Blast Effect -------------------------------------------------------------

class BlastEffect : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY +NOCLIP
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.666;
	}
	States
	{
	Spawn:
		RADE ABCDEFGHI 4;
		Stop;
	}
}
