
//===========================================================================
//
// Cyberdemon
//
//===========================================================================
class Cyberdemon : Actor
{
	Default
	{
		Health 4000;
		Radius 40;
		Height 110;
		Mass 1000;
		Speed 16;
		PainChance 20;
		Monster;
		MinMissileChance 160;
		MissileChanceMult 0.5;
		+BOSS 
		+FLOORCLIP
		+NORADIUSDMG
		+DONTMORPH
		+BOSSDEATH
		+E2M8BOSS
		+E4M6BOSS
		SeeSound "cyber/sight";
		PainSound "cyber/pain";
		DeathSound "cyber/death";
		ActiveSound "cyber/active";
		Obituary "$OB_CYBORG";
		Tag "$FN_CYBER";
	}
	States
	{
	Spawn:
		CYBR AB 10 A_Look;
		Loop;
	See:
		CYBR A 3 A_Hoof;
		CYBR ABBCC 3 A_Chase;
		CYBR D 3 A_Metal;
		CYBR D 3 A_Chase;
		Loop;
	Missile:
		CYBR E 6 A_FaceTarget;
		CYBR F 12 A_CyberAttack;
		CYBR E 12 A_FaceTarget;
		CYBR F 12 A_CyberAttack;
		CYBR E 12 A_FaceTarget;
		CYBR F 12 A_CyberAttack;
		Goto See;
	Pain:
		CYBR G 10 A_Pain;
		Goto See;
	Death:
		CYBR H 10;
		CYBR I 10 A_Scream;
		CYBR JKL 10;
		CYBR M 10 A_NoBlocking;
		CYBR NO 10;
		CYBR P 30;
		CYBR P -1 A_BossDeath;
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
	void A_CyberAttack()
	{
		if (target)
		{
			A_FaceTarget();
			SpawnMissile (target, "Rocket");
		}
	}
	
	void A_Hoof()
	{
		A_StartSound("cyber/hoof", CHAN_BODY, CHANF_DEFAULT, 1, ATTN_IDLE);
		A_Chase();
	}
	
}
