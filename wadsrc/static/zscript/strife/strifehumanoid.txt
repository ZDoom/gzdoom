
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
		BURN A 3 Bright A_PlaySound("human/imonfire", CHAN_VOICE);
		BURN B 3 Bright A_DropFire;
		BURN C 3 Bright A_Wander;
		BURN D 3 Bright A_NoBlocking;
		BURN E 5 Bright A_DropFire;
		BURN FGH 5 Bright A_Wander;
		BURN I 5 Bright A_DropFire;
		BURN JKL 5 Bright A_Wander;
		BURN M 5 Bright A_DropFire;
		BURN N 5 Bright;
		BURN OPQPQ 5 Bright;
		BURN RSTU 7 Bright;
		BURN V -1;
		Stop;
	Disintegrate:
		DISR A 5 A_PlaySound("misc/disruptordeath", CHAN_VOICE);
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
