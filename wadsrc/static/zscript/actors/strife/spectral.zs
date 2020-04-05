

// base for all spectral monsters which hurt when being touched--------------

class SpectralMonster : Actor
{
	Default
	{
		Monster;
		+SPECIAL
		+SPECTRAL
		+NOICEDEATH
	}
	
	override void Touch (Actor toucher)
	{
		toucher.DamageMobj (self, self, 5, 'Melee');
	}
	

	//============================================================================

	void A_SpectreChunkSmall ()
	{
		Actor foo = Spawn("AlienChunkSmall", pos + (0, 0, 10), ALLOW_REPLACE);

		if (foo != null)
		{
			int t;

			t = random[SpectreChunk](0, 7);
			foo.Vel.X = t - random[SpectreChunk](0, 15);

			t = random[SpectreChunk](0, 7);
			foo.Vel.Y = t - random[SpectreChunk](0, 15);

			foo.Vel.Z = random[SpectreChunk](0, 15);
		}
	}

	void A_SpectreChunkLarge ()
	{
		Actor foo = Spawn("AlienChunkLarge", pos + (0, 0, 10), ALLOW_REPLACE);

		if (foo != null)
		{
			int t;

			t = random[SpectreChunk](0, 7);
			foo.Vel.X = t - random[SpectreChunk](0, 15);
			
			t = random[SpectreChunk](0, 7);
			foo.Vel.Y = t - random[SpectreChunk](0, 15);

			foo.Vel.Z = random[SpectreChunk](0, 7);
		}
	}

	void A_Spectre3Attack ()
	{
		if (target == null)
			return;

		Actor foo = Spawn("SpectralLightningV2", Pos + (0, 0, 32), ALLOW_REPLACE);
		if (foo != null)
		{
			foo.Vel.Z = -12;
			foo.target = self;
			foo.FriendPlayer = 0;
			foo.tracer = target;
		}

		Angle -= 90.;
		for (int i = 0; i < 20; ++i)
		{
			Angle += 9.;
			SpawnSubMissile ("SpectralLightningBall2", self);
		}
		Angle -= 90.;
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


// Container for all spectral lightning deaths ------------------------------

class SpectralLightningBase : Actor
{
	Default
	{
		+NOTELEPORT
		+ACTIVATEIMPACT
		+ACTIVATEPCROSS
		+STRIFEDAMAGE
		+ZDOOMTRANS
		MaxStepHeight 4;
		RenderStyle "Add";
		SeeSound "weapons/sigil";
		DeathSound "weapons/sigilhit";
	}
	States
	{
	Death:
		ZAP1 B 3 A_Explode(32,32);
		ZAP1 A 3 A_AlertMonsters;
		ZAP1 BCDEFE 3;
		ZAP1 DCB 2;
		ZAP1 A 1;
		Stop;
	}
}

// Spectral Lightning death that does not explode ---------------------------

class SpectralLightningDeath1 : SpectralLightningBase
{
	States
	{
	Death:
		Goto Super::Death+1;
	}
}

// Spectral Lightning death that does not alert monsters --------------------

class SpectralLightningDeath2 : SpectralLightningBase
{
	States
	{
	Death:
		Goto Super::Death+2;
	}
}

// Spectral Lightning death that is shorter than the rest -------------------

class SpectralLightningDeathShort : SpectralLightningBase
{
	States
	{
	Death:
		Goto Super::Death+6;
	}
}

// Spectral Lightning (Ball Shaped #1) --------------------------------------

class SpectralLightningBall1 : SpectralLightningBase
{
	Default
	{
		Speed 30;
		Radius 8;
		Height 16;
		Damage 70;
		Projectile;
		+SPECTRAL
	}
	States
	{
	Spawn:
		ZOT3 ABCDE 4 Bright;
		Loop;
	}
}

// Spectral Lightning (Ball Shaped #2) --------------------------------------

class SpectralLightningBall2 : SpectralLightningBall1
{
	Default
	{
		Damage 20;
	}
}

// Spectral Lightning (Horizontal #1) ---------------------------------------

class SpectralLightningH1 : SpectralLightningBase
{
	Default
	{
		Speed 30;
		Radius 8;
		Height 16;
		Damage 70;
		Projectile;
		+SPECTRAL
	}


	States
	{
	Spawn:
		ZAP6 A 4 Bright;
		ZAP6 BC 4 Bright A_SpectralLightningTail;
		Loop;
	}
	
	void A_SpectralLightningTail ()
	{
		Actor foo = Spawn("SpectralLightningHTail", Vec3Offset(-Vel.X, -Vel.Y, 0.), ALLOW_REPLACE);
		if (foo != null)
		{
			foo.Angle = Angle;
			foo.FriendPlayer = FriendPlayer;
		}
	}
}


// Spectral Lightning (Horizontal #2) -------------------------------------

class SpectralLightningH2 : SpectralLightningH1
{
	Default
	{
		Damage 20;
	}
}

// Spectral Lightning (Horizontal #3) -------------------------------------

class SpectralLightningH3 : SpectralLightningH1
{
	Default
	{
		Damage 10;
	}
}

// ASpectralLightningHTail --------------------------------------------------

class SpectralLightningHTail : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DROPOFF
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		ZAP6 ABC 5 Bright;
		Stop;
	}
}	

// Spectral Lightning (Big Ball #1) -----------------------------------------

class SpectralLightningBigBall1 : SpectralLightningDeath2
{
	Default
	{
		Speed 18;
		Radius 20;
		Height 40;
		Damage 130;
		Projectile;
		+SPECTRAL
	}

	States
	{
	Spawn:
		ZAP7 AB 4 Bright A_SpectralBigBallLightning;
		ZAP7 CDE 6 Bright A_SpectralBigBallLightning;
		Loop;
	}
	
	void A_SpectralBigBallLightning ()
	{
		Class<Actor> cls = "SpectralLightningH3";
		if (cls)
		{
			angle += 90.;
			SpawnSubMissile (cls, target);
			angle += 180.;
			SpawnSubMissile (cls, target);
			angle -= 270.;
			SpawnSubMissile (cls, target);
		}
	}
	
}


// Spectral Lightning (Big Ball #2 - less damaging) -------------------------

class SpectralLightningBigBall2 : SpectralLightningBigBall1
{
	Default
	{
		Damage 30;
	}
}

// Sigil Lightning (Vertical #1) --------------------------------------------

class SpectralLightningV1 : SpectralLightningDeathShort
{
	Default
	{
		Speed 22;
		Radius 8;
		Height 24;
		Damage 100;
		Projectile;
		DamageType "SpectralLow";
		+SPECTRAL
	}
	States
	{
	Spawn:
		ZOT1 AB 4 Bright;
		ZOT1 CDE 6 Bright;
		Loop;
	}
}

// Sigil Lightning (Vertical #2 - less damaging) ----------------------------

class SpectralLightningV2 : SpectralLightningV1
{
	Default
	{
		Damage 50;
	}
}

// Sigil Lightning Spot (roams around dropping lightning from above) --------

class SpectralLightningSpot : SpectralLightningDeath1
{
	Default
	{
		Speed 18;
		ReactionTime 70;
		+NOBLOCKMAP
		+NOBLOCKMONST
		+NODROPOFF
		RenderStyle "Translucent";
		Alpha 0.6;
	}

	States
	{
	Spawn:
		ZAP5 A 4 Bright A_Countdown;
		ZAP5 B 4 Bright A_SpectralLightning;
		ZAP5 CD 4 Bright A_Countdown;
		Loop;
	}
	
	void A_SpectralLightning ()
	{
		if (threshold != 0)
			--threshold;

		Vel.X += random2[Zap5](3);
		Vel.Y += random2[Zap5](3);

		double xo = random2[Zap5](3) * 50.;
		double yo = random2[Zap5](3) * 50.;
		
		class<Actor> cls;
		if (threshold > 25) cls = "SpectralLightningV2";
		else cls = "SpectralLightningV1";

		Actor flash = Spawn (cls, Vec2OffsetZ(xo, yo, ONCEILINGZ), ALLOW_REPLACE);

		if (flash != null)
		{
			flash.target = target;
			flash.Vel.Z = -18;
			flash.FriendPlayer = FriendPlayer;
		}

		flash = Spawn("SpectralLightningV2", (pos.xy, ONCEILINGZ), ALLOW_REPLACE);

		if (flash != null)
		{
			flash.target = target;
			flash.Vel.Z = -18;
			flash.FriendPlayer = FriendPlayer;
		}
	}
	
}

// Sigil Lightning (Big Vertical #1) ----------------------------------------

class SpectralLightningBigV1 : SpectralLightningDeath1
{
	Default
	{
		Speed 28;
		Radius 8;
		Height 16;
		Damage 120;
		Projectile;
		+SPECTRAL
	}
	States
	{
	Spawn:
		ZOT2 ABCDE 4 Bright A_Tracer2;
		Loop;
	}
}

// Actor 90 -----------------------------------------------------------------

class SpectralLightningBigV2 : SpectralLightningBigV1
{
	Default
	{
		Damage 60;
	}
}
