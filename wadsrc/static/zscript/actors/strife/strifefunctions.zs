// common Strife action functions that are used by multiple different actors

extend class Actor
{

	//============================================================================

	void A_FLoopActiveSound()
	{
		if (ActiveSound != 0 && !(Level.maptime & 7))
		{
			A_PlaySound (ActiveSound, CHAN_VOICE);
		}
	}

	void A_LoopActiveSound()
	{
		A_PlaySound(ActiveSound, CHAN_VOICE, 1, true);
	}

	//============================================================================
	//
	// 
	//
	//============================================================================

	void A_Countdown()
	{
		if (--reactiontime <= 0)
		{
			ExplodeMissile ();
			bSkullFly = false;
		}
	}

	//============================================================================
	//
	// A_ClearSoundTarget
	//
	//============================================================================

	void A_ClearSoundTarget()
	{
		CurSector.SoundTarget = null;
		for (Actor mo = CurSector.thinglist; mo != null; mo = mo.snext)
		{
			mo.LastHeard = null;
		}
	}
	
	//==========================================================================
	//
	// A_TossGib
	//
	//==========================================================================

	void A_TossGib()
	{
		class <Actor> gibtype;
		if (bNoBlood) gibtype = "Junk";
		else gibtype = "Meat";
		Actor gib = Spawn (gibtype, pos + (0,0,24), ALLOW_REPLACE);

		if (gib == null)
		{
			return;
		}

		gib.Angle = random[GibTosser]() * (360 / 256.);
		gib.VelFromAngle(random[GibTosser](0, 15));
		gib.Vel.Z = random[GibTosser](0, 15);
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_ShootGun()
	{
		if (!target) return;
		A_PlaySound ("monsters/rifle", CHAN_WEAPON);
		A_FaceTarget ();
		double pitch = AimLineAttack (angle, MISSILERANGE);
		LineAttack (Angle + Random2[ShootGun]() * (11.25 / 256), MISSILERANGE, pitch, 3*random[ShootGun](1, 5), 'Hitscan', "StrifePuff");
	}
	
	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_SetShadow()
	{
		bShadow = true;
		A_SetRenderStyle(HR_SHADOW, STYLE_Translucent);
	}

	void A_ClearShadow()
	{
		bShadow = false;
		A_SetRenderStyle(1, STYLE_Normal);
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_GetHurt()
	{
		bInCombat = true;
		if (random[HurtMe](0, 4) == 0)
		{
			A_PlaySound (PainSound, CHAN_VOICE);
			health--;
		}
		if (health <= 0)
		{
			Die (target, target);
		}
	}
	
	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_DropFire()
	{
		Actor drop = Spawn("FireDroplet", pos + (0,0,24), ALLOW_REPLACE);
		if (drop != null)
		{
			drop.Vel.Z = -1.;
		}
		A_Explode(64, 64, XF_NOSPLASH|XF_HURTSOURCE|XF_NOTMISSILE, damagetype: 'Fire');
	}	

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_RemoveForceField()
	{
		bSpecial = false;
		CurSector.RemoveForceField();
	}
	
	//==========================================================================
	//
	//
	//
	//==========================================================================

	void A_AlertMonsters(double maxdist = 0, int flags = 0)
	{
		Actor target = null;
		Actor emitter = self;

		if (player != null || (Flags & AMF_TARGETEMITTER))
		{
			target = self;
		}
		else if (self.target != null && (self.target.player != null || (Flags & AMF_TARGETNONPLAYER)))
		{
			target = self.target;
		}

		if (Flags & AMF_EMITFROMTARGET) emitter = target;

		if (target != null && emitter != null)
		{
			emitter.SoundAlert(target, false, maxdist);
		}
	}

	//============================================================================
	//
	// A_RocketInFlight
	//
	//============================================================================

	void A_RocketInFlight()
	{
		A_PlaySound ("misc/missileinflight", CHAN_VOICE);
		SpawnPuff ("MiniMissilePuff", Pos, Angle - 180, Angle - 180, 2, PF_HITTHING);
		Actor trail = Spawn("RocketTrail", Vec3Offset(-Vel.X, -Vel.Y, 0.), ALLOW_REPLACE);
		if (trail != null)
		{
			trail.Vel.Z = 1;
		}
	}

	
}
