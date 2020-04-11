
// Serpent ------------------------------------------------------------------

class Serpent : Actor
{
	Default
	{
		Health 90;
		PainChance 96;
		Speed 12;
		Radius 32;
		Height 70;
		Mass 0x7fffffff;
		Monster;
		-SHOOTABLE
		+NOBLOOD
		+CANTLEAVEFLOORPIC +NONSHOOTABLE
		+STAYMORPHED +DONTBLAST +NOTELEOTHER
		+INVISIBLE
		SeeSound "SerpentSight";
		AttackSound "SerpentAttack";
		PainSound "SerpentPain";
		DeathSound "SerpentDeath";
		HitObituary "$OB_SERPENTHIT";
		Tag "$FN_SERPENT";
	}

	States
	{
	Spawn:
		SSPT H 10 A_Look;
		Loop;
	See:
		SSPT HH 1 A_Chase("Melee", null, CHF_NIGHTMAREFAST|CHF_NOPLAYACTIVE);
		SSPT H 2 A_SerpentHumpDecide;
		Loop;
	Pain:
		SSPT L 5;
		SSPT L 5 A_Pain;
	Dive:
		SSDV ABC 4;
		SSDV D 4 A_UnSetShootable;
		SSDV E 3 A_StartSound("SerpentActive", CHAN_BODY);
		SSDV F 3;
		SSDV GH 4;
		SSDV I 3;
		SSDV J 3 A_SerpentHide;
		Goto See;
	Melee:
		SSPT A 1 A_UnHideThing;
		SSPT A 1 A_StartSound("SerpentBirth", CHAN_BODY);
		SSPT B 3 A_SetShootable;
		SSPT C 3;
		SSPT D 4 A_SerpentCheckForAttack;
		Goto Dive;
	Death:
		SSPT O 4;
		SSPT P 4 A_Scream;
		SSPT Q 4 A_NoBlocking;
		SSPT RSTUVWXYZ 4;
		Stop;
	XDeath:
		SSXD A 4;
		SSXD B 4 A_SpawnItemEx("SerpentHead", 0, 0, 45);
		SSXD C 4 A_NoBlocking;
		SSXD DE 4;
		SSXD FG 3;
		SSXD H 3 A_SerpentSpawnGibs;
		Stop;
	Ice:
		SSPT [ 5 A_FreezeDeath;
		SSPT [ 1 A_FreezeDeathChunks;
		Wait;
	Walk:
		SSPT IJI 5 A_Chase("Attack", null, CHF_NIGHTMAREFAST);
		SSPT J 5 A_SerpentCheckForAttack;
		Goto Dive;
	Hump:
		SSPT H 3 A_SerpentUnHide;
		SSPT EFGEF 3 A_SerpentRaiseHump;
		SSPT GEF 3;
		SSPT GEFGE 3 A_SerpentLowerHump;
		SSPT F 3 A_SerpentHide;
		Goto See;
	Attack:
		SSPT K 6 A_FaceTarget;
		SSPT L 5 A_SerpentChooseAttack;
		Goto MeleeAttack;
	MeleeAttack:
		SSPT N 5 A_SerpentMeleeAttack;
		Goto Dive;
	}
	
	//============================================================================
	//
	// A_SerpentUnHide
	//
	//============================================================================

	void A_SerpentUnHide()
	{
		bInvisible = false;
		Floorclip = 24;
	}

	//============================================================================
	//
	// A_SerpentHide
	//
	//============================================================================

	void A_SerpentHide()
	{
		bInvisible = true;
		Floorclip = 0;
	}

	//============================================================================
	//
	// A_SerpentRaiseHump
	// 
	// Raises the hump above the surface by raising the floorclip level
	//============================================================================

	void A_SerpentRaiseHump()
	{
		Floorclip -= 4;
	}

	//============================================================================
	//
	// A_SerpentLowerHump
	// 
	//============================================================================

	void A_SerpentLowerHump()
	{
		Floorclip += 4;
	}

	//============================================================================
	//
	// A_SerpentHumpDecide
	//
	//		Decided whether to hump up, or if the mobj is a serpent leader, 
	//			to missile attack
	//============================================================================

	void A_SerpentHumpDecide()
	{
		if (MissileState != NULL)
		{
			if (random[SerpentHump]() > 30)
			{
				return;
			}
			else if (random[SerpentHump]() < 40)
			{ // Missile attack
				SetState (MeleeState);
				return;
			}
		}
		else if (random[SerpentHump]() > 3)
		{
			return;
		}
		if (!CheckMeleeRange ())
		{ // The hump shouldn't occur when within melee range
			if (MissileState != NULL && random[SerpentHump]() < 128)
			{
				SetState (MeleeState);
			}
			else
			{	
				SetStateLabel("Hump");
				A_StartSound ("SerpentActive", CHAN_BODY);
			}
		}
	}

	//============================================================================
	//
	// A_SerpentCheckForAttack
	//
	//============================================================================

	void A_SerpentCheckForAttack()
	{
		if (!target)
		{
			return;
		}
		if (MissileState != NULL)
		{
			if (!CheckMeleeRange ())
			{
				SetStateLabel ("Attack");
				return;
			}
		}
		if (CheckMeleeRange2 ())
		{
			SetStateLabel ("Walk");
		}
		else if (CheckMeleeRange ())
		{
			if (random[SerpentAttack]() < 32)
			{
				SetStateLabel ("Walk");
			}
			else
			{
				SetStateLabel ("Attack");
			}
		}
	}

	//============================================================================
	//
	// A_SerpentChooseAttack
	//
	//============================================================================
	
	void A_SerpentChooseAttack()
	{
		if (!target || CheckMeleeRange())
		{
			return;
		}
		if (MissileState != NULL)
		{
			SetState (MissileState);
		}
	}
		
	//============================================================================
	//
	// A_SerpentMeleeAttack
	//
	//============================================================================

	void A_SerpentMeleeAttack()
	{
		let targ = target;
		if (!targ)
		{
			return;
		}
		if (CheckMeleeRange ())
		{
			int damage = random[SerpentAttack](1, 8) * 5;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			A_StartSound ("SerpentMeleeHit", CHAN_BODY);
		}
		if (random[SerpentAttack]() < 96)
		{
			A_SerpentCheckForAttack();
		}
	}

	//============================================================================
	//
	// A_SerpentSpawnGibs
	//
	//============================================================================

	void A_SerpentSpawnGibs()
	{
		static const class<Actor> GibTypes[] =
		{
			"SerpentGib3",
			"SerpentGib2",
			"SerpentGib1"
		};

		for (int i = 2; i >= 0; --i)
		{
			double x = (random[SerpentGibs]() - 128) / 16.;
			double y = (random[SerpentGibs]() - 128) / 16.;

			Actor mo = Spawn (GibTypes[i], Vec2OffsetZ(x, y, floorz + 1), ALLOW_REPLACE);
			if (mo)
			{
				mo.Vel.X = (random[SerpentGibs]() - 128) / 1024.f;
				mo.Vel.Y = (random[SerpentGibs]() - 128) / 1024.f;
				mo.Floorclip = 6;
			}
		}
	}
}

extend class Actor
{
	//----------------------------------------------------------------------------
	//
	// FUNC P_CheckMeleeRange2
	//
	// This belongs to the Serpent but was initially exported on Actor
	// so it needs to remain there.
	//
	//----------------------------------------------------------------------------

	bool CheckMeleeRange2 ()
	{
		Actor mo;
		double dist;


		if (!target || (CurSector.Flags & Sector.SECF_NOATTACK))
		{
			return false;
		}
		mo = target;
		dist = mo.Distance2D (self);
		if (dist >= 128 || dist < meleerange + mo.radius)
		{
			return false;
		}
		if (mo.pos.Z > pos.Z + height)
		{ // Target is higher than the attacker
			return false;
		}
		else if (pos.Z > mo.pos.Z + mo.height)
		{ // Attacker is higher
			return false;
		}
		else if (IsFriend(mo))
		{
			// killough 7/18/98: friendly monsters don't attack other friends
			return false;
		}
		return CheckSight(mo);
	}
}

// Serpent Leader -----------------------------------------------------------

class SerpentLeader : Serpent
{
	Default
	{
		Mass 200;
		Obituary "$OB_SERPENT";
	}
	States
	{
	Missile:
		SSPT N 5 A_SpawnProjectile("SerpentFX", 32, 0);
		Goto Dive;
	}
}

// Serpent Missile Ball -----------------------------------------------------

class SerpentFX : Actor
{
	Default
	{
		Speed 15;
		Radius 8;
		Height 10;
		Damage 4;
		Projectile;
		-ACTIVATEIMPACT -ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
		DeathSound "SerpentFXHit";
	}
	States
	{
	Spawn:
		SSFX A 0;
		SSFX A 3 Bright A_StartSound("SerpentFXContinuous", CHAN_BODY, CHANF_LOOPING);
		SSFX BAB 3 Bright;
		Goto Spawn+1;
	Death:
		SSFX C 4 Bright A_StopSound(CHAN_BODY);
		SSFX DEFGH 4 Bright;
		Stop;
	}
}

// Serpent Head -------------------------------------------------------------

class SerpentHead : Actor
{
	Default
	{
		Radius 5;
		Height 10;
		Gravity 0.125;
		+NOBLOCKMAP
	}

	States
	{
	Spawn:
		SSXD IJKLMNOP 4 A_SerpentHeadCheck;
		Loop;
	Death:
		SSXD S -1;
		Loop;
	}
	
	//============================================================================
	//
	// A_SerpentHeadCheck
	//
	//============================================================================

	void A_SerpentHeadCheck()
	{
		if (pos.z <= floorz)
		{
			if (GetFloorTerrain().IsLiquid)
			{
				HitFloor ();
				Destroy();
			}
			else
			{
				SetStateLabel ("Death");
			}
		}
	}
}

// Serpent Gib 1 ------------------------------------------------------------

class SerpentGib1 : Actor
{
	Default
	{
		Radius 3;
		Height 3;
		+NOBLOCKMAP +NOGRAVITY
	}

	States
	{
	Spawn:
		SSXD Q 6;
		SSXD Q 6 A_FloatGib;
		SSXD QQ 8 A_FloatGib;
		SSXD QQ 12 A_FloatGib;
		SSXD Q 232 A_DelayGib;
		SSXD QQ 12 A_SinkGib;
		SSXD QQQ 8 A_SinkGib;
		Stop;
	}
	
	//============================================================================
	//
	// A_FloatGib
	//
	//============================================================================

	void A_FloatGib()
	{
		Floorclip -= 1;
	}

	//============================================================================
	//
	// A_SinkGib
	//
	//============================================================================

	void A_SinkGib()
	{
		Floorclip += 1;
	}

	//============================================================================
	//
	// A_DelayGib
	//
	//============================================================================

	void A_DelayGib()
	{
		tics -= random[DelayGib]() >> 2;
	}

	
}

// Serpent Gib 2 ------------------------------------------------------------

class SerpentGib2 : SerpentGib1
{
	States
	{
	Spawn:
		SSXD R 6;
		SSXD R 6 A_FloatGib;
		SSXD RR 8 A_FloatGib;
		SSXD RR 12 A_FloatGib;
		SSXD R 232 A_DelayGib;
		SSXD RR 12 A_SinkGib;
		SSXD RRR 8 A_SinkGib;
		Stop;
	}
}

// Serpent Gib 3 ------------------------------------------------------------

class SerpentGib3 : SerpentGib1
{
	States
	{
	Spawn:
		SSXD T 6;
		SSXD T 6 A_FloatGib;
		SSXD TT 8 A_FloatGib;
		SSXD TT 12 A_FloatGib;
		SSXD T 232 A_DelayGib;
		SSXD TT 12 A_SinkGib;
		SSXD TTT 8 A_SinkGib;
		Stop;
	}
}
