//===========================================================================
//
// Imp
//
//===========================================================================
class DoomImp : Actor
{
	Default
	{
		Health 60;
		Radius 20;
		Height 56;
		Mass 100;
		Speed 8;
		PainChance 200;
		Monster;
		+FLOORCLIP
		SeeSound "imp/sight";
		PainSound "imp/pain";
		DeathSound "imp/death";
		ActiveSound "imp/active";
		HitObituary "$OB_IMPHIT";
		Obituary "$OB_IMP";
		Tag "$FN_IMP";
	}
	States
	{
	Spawn:
		TROO AB 10 A_Look;
		Loop;
	See:
		TROO AABBCCDD 3 A_Chase;
		Loop;
	Melee:
	Missile:
		TROO EF 8 A_FaceTarget;
		TROO G 6 A_TroopAttack ;
		Goto See;
	Pain:
		TROO H 2;
		TROO H 2 A_Pain;
		Goto See;
	Death:
		TROO I 8;
		TROO J 8 A_Scream;
		TROO K 6;
		TROO L 6 A_NoBlocking;
		TROO M -1;
		Stop;
	XDeath:
		TROO N 5;
		TROO O 5 A_XScream;
		TROO P 5;
		TROO Q 5 A_NoBlocking;
		TROO RST 5;
		TROO U -1;
		Stop;
	Raise:
		TROO ML 8;
		TROO KJI 6;
		Goto See;
	}
}

//===========================================================================
//
// Imp fireball
//
//===========================================================================
class DoomImpBall : Actor
{
	Default
	{
		Radius 6;
		Height 8;
		Speed 10;
		FastSpeed 20;
		Damage 3;
		Projectile;
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 1;
		SeeSound "imp/attack";
		DeathSound "imp/shotx";
	}
	States
	{
	Spawn:
		BAL1 AB 4 BRIGHT;
		Loop;
	Death:
		BAL1 CDE 6 BRIGHT;
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
	void A_TroopAttack()
	{
		let targ = target;
		if (targ)
		{
			if (CheckMeleeRange())
			{
				int damage = random[pr_troopattack](1, 8) * 3;
				A_StartSound ("imp/melee", CHAN_WEAPON);
				int newdam = targ.DamageMobj (self, self, damage, "Melee");
				targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			}
			else
			{
				// launch a missile
				SpawnMissile (targ, "DoomImpBall");
			}
		}
	}
}
