
// Scriptable marine -------------------------------------------------------

class ScriptedMarine : Actor
{
	const MARINE_PAIN_CHANCE = 160;

	enum EMarineWeapon
	{
		WEAPON_Dummy,
		WEAPON_Fist,
		WEAPON_BerserkFist,
		WEAPON_Chainsaw,
		WEAPON_Pistol,
		WEAPON_Shotgun,
		WEAPON_SuperShotgun,
		WEAPON_Chaingun,
		WEAPON_RocketLauncher,
		WEAPON_PlasmaRifle,
		WEAPON_Railgun,
		WEAPON_BFG
	};
	
	struct WeaponStates
	{
		state melee;
		state missile;
	}

	int CurrentWeapon;
	SpriteID SpriteOverride;
	
	Default
	{
		Health 100;
		Radius 16;
		Height 56;
		Mass 100;
		Speed 8;
		Painchance MARINE_PAIN_CHANCE;
		MONSTER;
		-COUNTKILL
		Translation 0;
		Damage 100;
		DeathSound "*death";
		PainSound "*pain50";
	}
	
	States
	{
	Spawn:
		PLAY A 4 A_MarineLook;
		PLAY A 4 A_MarineNoise;
		Loop;
	Idle:
		PLAY A 4 A_MarineLook;
		PLAY A 4 A_MarineNoise;
		PLAY A 4 A_MarineLook;
		PLAY B 4 A_MarineNoise;
		PLAY B 4 A_MarineLook;
		PLAY B 4 A_MarineNoise;
		Loop;
	See:
		PLAY ABCD 4 A_MarineChase;
		Loop;

	Melee.Fist:		
		PLAY E 4 A_FaceTarget;
		PLAY E 4 A_M_Punch(1);
		PLAY A 9;
		PLAY A 0 A_M_Refire(1, "FistEnd");
		Loop;
	FistEnd:
		PLAY A 5 A_FaceTarget;
		Goto See;
	Melee.Berserk:
		PLAY E 4 A_FaceTarget;
		PLAY E 4 A_M_Punch(10);
		PLAY A 9;
		PLAY A 0 A_M_Refire(1, "FistEnd");
		Loop;
	Melee.Chainsaw:
		PLAY E 4 A_MarineNoise;
		PLAY E 4 A_M_Saw;
		PLAY E 0 A_M_SawRefire;
		goto Melee.Chainsaw+1;

	Missile:
	Missile.None:
		PLAY E 12 A_FaceTarget;
		Goto Idle;
		PLAY F 6 BRIGHT;
		Loop;
	Missile.Pistol:
		PLAY E 4 A_FaceTarget;
		PLAY F 6 BRIGHT A_M_FirePistol(1);
		PLAY A 4 A_FaceTarget;
		PLAY A 0 A_M_Refire(0, "ShootEnd");
		Goto Fireloop.Pistol;
	ShootEnd:
		PLAY A 5;
		Goto See;
	Fireloop.Pistol:
		PLAY F 6 BRIGHT A_M_FirePistol(0);
		PLAY A 4 A_FaceTarget;
		PLAY A 0 A_M_Refire(0, "ShootEnd");
		Goto Fireloop.Pistol;
	Missile.Shotgun:
		PLAY E 3 A_M_CheckAttack;
		PLAY F 7 BRIGHT A_M_FireShotgun;
		Goto See;
	Missile.SSG:
		PLAY E 3 A_M_CheckAttack;
		PLAY F 7 BRIGHT A_M_FireShotgun2;
		Goto See;
	Missile.Chaingun:
		PLAY E 4 A_FaceTarget;
		PLAY FF 4 BRIGHT A_M_FireCGun(1);
		PLAY FF 4 BRIGHT A_M_FireCGun(0);
		PLAY A 0 A_M_Refire(0, "See");
		Goto Missile.Chaingun+3;
	Missile.Rocket:
		PLAY E 8;
		PLAY F 6 BRIGHT A_M_FireMissile;
		PLAY E 6;
		PLAY A 0 A_M_Refire(0, "See");
		Loop;
	Missile.Plasma:
		PLAY E 2 A_FaceTarget;
		PLAY E 0 A_FaceTarget;
		PLAY F 3 BRIGHT A_M_FirePlasma;
		PLAY A 0 A_M_Refire(0, "See");
		Goto Missile.Plasma+1;
	Missile.Railgun:
		PLAY E 4 A_M_CheckAttack;
		PLAY F 6 BRIGHT A_M_FireRailgun;
		Goto See;
	Missile.BFG:
		PLAY E 5 A_M_BFGSound;
		PLAY EEEEE 5 A_FaceTarget;
		PLAY F 6 BRIGHT A_M_FireBFG;
		PLAY A 4 A_FaceTarget;
		PLAY A 0 A_M_Refire(0, "See");
		Loop;

	SkipAttack:
		PLAY A 1;
		Goto See;
	Pain:
		PLAY G 4;
		PLAY G 4 A_Pain;
		Goto Idle;
	Death:
		PLAY H 10;
		PLAY I 10 A_Scream;
		PLAY J 10 A_NoBlocking;
		PLAY KLM 10;
		PLAY N -1;
		Stop;
	XDeath:
		PLAY O 5;
		PLAY P 5 A_XScream;
		PLAY Q 5 A_NoBlocking;
		PLAY RSTUV 5;
		PLAY W -1;
		Stop;
	Raise:
		PLAY MLKJIH 5;
		Goto See;
	}
	
	//============================================================================
	//
	// 
	//
	//============================================================================

	private bool GetWeaponStates(int weap, out WeaponStates wstates)
	{
		static const statelabel MeleeNames[] = 
		{
			"Melee.None", "Melee.Fist", "Melee.Berserk", "Melee.Chainsaw", "Melee.Pistol", "Melee.Shotgun", 
			"Melee.SSG", "Melee.Chaingun", "Melee.Rocket", "Melee.Plasma", "Melee.Railgun", "Melee.BFG"
		};

		static const statelabel MissileNames[] = 
		{
			"Missile.None", "Missile.Fist", "Missile.Berserk", "Missile.Chainsaw", "Missile.Pistol", "Missile.Shotgun", 
			"Missile.SSG", "Missile.Chaingun", "Missile.Rocket", "Missile.Plasma", "Missile.Railgun", "Missile.BFG"
		};
		
		if (weap < WEAPON_Dummy || weap > WEAPON_BFG) weap = WEAPON_Dummy;

		wstates.melee = FindState(MeleeNames[weap], true);
		wstates.missile = FindState(MissileNames[weap], true);

		return wstates.melee != null || wstates.missile != null;
	}

	//============================================================================
	//
	// 
	//
	//============================================================================

	override void BeginPlay ()
	{
		Super.BeginPlay ();

		// Set the current weapon
		for(int i = WEAPON_Dummy; i <= WEAPON_BFG; i++)
		{
			WeaponStates wstates;
			if (GetWeaponStates(i, wstates))
			{
				if (wstates.melee == MeleeState && wstates.missile == MissileState)
				{
					CurrentWeapon = i;
				}
			}
		}
	}

	//============================================================================
	//
	// 
	//
	//============================================================================
	
	override void Tick ()
	{
		Super.Tick ();

		// Override the standard sprite, if desired
		if (SpriteOverride != 0 && sprite == SpawnState.sprite)
		{
			sprite = SpriteOverride;
		}

		if (special1 != 0)
		{
			if (CurrentWeapon == WEAPON_SuperShotgun)
			{ // Play SSG reload sounds
				int ticks = level.maptime - special1;
				if (ticks < 47)
				{
					switch (ticks)
					{
					case 14:
						A_PlaySound ("weapons/sshoto", CHAN_WEAPON);
						break; 
					case 28:   
						A_PlaySound ("weapons/sshotl", CHAN_WEAPON);
						break;
					case 41:  
						A_PlaySound ("weapons/sshotc", CHAN_WEAPON);
						break;
					}
				}
				else
				{
					special1 = 0;
				}
			}
			else
			{ // Wait for a long refire time
				if (level.maptime >= special1)
				{
					special1 = 0;
				}
				else
				{
					bJustAttacked = true;
				}
			}
		}
	}

	//============================================================================
	//
	// A_M_Refire
	//
	//============================================================================

	void A_M_Refire (bool ignoremissile = false, statelabel jumpto = null)
	{
		if (target == null || target.health <= 0)
		{
			if (MissileState && random[SMarineRefire]() < 160)
			{ // Look for a new target most of the time
				if (LookForPlayers (true) && CheckMissileRange ())
				{ // Found somebody new and in range, so don't stop shooting
					return;
				}
			}
			if (jumpto != null) SetStateLabel (jumpto);
			else SetState(CurState + 1);
			return;
		}
		if (((ignoremissile || MissileState == null) && !CheckMeleeRange ()) ||
			!CheckSight (target) ||	random[SMarineRefire]() < 4)	// Small chance of stopping even when target not dead
		{
			if (jumpto != null) SetStateLabel (jumpto);
			else SetState(CurState + 1);
		}
	}

	//============================================================================
	//
	// A_M_SawRefire
	//
	//============================================================================

	void A_M_SawRefire ()
	{
		if (target == null || target.health <= 0 || !CheckMeleeRange ())
		{
			SetStateLabel ("See");
		}
	}

	//============================================================================
	//
	// A_MarineNoise
	//
	//============================================================================

	void A_MarineNoise ()
	{
		if (CurrentWeapon == WEAPON_Chainsaw)
		{
			A_PlaySound ("weapons/sawidle", CHAN_WEAPON);
		}
	}

	//============================================================================
	//
	// A_MarineChase
	//
	//============================================================================

	void A_MarineChase ()
	{
		A_MarineNoise();
		A_Chase ();
	}

	//============================================================================
	//
	// A_MarineLook
	//
	//============================================================================

	void A_MarineLook ()
	{
		A_MarineNoise();
		A_Look();
	}

	//============================================================================
	//
	// A_M_Punch (also used in the rocket attack.)
	//
	//============================================================================

	void A_M_Punch(int damagemul)
	{
		FTranslatedLineTarget t;

		if (target == null)
			return;

		int damage = (random[SMarinePunch](1, 10) << 1) * damagemul;

		A_FaceTarget ();
		double ang = angle + random2[SMarinePunch]() * (5.625 / 256);
		double pitch = AimLineAttack (ang, DEFMELEERANGE);
		LineAttack (ang, DEFMELEERANGE, pitch, damage, 'Melee', "BulletPuff", true, t);

		// turn to face target
		if (t.linetarget)
		{
			A_PlaySound ("*fist", CHAN_WEAPON);
			angle = t.angleFromSource;
		}
	}

	//============================================================================
	//
	// P_GunShot2
	//
	//============================================================================

	private void GunShot2 (bool accurate, double pitch, class<Actor> pufftype)
	{
		int damage = 5 * random[SMarineGunshot](1,3);
		double ang = angle;

		if (!accurate)
		{
			ang += Random2[SMarineGunshot]() * (5.625 / 256);
		}

		LineAttack (ang, MISSILERANGE, pitch, damage, 'Hitscan', pufftype);
	}

	//============================================================================
	//
	// A_M_FirePistol
	//
	//============================================================================

	void A_M_FirePistol (bool accurate)
	{
		if (target == null)
			return;

		A_PlaySound ("weapons/pistol", CHAN_WEAPON);
		A_FaceTarget ();
		GunShot2 (accurate, AimLineAttack (angle, MISSILERANGE), "BulletPuff");
	}

	//============================================================================
	//
	// A_M_FireShotgun
	//
	//============================================================================

	void A_M_FireShotgun ()
	{
		if (target == null)
			return;

		A_PlaySound ("weapons/shotgf", CHAN_WEAPON);
		A_FaceTarget ();
		double pitch = AimLineAttack (angle, MISSILERANGE);
		for (int i = 0; i < 7; ++i)
		{
			GunShot2 (false, pitch, "BulletPuff");
		}
		special1 = level.maptime + 27;
	}

	//============================================================================
	//
	// A_M_CheckAttack
	//
	//============================================================================

	void A_M_CheckAttack ()
	{
		if (special1 != 0 || target == null)
		{
			SetStateLabel ("SkipAttack");
		}
		else
		{
			A_FaceTarget ();
		}
	}

	//============================================================================
	//
	// A_M_FireShotgun2
	//
	//============================================================================

	void A_M_FireShotgun2 ()
	{
		if (target == null)
			return;

		A_PlaySound ("weapons/sshotf", CHAN_WEAPON);
		A_FaceTarget ();
		double pitch = AimLineAttack (angle, MISSILERANGE);
		for (int i = 0; i < 20; ++i)
		{
			int damage = 5*(random[SMarineFireSSG](1, 3));
			double ang = angle + Random2[SMarineFireSSG]() * (11.25 / 256);

			LineAttack (ang, MISSILERANGE, pitch + Random2[SMarineFireSSG]() * (7.097 / 256), damage, 'Hitscan', "BulletPuff");
		}
		special1 = level.maptime;
	}

	//============================================================================
	//
	// A_M_FireCGun
	//
	//============================================================================

	void A_M_FireCGun(bool accurate)
	{
		if (target == null)
			return;

		A_PlaySound ("weapons/chngun", CHAN_WEAPON);
		A_FaceTarget ();
		GunShot2 (accurate, AimLineAttack (angle, MISSILERANGE), "BulletPuff");
	}

	//============================================================================
	//
	// A_M_FireMissile
	//
	// Giving a marine a rocket launcher is probably a bad idea unless you pump
	// up his health, because he's just as likely to kill himself as he is to
	// kill anything else with it.
	//
	//============================================================================

	void A_M_FireMissile ()
	{
		if (target == null)
			return;

		if (CheckMeleeRange ())
		{ // If too close, punch it
			A_M_Punch(1);
		}
		else
		{
			A_FaceTarget ();
			SpawnMissile (target, "Rocket");
		}
	}

	//============================================================================
	//
	// A_M_FireRailgun
	//
	//============================================================================

	void A_M_FireRailgun ()
	{
		if (target == null)
			return;

		A_MonsterRail();
		special1 = level.maptime + 50;
	}

	//============================================================================
	//
	// A_M_FirePlasma
	//
	//============================================================================

	void A_M_FirePlasma ()
	{
		if (target == null)
			return;

		A_FaceTarget ();
		SpawnMissile (target, "PlasmaBall");
		special1 = level.maptime + 20;
	}

	//============================================================================
	//
	// A_M_BFGsound
	//
	//============================================================================

	void A_M_BFGsound ()
	{
		if (target == null)
			return;

		if (special1 != 0)
		{
			SetState (SeeState);
		}
		else
		{
			A_FaceTarget ();
			A_PlaySound ("weapons/bfgf", CHAN_WEAPON);
			// Don't interrupt the firing sequence
			PainChance = 0;
		}
	}

	//============================================================================
	//
	// A_M_FireBFG
	//
	//============================================================================

	void A_M_FireBFG ()
	{
		if (target == null)
			return;

		A_FaceTarget ();
		SpawnMissile (target, "BFGBall");
		special1 = level.maptime + 30;
		PainChance = MARINE_PAIN_CHANCE;
	}
		
	//---------------------------------------------------------------------------

	final void SetWeapon (int type)
	{
		WeaponStates wstates;
		if (GetWeaponStates(type, wstates))
		{
			static const class<Actor> classes[] = {
				"ScriptedMarine",
				"MarineFist",
				"MarineBerserk",
				"MarineChainsaw",
				"MarinePistol",
				"MarineShotgun",
				"MarineSSG",
				"MarineChaingun",
				"MarineRocket",
				"MarinePlasma",
				"MarineRailgun",
				"MarineBFG"
			};
			
			MeleeState = wstates.melee;
			MissileState = wstates.missile;
			DecalGenerator = GetDefaultByType(classes[type]).DecalGenerator;
		}
	}

	final void SetSprite (class<Actor> source)
	{
		if (source == null)
		{ // A valid actor class wasn't passed, so use the standard sprite
			SpriteOverride = sprite = SpawnState.sprite;
			// Copy the standard scaling
			Scale = Default.Scale;
		}
		else
		{ // Use the same sprite and scaling the passed class spawns with
			readonly<Actor> def = GetDefaultByType (source);
			SpriteOverride = sprite = def.SpawnState.sprite;
			Scale = def.Scale;
		}
	}
}

extend class Actor
{
	//============================================================================
	//
	// A_M_Saw (this is globally exported)
	//
	//============================================================================

	void A_M_Saw(sound fullsound = "weapons/sawfull", sound hitsound = "weapons/sawhit", int damage = 2, class<Actor> pufftype = "BulletPuff")
	{
		if (target == null)
			return;

		if (pufftype == null) pufftype = "BulletPuff";
		if (damage == 0) damage = 2;

		A_FaceTarget ();
		if (CheckMeleeRange ())
		{
			FTranslatedLineTarget t;

			damage *= random[SMarineSaw](1, 10);
			double ang = angle + Random2[SMarineSaw]() * (5.625 / 256);
			
			LineAttack (angle, SAWRANGE, AimLineAttack (angle, SAWRANGE), damage, 'Melee', pufftype, false, t);

			if (!t.linetarget)
			{
				A_PlaySound (fullsound, 1, CHAN_WEAPON);
				return;
			}
			A_PlaySound (hitsound, CHAN_WEAPON);
				
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
		}
		else
		{
			A_PlaySound (fullsound, 1, CHAN_WEAPON);
		}
	}
}

//---------------------------------------------------------------------------

class MarineFist : ScriptedMarine
{
	States
	{
	Melee:		
		Goto Super::Melee.Fist;
	Missile:
		Stop;
	}
}


//---------------------------------------------------------------------------

class MarineBerserk : MarineFist
{
	States
	{
	Melee:		
		Goto Super::Melee.Berserk;
	Missile:
		Stop;
	}
}
//---------------------------------------------------------------------------

class MarineChainsaw : ScriptedMarine
{
	States
	{
	Melee:
		Goto Super::Melee.Chainsaw;
	Missile:
		Stop;
	}
}



//---------------------------------------------------------------------------

class MarinePistol : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.Pistol;
	}

}

//---------------------------------------------------------------------------

class MarineShotgun : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.Shotgun;
	}

}



//---------------------------------------------------------------------------

class MarineSSG : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.SSG;
	}
}

//---------------------------------------------------------------------------

class MarineChaingun : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.Chaingun;
	}
}


//---------------------------------------------------------------------------

class MarineRocket : MarineFist
{
	States
	{
	Missile:
		Goto Super::Missile.Rocket;
	}

}

//---------------------------------------------------------------------------

class MarinePlasma : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.Plasma;
	}

}

//---------------------------------------------------------------------------

class MarineRailgun : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.Railgun;
	}

}

//---------------------------------------------------------------------------

class MarineBFG : ScriptedMarine
{
	States
	{
	Missile:
		Goto Super::Missile.BFG;
	}
}
