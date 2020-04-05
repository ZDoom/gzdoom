
// Wraith -------------------------------------------------------------------

class Wraith : Actor
{
	Default
	{
		Health 150;
		PainChance 25;
		Speed 11;
		Height 55;
		Mass 75;
		Damage 10;
		Monster;
		+NOGRAVITY +DROPOFF +FLOAT
		+FLOORCLIP +TELESTOMP
		SeeSound "WraithSight";
		AttackSound "WraithAttack";
		PainSound "WraithPain";
		DeathSound "WraithDeath";
		ActiveSound "WraithActive";
		HitObituary "$OB_WRAITHHIT";
		Obituary "$OB_WRAITH";
		Tag "$FN_WRAITH";
	}

	States
	{
	Spawn:
		WRTH A 10;
		WRTH B 5 A_WraithInit;
		Goto Look;
	Look:
		WRTH AB 15 A_Look;
		Loop;
	See:
		WRTH ABCD 4 A_WraithChase;
		Loop;
	Pain:
		WRTH A 2;
		WRTH H 6 A_Pain;
		Goto See;
	Melee:
		WRTH E 6 A_FaceTarget;
		WRTH F 6 A_WraithFX3;
		WRTH G 6 A_WraithMelee;
		Goto See;
	Missile:
		WRTH E 6 A_FaceTarget;
		WRTH F 6;
		WRTH G 6 A_SpawnProjectile("WraithFX1");
		Goto See;
	Death:
		WRTH I 4;
		WRTH J 4 A_Scream;
		WRTH KL 4;
		WRTH M 4 A_NoBlocking;
		WRTH N 4 A_QueueCorpse;
		WRTH O 4;
		WRTH PQ 5;
		WRTH R -1;
		Stop;
	XDeath:
		WRT2 A 5;
		WRT2 B 5 A_Scream;
		WRT2 CD 5;
		WRT2 E 5 A_NoBlocking;
		WRT2 F 5 A_QueueCorpse;
		WRT2 G 5;
		WRT2 H -1;
		Stop;
	Ice:
		WRT2 I 5 A_FreezeDeath;
		WRT2 I 1 A_FreezeDeathChunks;
		Wait;
	}
	
	//============================================================================
	//
	// A_WraithInit
	//
	//============================================================================

	void A_WraithInit()
	{
		AddZ(48);

		// [RH] Make sure the wraith didn't go into the ceiling
		if (pos.z + height > ceilingz)
		{
			SetZ(ceilingz - Height);
		}

		WeaveIndexZ = 0;			// index into floatbob
	}


	//============================================================================
	//
	// A_WraithChase
	//
	//============================================================================

	void A_WraithChase()
	{
		int weaveindex = WeaveIndexZ;
		AddZ(BobSin(weaveindex));
		WeaveIndexZ = (weaveindex + 2) & 63;
		A_Chase ();
		A_WraithFX4 ();
	}

	//============================================================================
	//
	// A_WraithFX3
	//
	// Spawn an FX3 around the wraith during attacks
	//
	//============================================================================

	void A_WraithFX3()
	{
		int numdropped = random[WraithFX3](0,14);

		while (numdropped-- > 0)
		{
			double xo = (random[WraithFX3]() - 128) / 32.;
			double yo = (random[WraithFX3]() - 128) / 32.;
			double zo = random[WraithFX3]() / 64.;

			Actor mo = Spawn("WraithFX3", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
			if (mo)
			{
				mo.floorz = floorz;
				mo.ceilingz = ceilingz;
				mo.target = self;
			}
		}
	}

	//============================================================================
	//
	// A_WraithFX4
	//
	// Spawn an FX4 during movement
	//
	//============================================================================

	void A_WraithFX4 ()
	{
		int chance = random[WraithFX4]();
		bool spawn4, spawn5;

		if (chance < 10)
		{
			spawn4 = true;
			spawn5 = false;
		}
		else if (chance < 20)
		{
			spawn4 = false;
			spawn5 = true;
		}
		else if (chance < 25)
		{
			spawn4 = true;
			spawn5 = true;
		}
		else
		{
			spawn4 = false;
			spawn5 = false;
		}

		if (spawn4)
		{
			double xo = (random[WraithFX4]() - 128) / 16.;
			double yo = (random[WraithFX4]() - 128) / 16.;
			double zo = (random[WraithFX4]() / 64.);

			Actor mo = Spawn ("WraithFX4", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
			if (mo)
			{
				mo.floorz = floorz;
				mo.ceilingz = ceilingz;
				mo.target = self;
			}
		}
		if (spawn5)
		{
			double xo = (random[WraithFX4]() - 128) / 32.;
			double yo = (random[WraithFX4]() - 128) / 32.;
			double zo = (random[WraithFX4]() / 64.);

			Actor mo = Spawn ("WraithFX5", Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
			if (mo)
			{
				mo.floorz = floorz;
				mo.ceilingz = ceilingz;
				mo.target = self;
			}
		}
	}

	//============================================================================
	//
	// A_WraithMelee
	//
	//============================================================================

	void A_WraithMelee()
	{
		// Steal health from target and give to self
		if (CheckMeleeRange() && (random[StealHealth]()<220))
		{
			int amount = random[StealHealth](1, 8) * 2;
			target.DamageMobj (self, self, amount, 'Melee');
			health += amount;
		}
	}
}

// Buried wraith ------------------------------------------------------------

class WraithBuried : Wraith
{
	Default
	{
		Height 68;
		-SHOOTABLE
		-SOLID
		+DONTMORPH
		+DONTBLAST
		+SPECIALFLOORCLIP
		+STAYMORPHED
		+INVISIBLE
		PainChance 0;
	}
	
	
	States
	{
	Spawn:
		Goto Super::Look;
	See:
		WRTH A 2 A_WraithRaiseInit;
		WRTH A 2 A_WraithRaise;
		WRTH A 2 A_FaceTarget;
		WRTH BB 2 A_WraithRaise;
		Goto See + 1;
	Chase:
		Goto Super::See;
	}
	
	//============================================================================
	//
	// A_WraithRaiseInit
	//
	//============================================================================

	void A_WraithRaiseInit()
	{
		bInvisible = false;
		bNonShootable = false;
		bDontBlast = false;
		bShootable = true;
		bSolid = true;
		Floorclip = Height;
	}

	//============================================================================
	//
	// A_WraithRaise
	//
	//============================================================================

	void A_WraithRaise()
	{
		if (RaiseMobj (2))
		{
			// Reached it's target height
			// [RH] Once a buried wraith is fully raised, it should be
			// morphable, right?
			bDontMorph = false;
			bSpecialFloorClip = false;
			SetStateLabel ("Chase");
			// [RH] Reset PainChance to a normal wraith's.
			PainChance = GetDefaultByType("Wraith").PainChance;
		}

		SpawnDirt (radius);
	}

	
}

// Wraith FX 1 --------------------------------------------------------------

class WraithFX1 : Actor
{
	Default
	{
		Speed 14;
		Radius 10;
		Height 6;
		Mass 5;
		Damage 5;
		DamageType "Fire";
		Projectile;
		+FLOORCLIP
		SeeSound "WraithMissileFire";
		DeathSound "WraithMissileExplode";
	}


	States
	{
	Spawn:
		WRBL A 3 Bright;
		WRBL B 3 Bright A_WraithFX2;
		WRBL C 3 Bright;
		Loop;
	Death:
		WRBL D 4 Bright;
		WRBL E 4 Bright A_WraithFX2;
		WRBL F 4 Bright;
		WRBL GH 3 Bright A_WraithFX2;
		WRBL I 3 Bright;
		Stop;
	}
	
	//============================================================================
	//
	// A_WraithFX2 - spawns sparkle tail of missile
	//
	//============================================================================

	void A_WraithFX2()
	{
		for (int i = 2; i; --i)
		{
			Actor mo = Spawn ("WraithFX2", Pos, ALLOW_REPLACE);
			if(mo)
			{
				double newangle = random[WraithFX2]() * (360 / 1024.f);
				if (random[WraithFX2]() >= 128)
				{
					newangle = -newangle;
				}
				newangle += angle;
				mo.Vel.X = ((random[WraithFX2]() / 512.) + 1) * cos(newangle);
				mo.Vel.Y = ((random[WraithFX2]() / 512.) + 1) * sin(newangle);
				mo.Vel.Z = 0;
				mo.target = self;
				mo.Floorclip = 10;
			}
		}
	}

	
}

// Wraith FX 2 --------------------------------------------------------------

class WraithFX2 : Actor
{
	Default
	{
		Radius 2;
		Height 5;
		Mass 5;
		+NOBLOCKMAP +DROPOFF
		+FLOORCLIP +NOTELEPORT
	}
	States
	{
	Spawn:
		WRBL JKLMNOP 4 Bright;
		Stop;
	}
}

// Wraith FX 3 --------------------------------------------------------------

class WraithFX3 : Actor
{
	Default
	{
		Radius 2;
		Height 5;
		Mass 5;
		+NOBLOCKMAP +DROPOFF +MISSILE
		+FLOORCLIP +NOTELEPORT
		DeathSound "Drip";
	}
	States
	{
	Spawn:
		WRBL QRS 4 Bright;
		Loop;
	Death:
		WRBL S 4 Bright;
		Stop;
	}
}

// Wraith FX 4 --------------------------------------------------------------

class WraithFX4 : Actor
{
	Default
	{
		Radius 2;
		Height 5;
		Mass 5;
		+NOBLOCKMAP +DROPOFF +MISSILE
		+NOTELEPORT
		DeathSound "Drip";
	}
	States
	{
	Spawn:
		WRBL TUVW 4;
		Loop;
	Death:
		WRBL W 10;
		Stop;
	}
}

// Wraith FX 5 --------------------------------------------------------------

class WraithFX5 : WraithFX4
{
	States
	{
	Spawn:
		WRBL XYZ 7;
		Loop;
	Death:
		WRBL Z 35;
		Stop;
	}
}
