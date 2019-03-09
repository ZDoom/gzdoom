
//===========================================================================
//
// Flemoidus Commonus
//
//===========================================================================

class FlemoidusCommonus : ZombieMan
{
	Default
	{
		DropItem "";
		Obituary "$OB_COMMONUS";
	}
	States
	{
		Missile:
			Stop;
		Melee:
			goto Super::Missile;
	}
}

//===========================================================================
//
// Flemoidus Bipedicus
//
//===========================================================================

class FlemoidusBipedicus : ShotgunGuy
{
	Default
	{
		DropItem "";
		Obituary "$OB_BIPEDICUS";
	}
	States
	{
		Missile:
			Stop;
		Melee:
			goto Super::Missile;
	}
}

//===========================================================================
//
// Flemoidus Bipedicus w/ Armor
//
//===========================================================================

class ArmoredFlemoidusBipedicus : DoomImp
{
	Default
	{
		Obituary "$OB_BIPEDICUS2";
		HitObituary "$OB_BIPEDICUS2";
	}
}

//===========================================================================
//
// Flemoidus Cycloptis Commonus
//
//===========================================================================

class FlemoidusCycloptisCommonus : Demon
{
	Default
	{
		Obituary "$OB_CYCLOPTIS";
	}
}

//===========================================================================
//
// The Flembrane
//
//===========================================================================

class Flembrane : BaronOfHell
{
	Default
	{
		radius 44;
		height 100;
		speed 0;
		Obituary "$OB_FLEMBRANE";
	}
	States
	{
		Missile:
			BOSS EF 3 A_FaceTarget;
			BOSS G 0 A_BruisAttack;
			goto See;
	}
}

//===========================================================================

class ChexSoul : LostSoul
{
	Default
	{
		height 0;
	}
}
