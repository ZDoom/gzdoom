
class StateProvider : Inventory
{
	//==========================================================================
	//
	// State jump function
	//
	//==========================================================================

	action state A_JumpIfNoAmmo(statelabel label)
	{
		if (stateinfo == null || stateinfo.mStateType != STATE_Psprite || player == null || player.ReadyWeapon == null ||
			player.ReadyWeapon.CheckAmmo(player.ReadyWeapon.bAltFire, false, true))
		{
			return null;
		}
		else return ResolveState(label);
	}

	//===========================================================================
	//
	// Modified code pointer from Skulltag
	//
	//===========================================================================

	action state A_CheckForReload(int count, statelabel jump, bool dontincrement = false)
	{
		if (stateinfo == null || stateinfo.mStateType != STATE_Psprite || player == null || player.ReadyWeapon == null)
		{
			return null;
		}

		state ret = null;

		let weapon = player.ReadyWeapon;

		int ReloadCounter = weapon.ReloadCounter;
		if (!dontincrement || ReloadCounter != 0)
			ReloadCounter = (weapon.ReloadCounter+1) % count;
		else // 0 % 1 = 1?  So how do we check if the weapon was never fired?  We should only do this when we're not incrementing the counter though.
			ReloadCounter = 1;

		// If we have not made our last shot...
		if (ReloadCounter != 0)
		{
			// Go back to the refire frames, instead of continuing on to the reload frames.
			ret = ResolveState(jump);
		}
		else
		{
			// We need to reload. However, don't reload if we're out of ammo.
			weapon.CheckAmmo(false, false);
		}
		if (!dontincrement)
		{
			weapon.ReloadCounter = ReloadCounter;
		}
		return ret;
	}

	//===========================================================================
	//
	// Resets the counter for the above function
	//
	//===========================================================================

	action void A_ResetReloadCounter()
	{
		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite && player != null && player.ReadyWeapon != null)
			player.ReadyWeapon.ReloadCounter = 0;
	}

	
	//---------------------------------------------------------------------------
	//
	//
	//
	//---------------------------------------------------------------------------

	action void A_FireBullets(double spread_xy, double spread_z, int numbullets, int damageperbullet, class<Actor> pufftype = "BulletPuff", int flags = 1, double range = 0, class<Actor> missile = null, double Spawnheight = 32, double Spawnofs_xy = 0)
	{
		let player = player;
		if (!player) return;

		let pawn = PlayerPawn(self);
		let weapon = player.ReadyWeapon;

		int i;
		double bangle;
		double bslope = 0.;
		int laflags = (flags & FBF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

		if ((flags & FBF_USEAMMO) && weapon &&  stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weapon.DepleteAmmo(weapon.bAltFire, true))
				return;	// out of ammo
		}
		
		if (range == 0)	range = PLAYERMISSILERANGE;

		if (!(flags & FBF_NOFLASH)) pawn.PlayAttacking2 ();

		if (!(flags & FBF_NOPITCH)) bslope = BulletSlope();
		bangle = Angle;

		if (pufftype == NULL) pufftype = 'BulletPuff';

		if (weapon != NULL)
		{
			A_StartSound(weapon.AttackSound, CHAN_WEAPON);
		}

		if ((numbullets == 1 && !player.refire) || numbullets == 0)
		{
			int damage = damageperbullet;

			if (!(flags & FBF_NORANDOM))
				damage *= random[cabullet](1, 3);

			let puff = LineAttack(bangle, range, bslope, damage, 'Hitscan', pufftype, laflags);

			if (missile != null)
			{
				bool temp = false;
				double ang = Angle - 90;
				Vector2 ofs = AngleToVector(ang, Spawnofs_xy);
				Actor proj = SpawnPlayerMissile(missile, bangle, ofs.X, ofs.Y, Spawnheight);
				if (proj)
				{
					if (!puff)
					{
						temp = true;
						puff = LineAttack(bangle, range, bslope, 0, 'Hitscan', pufftype, laflags | LAF_NOINTERACT);
					}
					AimBulletMissile(proj, puff, flags, temp, false);
				}
			}
		}
		else 
		{
			if (numbullets < 0)
				numbullets = 1;
			for (i = 0; i < numbullets; i++)
			{
				double pangle = bangle;
				double slope = bslope;

				if (flags & FBF_EXPLICITANGLE)
				{
					pangle += spread_xy;
					slope += spread_z;
				}
				else
				{
					pangle += spread_xy * Random2[cabullet]() / 255.;
					slope += spread_z * Random2[cabullet]() / 255.;
				}

				int damage = damageperbullet;

				if (!(flags & FBF_NORANDOM))
					damage *= random[cabullet](1, 3);

				let puff = LineAttack(pangle, range, slope, damage, 'Hitscan', pufftype, laflags);

				if (missile != null)
				{
					bool temp = false;
					double ang = Angle - 90;
					Vector2 ofs = AngleToVector(ang, Spawnofs_xy);
					Actor proj = SpawnPlayerMissile(missile, bangle, ofs.X, ofs.Y, Spawnheight);
					if (proj)
					{
						if (!puff)
						{
							temp = true;
							puff = LineAttack(bangle, range, bslope, 0, 'Hitscan', pufftype, laflags | LAF_NOINTERACT);
						}
						AimBulletMissile(proj, puff, flags, temp, false);
					}
				}
			}
		}
	}


	//==========================================================================
	//
	// A_FireProjectile
	//
	//==========================================================================

	action Actor A_FireProjectile(class<Actor> missiletype, double angle = 0, bool useammo = true, double spawnofs_xy = 0, double spawnheight = 0, int flags = 0, double pitch = 0)	
	{
		let player = self.player;
		if (!player) return null;

		let weapon = player.ReadyWeapon;

		FTranslatedLineTarget t;

			// Only use ammo if called from a weapon
		if (useammo && weapon && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weapon.DepleteAmmo(weapon.bAltFire, true))
				return null;	// out of ammo
		}

		if (missiletype) 
		{
			double ang = self.Angle - 90;
			Vector2 ofs = AngleToVector(ang, spawnofs_xy);
			double shootangle = self.Angle;

			if (flags & FPF_AIMATANGLE) shootangle += angle;

			// Temporarily adjusts the pitch
			double saved_player_pitch = self.Pitch;
			self.Pitch += pitch;
			let misl = SpawnPlayerMissile (missiletype, shootangle, ofs.X, ofs.Y, spawnheight, t, false, (flags & FPF_NOAUTOAIM) != 0);
			self.Pitch = saved_player_pitch;

			// automatic handling of seeker missiles
			if (misl)
			{
				if (flags & FPF_TRANSFERTRANSLATION)
					misl.Translation = Translation;
				if (t.linetarget && !t.unlinked && misl.bSeekerMissile)
					misl.tracer = t.linetarget;
				if (!(flags & FPF_AIMATANGLE))
				{
					// This original implementation is to aim straight ahead and then offset
					// the angle from the resulting direction. 
					misl.Angle += angle;
					misl.VelFromAngle(misl.Vel.XY.Length());
				}
			}
			return misl;
		}
		return null;
	}

//==========================================================================
//
// A_CustomPunch
//
// Berserk is not handled here. That can be done with A_CheckIfInventory
//
//==========================================================================

	action void  A_CustomPunch(int damage, bool norandom = false, int flags = CPF_USEAMMO, class<Actor> pufftype = "BulletPuff", double range = 0, double lifesteal = 0, int lifestealmax = 0, class<BasicArmorBonus> armorbonustype = "ArmorBonus", sound MeleeSound = 0, sound MissSound = "")
	{
		let player = self.player;
		if (!player) return;

		let weapon = player.ReadyWeapon;

		double angle;
		double pitch;
		FTranslatedLineTarget t;
		int			actualdamage;

		if (!norandom)
			damage *= random[cwpunch](1, 8);

		angle = self.Angle + random2[cwpunch]() * (5.625 / 256);
		if (range == 0) range = DEFMELEERANGE;
		pitch = AimLineAttack (angle, range, t, 0., ALF_CHECK3D);

		// only use ammo when actually hitting something!
		if ((flags & CPF_USEAMMO) && t.linetarget && weapon && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weapon.DepleteAmmo(weapon.bAltFire, true))
				return;	// out of ammo
		}

		if (pufftype == NULL)
			pufftype = 'BulletPuff';
		int puffFlags = LAF_ISMELEEATTACK | ((flags & CPF_NORANDOMPUFFZ) ? LAF_NORANDOMPUFFZ : 0);

		Actor puff;
		[puff, actualdamage] = LineAttack (angle, range, pitch, damage, 'Melee', pufftype, puffFlags, t);

		if (!t.linetarget)
		{
			if (MissSound) A_StartSound(MissSound, CHAN_WEAPON);
		}
		else
		{
			if (lifesteal > 0 && !(t.linetarget.bDontDrain))
			{
				if (flags & CPF_STEALARMOR)
				{
					if (armorbonustype == NULL)
					{
						armorbonustype = 'ArmorBonus';
					}
					if (armorbonustype != NULL)
					{
						let armorbonus = BasicArmorBonus(Spawn(armorbonustype));
						if (armorbonus)
						{
							armorbonus.SaveAmount *= int(actualdamage * lifesteal);
							if (lifestealmax > 0) armorbonus.MaxSaveAmount = lifestealmax;
							armorbonus.bDropped = true;
							armorbonus.ClearCounters();

							if (!armorbonus.CallTryPickup(self))
							{
								armorbonus.Destroy ();
							}
						}
					}
				}
				else
				{
					GiveBody (int(actualdamage * lifesteal), lifestealmax);
				}
			}
			if (weapon != NULL)
			{
				if (MeleeSound) A_StartSound(MeleeSound, CHAN_WEAPON);
				else			A_StartSound(weapon.AttackSound, CHAN_WEAPON);
			}

			if (!(flags & CPF_NOTURN))
			{
				// turn to face target
				self.Angle = t.angleFromSource;
			}

			if (flags & CPF_PULLIN) self.bJustAttacked = true;
			if (flags & CPF_DAGGER) t.linetarget.DaggerAlert (self);
		}
	}

	//==========================================================================
	//
	// customizable railgun attack function
	//
	//==========================================================================
	
	action void A_RailAttack(int damage, int spawnofs_xy = 0, bool useammo = true, color color1 = 0, color color2 = 0, int flags = 0, double maxdiff = 0, class<Actor> pufftype = "BulletPuff", double spread_xy = 0, double spread_z = 0, double range = 0, int duration = 0, double sparsity = 1.0, double driftspeed = 1.0, class<Actor> spawnclass = "none", double spawnofs_z = 0, int spiraloffset = 270, int limit = 0)
	{
		if (range == 0) range = 8192;
		if (sparsity == 0) sparsity=1.0;

		let player = self.player;
		if (!player) return;

		let weapon = player.ReadyWeapon;

		if (useammo && weapon != NULL && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			if (!weapon.DepleteAmmo(weapon.bAltFire, true))
				return;	// out of ammo
		}

		if (!(flags & RGF_EXPLICITANGLE))
		{
			spread_xy = spread_xy * Random2[crailgun]() / 255.;
			spread_z = spread_z * Random2[crailgun]() / 255.;
		}

		FRailParams p;
		p.damage = damage;
		p.offset_xy = spawnofs_xy;
		p.offset_z = spawnofs_z;
		p.color1 = color1;
		p.color2 = color2;
		p.maxdiff = maxdiff;
		p.flags = flags;
		p.puff = pufftype;
		p.angleoffset = spread_xy;
		p.pitchoffset = spread_z;
		p.distance = range;
		p.duration = duration;
		p.sparsity = sparsity;
		p.drift = driftspeed;
		p.spawnclass = spawnclass;
		p.SpiralOffset = SpiralOffset;
		p.limit = limit;
		self.RailAttack(p);
	}
	

	
	//---------------------------------------------------------------------------
	//
	// PROC A_ReFire
	//
	// The player can re-fire the weapon without lowering it entirely.
	//
	//---------------------------------------------------------------------------

	action void A_ReFire(statelabel flash = null)
	{
		let player = player;
		bool pending;

		if (NULL == player)
		{
			return;
		}
		pending = player.PendingWeapon != WP_NOCHANGE && (player.WeaponState & WF_REFIRESWITCHOK);
		if ((player.cmd.buttons & BT_ATTACK)
			&& !player.ReadyWeapon.bAltFire && !pending && player.health > 0)
		{
			player.refire++;
			player.mo.FireWeapon(ResolveState(flash));
		}
		else if ((player.cmd.buttons & BT_ALTATTACK)
			&& player.ReadyWeapon.bAltFire && !pending && player.health > 0)
		{
			player.refire++;
			player.mo.FireWeaponAlt(ResolveState(flash));
		}
		else
		{
			player.refire = 0;
			player.ReadyWeapon.CheckAmmo (player.ReadyWeapon.bAltFire? Weapon.AltFire : Weapon.PrimaryFire, true);
		}
	}
	
	

	action void A_ClearReFire()
	{
		if (NULL != player)	player.refire = 0;
	}
}

class CustomInventory : StateProvider
{
	Default
	{
		DefaultStateUsage SUF_ACTOR|SUF_OVERLAY|SUF_ITEM;
	}
	
	//---------------------------------------------------------------------------
	//
	//
	//---------------------------------------------------------------------------

	// This is only here, because these functions were originally exported on Inventory, despite only working for weapons, so this is here to satisfy some potential old mods having called it through CustomInventory.
	deprecated("2.3", "must be called from Weapon") action void A_GunFlash(statelabel flash = null, int flags = 0) {}
	deprecated("2.3", "must be called from Weapon") action void A_Lower() {}
	deprecated("2.3", "must be called from Weapon") action void A_Raise() {}
	deprecated("2.3", "must be called from Weapon") action void A_CheckReload() {}
	deprecated("3.7", "must be called from Weapon") action void A_WeaponReady(int flags = 0) {}	// this was somehow missed in 2.3 ...
	native bool CallStateChain (Actor actor, State state);
		
	//===========================================================================
	//
	// ACustomInventory :: SpecialDropAction
	//
	//===========================================================================

	override bool SpecialDropAction (Actor dropper)
	{
		return CallStateChain (dropper, FindState('Drop'));
	}

	//===========================================================================
	//
	// ACustomInventory :: Use
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		return CallStateChain (Owner, FindState('Use'));
	}

	//===========================================================================
	//
	// ACustomInventory :: TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor toucher)
	{
		let pickupstate = FindState('Pickup');
		bool useok = CallStateChain (toucher, pickupstate);
		if ((useok || pickupstate == NULL) && FindState('Use') != NULL)
		{
			useok = Super.TryPickup (toucher);
		}
		else if (useok)
		{
			GoAwayAndDie();
		}
		return useok;
	}
}
