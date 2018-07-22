//===========================================================================
//
// Arachnotron
//
//===========================================================================
class Arachnotron : Actor
{
	Default
	{
		Health 500;
		Radius 64;
		Height 64;
		Mass 600;
		Speed 12;
		PainChance 128;
		Monster;
		+FLOORCLIP
		+BOSSDEATH
		SeeSound "baby/sight";
		PainSound "baby/pain";
		DeathSound "baby/death";
		ActiveSound "baby/active";
		Obituary "$OB_BABY";
		Tag "$FN_ARACH";
	}
	States
	{
	Spawn:
		BSPI AB 10 A_Look;
		Loop;
	See:
		BSPI A 20;
		BSPI A 3 A_BabyMetal;
		BSPI ABBCC 3 A_Chase;
		BSPI D 3 A_BabyMetal;
		BSPI DEEFF 3 A_Chase;
		Goto See+1;
	Missile:
		BSPI A 20 BRIGHT A_FaceTarget;
		BSPI G 4 BRIGHT A_BspiAttack;
		BSPI H 4 BRIGHT;
		BSPI H 1 BRIGHT A_SpidRefire;
		Goto Missile+1;
	Pain:
		BSPI I 3;
		BSPI I 3 A_Pain;
		Goto See+1;
	Death:
		BSPI J 20 A_Scream;
		BSPI K 7 A_NoBlocking;
		BSPI LMNO 7;
		BSPI P -1 A_BossDeath;
		Stop;
    Raise:
		BSPI P 5;
		BSPI ONMLKJ 5;
		Goto See+1;
	}
}

//===========================================================================
//
// Arachnotron plasma
//
//===========================================================================
class ArachnotronPlasma : Actor
{
	Default
	{
		Radius 13;
		Height 8;
		Speed 25;
		Damage 5;
		Projectile;
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 0.75;
		SeeSound "baby/attack";
		DeathSound "baby/shotx";
	}
	States
	{
	Spawn:
		APLS AB 5 BRIGHT;
		Loop;
	Death:
		APBX ABCDE 5 BRIGHT;
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
	void A_BspiAttack()
	{
		if (target)
		{
			A_FaceTarget();
			SpawnMissile(target, "ArachnotronPlasma");
		}
	}
	
	void A_BabyMetal()
	{
		A_PlaySound("baby/walk", CHAN_BODY, 1, false, ATTN_IDLE);
		A_Chase();
	}
}

