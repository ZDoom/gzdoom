extend class Actor
{
	//---------------------------------------------------------------------------
	//
	// Used by A_CustomBulletAttack and A_FireBullets
	//
	//---------------------------------------------------------------------------

	static void AimBulletMissile(Actor proj, Actor puff, int flags, bool temp, bool cba)
	{
		if (proj && puff)
		{
			// FAF_BOTTOM = 1
			// Aim for the base of the puff as that's where blood puffs will spawn... roughly.

			proj.A_Face(puff, 0., 0., 0., 0., 1);
			proj.Vel3DFromAngle(proj.Speed, proj.Angle, proj.Pitch);

			if (!temp)
			{
				if (cba)
				{
					if (flags & CBAF_PUFFTARGET)	proj.target = puff;
					if (flags & CBAF_PUFFMASTER)	proj.master = puff;
					if (flags & CBAF_PUFFTRACER)	proj.tracer = puff;
				}
				else
				{
					if (flags & FBF_PUFFTARGET)	proj.target = puff;
					if (flags & FBF_PUFFMASTER)	proj.master = puff;
					if (flags & FBF_PUFFTRACER)	proj.tracer = puff;
				}
			}
		}
		if (puff && temp)
		{
			puff.Destroy();
		}
	}
	
	//---------------------------------------------------------------------------
	//
	//
	//
	//---------------------------------------------------------------------------

	void A_CustomBulletAttack(double spread_xy, double spread_z, int numbullets, int damageperbullet, class<Actor> pufftype = "BulletPuff", double range = 0, int flags = 0, int ptr = AAPTR_TARGET, class<Actor> missile = null, double Spawnheight = 32, double Spawnofs_xy = 0)
	{
		let ref = GetPointer(ptr);

		if (range == 0)
			range = MISSILERANGE;

		int i;
		double bangle;
		double bslope = 0.;
		int laflags = (flags & CBAF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

		if (ref != NULL || (flags & CBAF_AIMFACING))
		{
			if (!(flags & CBAF_AIMFACING))
			{
				A_Face(ref);
			}
			bangle = self.Angle;

			if (!(flags & CBAF_NOPITCH)) bslope = AimLineAttack (bangle, MISSILERANGE);
			if (pufftype == null) pufftype = 'BulletPuff';

			A_StartSound(AttackSound, CHAN_WEAPON);
			for (i = 0; i < numbullets; i++)
			{
				double pangle = bangle;
				double slope = bslope;

				if (flags & CBAF_EXPLICITANGLE)
				{
					pangle += spread_xy;
					slope += spread_z;
				}
				else
				{
					pangle += spread_xy * Random2[cwbullet]() / 255.;
					slope += spread_z * Random2[cwbullet]() / 255.;
				}

				int damage = damageperbullet;

				if (!(flags & CBAF_NORANDOM))
					damage *= random[cwbullet](1, 3);

				let puff = LineAttack(pangle, range, slope, damage, 'Hitscan', pufftype, laflags);
				if (missile != null && pufftype != null)
				{
					double ang = pangle - 90;
					let ofs = AngleToVector(ang, Spawnofs_xy);
					let pos = self.pos;
					SetXYZ(Vec3Offset(ofs.x, ofs.y, 0.));
					let proj = SpawnMissileAngleZSpeed(Pos.Z + GetBobOffset() + Spawnheight, missile, self.Angle, 0, GetDefaultByType(missile).Speed, self, false);
					SetXYZ(pos);
					
					if (proj)
					{
						bool temp = (puff == null);
						if (!puff)
						{
							puff = LineAttack(pangle, range, slope, 0, 'Hitscan', pufftype, laflags | LAF_NOINTERACT);
						}
						if (puff)
						{			
							AimBulletMissile(proj, puff, flags, temp, true);
						}
					}
				}
			}
		}
	}

	//============================================================================
	//
	// P_DaggerAlert
	//
	//============================================================================

	void DaggerAlert(Actor target)
	{
		Actor looker;

		if (LastHeard != NULL)
			return;
		if (health <= 0)
			return;
		if (!bIsMonster)
			return;
		if (bInCombat)
			return;
		bInCombat = true;

		self.target = target;
		let painstate = FindState('Pain', 'Dagger');
		if (painstate != NULL)
		{
			SetState(painstate);
		}

		for (looker = cursector.thinglist; looker != NULL; looker = looker.snext)
		{
			if (looker == self || looker == target)
				continue;

			if (looker.health <= 0)
				continue;

			if (!looker.bSeesDaggers)
				continue;

			if (!looker.bInCombat)
			{
				if (!looker.CheckSight(target) && !looker.CheckSight(self))
					continue;

				looker.target = target;
				if (looker.SeeSound)
				{
					looker.A_StartSound(looker.SeeSound, CHAN_VOICE);
				}
				looker.SetState(looker.SeeState);
				looker.bInCombat = true;
			}
		}
	}

	//===========================================================================
	//
	// Common code for A_SpawnItem and A_SpawnItemEx
	//
	//===========================================================================

	bool InitSpawnedItem(Actor mo, int flags)
	{
		if (mo == NULL)
		{
			return false;
		}
		Actor originator = self;

		if (!(mo.bDontTranslate))
		{
			if (flags & SXF_TRANSFERTRANSLATION)
			{
				mo.Translation = Translation;
			}
			else if (flags & SXF_USEBLOODCOLOR)
			{
				// [XA] Use the spawning actor's BloodColor to translate the newly-spawned object.
				mo.Translation = BloodTranslation;
			}
		}
		if (flags & SXF_TRANSFERPOINTERS)
		{
			mo.target = self.target;
			mo.master = self.master; // This will be overridden later if SXF_SETMASTER is set
			mo.tracer = self.tracer;
		}

		mo.Angle = self.Angle;
		if (flags & SXF_TRANSFERPITCH)
		{
			mo.Pitch = self.Pitch;
		}
		if (!(flags & SXF_ORIGINATOR))
		{
			while (originator && (originator.bMissile || originator.default.bMissile))
			{
				originator = originator.target;
			}
		}
		if (flags & SXF_TELEFRAG) 
		{
			mo.TeleportMove(mo.Pos, true);
			// This is needed to ensure consistent behavior.
			// Otherwise it will only spawn if nothing gets telefragged
			flags |= SXF_NOCHECKPOSITION;	
		}
		if (mo.bIsMonster)
		{
			if (!(flags & SXF_NOCHECKPOSITION) && !mo.TestMobjLocation())
			{
				// The monster is blocked so don't spawn it at all!
				mo.ClearCounters();
				mo.Destroy();
				return false;
			}
			else if (originator && !(flags & SXF_NOPOINTERS))
			{
				if (originator.bIsMonster)
				{
					// If this is a monster transfer all friendliness information
					mo.CopyFriendliness(originator, true);
				}
				else if (originator.player)
				{
					// A player always spawns a monster friendly to him
					mo.bFriendly = true;
					mo.SetFriendPlayer(originator.player);

					Actor attacker=originator.player.attacker;
					if (attacker)
					{
						if (!(attacker.bFriendly) || 
							(deathmatch && attacker.FriendPlayer != 0 && attacker.FriendPlayer != mo.FriendPlayer))
						{
							// Target the monster which last attacked the player
							mo.LastHeard = mo.target = attacker;
						}
					}
				}
			}
		}
		else if (!(flags & SXF_TRANSFERPOINTERS))
		{
			// If this is a missile or something else set the target to the originator
			mo.target = originator ? originator : self;
		}
		if (flags & SXF_NOPOINTERS)
		{
			//[MC]Intentionally eliminate pointers. Overrides TRANSFERPOINTERS, but is overridden by SETMASTER/TARGET/TRACER.
			mo.LastHeard = NULL; //Sanity check.
			mo.target = NULL;
			mo.master = NULL;
			mo.tracer = NULL;
		}
		if (flags & SXF_SETMASTER)
		{ // don't let it attack you (optional)!
			mo.master = originator;
		}
		if (flags & SXF_SETTARGET)
		{
			mo.target = originator;
		}
		if (flags & SXF_SETTRACER)
		{
			mo.tracer = originator;
		}
		if (flags & SXF_TRANSFERSCALE)
		{
			mo.Scale = self.Scale;
		}
		if (flags & SXF_TRANSFERAMBUSHFLAG)
		{
			mo.bAmbush = bAmbush;
		}
		if (flags & SXF_CLEARCALLERTID)
		{
			self.ChangeTid(0);
		}
		if (flags & SXF_TRANSFERSPECIAL)
		{
			mo.special = self.special;
			mo.args[0] = self.args[0];
			mo.args[1] = self.args[1];
			mo.args[2] = self.args[2];
			mo.args[3] = self.args[3];
			mo.args[4] = self.args[4];
		}
		if (flags & SXF_CLEARCALLERSPECIAL)
		{
			self.special = 0;
			self.args[0] = 0;
			self.args[1] = 0;
			self.args[2] = 0;
			self.args[3] = 0;
			self.args[4] = 0;
		}
		if (flags & SXF_TRANSFERSTENCILCOL)
		{
			mo.SetShade(self.fillcolor);
		}
		if (flags & SXF_TRANSFERALPHA)
		{
			mo.Alpha = self.Alpha;
		}
		if (flags & SXF_TRANSFERRENDERSTYLE)
		{
			mo.RenderStyle = self.RenderStyle;
		}
		
		if (flags & SXF_TRANSFERSPRITEFRAME)
		{
			mo.sprite = self.sprite;
			mo.frame = self.frame;
		}

		if (flags & SXF_TRANSFERROLL)
		{
			mo.Roll = self.Roll;
		}

		if (flags & SXF_ISTARGET)
		{
			self.target = mo;
		}
		if (flags & SXF_ISMASTER)
		{
			self.master = mo;
		}
		if (flags & SXF_ISTRACER)
		{
			self.tracer = mo;
		}
		return true;
	}


	//===========================================================================
	//
	// A_SpawnItem
	//
	// Spawns an item in front of the caller like Heretic's time bomb
	//
	//===========================================================================

	action bool, Actor A_SpawnItem(class<Actor> missile = "Unknown", double distance = 0, double zheight = 0, bool useammo = true, bool transfer_translation = false)
	{
		if (missile == NULL)
		{
			return false, null;
		}

		// Don't spawn monsters if this actor has been massacred
		if (DamageType == 'Massacre' && GetDefaultByType(missile).bIsMonster)
		{
			return true, null;
		}

		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			let player = self.player;
			if (player == null) return false, null;
			let weapon = player.ReadyWeapon;
			// Used from a weapon, so use some ammo

			if (weapon == NULL || (useammo && !weapon.DepleteAmmo(weapon.bAltFire)))
			{
				return true, null;
			}
		}

		let mo = Spawn(missile, Vec3Angle(distance, Angle, -Floorclip + GetBobOffset() + zheight), ALLOW_REPLACE);

		int flags = (transfer_translation ? SXF_TRANSFERTRANSLATION : 0) + (useammo ? SXF_SETMASTER : 0);
		bool res = InitSpawnedItem(mo, flags);	// for an inventory item's use state
		return res, mo;
	}

	//===========================================================================
	//
	// A_SpawnItemEx
	//
	// Enhanced spawning function
	//
	//===========================================================================
	bool, Actor A_SpawnItemEx(class<Actor> missile, double xofs = 0, double yofs = 0, double zofs = 0, double xvel = 0, double yvel = 0, double zvel = 0, double angle = 0, int flags = 0, int failchance = 0, int tid=0)
	{
		if (missile == NULL) 
		{
			return false, null;
		}
		if (failchance > 0 && random[spawnitemex]() < failchance)
		{
			return true, null;
		}
		// Don't spawn monsters if this actor has been massacred
		if (DamageType == 'Massacre' && GetDefaultByType(missile).bIsMonster)
		{
			return true, null;
		}

		Vector2 pos;

		if (!(flags & SXF_ABSOLUTEANGLE))
		{
			angle += self.Angle;
		}
		double s = sin(angle);
		double c = cos(angle);

		if (flags & SXF_ABSOLUTEPOSITION)
		{
			pos = Vec2Offset(xofs, yofs);
		}
		else
		{
			// in relative mode negative y values mean 'left' and positive ones mean 'right'
			// This is the inverse orientation of the absolute mode!
			pos = Vec2Offset(xofs * c + yofs * s, xofs * s - yofs*c);
		}

		if (!(flags & SXF_ABSOLUTEVELOCITY))
		{
			// Same orientation issue here!
			double newxvel = xvel * c + yvel * s;
			yvel = xvel * s - yvel * c;
			xvel = newxvel;
		}

		let mo = Spawn(missile, (pos, self.pos.Z - Floorclip + GetBobOffset() + zofs), ALLOW_REPLACE);
		bool res = InitSpawnedItem(mo, flags);
		if (res)
		{
			if (tid != 0)
			{
				mo.ChangeTid(tid);
			}
			mo.Vel = (xvel, yvel, zvel);
			if (flags & SXF_MULTIPLYSPEED)
			{
				mo.Vel *= mo.Speed;
			}
			mo.Angle = angle;
		}
		return res, mo;
	}

	
	//===========================================================================
	//
	// A_ThrowGrenade
	//
	// Throws a grenade (like Hexen's fighter flechette)
	//
	//===========================================================================
	action bool, Actor A_ThrowGrenade(class<Actor> missile, double zheight = 0, double xyvel = 0, double zvel = 0, bool useammo = true)
	{
		if (missile == NULL)
		{
			return false, null;
		}
		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			let player = self.player;
			if (player == null) return false, null;
			let weapon = player.ReadyWeapon;
			// Used from a weapon, so use some ammo

			if (weapon == NULL || (useammo && !weapon.DepleteAmmo(weapon.bAltFire)))
			{
				return true, null;
			}
		}

		let bo = Spawn(missile, pos + (0, 0, (-Floorclip + GetBobOffset() + zheight + 35 + (player? player.crouchoffset : 0.))), ALLOW_REPLACE);
		if (bo)
		{
			self.PlaySpawnSound(bo);
			if (xyvel != 0)
				bo.Speed = xyvel;
			bo.Angle = Angle + (random[grenade](-4, 3) * (360./256.));

			let pitch = -self.Pitch;
			let angle = bo.Angle;

			// There are two vectors we are concerned about here: xy and z. We rotate
			// them separately according to the shooter's pitch and then sum them to
			// get the final velocity vector to shoot with.

			double xy_xyscale = bo.Speed * cos(pitch);
			double xy_velz = bo.Speed * sin(pitch);
			double xy_velx = xy_xyscale * cos(angle);
			double xy_vely = xy_xyscale * sin(angle);

			pitch = self.Pitch;
			double z_xyscale = zvel * sin(pitch);
			double z_velz = zvel * cos(pitch);
			double z_velx = z_xyscale * cos(angle);
			double z_vely = z_xyscale * sin(angle);

			bo.Vel.X = xy_velx + z_velx + Vel.X / 2;
			bo.Vel.Y = xy_vely + z_vely + Vel.Y / 2;
			bo.Vel.Z = xy_velz + z_velz;

			bo.target = self;
			if (!bo.CheckMissileSpawn(radius)) bo = null;
			return true, bo;
		} 
		else
		{
			return false, null;
		}
	}

	//---------------------------------------------------------------------------
	//
	// P_CheckSplash
	//
	// Checks for splashes caused by explosions
	//
	//---------------------------------------------------------------------------

	void CheckSplash(double distance)
	{
		double floorh;
		sector floorsec;
		[floorh, floorsec] = curSector.LowestFloorAt(pos.XY);

		if (pos.Z <= floorz + distance && floorsector == floorsec && curSector.GetHeightSec() == NULL && floorsec.heightsec == NULL)
		{
			// Explosion splashes never alert monsters. This is because A_Explode has
			// a separate parameter for that so this would get in the way of proper 
			// behavior.
			Vector3 pos = PosRelative(floorsec);
			pos.Z = floorz;
			HitWater (floorsec, pos, false, false);
		}
	}

	//==========================================================================
	//
	// Parameterized version of A_Explode
	//
	//==========================================================================

	int A_Explode(int damage = -1, int distance = -1, int flags = XF_HURTSOURCE, bool alert = false, int fulldamagedistance = 0, int nails = 0, int naildamage = 10, class<Actor> pufftype = "BulletPuff", name damagetype = "none")
	{

		if (damage < 0)	// get parameters from metadata
		{
			damage = ExplosionDamage;
			distance = ExplosionRadius;
			flags = !DontHurtShooter;
			alert = false;
		}
		if (distance <= 0) distance = damage;

		// NailBomb effect, from SMMU but not from its source code: instead it was implemented and
		// generalized from the documentation at http://www.doomworld.com/eternity/engine/codeptrs.html

		if (nails)
		{
			double ang;
			for (int i = 0; i < nails; i++)
			{
				ang = i*360./nails;
				// Comparing the results of a test wad with Eternity, it seems A_NailBomb does not aim
				LineAttack(ang, MISSILERANGE, 0.,
					//P_AimLineAttack (self, ang, MISSILERANGE), 
					naildamage, 'Hitscan', pufftype, bMissile ? LAF_TARGETISSOURCE : 0);
			}
		}

		if (!(flags & XF_EXPLICITDAMAGETYPE) && damagetype == 'None')
		{
			damagetype = self.DamageType;
		}

		int pflags = 0;
		if (flags & XF_HURTSOURCE)	pflags |= RADF_HURTSOURCE;
		if (flags & XF_NOTMISSILE)	pflags |= RADF_SOURCEISSPOT;
		if (flags & XF_THRUSTZ)	pflags |= RADF_THRUSTZ;

		int count = RadiusAttack (target, damage, distance, damagetype, pflags, fulldamagedistance);
		if (!(flags & XF_NOSPLASH)) CheckSplash(distance);
		if (alert && target != NULL && target.player != NULL)
		{
			SoundAlert(target);
		}
		return count;
	}

	//==========================================================================
	//
	// A_RadiusThrust
	//
	//==========================================================================

	void A_RadiusThrust(int force = 128, int distance = -1, int flags = RTF_AFFECTSOURCE, int fullthrustdistance = 0)
	{
		if (force == 0) force = 128;
		if (distance <= 0) distance = abs(force);
		bool nothrust = false;

		if (target)
		{
			nothrust = target.bNoDamageThrust;
			// Temporarily negate MF2_NODMGTHRUST on the shooter, since it renders this function useless.
			if (!(flags & RTF_NOTMISSILE))
			{
				target.bNoDamageThrust = false;
			}
		}
		RadiusAttack (target, force, distance, DamageType, flags | RADF_NODAMAGE, fullthrustdistance);
		CheckSplash(distance);
		if (target) target.bNoDamageThrust = nothrust;
	}

	//==========================================================================
	//
	// A_Detonate
	// killough 8/9/98: same as A_Explode, except that the damage is variable
	//
	//==========================================================================

	void A_Detonate()
	{
		int damage = GetMissileDamage(0, 1);
		RadiusAttack (target, damage, damage, DamageType, RADF_HURTSOURCE);
		CheckSplash(damage);
	}

	//==========================================================================
	//
	// old customizable attack functions which use actor parameters.
	//
	//==========================================================================
	
	private void DoAttack (bool domelee, bool domissile, int MeleeDamage, Sound MeleeSound, Class<Actor> MissileType,double MissileHeight)
	{
		let targ = target;
		if (targ == NULL) return;

		A_FaceTarget ();
		if (domelee && MeleeDamage>0 && CheckMeleeRange ())
		{
			int damage = random[CustomMelee](1, 8) * MeleeDamage;
			if (MeleeSound) A_StartSound (MeleeSound, CHAN_WEAPON);
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
		}
		else if (domissile && MissileType != NULL)
		{
			// This seemingly senseless code is needed for proper aiming.
			double add = MissileHeight + GetBobOffset() - 32;
			AddZ(add);
			Actor missile = SpawnMissileXYZ (Pos + (0, 0, 32), targ, MissileType, false);
			AddZ(-add);

			if (missile)
			{
				// automatic handling of seeker missiles
				if (missile.bSeekerMissile)
				{
					missile.tracer = targ;
				}
				missile.CheckMissileSpawn(radius);
			}
		}
	}

	deprecated("2.3", "Use CustomMeleeAttack() instead") void A_MeleeAttack()
	{
		DoAttack(true, false, MeleeDamage, MeleeSound, NULL, 0);
	}

	deprecated("2.3", "Use A_SpawnProjectile() instead") void A_MissileAttack()
	{
		Class<Actor> MissileType = MissileName;
		DoAttack(false, true, 0, 0, MissileType, MissileHeight);
	}

	deprecated("2.3", "Use A_BasicAttack() instead") void A_ComboAttack()
	{
		Class<Actor> MissileType = MissileName;
		DoAttack(true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
	}

	void A_BasicAttack(int melee_damage, sound melee_sound, class<actor> missile_type, double missile_height)
	{
		DoAttack(true, true, melee_damage, melee_sound, missile_type, missile_height);
	}

	
	//==========================================================================
	//
	// called with the victim as 'self'
	//
	//==========================================================================
	
	virtual void SpawnLineAttackBlood(Actor attacker, Vector3 bleedpos, double SrcAngleFromTarget, int originaldamage, int actualdamage)
	{
		if (!bNoBlood && !bDormant && !bInvulnerable)
		{
			let player = attacker.player;
			let weapon = player? player.ReadyWeapon : null;
			let axeBlood = (weapon && weapon.bAxeBlood);
			let bloodsplatter = attacker.bBloodSplatter || axeBlood;

			if (!bloodsplatter)
			{
				SpawnBlood(bleedpos, SrcAngleFromTarget, actualdamage > 0 ? actualdamage : originaldamage);
			}
			else if (originaldamage)
			{
				if (axeBlood)
				{
					BloodSplatter(bleedpos, SrcAngleFromTarget, true);
				}
				// No else here...
				if (random[LineAttack]() < 192)
				{
					BloodSplatter(bleedpos, SrcAngleFromTarget, false);
				}
			}
		}
	}
}
