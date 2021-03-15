
// Programmer ---------------------------------------------------------------

class Programmer : Actor
{
	Default
	{
		Health 1100;
		PainChance 50;
		Speed 26;
		FloatSpeed 5;
		Radius 45;
		Height  60;
		Mass 800;
		Damage 4;
		Monster;
		+NOGRAVITY
		+FLOAT
		+NOBLOOD
		+NOTDMATCH
		+DONTMORPH
		+NOBLOCKMONST
		+LOOKALLAROUND
		+NOICEDEATH
		+NOTARGETSWITCH
		DamageFactor "Fire", 0.5;
		MinMissileChance 150;
		Tag "$TAG_PROGRAMMER";
		AttackSound "programmer/attack";
		PainSound "programmer/pain";
		DeathSound "programmer/death";
		ActiveSound "programmer/active";
		Obituary "$OB_PROGRAMMER";
		DropItem "Sigil1";
	}
	
	States
	{
	Spawn:
		PRGR A 5 A_Look;
		PRGR A 1 A_SentinelBob;
		Loop;
	See:
		PRGR A 160 A_SentinelBob;
		PRGR BCD 5 A_SentinelBob;
		PRGR EF 2 A_SentinelBob;
		PRGR EF 3 A_Chase;
		Goto See+4;
	Melee:
		PRGR E 2 A_SentinelBob;
		PRGR F 3 A_SentinelBob;
		PRGR E 3 A_FaceTarget;
		PRGR F 4 A_ProgrammerMelee;
		Goto See+4;
	Missile:
		PRGR G 5 A_FaceTarget;
		PRGR H 5 A_SentinelBob;
		PRGR I 5 Bright A_FaceTarget;
		PRGR J 5 Bright A_SpotLightning;
		Goto See+4;
	Pain:
		PRGR K 5 A_Pain;
		PRGR L 5 A_SentinelBob;
		Goto See+4;
	Death:
		PRGR L 7 Bright A_TossGib;
		PRGR M 7 Bright A_Scream;
		PRGR N 7 Bright A_TossGib;
		PRGR O 7 Bright A_NoBlocking;
		PRGR P 7 Bright A_TossGib;
		PRGR Q 7 Bright A_SpawnProgrammerBase;
		PRGR R 7 Bright;
		PRGR S 6 Bright;
		PRGR TUVW 5 Bright;
		PRGR X 32 Bright;
		PRGR X -1 Bright A_ProgrammerDeath;
		Stop;
	}
	
	//============================================================================
	//
	// A_ProgrammerMelee
	//
	//============================================================================

	void A_ProgrammerMelee ()
	{
		if (target == null)
			return;

		A_FaceTarget ();

		if (!CheckMeleeRange ())
			return;

		A_StartSound("programmer/clank", CHAN_WEAPON);

		int damage = random[Programmer](1, 10) * 6;
		int newdam = target.DamageMobj (self, self, damage, 'Melee');
		target.TraceBleed (newdam > 0 ? newdam : damage, self);
	}

	//============================================================================
	//
	// A_SpawnProgrammerBase
	//
	//============================================================================

	void A_SpawnProgrammerBase ()
	{
		Actor foo = Spawn("ProgrammerBase", Pos + (0,0,24), ALLOW_REPLACE);
		if (foo != null)
		{
			foo.Angle = Angle + 180. + Random2[Programmer]() * (360. / 1024.);
			foo.VelFromAngle();
			foo.Vel.Z = random[Programmer]() / 128.;
		}
	}

	//============================================================================
	//
	// A_ProgrammerDeath
	//
	//============================================================================

	void A_ProgrammerDeath ()
	{
		if (!CheckBossDeath ())
			return;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].health > 0)
			{
				players[i].mo.GiveInventoryType ("ProgLevelEnder");
				break;
			}
		}
		// the sky change scripts are now done as special actions in MAPINFO
		A_BossDeath();
	}
	
	//============================================================================
	//
	// A_SpotLightning
	//
	//============================================================================

	void A_SpotLightning()
	{
		if (target == null) return;

		Actor spot = Spawn("SpectralLightningSpot", (target.pos.xy, target.floorz), ALLOW_REPLACE);
		if (spot != null)
		{
			spot.threshold = 25;
			spot.target = self;
			spot.FriendPlayer = 0;
			spot.tracer = target;
		}
	}
	
}


// The Programmer's base for when he dies -----------------------------------

class ProgrammerBase : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOCLIP
		+NOBLOOD
	}
	States
	{
	Spawn:
		BASE A 5 Bright A_Explode(32,32,1,1);
		BASE BCD 5 Bright;
		BASE EFG 5;
		BASE H -1;
		Stop;
	}
}


// The Programmer level ending thing ----------------------------------------

class ProgLevelEnder : Inventory
{
	Default
	{
		+INVENTORY.UNDROPPABLE
	}
	

	//============================================================================
	//
	// AProgLevelEnder :: Tick
	//
	// Fade to black, end the level, then unfade.
	//
	//============================================================================

	override void Tick ()
	{
		if (special2 == 0)
		{ // fade out over .66 second
			special1 += 255 / (TICRATE*2/3);
			if (++special1 >= 255)
			{
				special1 = 255;
				special2 = 1;
				Level.ExitLevel(0, false);
			}
		}
		else
		{ // fade in over two seconds
			special1 -= 255 / (TICRATE*2);
			if (special1 <= 0)
			{
				Destroy ();
			}
		}
	}

	//============================================================================
	//
	// AProgLevelEnder :: GetBlend
	//
	//============================================================================

	override Color GetBlend ()
	{
		return Color(special1, 0, 0, 0);
	}
}
