
// Boss spot ----------------------------------------------------------------

class BossSpot : SpecialSpot
{
	Default
	{
		+INVISIBLE
	}
}

// Sorcerer (D'Sparil on his serpent) ---------------------------------------

class Sorcerer1 : Actor
{
	Default
	{
		Health 2000;
		Radius 28;
		Height 100;
		Mass 800;
		Speed 16;
		PainChance 56;
		Monster;
		+BOSS
		+DONTMORPH
		+NORADIUSDMG
		+NOTARGET
		+NOICEDEATH
		+FLOORCLIP
		+DONTGIB
		SeeSound "dsparilserpent/sight";
		AttackSound "dsparilserpent/attack";
		PainSound "dsparilserpent/pain";
		DeathSound "dsparilserpent/death";
		ActiveSound "dsparilserpent/active";
		Obituary "$OB_DSPARIL1";
		HitObituary "$OB_DSPARIL1HIT";
		Tag "$FN_DSPARIL";
	}


	States
	{
	Spawn:
		SRCR AB 10 A_Look;
		Loop;
	See:
		SRCR ABCD 5 A_Sor1Chase;
		Loop;
	Pain:
		SRCR Q 6 A_Sor1Pain;
		Goto See;
	Missile:
		SRCR Q 7 A_FaceTarget;
		SRCR R 6 A_FaceTarget;
		SRCR S 10 A_Srcr1Attack;
		Goto See;
	Missile2:
		SRCR S 10 A_FaceTarget;
		SRCR Q 7 A_FaceTarget;
		SRCR R 6 A_FaceTarget;
		SRCR S 10 A_Srcr1Attack;
		Goto See;
	Death:
		SRCR E 7;
		SRCR F 7 A_Scream;
		SRCR G 7;
		SRCR HIJK 6;
		SRCR L 25 A_PlaySound("dsparil/zap", CHAN_BODY, 1, false, ATTN_NONE);
		SRCR MN 5;
		SRCR O 4;
		SRCR L 20 A_PlaySound("dsparil/zap", CHAN_BODY, 1, false, ATTN_NONE);
		SRCR MN 5;
		SRCR O 4;
		SRCR L 12;
		SRCR P -1 A_SorcererRise;
	}
	
	
	//----------------------------------------------------------------------------
	//
	// PROC A_Sor1Pain
	//
	//----------------------------------------------------------------------------

	void A_Sor1Pain ()
	{
		special1 = 20; // Number of steps to walk fast
		A_Pain();
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Sor1Chase
	//
	//----------------------------------------------------------------------------

	void A_Sor1Chase ()
	{
		if (special1)
		{
			special1--;
			tics -= 3;
		}
		A_Chase();
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Srcr1Attack
	//
	// Sorcerer demon attack.
	//
	//----------------------------------------------------------------------------

	void A_Srcr1Attack ()
	{
		let targ = target;
		if (!targ)
		{
			return;
		}
		A_PlaySound (AttackSound, CHAN_BODY);
		if (CheckMeleeRange ())
		{
			int damage = random[Srcr1Attack](1,8) * 8;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}

		if (health > (SpawnHealth()/3)*2)
		{ // Spit one fireball
			SpawnMissileZ (pos.z + 48, targ, "SorcererFX1");
		}
		else
		{ // Spit three fireballs
			Actor mo = SpawnMissileZ (pos.z + 48, targ, "SorcererFX1");
			if (mo != null)
			{
				double ang = mo.angle;
				SpawnMissileAngleZ(pos.z + 48, "SorcererFX1", ang - 3, mo.Vel.Z);
				SpawnMissileAngleZ(pos.z + 48, "SorcererFX1", ang + 3, mo.Vel.Z);
			}
			if (health < SpawnHealth()/3)
			{ // Maybe attack again
				if (special1)
				{ // Just attacked, so don't attack again
					special1 = 0;
				}
				else
				{ // Set state to attack again
					special1 = 1;
					SetStateLabel("Missile2");
				}
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_SorcererRise
	//
	//----------------------------------------------------------------------------

	void A_SorcererRise ()
	{
		bSolid = false;
		Actor mo = Spawn("Sorcerer2", Pos, ALLOW_REPLACE);
		if (mo != null)
		{
			mo.Translation = Translation;
			mo.SetStateLabel("Rise");
			mo.angle = angle;
			mo.CopyFriendliness (self, true);
		}
	}
}


// Sorcerer FX 1 ------------------------------------------------------------

class SorcererFX1 : Actor
{
	Default
	{
		Radius 10;
		Height 10;
		Speed 20;
		FastSpeed 28;
		Damage 10;
		DamageType "Fire";
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX14 ABC 6 BRIGHT;
		Loop;
	Death:
		FX14 DEFGH 5 BRIGHT;
		Stop;
	}
}
		

// Sorcerer 2 (D'Sparil without his serpent) --------------------------------

class Sorcerer2 : Actor
{
	Default
	{
		Health 3500;
		Radius 16;
		Height 70;
		Mass 300;
		Speed 14;
		Painchance 32;
		Monster;
		+DROPOFF
		+BOSS
		+DONTMORPH
		+FULLVOLACTIVE
		+NORADIUSDMG
		+NOTARGET
		+NOICEDEATH
		+FLOORCLIP
		+BOSSDEATH
		SeeSound "dsparil/sight";
		AttackSound "dsparil/attack";
		PainSound "dsparil/pain";
		ActiveSound "dsparil/active";
		Obituary "$OB_DSPARIL2";
		HitObituary "$OB_DSPARIL2HIT";
		Tag "$FN_DSPARIL";
	}


	States
	{
	Spawn:
		SOR2 MN 10 A_Look;
		Loop;
	See:
		SOR2 MNOP 4 A_Chase;
		Loop;
	Rise:
		SOR2 AB 4;
		SOR2 C 4 A_PlaySound("dsparil/rise", CHAN_BODY, 1, false, ATTN_NONE);
		SOR2 DEF 4;
		SOR2 G 12 A_PlaySound("dsparil/sight", CHAN_BODY, 1, false, ATTN_NONE);
		Goto See;
	Pain:
		SOR2 Q 3;
		SOR2 Q 6 A_Pain;
		Goto See;
	Missile:
		SOR2 R 9 A_Srcr2Decide;
		SOR2 S 9 A_FaceTarget;
		SOR2 T 20 A_Srcr2Attack;
		Goto See;
	Teleport:
		SOR2 LKJIHG 6;
		Goto See;
	Death:
		SDTH A 8 A_Sor2DthInit;
		SDTH B 8;
		SDTH C 8 A_PlaySound("dsparil/scream", CHAN_BODY, 1, false, ATTN_NONE);
	DeathLoop:
		SDTH DE 7;
		SDTH F 7 A_Sor2DthLoop;
		SDTH G 6 A_PlaySound("dsparil/explode", CHAN_BODY, 1, false, ATTN_NONE);
		SDTH H 6;
		SDTH I 18;
		SDTH J 6 A_NoBlocking;
		SDTH K 6 A_PlaySound("dsparil/bones", CHAN_BODY, 1, false, ATTN_NONE);
		SDTH LMN 6;
		SDTH O -1 A_BossDeath;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC P_DSparilTeleport
	//
	//----------------------------------------------------------------------------

	void DSparilTeleport ()
	{
		SpotState state = SpotState.GetSpotState();
		if (state == null) return;

		Actor spot = state.GetSpotWithMinMaxDistance("BossSpot", pos.x, pos.y, 128, 0);
		if (spot == null) return;

		Vector3 prev = Pos;
		if (TeleportMove (spot.Pos, false))
		{
			Actor mo = Spawn("Sorcerer2Telefade", prev, ALLOW_REPLACE);
			if (mo) 
			{
				mo.Translation = Translation;
				mo.A_PlaySound("misc/teleport", CHAN_BODY);
			}
			SetStateLabel ("Teleport");
			A_PlaySound ("misc/teleport", CHAN_BODY);
			SetZ(floorz);
			angle = spot.angle;
			vel = (0,0,0); 
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Srcr2Decide
	//
	//----------------------------------------------------------------------------

	void A_Srcr2Decide ()
	{
		static const int chance[] =
		{
			192, 120, 120, 120, 64, 64, 32, 16, 0
		};

		int health8 = max(1, SpawnHealth() / 8);
		int chanceindex = min(8, health / health8);

		if (random[Srcr2Decide]() < chance[chanceindex])
		{
			DSparilTeleport ();
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Srcr2Attack
	//
	//----------------------------------------------------------------------------

	void A_Srcr2Attack ()
	{
		let targ = target;
		if (!targ)
		{
			return;
		}
		A_PlaySound (AttackSound, CHAN_BODY, 1, false, ATTN_NONE);
		if (CheckMeleeRange())
		{
			int damage = random[Srcr2Atk](1, 8) * 20;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}
		int chance = health < SpawnHealth()/2 ? 96 : 48;
		if (random[Srcr2Atk]() < chance)
		{ // Wizard spawners

			SpawnMissileAngle("Sorcerer2FX2", Angle - 45, 0.5);
			SpawnMissileAngle("Sorcerer2FX2", Angle + 45, 0.5);
		}
		else
		{ // Blue bolt
			SpawnMissile (targ, "Sorcerer2FX1");
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Sor2DthInit
	//
	//----------------------------------------------------------------------------

	void A_Sor2DthInit ()
	{
		special1 = 7; // Animation loop counter
		Thing_Destroy(0); // Kill monsters early
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_Sor2DthLoop
	//
	//----------------------------------------------------------------------------

	void A_Sor2DthLoop ()
	{
		if (--special1)
		{ // Need to loop
			SetStateLabel("DeathLoop");
		}
	}
}



// Sorcerer 2 FX 1 ----------------------------------------------------------

class Sorcerer2FX1 : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 20;
		FastSpeed 28;
		Damage 1;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX16 ABC 3 BRIGHT A_BlueSpark;
		Loop;
	Death:
		FX16 G 5 BRIGHT A_Explode(random[S2FX1](80,111));
		FX16 HIJKL 5 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_BlueSpark
	//
	//----------------------------------------------------------------------------

	void A_BlueSpark ()
	{
		for (int i = 0; i < 2; i++)
		{
			Actor mo = Spawn("Sorcerer2FXSpark", pos, ALLOW_REPLACE);
			if (mo != null)
			{
				mo.Vel.X = Random2[BlueSpark]() / 128.;
				mo.Vel.Y = Random2[BlueSpark]() / 128.;
				mo.Vel.Z = 1. + Random[BlueSpark]() / 256.;
			}
		}
	}
}

// Sorcerer 2 FX Spark ------------------------------------------------------

class Sorcerer2FXSpark : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
		+NOGRAVITY
		+NOTELEPORT
		+CANNOTPUSH
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX16 DEF 12 BRIGHT;
		Stop;
	}	
}

// Sorcerer 2 FX 2 ----------------------------------------------------------

class Sorcerer2FX2 : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 6;
		Damage 10;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FX11 A 35 BRIGHT;
		FX11 A 5 BRIGHT A_GenWizard;
		FX11 B 5 BRIGHT;
		Goto Spawn+1;
	Death:
		FX11 CDEFG 5 BRIGHT;
		Stop;

	}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

	void A_GenWizard ()
	{
		Actor mo = Spawn("Wizard", pos, ALLOW_REPLACE);
		if (mo != null)
		{
			mo.AddZ(-mo.Default.Height / 2, false);
			if (!mo.TestMobjLocation ())
			{ // Didn't fit
				mo.ClearCounters();
				mo.Destroy ();
			}
			else
			{ // [RH] Make the new wizards inherit D'Sparil's target
				mo.CopyFriendliness (self.target, true);

				Vel = (0,0,0);
				SetStateLabel('Death');
				bMissile = false;
				mo.master = target;
				SpawnTeleportFog(pos, false, true);
			}
		}
	}
}

// Sorcerer 2 Telefade ------------------------------------------------------

class Sorcerer2Telefade : Actor
{
	Default
	{
		+NOBLOCKMAP
	}

	States
	{
	Spawn:
		SOR2 GHIJKL 6;
		Stop;
	}
}

