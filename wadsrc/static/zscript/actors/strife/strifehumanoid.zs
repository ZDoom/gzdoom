
// Humanoid Base Class ------------------------------------------------------


class StrifeHumanoid : Actor
{
	Default
	{
		MaxStepHeight 16;
		MaxDropoffHeight 32;
		CrushPainSound "misc/pcrush";
	}
	States
	{
	Burn:
		BURN A 3 Bright Light("PhFire_FX1") A_StartSound("human/imonfire", CHAN_VOICE);
		BURN B 3 Bright Light("PhFire_FX2") A_DropFire;
		BURN C 3 Bright Light("PhFire_FX3") A_Wander;
		BURN D 3 Bright Light("PhFire_FX4") A_NoBlocking;
		BURN E 5 Bright Light("PhFire_FX5") A_DropFire;
		BURN F 5 Bright Light("PhFire_FX6") A_Wander;
		BURN G 5 Bright Light("PhFire_FX7") A_Wander;
		BURN H 5 Bright Light("PhFire_FX6") A_Wander;
		BURN I 5 Bright Light("PhFire_FX5") A_DropFire;
		BURN J 5 Bright Light("PhFire_FX4") A_Wander;
		BURN K 5 Bright Light("PhFire_FX3") A_Wander;
		BURN L 5 Bright Light("PhFire_FX2") A_Wander;
		BURN M 5 Bright Light("PhFire_FX1") A_DropFire;
		BURN N 5 Bright Light("PhFire_FX2");
		BURN O 5 Bright Light("PhFire_FX3");
		BURN P 5 Bright Light("PhFire_FX4");
		BURN Q 5 Bright Light("PhFire_FX5");
		BURN P 5 Bright Light("PhFire_FX4");
		BURN Q 5 Bright Light("PhFire_FX5");
		BURN R 7 Bright Light("PhFire_FX8");
		BURN S 7 Bright Light("PhFire_FX9");
		BURN T 7 Bright Light("PhFire_FX10");
		BURN U 7 Bright Light("PhFire_FX11");
		BURN V -1;
		Stop;
	Disintegrate:
		DISR A 5 A_StartSound("misc/disruptordeath", CHAN_VOICE);
		DISR BC 5;
		DISR D 5 A_NoBlocking;
		DISR EF 5;
		DISR GHIJ 4;
		MEAT D 700;
		Stop;
	}
}


// Fire Droplet -------------------------------------------------------------

class FireDroplet : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOCLIP
	}
	States
	{
	Spawn:
		FFOT ABCD 9 Bright;
		Stop;
	}
}
