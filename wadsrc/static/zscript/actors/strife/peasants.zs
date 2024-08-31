
// Peasant Base Class -------------------------------------------------------

class Peasant : StrifeHumanoid
{
	Default
	{
		Health 31;
		PainChance 200;
		Speed 8;
		Radius 20;
		Height 56;
		Monster;
		+NEVERTARGET
		-COUNTKILL
		+NOSPLASHALERT
		+FLOORCLIP
		+JUSTHIT
		MinMissileChance 150;
		MaxStepHeight 16;
		MaxDropoffHeight 32;
		Tag "$TAG_PEASANT";
		SeeSound "peasant/sight";
		AttackSound "peasant/attack";
		PainSound "peasant/pain";
		DeathSound "peasant/death";
		HitObituary "$OB_PEASANT";
	}
	States
	{
	Spawn:
		PEAS A 10 A_Look2;
		Loop;
	See:
		PEAS AABBCCDD 5 A_Wander;
		Goto Spawn;
	Melee:
		PEAS E 10 A_FaceTarget;
		PEAS F 8 A_CustomMeleeAttack(2*random[PeasantAttack](1,5)+2);
		PEAS E 8;
		Goto See;
	Pain:
		PEAS O 3;
		PEAS O 3 A_Pain;
		Goto Melee;
	Wound:
		PEAS G 5;
		PEAS H 10 A_GetHurt;
		PEAS I 6;
		Goto Wound+1;
	Death:
		PEAS G 5;
		PEAS H 5 A_Scream;
		PEAS I 6;
		PEAS J 5 A_NoBlocking;
		PEAS K 5;
		PEAS L 6;
		PEAS M 8;
		PEAS N 1400;
		GIBS U 5;
		GIBS V 1400;
		Stop;
	XDeath:
		GIBS M 5 A_TossGib;
		GIBS N 5 A_XScream;
		GIBS O 5 A_NoBlocking;
		GIBS PQRS 4 A_TossGib;
		Goto Death+8;
	}
}

// Peasant Variant 1 --------------------------------------------------------

class Peasant1 : Peasant
{
	Default
	{
		Speed 4;
	}
}

class Peasant2 : Peasant
{
	Default
	{
		Speed 5;
	}
}

class Peasant3 : Peasant
{
	Default
	{
		Speed 5;
	}
}

class Peasant4 : Peasant
{
	Default
	{
		Translation 0;
		Speed 7;
	}
}

class Peasant5 : Peasant
{
	Default
	{
		Translation 0;
		Speed 7;
	}
}

class Peasant6 : Peasant
{
	Default
	{
		Translation 0;
		Speed 7;
	}
}

class Peasant7 : Peasant
{
	Default
	{
		Translation 2;
	}
}

class Peasant8 : Peasant
{
	Default
	{
		Translation 2;
	}
}

class Peasant9 : Peasant
{
	Default
	{
		Translation 2;
	}
}

class Peasant10 : Peasant
{
	Default
	{
		Translation 1;
	}
}

class Peasant11 : Peasant
{
	Default
	{
		Translation 1;
	}
}

class Peasant12 : Peasant
{
	Default
	{
		Translation 1;
	}
}

class Peasant13 : Peasant
{
	Default
	{
		Translation 3;
	}
}

class Peasant14 : Peasant
{
	Default
	{
		Translation 3;
	}
}

class Peasant15 : Peasant
{
	Default
	{
		Translation 3;
	}
}

class Peasant16 : Peasant
{
	Default
	{
		Translation 5;
	}
}

class Peasant17 : Peasant
{
	Default
	{
		Translation 5;
	}
}

class Peasant18 : Peasant
{
	Default
	{
		Translation 5;
	}
}

class Peasant19 : Peasant
{
	Default
	{
		Translation 4;
	}
}

class Peasant20 : Peasant
{
	Default
	{
		Translation 4;
	}
}

class Peasant21 : Peasant
{
	Default
	{
		Translation 4;
	}
}

class Peasant22 : Peasant
{
	Default
	{
		Translation 6;
	}
}

