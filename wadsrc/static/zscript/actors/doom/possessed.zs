
//===========================================================================
//
// Zombie man
//
//===========================================================================
class ZombieMan : Actor
{
	Default
	{
		Health 20;
		Radius 20;
		Height 56;
		Speed 8;
		PainChance 200;
		Monster;
		+FLOORCLIP
		SeeSound "grunt/sight";
		AttackSound "grunt/attack";
		PainSound "grunt/pain";
		DeathSound "grunt/death";
		ActiveSound "grunt/active";
		Obituary "$OB_ZOMBIE";
		Tag "$FN_ZOMBIE";
		DropItem "Clip";
	}
	States
	{
	Spawn:
		POSS AB 10 A_Look;
		Loop;
	See:
		POSS AABBCCDD 4 A_Chase;
		Loop;
	Missile:
		POSS E 10 A_FaceTarget;
		POSS F 8 A_PosAttack;
		POSS E 8;
		Goto See;
	Pain:
		POSS G 3;
		POSS G 3 A_Pain;
		Goto See;
	Death:
		POSS H 5;
		POSS I 5 A_Scream;
		POSS J 5 A_NoBlocking;
		POSS K 5;
		POSS L -1;
		Stop;
	XDeath:
		POSS M 5;
		POSS N 5 A_XScream;
		POSS O 5 A_NoBlocking;
		POSS PQRST 5;
		POSS U -1;
		Stop;
	Raise:
		POSS K 5;
		POSS JIH 5;
		Goto See;
	}
}

//===========================================================================
//
// Sergeant / Shotgun guy
//
//===========================================================================
class ShotgunGuy : Actor
{
	Default
	{
		Health 30;
		Radius 20;
		Height 56;
		Mass 100;
		Speed 8;
		PainChance 170;
		Monster;
		+FLOORCLIP
		SeeSound "shotguy/sight";
		AttackSound "shotguy/attack";
		PainSound "shotguy/pain";
		DeathSound "shotguy/death";
		ActiveSound "shotguy/active";
		Obituary "$OB_SHOTGUY";
		Tag "$FN_SHOTGUN";
		DropItem "Shotgun";
	}
	States
	{
	Spawn:
		SPOS AB 10 A_Look;
		Loop;
	See:
		SPOS AABBCCDD 3 A_Chase;
		Loop;
	Missile:
		SPOS E 10 A_FaceTarget;
		SPOS F 10 BRIGHT A_SposAttackUseAtkSound;
		SPOS E 10;
		Goto See;
	Pain:
		SPOS G 3;
		SPOS G 3 A_Pain;
		Goto See;
	Death:
		SPOS H 5;
		SPOS I 5 A_Scream;
		SPOS J 5 A_NoBlocking;
		SPOS K 5;
		SPOS L -1;
		Stop;
	XDeath:
		SPOS M 5;
		SPOS N 5 A_XScream;
		SPOS O 5 A_NoBlocking;
		SPOS PQRST 5;
		SPOS U -1;
		Stop;
	Raise:
		SPOS L 5;
		SPOS KJIH 5;
		Goto See;
	}
}

//===========================================================================
//
// Chaingunner
//
//===========================================================================
class ChaingunGuy : Actor
{
	Default
	{
		Health 70;
		Radius 20;
		Height 56;
		Mass 100;
		Speed 8;
		PainChance 170;
		Monster;
		+FLOORCLIP
		SeeSound "chainguy/sight";
		PainSound "chainguy/pain";
		DeathSound "chainguy/death";
		ActiveSound "chainguy/active";
		AttackSound "chainguy/attack";
		Obituary "$OB_CHAINGUY";
		Tag "$FN_HEAVY";
		Dropitem "Chaingun";
	}
	States
	{
	Spawn:
		CPOS AB 10 A_Look;
		Loop;
	See:
		CPOS AABBCCDD 3 A_Chase;
		Loop;
	Missile:
		CPOS E 10 A_FaceTarget;
		CPOS FE 4 BRIGHT A_CPosAttack;
		CPOS F 1 A_CPosRefire;
		Goto Missile+1;
	Pain:
		CPOS G 3;
		CPOS G 3 A_Pain;
		Goto See;
	Death:
		CPOS H 5;
		CPOS I 5 A_Scream;
		CPOS J 5 A_NoBlocking;
		CPOS KLM 5;
		CPOS N -1;
		Stop;
	XDeath:
		CPOS O 5;
		CPOS P 5 A_XScream;
		CPOS Q 5 A_NoBlocking;
		CPOS RS 5;
		CPOS T -1;
		Stop;
	Raise:
		CPOS N 5;
		CPOS MLKJIH 5;
		Goto See;
	}
}

//===========================================================================
//
// SS Nazi
//
//===========================================================================
class WolfensteinSS : Actor
{
	Default
	{
		Health 50;
		Radius 20;
		Height 56;
		Speed 8;
		PainChance 170;
		Monster;
		+FLOORCLIP
		SeeSound "wolfss/sight";
		PainSound "wolfss/pain";
		DeathSound "wolfss/death";
		ActiveSound "wolfss/active";
		AttackSound "wolfss/attack";
		Obituary "$OB_WOLFSS";
		Tag "$FN_WOLFSS";
		Dropitem "Clip";
	}
	States
	{
	Spawn:
		SSWV AB 10 A_Look;
		Loop;
	See:
		SSWV AABBCCDD 3 A_Chase;
		Loop;
	Missile:
		SSWV E 10 A_FaceTarget;
		SSWV F 10 A_FaceTarget;
		SSWV G 4 BRIGHT A_CPosAttack;
		SSWV F 6 A_FaceTarget;
		SSWV G 4 BRIGHT A_CPosAttack;
		SSWV F 1 A_CPosRefire;
		Goto Missile+1;
	Pain:
		SSWV H 3;
		SSWV H 3 A_Pain;
		Goto See;
	Death:
		SSWV I 5;
		SSWV J 5 A_Scream;
		SSWV K 5 A_NoBlocking;
		SSWV L 5;
		SSWV M -1;
		Stop;
	XDeath:
		SSWV N 5 ;
		SSWV O 5 A_XScream;
		SSWV P 5 A_NoBlocking;
		SSWV QRSTU 5;
		SSWV V -1;
		Stop;
	Raise:
		SSWV M 5;
		SSWV LKJI 5;
		Goto See ;
	}
}

//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	void A_PosAttack()
	{
		if (target)
		{
			A_FaceTarget();
			double ang = angle;
			double slope = AimLineAttack(ang, MISSILERANGE);
			A_StartSound("grunt/attack", CHAN_WEAPON);
			ang  += Random2[PosAttack]() * (22.5/256);
			int damage = Random[PosAttack](1, 5) * 3;
			LineAttack(ang, MISSILERANGE, slope, damage, "Hitscan", "Bulletpuff");
		}
	}
	
	
	private void A_SPosAttackInternal()
	{
		if (target)
		{
			A_FaceTarget();
			double bangle = angle;
			double slope = AimLineAttack(bangle, MISSILERANGE);
		
			for (int i=0 ; i<3 ; i++)
			{
				double ang = bangle + Random2[SPosAttack]() * (22.5/256);
				int damage = Random[SPosAttack](1, 5) * 3;
				LineAttack(ang, MISSILERANGE, slope, damage, "Hitscan", "Bulletpuff");
			}
		}
    }
	
	void A_SPosAttackUseAtkSound()
	{
		if (target)
		{
			A_StartSound(AttackSound, CHAN_WEAPON);
			A_SPosAttackInternal();
		}
	}
	
	// This version of the function, which uses a hard-coded sound, is meant for Dehacked only.
	void A_SPosAttack()
	{
		if (target)
		{
			A_StartSound("shotguy/attack", CHAN_WEAPON);
			A_SPosAttackInternal();
		}
	}
	
	private void A_CPosAttackInternal(Sound snd)
	{
		if (target)
		{
			if (bStealth) visdir = 1;
			A_StartSound(snd, CHAN_WEAPON);
			A_FaceTarget();
			double slope = AimLineAttack(angle, MISSILERANGE);
			double ang = angle + Random2[CPosAttack]() * (22.5/256);
			int damage = Random[CPosAttack](1, 5) * 3;
			LineAttack(ang, MISSILERANGE, slope, damage, "Hitscan", "Bulletpuff");
		}
	}

	void A_CPosAttack()
	{
		A_CPosAttackInternal(AttackSound);
	}
	
	void A_CPosAttackDehacked()
	{
		A_CPosAttackInternal("chainguy/attack");
	}

	
	void A_CPosRefire()
	{
		// keep firing unless target got out of sight
		A_FaceTarget();
		if (Random[CPosRefire](0, 255) >= 40)
		{
			if (!target
				|| HitFriend()
				|| target.health <= 0
				|| !CheckSight(target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES))
			{
				SetState(SeeState);
			}
		}
	}
}
