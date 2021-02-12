

// Stalker ------------------------------------------------------------------

class Stalker : Actor
{
	Default
	{
		Health 80;
		Painchance 40;
		Speed 16;
		Radius 31;
		Height 25;
		Monster;
		+NOGRAVITY
		+DROPOFF
		+NOBLOOD
		+SPAWNCEILING
		+INCOMBAT
		+NOVERTICALMELEERANGE
		MaxDropOffHeight 32;
		MinMissileChance 150;
		SeeSound "stalker/sight";
		AttackSound "stalker/attack";
		PainSound "stalker/pain";
		DeathSound "stalker/death";
		ActiveSound "stalker/active";
		HitObituary "$OB_STALKER";
	}

	States
	{
	Spawn:
		STLK A 1 A_StalkerLookInit;
		Loop;
	LookCeiling:
		STLK A 10 A_Look;
		Loop;
	LookFloor:
		STLK J 10 A_Look;
		Loop;
	See:
		STLK A 1 Slow A_StalkerChaseDecide;
		STLK ABB 3 Slow A_Chase;
		STLK C 3 Slow A_StalkerWalk;
		STLK C 3 Slow A_Chase;
		Loop;
	Melee:
		STLK J 3 Slow A_FaceTarget;
		STLK K 3 Slow A_StalkerAttack;
	SeeFloor:
		STLK J 3 A_StalkerWalk;
		STLK KK 3 A_Chase;
		STLK L 3 A_StalkerWalk;
		STLK L 3 A_Chase;
		Loop;
	Pain:
		STLK L 1 A_Pain;
		Goto See;
	Drop:
		STLK C 2 A_StalkerDrop;
		STLK IHGFED 3;
		Goto SeeFloor;
	Death:
		STLK O 4;
		STLK P 4 A_Scream;
		STLK QRST 4;
		STLK U 4 A_NoBlocking;
		STLK VW 4;
		STLK XYZ[ 4 Bright;
		Stop;
	}
		
	void A_StalkerChaseDecide ()
	{
		if (!bNoGravity)
		{
			SetStateLabel("SeeFloor");
		}
		else if (ceilingz > pos.z + height)
		{
			SetStateLabel("Drop");
		}
	}

	void A_StalkerLookInit ()
	{
		State st;
		if (bNoGravity)
		{
			st = FindState("LookCeiling");
		}
		else
		{
			st = FindState("LookFloor");
		}
		if (st != CurState.NextState)
		{
			SetState (st);
		}
	}

	void A_StalkerDrop ()
	{
		bNoVerticalMeleeRange = false;
		bNoGravity = false;
	}

	void A_StalkerAttack ()
	{
		if (bNoGravity)
		{
			SetStateLabel("Drop");
		}
		else if (target != null)
		{
			A_FaceTarget ();
			if (CheckMeleeRange ())
			{
				let targ = target;
				int damage = random[Stalker](1, 8) * 2;

				int newdam = targ.DamageMobj (self, self, damage, 'Melee');
				targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			}
		}
	}

	void A_StalkerWalk ()
	{
		A_StartSound ("stalker/walk", CHAN_BODY);
		A_Chase ();
	}

	
}
