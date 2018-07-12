//===========================================================================
//
// Spider boss
//
//===========================================================================
class SpiderMastermind : Actor
{
	Default
	{
		Health 3000;
		Radius 128;
		Height 100;
		Mass 1000;
		Speed 12;
		PainChance 40;
		Monster;
		+BOSS
		+MISSILEMORE
		+FLOORCLIP
		+NORADIUSDMG
		+DONTMORPH
		+BOSSDEATH
		SeeSound "spider/sight";
		AttackSound "spider/attack";
		PainSound "spider/pain";
		DeathSound "spider/death";
		ActiveSound "spider/active";
		Obituary "$OB_SPIDER";
		Tag "$FN_SPIDER";
	}
	States
	{
	Spawn:
		SPID AB 10 A_Look;
		Loop;
	See:
		SPID A 3 A_Metal;
		SPID ABB 3 A_Chase;
		SPID C 3 A_Metal;
		SPID CDD 3 A_Chase;
		SPID E 3 A_Metal;
		SPID EFF 3 A_Chase;
		Loop;
	Missile:
		SPID A 20 BRIGHT A_FaceTarget;
		SPID G 4 BRIGHT A_SPosAttackUseAtkSound;
		SPID H 4 BRIGHT A_SposAttackUseAtkSound;
		SPID H 1 BRIGHT A_SpidRefire;
		Goto Missile+1;
	Pain:
		SPID I 3;
		SPID I 3 A_Pain;
		Goto See;
	Death:
		SPID J 20 A_Scream;
		SPID K 10 A_NoBlocking;
		SPID LMNOPQR 10;
		SPID S 30;
		SPID S -1 A_BossDeath;
		Stop;
	}
}

//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	void A_SpidRefire()
	{
		// keep firing unless target got out of sight
		A_FaceTarget();
		if (Random[CPosRefire](0, 255) >= 10)
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

	void A_Metal()
	{
		A_PlaySound("spider/walk", CHAN_BODY, 1, false, ATTN_IDLE);
		A_Chase();
	}
}
