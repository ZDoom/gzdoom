
// Entity Nest --------------------------------------------------------------

class EntityNest : Actor
{
	Default
	{
		Radius 84;
		Height 47;
		+SOLID
		+NOTDMATCH
		+FLOORCLIP
	}
	States
	{
	Spawn:
		NEST A -1;
		Stop;
	}
}

// Entity Pod ---------------------------------------------------------------

class EntityPod : Actor
{
	Default
	{
		Radius 25;
		Height 91;
		+SOLID
		+NOTDMATCH
		SeeSound "misc/gibbed";
	}


	States
	{
	Spawn:
		PODD A 60 A_Look;
		Loop;
	See:
		PODD A 360;
		PODD B 9 A_NoBlocking;
		PODD C 9;
		PODD D 9 A_SpawnEntity;
		PODD E -1;
		Stop;
	}
	
	void A_SpawnEntity ()
	{
		Actor entity = Spawn("EntityBoss", pos + (0,0,70), ALLOW_REPLACE);
		if (entity != null)
		{
			entity.Angle = self.Angle;
			entity.CopyFriendliness(self, true);
			entity.Vel.Z = 5;
			entity.tracer = self;
		}
	}
	
}


//  --------------------------------------------------------------

class EntityBoss : SpectralMonster
{
	Default
	{
		Health 2500;
		Painchance 255;
		Speed 13;
		Radius 130;
		Height 200;
		FloatSpeed 5;
		Mass 1000;
		Monster;
		+SPECIAL
		+NOGRAVITY
		+FLOAT
		+SHADOW
		+NOTDMATCH
		+DONTMORPH
		+NOTARGET
		+NOBLOCKMONST
		+INCOMBAT
		+LOOKALLAROUND
		+SPECTRAL
		+NOICEDEATH
		MinMissileChance 150;
		RenderStyle "Translucent";
		Alpha 0.5;
		SeeSound "entity/sight";
		AttackSound "entity/melee";
		PainSound "entity/pain";
		DeathSound "entity/death";
		ActiveSound "entity/active";
		Obituary "$OB_ENTITY";
	}

	States
	{
	Spawn:
		MNAM A 100;
		MNAM B 60 Bright;
		MNAM CDEFGHIJKL 4 Bright;
		MNAL A 4 Bright A_Look;
		MNAL B 4 Bright A_SentinelBob;
		Goto Spawn+12;
	See:
		MNAL AB 4 Bright A_Chase;
		MNAL C 4 Bright A_SentinelBob;
		MNAL DEF 4 Bright A_Chase;
		MNAL G 4 Bright A_SentinelBob;
		MNAL HIJ 4 Bright A_Chase;
		MNAL K 4 Bright A_SentinelBob;
		Loop;
	Melee:
		MNAL J 4 Bright A_FaceTarget;
		MNAL I 4 Bright A_CustomMeleeAttack((random[SpectreMelee](0,255)&9)*5);
		MNAL C 4 Bright;
		Goto See+2;
	Missile:
		MNAL F 4 Bright A_FaceTarget;
		MNAL I 4 Bright A_EntityAttack;
		MNAL E 4 Bright;
		Goto See+10;
	Pain:
		MNAL J 2 Bright A_Pain;
		Goto See+6;
	Death:
		MNAL L 7 Bright A_SpectreChunkSmall;
		MNAL M 7 Bright A_Scream;
		MNAL NO 7 Bright A_SpectreChunkSmall;
		MNAL P 7 Bright A_SpectreChunkLarge;
		MNAL Q 64 Bright A_SpectreChunkSmall;
		MNAL Q 6 Bright A_EntityDeath;
		Stop;
	}
	
	//  --------------------------------------------------------------

	private void A_SpectralMissile (class<Actor> missilename)
	{
		if (target != null)
		{
			Actor missile = SpawnMissileXYZ (Pos + (0,0,32), target, missilename, false);
			if (missile != null)
			{
				missile.tracer = target;
				missile.CheckMissileSpawn(radius);
			}
		}
	}

	//  --------------------------------------------------------------

	void A_EntityAttack()
	{
		// Apparent Strife bug: Case 5 was unreachable because they used % 5 instead of % 6.
		// I've fixed that by making case 1 duplicate it, since case 1 did nothing.
		switch (random[Entity](0, 4))
		{
		case 0:
			A_SpotLightning();
			break;

		case 2:
			A_SpectralMissile ("SpectralLightningH3");
			break;

		case 3:
			A_Spectre3Attack();
			break;

		case 4:
			A_SpectralMissile ("SpectralLightningBigV2");
			break;

		default:
			A_SpectralMissile ("SpectralLightningBigBall2");
			break;
		}
	}

	//  --------------------------------------------------------------

	void A_EntityDeath()
	{
		Actor second;
		double secondRadius = GetDefaultByType("EntitySecond").radius * 2;

		static const double turns[] = { 0, 90, -90 };

		Actor spot = tracer;
		if (spot == null) spot = self;

		for (int i = 0; i < 3; i++)
		{
			double an = Angle + turns[i];
			Vector3 pos = spot.Vec3Angle(secondRadius, an, tracer ? 70. : 0.);

			second = Spawn("EntitySecond", pos, ALLOW_REPLACE);
			if (second != null)
			{
				second.CopyFriendliness(self, true);
				second.A_FaceTarget();
				second.VelFromAngle(i == 0? 4.8828125 : secondRadius * 4., an);
			}
		}
	}
	
}

// Second Entity Boss -------------------------------------------------------

class EntitySecond : SpectralMonster
{
	Default
	{
		Health 990;
		Painchance 255;
		Speed 14;
		Radius 130;
		Height 200;
		FloatSpeed 5;
		Mass 1000;
		Monster;
		+SPECIAL
		+NOGRAVITY
		+FLOAT
		+SHADOW
		+NOTDMATCH
		+DONTMORPH
		+NOBLOCKMONST
		+INCOMBAT
		+LOOKALLAROUND
		+SPECTRAL
		+NOICEDEATH
		MinMissileChance 150;
		RenderStyle "Translucent";
		Alpha 0.25;
		SeeSound "alienspectre/sight";
		AttackSound "alienspectre/blade";
		PainSound "alienspectre/pain";
		DeathSound "alienspectre/death";
		ActiveSound "alienspectre/active";
		Obituary "$OB_ENTITY";
	}

	States
	{
	Spawn:
		MNAL R 10 Bright A_Look;
		Loop;
	See:
		MNAL R 5 Bright A_SentinelBob;
		MNAL ST 5 Bright A_Chase;
		MNAL U 5 Bright A_SentinelBob;
		MNAL V 5 Bright A_Chase;
		MNAL W 5 Bright A_SentinelBob;
		Loop;
	Melee:
		MNAL S 4 Bright A_FaceTarget;
		MNAL R 4 Bright A_CustomMeleeAttack((random[SpectreMelee](0,255)&9)*5);
		MNAL T 4 Bright A_SentinelBob;
		Goto See+1;
	Missile:
		MNAL W 4 Bright A_FaceTarget;
		MNAL U 4 Bright A_SpawnProjectile("SpectralLightningH3",32,0);
		MNAL V 4 Bright A_SentinelBob;
		Goto See+4;
	Pain:
		MNAL R 2 Bright A_Pain;
		Goto See;
	Death:
		MDTH A 3 Bright A_Scream;
		MDTH B 3 Bright A_TossGib;
		MDTH C 3 Bright A_NoBlocking;
		MDTH DEFGHIJKLMN 3 Bright A_TossGib;
		MDTH O 3 Bright A_SubEntityDeath;
		Stop;
	}
	
	//  --------------------------------------------------------------

	void A_SubEntityDeath ()
	{
		if (CheckBossDeath ())
		{
			Exit_Normal(0);
		}
	}
}






