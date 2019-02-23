// Centaur ------------------------------------------------------------------

class Centaur : Actor
{
	Default
	{
		Health 200;
		Painchance 135;
		Speed 13;
		Height 64;
		Mass 120;
		Monster;
		+FLOORCLIP
		+TELESTOMP
		+SHIELDREFLECT
		SeeSound "CentaurSight";
		AttackSound "CentaurAttack";
		PainSound "CentaurPain";
		DeathSound "CentaurDeath";
		ActiveSound "CentaurActive";
		HowlSound "PuppyBeat";
		Obituary "$OB_CENTAUR";
		DamageFactor "Electric", 3;
		Tag "$FN_CENTAUR";
	}
	States
	{
	Spawn:
		CENT AB 10 A_Look;
		Loop;
	See:
		CENT ABCD 4 A_Chase;
		Loop;
	Pain:
		CENT G 6 A_Pain;
		CENT G 6 A_SetReflectiveInvulnerable;
		CENT EEE 15 A_CentaurDefend;
		CENT E 1 A_UnsetReflectiveInvulnerable;
		Goto See;
	Melee:
		CENT H 5 A_FaceTarget;
		CENT I 4 A_FaceTarget;
		CENT J 7 A_CustomMeleeAttack(random[CentaurAttack](3,9));
		Goto See;
	Death:
		CENT K 4;
		CENT L 4 A_Scream;
		CENT MN 4;
		CENT O 4 A_NoBlocking;
		CENT PQ 4;
		CENT R 4 A_QueueCorpse;
		CENT S 4;
		CENT T -1;
		Stop;
	XDeath:
		CTXD A 4;
		CTXD B 4 A_NoBlocking;
		CTXD C 4
		{
			A_SpawnItemEx("CentaurSword", 0, 0, 45,
								1 + random[CentaurDrop](-128,127)*0.03125,
								1 + random[CentaurDrop](-128,127)*0.03125,
								8 + random[CentaurDrop](0,255)*0.015625, 270);
			A_SpawnItemEx("CentaurShield", 0, 0, 45,
								1 + random[CentaurDrop](-128,127)*0.03125,
								1 + random[CentaurDrop](-128,127)*0.03125,
								8 + random[CentaurDrop](0,255)*0.015625, 90);
		}
		CTXD D 3 A_Scream;
		CTXD E 4 A_QueueCorpse;
		CTXD F 3;
		CTXD G 4;
		CTXD H 3;
		CTXD I 4;
		CTXD J 3;
		CTXD K -1;
	Ice:
		CENT U 5 A_FreezeDeath;
		CENT U 1 A_FreezeDeathChunks;
		Wait;
	}	
}

extend class Actor
{
	void A_CentaurDefend()
	{
		A_FaceTarget ();
		if (CheckMeleeRange() && random[CentaurDefend]() < 32)
		{
			// This should unset REFLECTIVE as well
			// (unless you want the Centaur to reflect projectiles forever!)
			bReflective = false;
			bInvulnerable = false;
			SetState(MeleeState);
		}
	}
}

// Centaur Leader -----------------------------------------------------------

class CentaurLeader : Centaur
{
	Default
	{
		Health 250;
		PainChance 96;
		Speed 10;
		Obituary "$OB_SLAUGHTAUR";
		HitObituary "$OB_SLAUGHTAURHIT";
		Tag "$FN_SLAUGHTAUR";
	}
	States
	{
	Missile:
		CENT E 10 A_FaceTarget;
		CENT F 8 Bright A_SpawnProjectile("CentaurFX", 45, 0, 0, CMF_AIMOFFSET);
		CENT E 10 A_FaceTarget;
		CENT F 8 Bright A_SpawnProjectile("CentaurFX", 45, 0, 0, CMF_AIMOFFSET);
		Goto See;
	}
}		

// Mashed centaur -----------------------------------------------------------
//
// The mashed centaur is only placed through ACS. Nowhere in the game source
// is it ever referenced.

class CentaurMash : Centaur
{
	Default
	{
		+NOBLOOD
		+BLASTED
		-TELESTOMP
		+NOICEDEATH
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Death:
	XDeath:
	Ice:
		Stop;
	}
}

// Centaur projectile -------------------------------------------------------

class CentaurFX : Actor
{
	Default
	{
		Speed 20;
		Damage 4;
		Projectile;
		+SPAWNSOUNDSOURCE
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "CentaurLeaderAttack";
		DeathSound "CentaurMissileExplode";
	}
	States
	{
	Spawn:
		CTFX A -1 Bright;
		Stop;
	Death:
		CTFX B 4 Bright;
		CTFX C 3 Bright;
		CTFX D 4 Bright;
		CTFX E 3 Bright;
		CTFX F 2 Bright;
		Stop;
	}
}

// Centaur shield (debris) --------------------------------------------------

class CentaurShield : Actor
{
	Default
	{
		+DROPOFF
		+CORPSE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		CTDP ABCDEF 3;
		Goto Spawn+2;
	Crash:
		CTDP G 4;
		CTDP H 4 A_QueueCorpse;
		CTDP I 4;
		CTDP J -1;
		Stop;
	}
}

// Centaur sword (debris) ---------------------------------------------------

class CentaurSword : Actor
{
	Default
	{
		+DROPOFF
		+CORPSE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		CTDP KLMNOPQ 3;
		Goto Spawn+2;
	Crash:
		CTDP R 4;
		CTDP S 4 A_QueueCorpse;
		CTDP T -1;
		Stop;
	}
}


