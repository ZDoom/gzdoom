
// Winged Statue (no skull) -------------------------------------------------

class ZWingedStatueNoSkull : SwitchingDecoration
{
	Default
	{
		Radius 10;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		STWN A -1;
		Stop;
	Active:
		STWN B -1;
		Stop;
	}
}


// Gem pedestal -------------------------------------------------------------

class ZGemPedestal : SwitchingDecoration
{
	Default
	{
		Radius 10;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		GMPD A -1;
		Stop;
	Active:
		GMPD B -1;
		Stop;
	}
}


// Tree (destructible) ------------------------------------------------------

class TreeDestructible : Actor
{
	Default
	{
		Health 70;
		Radius 15;
		Height 180;
		DeathHeight 24;
		Mass 0x7fffffff;
		PainSound "TreeExplode";
		DeathSound "TreeBreak";
		+SOLID +SHOOTABLE +NOBLOOD +NOICEDEATH
	}
	States
	{
	Spawn:
		TRDT A -1;
		Stop;
	Death:
		TRDT B 5;
		TRDT C 5 A_Scream;
		TRDT DEF 5;
		TRDT G -1;
		Stop;
	Burn:
		TRDT H 5 Bright A_Pain;
		TRDT IJKL 5 Bright;
		TRDT M 5 Bright A_Explode(10, 128);
		TRDT N 5 Bright;
		TRDT OP 5;
		TRDT Q -1;
		Stop;
	} 
}


// Pottery1 ------------------------------------------------------------------

class Pottery1 : Actor
{
	Default
	{
		Health 15;
		Speed 10;
		Height 32;
		+SOLID +SHOOTABLE +NOBLOOD +DROPOFF +SMASHABLE
		+SLIDESONWALLS +PUSHABLE +TELESTOMP +CANPASS
		+NOICEDEATH
	}


	States
	{
	Spawn:
		POT1 A -1;
		Loop;
	Death:
		POT1 A 0 A_PotteryExplode;
		Stop;
	}
	
	//============================================================================
	//
	// A_PotteryExplode
	//
	//============================================================================

	void A_PotteryExplode()
	{
		Actor mo = null;
		int i;

		for(i = random[Pottery](3, 6); i; i--)
		{
			mo = Spawn ("PotteryBit", Pos, ALLOW_REPLACE);
			if (mo)
			{
				mo.SetState (mo.SpawnState + random[Pottery](0, 4));
				mo.Vel.X = random2[Pottery]() / 64.;
				mo.Vel.Y = random2[Pottery]() / 64.;
				mo.Vel.Z = random[Pottery](5, 12) * 0.75;
			}
		}
		mo.A_StartSound ("PotteryExplode", CHAN_BODY);
		// Spawn an item?
		Class<Actor> type = GetSpawnableType(args[0]);
		if (type != null)
		{
			if (!(level.nomonsters || sv_nomonsters) || !(GetDefaultByType (type).bIsMonster))
			{ // Only spawn monsters if not -nomonsters
				Spawn (type, Pos, ALLOW_REPLACE);
			}
		}
	}
}

// Pottery2 -----------------------------------------------------------------

class Pottery2 : Pottery1
{
	Default
	{
		Height 25;
	}
	States
	{
	Spawn:
		POT2 A -1;
		Stop;
	}
}

// Pottery3 -----------------------------------------------------------------

class Pottery3 : Pottery1
{
	Default
	{
		Height 25;
	}
	States
	{
	Spawn:
		POT3 A -1;
		Stop;
	}
}

// Pottery Bit --------------------------------------------------------------

class PotteryBit : Actor
{
	State LoopState;
	
	Default
	{
		Radius 5;
		Height 5;
		+MISSILE
		+NOTELEPORT
		+NOICEDEATH
	}

	States
	{
	Spawn:
		PBIT ABCDE -1;
		Stop;
	Death:
		PBIT F 0 A_PotteryChooseBit;
		Stop;
	Pottery1:
		PBIT F 140;
		PBIT F 1 A_PotteryCheck;
		Stop;
	Pottery2:
		PBIT G 140;
		PBIT G 1 A_PotteryCheck;
		Stop;
	Pottery3:
		PBIT H 140;
		PBIT H 1 A_PotteryCheck;
		Stop;
	Pottery4:
		PBIT I 140;
		PBIT I 1 A_PotteryCheck;
		Stop;
	Pottery5:
		PBIT J 140;
		PBIT J 1 A_PotteryCheck;
		Stop;
	}
	
	//============================================================================
	//
	// A_PotteryChooseBit
	//
	//============================================================================

	void A_PotteryChooseBit()
	{
		static const statelabel bits[] = { "Pottery1", "Pottery2", "Pottery3", "Pottery4", "Pottery5" };
		LoopState = FindState(bits[random[PotteryBit](0, 4)]);	// Save the state for jumping back to.
		SetState (LoopState);
		tics = 256 + (random[PotteryBit]() << 1);
	}

	//============================================================================
	//
	// A_PotteryCheck
	//
	//============================================================================

	void A_PotteryCheck()
	{
		for(int i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				Actor pmo = players[i].mo;
				if (CheckSight (pmo) && (absangle(pmo.AngleTo(self), pmo.Angle) <= 45))
				{ // jump back to previpusly set state.
					SetState (LoopState);
					return;
				}
			}
		}
	}
}


// Blood pool ---------------------------------------------------------------

class BloodPool : Actor
{
	States
	{
	Spawn:
		BDPL A -1;
		Stop;
	}
}


// Lynched corpse (no heart) ------------------------------------------------

class ZCorpseLynchedNoHeart : Actor
{
	Default
	{
		Radius 10;
		Height 100;
		+SOLID +SPAWNCEILING +NOGRAVITY
	}

	States
	{
	Spawn:
		CPS5 A 140 A_CorpseBloodDrip;
		Loop;
	}
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		Spawn ("BloodPool", (pos.xy, floorz), ALLOW_REPLACE);
	}

	//============================================================================
	//
	// A_CorpseBloodDrip
	//
	//============================================================================

	void A_CorpseBloodDrip()
	{
		if (random[CorpseDrip]() <= 128)
		{
			Spawn ("CorpseBloodDrip", pos + (0, 0, Height / 2), ALLOW_REPLACE);
		}
	}
}


// CorpseBloodDrip ----------------------------------------------------------

class CorpseBloodDrip : Actor
{
	Default
	{
		Radius 1;
		Height 4;
		Gravity 0.125;
		+MISSILE
		+NOICEDEATH
		DeathSound "Drip";
	}
	States
	{
	Spawn:
		BDRP A -1;
		Stop;
	Death:
		BDSH AB 3;
		BDSH CD 2;
		Stop;
	}
}


// Corpse bit ---------------------------------------------------------------

class CorpseBit : Actor
{
	Default
	{
		Radius 5;
		Height 5;
		+NOBLOCKMAP
		+TELESTOMP
	}
	States
	{
	Spawn:
		CPB1 A -1;
		Stop;
		CPB2 A -1;
		Stop;
		CPB3 A -1;
		Stop;
		CPB4 A -1;
		Stop;
	}
}


// Corpse (sitting, splatterable) -------------------------------------------

class ZCorpseSitting : Actor
{
	Default
	{
		Health 30;
		Radius 15;
		Height 35;
		+SOLID +SHOOTABLE +NOBLOOD
		+NOICEDEATH
		DeathSound "FireDemonDeath";
	}

	States
	{
	Spawn:
		CPS6 A -1;
		Stop;
	Death:
		CPS6 A 1 A_CorpseExplode;
		Stop;
	}
	
	//============================================================================
	//
	// A_CorpseExplode
	//
	//============================================================================

	void A_CorpseExplode()
	{
		Actor mo;

		for (int i = random[CorpseExplode](3, 6); i; i--)
		{
			mo = Spawn ("CorpseBit", Pos, ALLOW_REPLACE);
			if (mo)
			{
				mo.SetState (mo.SpawnState + random[CorpseExplode](0, 2));
				mo.Vel.X = random2[CorpseExplode]() / 64.;
				mo.Vel.Y = random2[CorpseExplode]() / 64.;
				mo.Vel.Z = random[CorpseExplode](5, 12) * 0.75;
			}
		}
		// Spawn a skull
		mo = Spawn ("CorpseBit", Pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.SetState (mo.SpawnState + 3);
			mo.Vel.X = random2[CorpseExplode]() / 64.;
			mo.Vel.Y = random2[CorpseExplode]() / 64.;
			mo.Vel.Z = random[CorpseExplode](5, 12) * 0.75;
		}
		A_StartSound (DeathSound, CHAN_BODY);
		Destroy ();
	}
}


// Leaf Spawner -------------------------------------------------------------

class LeafSpawner : Actor
{
	Default
	{
		+NOBLOCKMAP +NOSECTOR
		+INVISIBLE
	}


	States
	{
	Spawn:
		TNT1 A 20 A_LeafSpawn;
		Loop;
	}
	
	//============================================================================
	//
	// A_LeafSpawn
	//
	//============================================================================

	void A_LeafSpawn()
	{
		static const class<Actor> leaves[] = { "Leaf1", "Leaf2" };

		for (int i = random[LeafSpawn](1, 4); i; i--)
		{
			double xo = random2[LeafSpawn]() / 4.;
			double yo = random2[LeafSpawn]() / 4.;
			double zo = random[LeafSpawn]() / 4.;
			Actor mo = Spawn (leaves[random[LeafSpawn](0, 1)], Vec3Offset(xo, yo, zo), ALLOW_REPLACE);

			if (mo)
			{
				mo.Thrust(random[LeafSpawn]() / 128. + 3, angle);
				mo.target = self;
				mo.special1 = 0;
			}
		}
	}
}


// Leaf 1 -------------------------------------------------------------------

class Leaf1 : Actor
{
	Default
	{
		Radius 2;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP +MISSILE
		+NOTELEPORT +DONTSPLASH
		+NOICEDEATH
	}

	States
	{
	Spawn:
		LEF1 ABC 4;
		LEF1 D 4 A_LeafThrust;
		LEF1 EFG 4;
	Looping:
		LEF1 H 4 A_LeafThrust;
		LEF1 I 4;
		LEF1 AB 4;
		LEF1 C 4 A_LeafThrust;
		LEF1 DEF 4;
		LEF1 G 4 A_LeafThrust;
		LEF1 HI 4;
		Stop;
	Death:
		LEF3 D 10 A_LeafCheck;
		Wait;
	}
	
	//============================================================================
	//
	// A_LeafThrust
	//
	//============================================================================

	void A_LeafThrust()
	{
		if (random[LeafThrust]() <= 96)
		{
			Vel.Z += random[LeafThrust]() / 128. + 1;
		}
	}

	//============================================================================
	//
	// A_LeafCheck
	//
	//============================================================================

	void A_LeafCheck()
	{
		special1++;
		if (special1 >= 20)
		{
			Destroy();
			return;
		}
		double ang = target ? target.angle : angle;
		if (random[LeafCheck]() > 64)
		{
			if (Vel.X == 0 && Vel.Y == 0)
			{
				Thrust(random[LeafCheck]() / 128. + 1, ang);
			}
			return;
		}
		SetStateLabel ("Looping");
		Vel.Z = random[LeafCheck]() / 128. + 1;
		Thrust(random[LeafCheck]() / 128. + 2, ang);
		bMissile = true;
	}
}


// Leaf 2 -------------------------------------------------------------------

class Leaf2 : Leaf1
{
	States
	{
	Spawn:
		LEF2 ABC 4;
		LEF2 D 4 A_LeafThrust;
		LEF2 EFG 4;
		LEF2 H 4 A_LeafThrust;
		LEF2 I 4;
		LEF2 AB 4;
		LEF2 C 4 A_LeafThrust;
		LEF2 DEF 4;
		LEF2 G 4 A_LeafThrust;
		LEF2 HI 4;
		Stop;
	}
}


// Twined torch -------------------------------------------------------------

class ZTwinedTorch : SwitchableDecoration
{
	Default
	{
		Radius 10;
		Height 64;
		+SOLID
	}
	States
	{
	Active:
		TWTR A 0 Bright A_StartSound("Ignite");
	Spawn:
		TWTR ABCDEFGH 4 Bright;
		Loop;
	Inactive:
		TWTR I -1;
		Stop;
	}
}

class ZTwinedTorchUnlit : ZTwinedTorch
{
	States
	{
	Spawn:
		Goto Super::Inactive;
	}
}


// Wall torch ---------------------------------------------------------------

class ZWallTorch : SwitchableDecoration
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+FIXMAPTHINGPOS
		Radius 6.5;
	}
	States
	{
	Active:
		WLTR A 0 Bright A_StartSound("Ignite");
	Spawn:
		WLTR ABCDEFGH 5 Bright;
		Loop;
	Inactive:
		WLTR I -1;
		Stop;
	}
}

class ZWallTorchUnlit : ZWallTorch
{
	States
	{
	Spawn:
		Goto Super::Inactive;
	}
}


// Shrub1 -------------------------------------------------------------------

class ZShrub1 : Actor
{
	Default
	{
		Radius 8;
		Height 24;
		Health 20;
		Mass 0x7fffffff;
		+SOLID +SHOOTABLE +NOBLOOD +NOICEDEATH
		DeathSound "TreeExplode";
	}
	States
	{
	Spawn:
		SHB1 A -1;
		Stop;
	Burn:
		SHB1 B 7 Bright;
		SHB1 C 6 Bright A_Scream;
		SHB1 D 5 Bright;
		Stop;
	}
}


// Shrub2 -------------------------------------------------------------------

class ZShrub2 : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		Health 20;
		Mass 0x7fffffff;
		+SOLID +SHOOTABLE +NOBLOOD +NOICEDEATH
		DeathSound "TreeExplode";
	}
	States
	{
	Spawn:
		SHB2 A -1;
		Stop;
	Burn:
		SHB2 B 7 Bright;
		SHB2 C 6 Bright A_Scream;
		SHB2 D 5 Bright A_Explode(30, 64);
		SHB2 E 5 Bright;
		Stop;
	}
}


// Fire Bull ----------------------------------------------------------------

class ZFireBull : SwitchableDecoration
{
	Default
	{
		Radius 20;
		Height 80;
		+SOLID
	}
	States
	{
	Active:
		FBUL I 4 Bright A_StartSound("Ignite");
		FBUL J 4 Bright;
	Spawn:
		FBUL ABCDEFG 4 Bright;
		Loop;
	Inactive:
		FBUL JI 4 Bright;
		FBUL H -1;
		Stop;
	}
}

class ZFireBullUnlit : ZFireBull
{
	States
	{
	Spawn:
		Goto Super::Inactive+2;
	}
}


// Suit of armor ------------------------------------------------------------

class ZSuitOfArmor : Actor
{
	Default
	{
		Health 60;
		Radius 16;
		Height 72;
		Mass 0x7fffffff;
		+SOLID +SHOOTABLE +NOBLOOD
		+NOICEDEATH
		DeathSound "SuitofArmorBreak";
	}

	States
	{
	Spawn:
		ZSUI A -1;
		Stop;
	Death:
		ZSUI A 1 A_SoAExplode;
		Stop;
	}
	
	//===========================================================================
	//
	// A_SoAExplode - Suit of Armor Explode
	//
	//===========================================================================

	void A_SoAExplode()
	{
		for (int i = 0; i < 10; i++)
		{
			double xo = (random[SoAExplode]() - 128) / 16.;
			double yo = (random[SoAExplode]() - 128) / 16.;
			double zo = random[SoAExplode]() * Height / 256.;
			Actor mo = Spawn ("ZArmorChunk", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
			if (mo)
			{
				mo.SetState (mo.SpawnState + i);
				mo.Vel.X = random2[SoAExplode]() / 64.;
				mo.Vel.Y = random2[SoAExplode]() / 64.;
				mo.Vel.Z = random[SoAExplode](5, 12);
			}
		}
		// Spawn an item?
		Class<Actor> type = GetSpawnableType(args[0]);
		if (type != null)
		{
			if (!(level.nomonsters || sv_nomonsters) || !(GetDefaultByType (type).bIsMonster))
			{ // Only spawn monsters if not -nomonsters
				Spawn (type, Pos, ALLOW_REPLACE);
			}
		}
		A_StartSound (DeathSound, CHAN_BODY);
		Destroy ();
	}
}


// Armor chunk --------------------------------------------------------------

class ZArmorChunk : Actor
{
	Default
	{
		Radius 4;
		Height 8;
	}
	States
	{
	Spawn:
		ZSUI B -1;
		Stop;
		ZSUI C -1;
		Stop;
		ZSUI D -1;
		Stop;
		ZSUI E -1;
		Stop;
		ZSUI F -1;
		Stop;
		ZSUI G -1;
		Stop;
		ZSUI H -1;
		Stop;
		ZSUI I -1;
		Stop;
		ZSUI J -1;
		Stop;
		ZSUI K -1;
		Stop;
	}
}


// Bell ---------------------------------------------------------------------

class ZBell : Actor
{
	Default
	{
		Health 5;
		Radius 56;
		Height 120;
		Mass 0x7fffffff;
		+SOLID +SHOOTABLE +NOBLOOD +NOGRAVITY +SPAWNCEILING
		+NOICEDEATH
		DeathSound "BellRing";
	}

	States
	{
	Spawn:
		BBLL F -1;
		Stop;
	Death:
		BBLL A 4 A_BellReset1;
		BBLL BC 4;
		BBLL D 5 A_Scream;
		BBLL CB 4;
		BBLL A 3;
		BBLL E 4;
		BBLL F 5;
		BBLL G 6 A_Scream;
		BBLL F 5;
		BBLL EA 4;
		BBLL BC 5;
		BBLL D 6 A_Scream;
		BBLL CB 5;
		BBLL A 4;
		BBLL EF 5;
		BBLL G 7 A_Scream;
		BBLL FEA 5;
		BBLL B 6;
		BBLL C 6;
		BBLL D 7 A_Scream;
		BBLL CB 6;
		BBLL A 5;
		BBLL EF 6;
		BBLL G 7 A_Scream;
		BBLL FEABC 6;
		BBLL B 7;
		BBLL A 8;
		BBLL E 12;
		BBLL A 10;
		BBLL B 12;
		BBLL A 12;
		BBLL E 14;
		BBLL A 1 A_BellReset2;
		Goto Spawn;
	}
	
	override void Activate (Actor activator)
	{
		if (health > 0)
		{
			DamageMobj (activator, activator, 10, 'Melee', DMG_THRUSTLESS); // 'ring' the bell
		}
	}

	//===========================================================================
	//
	// A_BellReset1
	//
	//===========================================================================

	void A_BellReset1()
	{
		bNoGravity = true;
		Height = Default.Height;
	}

	//===========================================================================
	//
	// A_BellReset2
	//
	//===========================================================================

	void A_BellReset2()
	{
		bShootable = true;
		bCorpse = false;
		bKilled = false;
		health = 5;
	}
}


// "Christmas" Tree ---------------------------------------------------------

class ZXmasTree : Actor
{
	Default
	{
		Radius 11;
		Height 130;
		Health 20;
		Mass 0x7fffffff;
		+SOLID +SHOOTABLE +NOBLOOD +NOICEDEATH
		DeathSound "TreeExplode";
	}
	States
	{
	Spawn:
		XMAS A -1;
		Stop;
	Burn:
		XMAS B 6 Bright;
		XMAS C 6 Bright A_Scream;
		XMAS D 5 Bright;
		XMAS E 5 Bright A_Explode(30, 64);
		XMAS F 5 Bright;
		XMAS G 4 Bright;
		XMAS H 5;
		XMAS I 4 A_NoBlocking;
		XMAS J 4;
		XMAS K -1;
		Stop;
	}
}

// Cauldron -----------------------------------------------------------------

class ZCauldron : SwitchableDecoration
{
	Default
	{
		Radius 12;
		Height 26;
		+SOLID
	}
	States
	{
	Active:
		CDRN B 0 Bright A_StartSound("Ignite");
	Spawn:
		CDRN BCDEFGH 4 Bright;
		Loop;
	Inactive:
		CDRN A -1;
		Stop;
	}
}

class ZCauldronUnlit : ZCauldron
{
	States
	{
	Spawn:
		Goto Super::Inactive;
	}
}


// Water Drip ---------------------------------------------------------------

class HWaterDrip : Actor
{
	Default
	{
		+MISSILE
		+NOTELEPORT
		Gravity 0.125;
		Mass 1;
		DeathSound "Drip";
	}
	States
	{
	Spawn:
		HWAT A -1;
		Stop;
	}
}

