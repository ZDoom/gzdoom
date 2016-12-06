
// Base class for the beggars ---------------------------------------------

class Beggar : StrifeHumanoid
{
	Default
	{
		Health 20;
		PainChance 250;
		Speed 3;
		Radius 20;
		Height 56;
		Monster;
		+JUSTHIT
		-COUNTKILL
		+NOSPLASHALERT
		MinMissileChance 150;
		Tag "$TAG_BEGGAR";
		MaxStepHeight 16;
		MaxDropoffHeight 32;
		HitObituary "$OB_BEGGAR";

		AttackSound "beggar/attack";
		PainSound "beggar/pain";
		DeathSound "beggar/death";
	}
	States
	{
	Spawn:
		BEGR A 10 A_Look;
		Loop;
	See:
		BEGR AABBCC 4 A_Wander;
		Loop;
	Melee:
		BEGR D 8;
		BEGR D 8 A_CustomMeleeAttack(2*random[PeasantAttack](1,5)+2);
		BEGR E 1 A_Chase;
		BEGR D 8 A_SentinelRefire;
		Loop;
	Pain:
		BEGR A 3 A_Pain;
		BEGR A 3 A_Chase;
		Goto Melee;
	Death:
		BEGR F 4;
		BEGR G 4 A_Scream;
		BEGR H 4;
		BEGR I 4 A_NoBlocking;
		BEGR JKLM 4;
		BEGR N -1;
		Stop;
	XDeath:
		BEGR F 5 A_TossGib;
		GIBS M 5 A_TossGib;
		GIBS N 5 A_XScream;
		GIBS O 5 A_NoBlocking;
		GIBS PQRST 4 A_TossGib;
		GIBS U 5;
		GIBS V 1400;
		Stop;
	}
}
			

// Beggars -----------------------------------------------------------------

class Beggar1 : Beggar
{
}


class Beggar2 : Beggar
{
}


class Beggar3 : Beggar
{
}


class Beggar4 : Beggar
{
}


class Beggar5 : Beggar
{
}
