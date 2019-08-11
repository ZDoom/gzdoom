//===========================================================================
//
// Lost Soul
//
//===========================================================================
class LostSoul : Actor
{
	Default
	{
		Health 100;
		Radius 16;
		Height 56;
		Mass 50;
		Speed 8;
		Damage 3;
		PainChance 256;
		Monster;
		+FLOAT +NOGRAVITY +MISSILEMORE +DONTFALL +NOICEDEATH +ZDOOMTRANS +RETARGETAFTERSLAM
		AttackSound "skull/melee";
		PainSound "skull/pain";
		DeathSound "skull/death";
		ActiveSound "skull/active";
		RenderStyle "SoulTrans";
		Obituary "$OB_SKULL";
		Tag "$FN_LOST";
	}
	States
	{
	Spawn:
		SKUL AB 10 BRIGHT A_Look;
		Loop;
	See:
		SKUL AB 6 BRIGHT A_Chase;
		Loop;
	Missile:
		SKUL C 10 BRIGHT A_FaceTarget;
		SKUL D 4 BRIGHT A_SkullAttack;
		SKUL CD 4 BRIGHT;
		Goto Missile+2;
	Pain:
		SKUL E 3 BRIGHT;
		SKUL E 3 BRIGHT A_Pain;
		Goto See;
	Death:
		SKUL F 6 BRIGHT;
		SKUL G 6 BRIGHT A_Scream;
		SKUL H 6 BRIGHT;
		SKUL I 6 BRIGHT A_NoBlocking;
		SKUL J 6;
		SKUL K 6;
		Stop;
	}
}

class BetaSkull : LostSoul
{
	States
	{
	Spawn:
		SKUL A 10 A_Look;
		Loop;
	See:
		SKUL BCDA 5 A_Chase;
		Loop;
	Missile:
		SKUL E 4 A_FaceTarget;
		SKUL F 5 A_BetaSkullAttack;
		SKUL F 4;
		Goto See;
	Pain:
		SKUL G 4;
		SKUL H 2 A_Pain;
		Goto See;
		SKUL I 4;
		Goto See;
	Death:
		SKUL JKLM 5;
		SKUL N 5 A_Scream;
		SKUL O 5;
		SKUL P 5 A_Fall;
		SKUL Q 5 A_Stop;
		Wait;
	}
}

//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	const DEFSKULLSPEED = 20;
	
	void A_SkullAttack(double skullspeed = DEFSKULLSPEED)
	{
		if (target == null) return;

		if (skullspeed <= 0) skullspeed = DEFSKULLSPEED;

		bSkullfly = true;
		A_PlaySound(AttackSound, CHAN_VOICE);
		A_FaceTarget();
		VelFromAngle(skullspeed);
		Vel.Z = (target.pos.Z + target.Height/2 - pos.Z) / DistanceBySpeed(target, skullspeed);
	}

	void A_BetaSkullAttack()
	{
		if (target == null || target.GetSpecies() == self.GetSpecies()) return;

		A_PlaySound(AttackSound, CHAN_WEAPON);
		A_FaceTarget();
		
		int damage = GetMissileDamage(7,1);
		target.DamageMobj(self, self, damage, 'None');
	}
}



