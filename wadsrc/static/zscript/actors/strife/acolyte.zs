
// Base class for the acolytes ----------------------------------------------

class Acolyte : StrifeHumanoid
{
	Default
	{
		Health 70;
		PainChance 150;
		Speed 7;
		Radius 24;
		Height 64;
		Mass 400;
		Monster;
		+SEESDAGGERS
		+NOSPLASHALERT
		+FLOORCLIP
		+NEVERRESPAWN
		MinMissileChance 150;
		Tag "$TAG_ACOLYTE";
		SeeSound "acolyte/sight";
		PainSound "acolyte/pain";
		AttackSound "acolyte/rifle";
		DeathSound "acolyte/death";
		ActiveSound "acolyte/active";
		Obituary "$OB_ACOLYTE";
	}

	States
	{
	Spawn:
		AGRD A 5 A_Look2;
		Wait;
		AGRD B 8 A_ClearShadow;
		Loop;
		AGRD D 8;
		Loop;
		AGRD ABCDABCD 5 A_Wander;
		Loop;
	See:
		AGRD A 6 Fast Slow A_AcolyteBits;
		AGRD BCD 6 Fast Slow A_Chase;
		Loop;
	Missile:
		AGRD E 8 Fast Slow A_FaceTarget;
		AGRD FE 4 Fast Slow A_ShootGun;
		AGRD F 6 Fast Slow A_ShootGun;
		Goto See;
	Pain:
		AGRD O 8 Fast Slow A_Pain;
		Goto See;
	Death:
		AGRD G 4;
		AGRD H 4 A_Scream;
		AGRD I 4;
		AGRD J 3;
		AGRD K 3 A_NoBlocking;
		AGRD L 3;
		AGRD M 3 A_AcolyteDie;
		AGRD N -1;
		Stop;
	XDeath:
		GIBS A 5 A_NoBlocking;
		GIBS BC 5 A_TossGib;
		GIBS D 4 A_TossGib;
		GIBS E 4 A_XScream;
		GIBS F 4 A_TossGib;
		GIBS GH 4;
		GIBS I 5;
		GIBS J 5 A_AcolyteDie;
		GIBS K 5;
		GIBS L 1400;
		Stop;
	}
	
	//============================================================================
	//
	// A_AcolyteDie
	//
	//============================================================================

	void A_AcolyteDie ()
	{
		// [RH] Disable translucency here.
		A_SetRenderStyle(1, STYLE_Normal);

		// Only the Blue Acolyte does extra stuff on death.
		if (self is "AcolyteBlue")
		{
			int i;
			// Make sure somebody is still alive
			for (i = 0; i < MAXPLAYERS; ++i)
			{
				if (playeringame[i] && players[i].health > 0)
					break;
			}
			if (i == MAXPLAYERS)
				return;

			// Make sure all the other blue acolytes are dead, but do this only once in case of simultaneous kills.
			if (CheckBossDeath() && !players[i].mo.FindInventory("QuestItem7"))
			{
				players[i].mo.GiveInventoryType ("QuestItem7");
				players[i].SetLogNumber (14);
				players[i].SetSubtitleNumber (14);
				A_StopSound (CHAN_VOICE);
				A_PlaySound ("svox/voc14", CHAN_VOICE, 1, false, ATTN_NONE);
			}
		}
	}

	//============================================================================
	//
	// A_BeShadowyFoe
	//
	//============================================================================

	void A_BeShadowyFoe()
	{
		A_SetRenderStyle(HR_SHADOW, STYLE_Translucent);
		bFriendly = false;
	}

	//============================================================================
	//
	// A_AcolyteBits
	//
	//============================================================================

	void A_AcolyteBits()
	{
		if (SpawnFlags & MTF_SHADOW)
		{
			A_BeShadowyFoe();
		}
		if (SpawnFlags & MTF_ALTSHADOW)
		{
			if (bShadow)
			{
				// I dunno.
			}
			else
			{
				A_SetRenderStyle(0, STYLE_None);
			}
		}
	}
	
}
	

// Acolyte 1 ----------------------------------------------------------------

class AcolyteTan : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		DropItem "ClipOfBullets";
	}
}

// Acolyte 2 ----------------------------------------------------------------

class AcolyteRed : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		Translation 0;
	}
}

// Acolyte 3 ----------------------------------------------------------------

class AcolyteRust : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		Translation 1;
	}
}

// Acolyte 4 ----------------------------------------------------------------

class AcolyteGray : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		Translation 2;
	}
}

// Acolyte 5 ----------------------------------------------------------------

class AcolyteDGreen : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		Translation 3;
	}
}

// Acolyte 6 ----------------------------------------------------------------

class AcolyteGold : Acolyte
{
	Default
	{
		+MISSILEMORE +MISSILEEVENMORE
		Translation 4;
	}
}

// Acolyte 7 ----------------------------------------------------------------

class AcolyteLGreen : Acolyte
{
	Default
	{
		Health 60;
		Translation 5;
	}
}

// Acolyte 8 ----------------------------------------------------------------

class AcolyteBlue : Acolyte
{
	Default
	{
		Health 60;
		Translation 6;
	}
}

// Shadow Acolyte -----------------------------------------------------------

class AcolyteShadow : Acolyte
{
	Default
	{
		+MISSILEMORE
		DropItem "ClipOfBullets";
	}
	States
	{
	See:
		AGRD A 6 A_BeShadowyFoe;
		Goto Super::See+1;
	Pain:
		AGRD O 0 Fast Slow A_SetShadow;
		AGRD O 8 Fast Slow A_Pain;
		Goto See;
	}
}

// Some guy turning into an acolyte -----------------------------------------

class AcolyteToBe : Acolyte
{
	Default
	{
		Health 61;
		Radius 20;
		Height 56;
		DeathSound "becoming/death";
		-COUNTKILL
		-ISMONSTER
	}

	States
	{
	Spawn:
		ARMR A -1;
		Stop;
	Pain:
		ARMR A -1 A_HideDecepticon;
		Stop;
	Death:
		Goto XDeath;
	}
	
	//============================================================================
	//
	// A_HideDecepticon
	//
	// Hide the Acolyte-to-be							->
	// Hide the guy transforming into an Acolyte		->
	// Hide the transformer								->
	// Transformers are Autobots and Decepticons, and
	// Decepticons are the bad guys, so...				->
	//
	// Hide the Decepticon!
	//
	//============================================================================

	void A_HideDecepticon ()
	{
		Door_Close(999, 64);
		if (target != null && target.player != null)
		{
			SoundAlert (target);
		}
	}
}
