// no attempt has been made to merge this with existing code.
extend class Actor
{
	//
	// [XA] New mbf21 codepointers
	//

	//
	// A_SpawnObject
	// Basically just A_Spawn with better behavior and more args.
	//   args[0]: Type of actor to spawn
	//   args[1]: Angle (degrees, in fixed point), relative to calling actor's angle
	//   args[2]: X spawn offset (fixed point), relative to calling actor
	//   args[3]: Y spawn offset (fixed point), relative to calling actor
	//   args[4]: Z spawn offset (fixed point), relative to calling actor
	//   args[5]: X velocity (fixed point)
	//   args[6]: Y velocity (fixed point)
	//   args[7]: Z velocity (fixed point)
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_SpawnObject(class<Actor> type, double angle, double xofs, double yofs, double zofs, double xvel, double yvel, double zvel)
	{
		if (type == null)
			return;

		// Don't spawn monsters if this actor has been massacred
		if (DamageType == 'Massacre' && GetDefaultByType(type).bIsMonster)
		{
			return;
		}

		// calculate position offsets
		angle += self.Angle;
		double s = sin(angle);
		double c = cos(angle);
		let pos = Vec2Offset(xofs * c - yofs * s, xofs * s + yofs * c);

		// spawn it, yo
		let mo = Spawn(type, (pos, self.pos.Z - Floorclip + GetBobOffset() + zofs), ALLOW_REPLACE);
		if (!mo)
			return;

		// angle dangle
		mo.angle = angle;

		// set velocity
			// Same orientation issue here!
		mo.vel.X = xvel * c - yvel * s;
		mo.vel.Y = xvel * s + yvel * c;
		mo.vel.Z = zvel;

		// if spawned object is a missile, set target+tracer
		if (mo.bMissile || mo.bMbfBouncer)
		{
			// if spawner is also a missile, copy 'em
			if (default.bMissile || default.bMbfBouncer)
			{
				mo.target = self.target;
				mo.tracer = self.tracer;
			}
			// otherwise, set 'em as if a monster fired 'em
			else
			{
				mo.target = self;
				mo.tracer = self.target;
			}
		}

		// [XA] don't bother with the dont-inherit-friendliness hack
		// that exists in A_Spawn, 'cause WTF is that about anyway?

		// unfortunately this means that this function cannot transfer friendliness at all. Oh well...
	}

	//
	// A_MonsterProjectile
	// A parameterized monster projectile attack.
	//   args[0]: Type of actor to spawn
	//   args[1]: Angle (degrees, in fixed point), relative to calling actor's angle
	//   args[2]: Pitch (degrees, in fixed point), relative to calling actor's pitch; approximated
	//   args[3]: X/Y spawn offset, relative to calling actor's angle
	//   args[4]: Z spawn offset, relative to actor's default projectile fire height
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_MonsterProjectile(class<Actor> type, double angle, double pitch, double spawnofs_xy, double spawnofs_z)
	{
		if (!target || !type)
			return;

		A_FaceTarget();
		let mo = SpawnMissile(target, type);
		if (!mo)
			return;

		// adjust angle
		mo.angle += angle;

		Pitch += mo.PitchFromVel();
		let missilespeed = abs(cos(Pitch) * mo.Speed);
		mo.Vel3DFromAngle(mo.Speed, mo.angle, Pitch);

		// adjust position
		double x = Spawnofs_xy * cos(self.angle);
		double y = Spawnofs_xy * sin(self.angle);
		mo.SetOrigin(mo.Vec3Offset(x, y, Spawnofs_z), false);

		// always set the 'tracer' field, so this pointer
		// can be used to fire seeker missiles at will.
		mo.tracer = target;
	}

	//
	// A_MonsterBulletAttack
	// A parameterized monster bullet attack.
	//   args[0]: Horizontal spread (degrees, in fixed point)
	//   args[1]: Vertical spread (degrees, in fixed point)
	//   args[2]: Number of bullets to fire; if not set, defaults to 1
	//   args[3]: Base damage of attack (e.g. for 3d5, customize the 3); if not set, defaults to 3
	//   args[4]: Attack damage modulus (e.g. for 3d5, customize the 5); if not set, defaults to 5
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_MonsterBulletAttack(double hspread, double vspread, int numbullets, int damagebase, int damagemod)
	{
		if (!target)
			return;

		A_FaceTarget();
		A_StartSound(AttackSound, CHAN_WEAPON);

		let bangle = angle;
		let slope = AimLineAttack(bangle, MISSILERANGE);

		for (int i = 0; i < numbullets; i++)
		{
			int damage = (random[mbf21]() % damagemod + 1) * damagebase;
			let pangle = bangle + hspread * Random2[mbf21]() / 255.;
			let pslope = slope + vspread * Random2[mbf21]() / 255.;
			LineAttack(pangle, MISSILERANGE, pslope, damage, "Hitscan", "Bulletpuff");
		}
	}

	//
	// A_MonsterMeleeAttack
	// A parameterized monster melee attack.
	//   args[0]: Base damage of attack (e.g. for 3d8, customize the 3); if not set, defaults to 3
	//   args[1]: Attack damage modulus (e.g. for 3d8, customize the 8); if not set, defaults to 8
	//   args[2]: Sound to play if attack hits
	//   args[3]: Range (fixed point); if not set, defaults to monster's melee range
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_MonsterMeleeAttack(int damagebase, int damagemod, Sound hitsound, double range)
	{
		let targ = target;
		if (!targ)
			return;

		if (range == 0) range = meleerange;
		else range -= 20; // DSDA always subtracts 20 from the melee range.

		A_FaceTarget();
		if (!CheckMeleeRange(range))
			return;

		A_StartSound(hitsound, CHAN_WEAPON);

		int damage = (random[mbf21]() % damagemod + 1) * damagebase;
		int newdam = targ.DamageMobj(self, self, damage, "Melee");
		targ.TraceBleed(newdam > 0 ? newdam : damage, self);
	}

	//
	// A_NoiseAlert
	// Alerts nearby monsters (via sound) to the calling actor's target's presence.
	//
	void A_NoiseAlert()
	{
		if (target) SoundAlert(target);
	}

	//
	// A_HealChase
	// A parameterized version of A_VileChase.
	//   args[0]: State to jump to on the calling actor when resurrecting a corpse
	//   args[1]: Sound to play when resurrecting a corpse
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_HealChase(State healstate, Sound healsound)
	{
		if (!A_CheckForResurrection(healstate, healsound))
			A_Chase();
	}

	//
	// A_FindTracer
	// Search for a valid tracer (seek target), if the calling actor doesn't already have one.
	//   args[0]: field-of-view to search in (degrees, in fixed point); if zero, will search in all directions
	//   args[1]: distance to search (map blocks, i.e. 128 units)
	//
	void A_FindTracer(double fov, int dist)
	{
		// note: mbf21 fov is the angle of the entire cone, while
		// zdoom fov is defined as 1/2 of the cone, so halve it.
		if (!tracer) tracer = RoughMonsterSearch(dist, fov: fov/2);
	}

	//
	// A_ClearTracer
	// Clear current tracer (seek target).
	//
	void A_ClearTracer()
	{
		tracer = NULL;
	}

	//
	// A_JumpIfHealthBelow
	// Jumps to a state if caller's health is below the specified threshold.
	//   args[0]: State to jump to
	//   args[1]: Health threshold
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_JumpIfHealthBelow(State tstate, int health)
	{
		if (self.health < health) self.SetState(tstate);
	}

	//
	// A_JumpIfTargetInSight
	// Jumps to a state if caller's target is in line-of-sight.
	//   args[0]: State to jump to
	//   args[1]: Field-of-view to check (degrees, in fixed point); if zero, will check in all directions
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_JumpIfTargetInSight(State tstate, double fov)
	{
		if (!target)
			return;

		// Check FOV first since it's faster
		if (fov > 0 && !CheckFov(target, fov))
			return;

		if (CheckSight(target)) self.SetState(tstate);
	}

	//
	// A_JumpIfTargetCloser
	// Jumps to a state if caller's target is closer than the specified distance.
	//   args[0]: State to jump to
	//   args[1]: Distance threshold
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_JumpIfTargetCloser(State tstate, double dist)
	{
		if (!target)
			return;

		if (dist > Distance2D(target)) self.SetState(tstate);
	}

	//
	// A_JumpIfTracerInSight
	// Jumps to a state if caller's tracer (seek target) is in line-of-sight.
	//   args[0]: State to jump to
	//   args[1]: Field-of-view to check (degrees, in fixed point); if zero, will check in all directions
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_JumpIfTracerInSight(State tstate, double fov)
	{
		if (!tracer)
			return;

		// Check FOV first since it's faster
		if (fov > 0 && !CheckFov(tracer, fov))
			return;

		if (CheckSight(tracer)) self.SetState(tstate);
	}

	//
	// A_JumpIfTracerCloser
	// Jumps to a state if caller's tracer (seek target) is closer than the specified distance.
	//   args[0]: State to jump to
	//   args[1]: Distance threshold (fixed point)
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_JumpIfTracerCloser(State tstate, double dist)
	{
		if (!tracer)
			return;

		if (dist > Distance2D(tracer)) self.SetState(tstate);
	}

	// These are native to lock away the insanity.
	deprecated("2.3", "for Dehacked use only") native void MBF21_JumpIfFlagsSet(State tstate, int flags, int flags2);
	deprecated("2.3", "for Dehacked use only") native void MBF21_AddFlags(int flags, int flags2);
	deprecated("2.3", "for Dehacked use only") native void MBF21_RemoveFlags(int flags, int flags2);
}

extend class Weapon
{
	//
	// A_WeaponProjectile
	// A parameterized player weapon projectile attack. Does not consume ammo.
	//   args[0]: Type of actor to spawn
	//   args[1]: Angle (degrees, in fixed point), relative to calling player's angle
	//   args[2]: Pitch (degrees, in fixed point), relative to calling player's pitch; approximated
	//   args[3]: X/Y spawn offset, relative to calling player's angle
	//   args[4]: Z spawn offset, relative to player's default projectile fire height
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_WeaponProjectile(class<Actor> type, double angle, double pitch, double Spawnofs_xy, double Spawnofs_z)
	{
		if (!player || !type)
			return;

		FTranslatedLineTarget t;

		angle += self.angle;
		Vector2 ofs = AngleToVector(self.Angle - 90, spawnofs_xy);

		let mo = SpawnPlayerMissile(type, angle, ofs.x, ofs.y, Spawnofs_z, pLineTarget: t);
		if (!mo) return;

		Pitch += mo.PitchFromVel();
		mo.Vel3DFromAngle(mo.Speed, mo.angle, Pitch);

		// set tracer to the player's autoaim target,
		// so player seeker missiles prioritizing the
		// baddie the player is actually aiming at. ;)
		mo.tracer = t.linetarget;
	}

	//
	// A_WeaponBulletAttack
	// A parameterized player weapon bullet attack. Does not consume ammo.
	//   args[0]: Horizontal spread (degrees, in fixed point)
	//   args[1]: Vertical spread (degrees, in fixed point)
	//   args[2]: Number of bullets to fire; if not set, defaults to 1
	//   args[3]: Base damage of attack (e.g. for 5d3, customize the 5); if not set, defaults to 5
	//   args[4]: Attack damage modulus (e.g. for 5d3, customize the 3); if not set, defaults to 3
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_WeaponBulletAttack(double hspread, double vspread, int numbullets, int damagebase, int damagemod)
	{
		let bangle = angle;
		let slope = BulletSlope();

		for (int i = 0; i < numbullets; i++)
		{
			int damage = (random[mbf21]() % damagemod + 1) * damagebase;
			let pangle = bangle + hspread * Random2[mbf21]() / 255.;
			let pslope = slope + vspread * Random2[mbf21]() / 255.;
			LineAttack(pangle, PLAYERMISSILERANGE, pslope, damage, "Hitscan", "Bulletpuff");
		}
	}


	//
	// A_WeaponMeleeAttack
	// A parameterized player weapon melee attack.
	//   args[0]: Base damage of attack (e.g. for 2d10, customize the 2); if not set, defaults to 2
	//   args[1]: Attack damage modulus (e.g. for 2d10, customize the 10); if not set, defaults to 10
	//   args[2]: Berserk damage multiplier (fixed point); if not set, defaults to 1.0 (no change).
	//   args[3]: Sound to play if attack hits
	//   args[4]: Range (fixed point); if not set, defaults to player mobj's melee range
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_WeaponMeleeAttack(int damagebase, int damagemod, double zerkfactor, Sound hitsound, double range)
	{
		if (range == 0)
			range = meleerange;

		int damage = (Random[mbf21]() % damagemod + 1) * damagebase;
		if (FindInventory("PowerStrength"))
			damage = int(damage * zerkfactor);

		// slight randomization; weird vanillaism here. :P
		FTranslatedLineTarget t;
		double ang = angle + Random2[mbf21]() * (5.625 / 256);
		double pitch = AimLineAttack(ang, range + MELEEDELTA, t, 0., ALF_CHECK3D);
		LineAttack(ang, range, pitch, damage, 'Melee', "BulletPuff", LAF_ISMELEEATTACK, t);

		// turn to face target
		if (t.linetarget)
		{
			A_StartSound(hitsound, CHAN_WEAPON);
			angle = t.angleFromSource;
		}
	}

	//
	// A_WeaponAlert
	// Alerts monsters to the player's presence. Handy when combined with WPF_SILENT.
	//
	void A_WeaponAlert()
	{
		SoundAlert(self);
	}

	//
	// A_WeaponJump
	// Jumps to the specified state, with variable random chance.
	// Basically the same as A_RandomJump, but for weapons.
	//   args[0]: State number
	//   args[1]: Chance, out of 255, to make the jump
	//
	deprecated("2.3", "for Dehacked use only")
	action void MBF21_WeaponJump(State tstate, int chance)
	{
		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			let player = self.player;
			if (player == null) return;
			if (random[mbf21]() < chance)
				player.SetPSprite(stateinfo.mPSPIndex, tstate);
		}
	}

	//
	// A_ConsumeAmmo
	// Subtracts ammo from the player's "inventory". 'Nuff said.
	//   args[0]: Amount of ammo to consume. If zero, use the weapon's ammo-per-shot amount.
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_ConsumeAmmo(int consume)
	{
		let player = self.player;
		if (!player) return;
		let weap = player.ReadyWeapon;
		if (!weap) return;

		if (consume == 0) consume = -1;
		weap.DepleteAmmo(weap.bAltFire, false, consume, true);
	}

	//
	// A_CheckAmmo
	// Jumps to a state if the player's ammo is lower than the specified amount.
	//   args[0]: State to jump to
	//   args[1]: Minimum required ammo to NOT jump. If zero, use the weapon's ammo-per-shot amount.
	//
	deprecated("2.3", "for Dehacked use only")
	action void MBF21_CheckAmmo(State tstate, int amount)
	{
		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			let player = self.player;
			if (player == null) return;
			let weap = player.ReadyWeapon;
			if (!weap) return;

			if (amount == 0) amount = -1;
			if (!weap.CheckAmmo(weap.bAltFire ? AltFire : PrimaryFire, false, false, amount))
				player.SetPSprite(stateinfo.mPSPIndex, tstate);
		}
	}

	//
	// A_RefireTo
	// Jumps to a state if the player is holding down the fire button
	//   args[0]: State to jump to
	//   args[1]: If nonzero, skip the ammo check
	//
	deprecated("2.3", "for Dehacked use only")
	action void MBF21_RefireTo(State tstate, int skipcheck)
	{
		if (stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			let player = self.player;
			if (player == null) return;
			let weap = player.ReadyWeapon;
			if (!weap) return;

			let pending = player.PendingWeapon != WP_NOCHANGE && (player.WeaponState & WF_REFIRESWITCHOK);

			if (!skipcheck && !weap.CheckAmmo(weap.bAltFire ? AltFire : PrimaryFire, false, false)) return;

			if ((player.cmd.buttons & BT_ATTACK)
				&& !player.ReadyWeapon.bAltFire && !pending && player.health > 0)
			{
				player.SetPSprite(stateinfo.mPSPIndex, tstate);
			}
		}
	}

	//
	// A_GunFlashTo
	// Sets the weapon flash layer to the specified state.
	//   args[0]: State number
	//   args[1]: If nonzero, don't change the player actor state
	//
	deprecated("2.3", "for Dehacked use only")
	void MBF21_GunFlashTo(State tstate, int dontchangeplayer)
	{
		let player = self.player;
		if (player == null) return;
		Weapon weapon = player.ReadyWeapon;
		if (!weapon) return;

		if (!dontchangeplayer) player.mo.PlayAttacking2();
		player.SetPsprite(PSP_FLASH, tstate);
	}

	// needed to call A_SeekerMissile with proper defaults.
	deprecated("2.3", "for Dehacked use only")
	void MBF21_SeekTracer(double threshold, double turnmax)
	{
		A_SeekerMissile(int(threshold), int(turnmax), flags: SMF_PRECISE); // args get truncated to ints here, but it's close enough
	}

}