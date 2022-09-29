// Klaxon Warning Light -----------------------------------------------------

class KlaxonWarningLight : Actor
{
	Default
	{
		ReactionTime 60;
		Radius 5;
		+NOBLOCKMAP +AMBUSH
		+SPAWNCEILING +NOGRAVITY
		+FIXMAPTHINGPOS +NOSPLASHALERT
		+SYNCHRONIZED
	}
	States
	{
	Spawn:
		KLAX A 5 A_TurretLook;
		Loop;
	See:
		KLAX B 6 A_KlaxonBlare;
		KLAX C 60;
		Loop;
	}

}

// CeilingTurret ------------------------------------------------------------

class CeilingTurret : Actor
{
	Default
	{
		Health 125;
		Speed 0;
		Painchance 0;
		Mass 10000000;
		Monster;
		-SOLID
		-CANPASS
		+AMBUSH
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOOD
		+NOSPLASHALERT
		+DONTFALL
		MinMissileChance 150;
		Tag "$TAG_CEILINGTURRET";
		Obituary "$OB_TURRET";
		DeathSound "turret/death";
	}
	States
	{
	Spawn:
		TURT A 5 A_TurretLook;
		Loop;
	See:
		TURT A 2 A_Chase;
		Loop;
	Missile:
	Pain:
		TURT B 4 Slow A_ShootGun;
		TURT D 3 Slow A_SentinelRefire;
		TURT A 4 A_SentinelRefire;
		Loop;
	Death:
		BALL A 6 Bright A_Scream;
		BALL BCDE 6 Bright;
		TURT C -1;
		Stop;
	}
}


extend class Actor
{
	void A_TurretLook()
	{
		if (bInConversation)
			return;

		threshold = 0;
		Actor targ = LastHeard;
		if (targ != NULL && targ.health > 0 && targ.bShootable && !IsFriend(targ))
		{
			target = targ;
			if (bAmbush && !CheckSight (targ))
			{
				return;
			}
			if (SeeSound != 0)
			{
				A_StartSound (SeeSound, CHAN_VOICE);
			}
			LastHeard = NULL;
			threshold = 10;
			SetState (SeeState);
		}
	}

	void A_KlaxonBlare()
	{
		if (--reactiontime < 0)
		{
			target = NULL;
			reactiontime = Default.reactiontime;
			A_TurretLook();
			if (target == NULL)
			{
				SetIdle();
			}
			else
			{
				reactiontime = 50;
			}
		}
		if (reactiontime == 2)
		{
			// [RH] Unalert monsters near the alarm and not just those in the same sector as it.
			SoundAlert (NULL, false);
		}
		else if (reactiontime > 50)
		{
			A_StartSound ("misc/alarm", CHAN_VOICE);
		}
	}

}