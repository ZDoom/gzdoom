
// Ice Guy ------------------------------------------------------------------

class IceGuy : Actor
{
	Default
	{
		Health 120;
		PainChance 144;
		Speed 14;
		Radius 22;
		Height 75;
		Mass 150;
		DamageType "Ice";
		Monster;
		+NOBLOOD
		+TELESTOMP
		+NOICEDEATH
		SeeSound "IceGuySight";
		AttackSound "IceGuyAttack";
		ActiveSound "IceGuyActive";
		Obituary "$OB_ICEGUY";
		Tag "$FN_ICEGUY";
	}


	States
	{
	Spawn:
		ICEY A 10 A_IceGuyLook;
		Loop;
	See:
		ICEY A 4 A_Chase;
		ICEY B 4 A_IceGuyChase;
		ICEY CD 4 A_Chase;
		Loop;
	Pain:
		ICEY A 1 A_Pain;
		Goto See;
	Missile:
		ICEY EF 3 A_FaceTarget;
		ICEY G 8 Bright A_IceGuyAttack;
		ICEY F 4 A_FaceTarget;
		Goto See;
	Death:
		ICEY A 1 A_IceGuyDie;
		Stop;
	Inactive:
		ICEY A -1;
		Goto See;
	}
	
	//============================================================================
	//
	// SpawnWisp
	//
	//============================================================================

	private void SpawnWisp()
	{
		static const class<Actor> WispTypes[] = { "IceGuyWisp1", "IceGuyWisp2" };

		double dist = (random[IceGuyLook]() - 128) * radius / 128.;
		double an = angle + 90;
		Actor mo = Spawn(WispTypes[random[IceGuyLook](0, 1)], Vec3Angle(dist, an, 60.), ALLOW_REPLACE);
		if (mo)
		{
			mo.Vel = Vel;
			mo.target = self;
		}
	}
	
	//============================================================================
	//
	// A_IceGuyLook
	//
	//============================================================================

	void A_IceGuyLook()
	{
		A_Look();
		if (random[IceGuyLook]() < 64) SpawnWisp();
	}

	//============================================================================
	//
	// A_IceGuyChase
	//
	//============================================================================

	void A_IceGuyChase()
	{
		A_Chase();
		if (random[IceGuyLook]() < 128) SpawnWisp();
	}

	//============================================================================
	//
	// A_IceGuyAttack
	//
	//============================================================================

	void A_IceGuyAttack()
	{
		if(!target) 
		{
			return;
		}
		SpawnMissileXYZ(Vec3Angle(radius / 2, angle + 90, 40.), target, "IceGuyFX");
		SpawnMissileXYZ(Vec3Angle(radius / 2, angle - 90, 40.), target, "IceGuyFX");
		A_StartSound (AttackSound, CHAN_WEAPON);
	}
}

extend class Actor
{
	//============================================================================
	//
	// A_IceGuyDie (globally accessible)
	//
	//============================================================================

	void A_IceGuyDie()
	{
		Vel = (0,0,0);
		Height = Default.Height;
		A_FreezeDeathChunks();
	}

}

// Ice Guy Projectile -------------------------------------------------------

class IceGuyFX : Actor
{
	Default
	{
		Speed 14;
		Radius 8;
		Height 10;
		Damage 1;
		DamageType "Ice";
		Projectile;
		-ACTIVATEIMPACT -ACTIVATEPCROSS
		DeathSound "IceGuyMissileExplode";
	}

	States
	{
	Spawn:
		ICPR ABC 3 Bright A_SpawnItemEx("IceFXPuff", 0,0,2);
		Loop;
	Death:
		ICPR D 4 Bright;
		ICPR E 4 Bright A_IceGuyMissileExplode;
		ICPR FG 4 Bright;
		ICPR H 3 Bright;
		Stop;
	}
	

	//============================================================================
	//
	// A_IceGuyMissileExplode
	//
	//============================================================================

	void A_IceGuyMissileExplode()
	{
		for (int i = 0; i < 8; i++)
		{
			Actor mo = SpawnMissileAngleZ (pos.z+3, "IceGuyFX2", i*45., -0.3);
			if (mo)
			{
				mo.target = target;
			}
		}
	}
}

// Ice Guy Projectile's Puff ------------------------------------------------

class IceFXPuff : Actor
{
	Default
	{
		Radius 1;
		Height 1;
		+NOBLOCKMAP +NOGRAVITY +DROPOFF +SHADOW
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		ICPR IJK 3;
		ICPR LM 2;
		Stop;
	}
}

// Secondary Ice Guy Projectile (ejected by the primary projectile) ---------

class IceGuyFX2 : Actor
{
	Default
	{
		Speed 10;
		Radius 4;
		Height 4;
		Damage 1;
		DamageType "Ice";
		Gravity 0.125;
		+NOBLOCKMAP +DROPOFF +MISSILE
		+NOTELEPORT
		+STRIFEDAMAGE
	}
	States
	{
	Spawn:
		ICPR NOP 3 Bright;
		Loop;
	}
}

// Ice Guy Bit --------------------------------------------------------------

class IceGuyBit : Actor
{
	Default
	{
		Radius 1;
		Height 1;
		Gravity 0.125;
		+NOBLOCKMAP +DROPOFF
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ICPR Q 50 Bright;
		Stop;
		ICPR R 50 Bright;
		Stop;
	}
}

// Ice Guy Wisp 1 -----------------------------------------------------------

class IceGuyWisp1 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY +DROPOFF +MISSILE
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		ICWS ABCDEFGHI 2;
		Stop;
	}
}

// Ice Guy Wisp 2 -----------------------------------------------------------

class IceGuyWisp2 : IceGuyWisp1
{
	States
	{
	Spawn:
		ICWS JKLMNOPQR 2;
		Stop;
	}
}

