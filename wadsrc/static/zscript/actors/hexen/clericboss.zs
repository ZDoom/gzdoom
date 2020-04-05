
// Cleric Boss (Traductus) --------------------------------------------------

class ClericBoss : Actor
{
	Default
	{
		Health 800;
		PainChance 50;
		Speed 25;
		Radius 16;
		Height 64;
		Monster;
		+FLOORCLIP +TELESTOMP
		+DONTMORPH
		PainSound "PlayerClericPain";
		DeathSound "PlayerClericCrazyDeath";
		Obituary "$OB_CBOSS";
		Tag "$FN_CBOSS";
	}

	States
	{
	Spawn:
		CLER A 2;
		CLER A 3 A_ClassBossHealth;
		CLER A 5 A_Look;
		Wait;
	See:
		CLER ABCD 4 A_FastChase;
		Loop;
	Pain:
		CLER H 4;
		CLER H 4 A_Pain;
		Goto See;
	Melee:
	Missile:
		CLER EF 8 A_FaceTarget;
		CLER G 10 A_ClericAttack;
		Goto See;
	Death:
		CLER I 6;
		CLER K 6 A_Scream;
		CLER LL 6;
		CLER M 6 A_NoBlocking;
		CLER NOP 6;
		CLER Q -1;
		Stop;
	XDeath:
		CLER R 5 A_Scream;
		CLER S 5;
		CLER T 5 A_NoBlocking;
		CLER UVWXYZ 5;
		CLER [ -1;
		Stop;
	Ice:
		CLER \ 5 A_FreezeDeath;
		CLER \ 1 A_FreezeDeathChunks;
		Wait;
	Burn:
		CLER C 5 Bright A_StartSound("PlayerClericBurnDeath");
		FDTH D 4 Bright ;
		FDTH G 5 Bright ;
		FDTH H 4 Bright A_Scream;
		FDTH I 5 Bright ;
		FDTH J 4 Bright ;
		FDTH K 5 Bright ;
		FDTH L 4 Bright ;
		FDTH M 5 Bright ;
		FDTH N 4 Bright ;
		FDTH O 5 Bright ;
		FDTH P 4 Bright ;
		FDTH Q 5 Bright ;
		FDTH R 4 Bright ;
		FDTH S 5 Bright A_NoBlocking;
		FDTH T 4 Bright ;
		FDTH U 5 Bright ;
		FDTH V 4 Bright ;
		Stop;
	}
		
	//============================================================================
	//
	// A_ClericAttack
	//
	//============================================================================

	void A_ClericAttack()
	{
		if (!target) return;

		Actor missile = SpawnMissileZ (pos.z + 40., target, "HolyMissile");
		if (missile != null) missile.tracer = null;	// No initial target
		A_StartSound ("HolySymbolFire", CHAN_WEAPON);
	}
}
