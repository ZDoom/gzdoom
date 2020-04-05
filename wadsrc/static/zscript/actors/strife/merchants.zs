// Base class for the merchants ---------------------------------------------

class Merchant : Actor
{
	Default
	{
		Health 10000000;
		PainChance 256;	// a merchant should always enter the pain state when getting hurt
		Radius 20;
		Height 56;
		Mass 5000;
		CrushPainSound "misc/pcrush";
		+SOLID
		+SHOOTABLE
		+NOTDMATCH
		+NOSPLASHALERT
		+NODAMAGE
	}
	States
	{
	Spawn:
		MRST A 10 A_Look2;
		Loop;
		MRLK A 30 A_ActiveSound;
		Loop;
		MRLK B 30;
		Loop;
		MRBD ABCDEDCB 4;
		MRBD A 5;
		MRBD F 6;
		Loop;
	See:
	Pain:
		MRPN A 1;
		MRPN A 2 A_AlertMonsters;
		MRPN B 3 A_Pain;
		MRPN C 3;
		MRPN D 9 Door_CloseWaitOpen(999, 64, 960);
		MRPN C 4;
		MRPN B 3;
		MRPN A 3 A_ClearSoundTarget;
		Goto Spawn;
	Yes:
		MRYS A 20;
		// Fall through	
	Greetings:
		MRGT ABCDEFGHI 5;
		Goto Spawn;
	No:
		MRNO AB 6;
		MRNO C 10;
		MRNO BA 6;
		Goto Greetings;
	}
}


// Weapon Smith -------------------------------------------------------------

class WeaponSmith : Merchant
{
	Default
	{
		PainSound "smith/pain";
		Tag "$TAG_WEAPONSMITH";
	}
}


// Bar Keep -----------------------------------------------------------------

class BarKeep : Merchant
{
	Default
	{
		Translation 4;
		PainSound "barkeep/pain";
		ActiveSound "barkeep/active";
		Tag "$TAG_BARKEEP";
	}
}


// Armorer ------------------------------------------------------------------

class Armorer : Merchant
{
	Default
	{
		Translation 5;
		PainSound "armorer/pain";
		Tag "$TAG_ARMORER";
	}
}


// Medic --------------------------------------------------------------------

class Medic : Merchant
{
	Default
	{
		Translation 6;
		PainSound "medic/pain";
		Tag "$TAG_MEDIC";
	}
}

