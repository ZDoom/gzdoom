//===========================================================================
//
// Commander Keen
//
//===========================================================================
class CommanderKeen : Actor
{
	Default
	{
		Health 100;
		Radius 16;
		Height 72;
		Mass 10000000;
		PainChance 256;
		+SOLID 
		+SPAWNCEILING 
		+NOGRAVITY 
		+SHOOTABLE 
		+COUNTKILL 
		+NOICEDEATH
		+ISMONSTER
		PainSound "keen/pain";
		DeathSound "keen/death";
	}
	States
	{
	Spawn:
		KEEN A -1;
		Loop;
	Death:
		KEEN AB	6;
		KEEN C 6 A_Scream;
		KEEN DEFGH	6;
		KEEN I 6;
		KEEN J 6;
		KEEN K 6 A_KeenDie;
		KEEN L -1;
		Stop;
	Pain:
		KEEN M	4;
		KEEN M	8 A_Pain;
		Goto Spawn;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{

	//
	// A_KeenDie
	// DOOM II special, map 32.
	// Uses special tag 666 by default.
	//

	void A_KeenDie(int doortag = 666)
	{
		A_NoBlocking(false);

		// scan the remaining thinkers to see if all Keens are dead
		ThinkerIterator it = ThinkerIterator.Create(GetClass());
		Actor mo;
		while (mo = Actor(it.Next(true)))
		{
			if (mo.health > 0 && mo != self)
			{
				// Added check for Dehacked and repurposed inventory items.
				let inv = Inventory(mo);
				if (inv == null || inv.Owner == null)
				{
					// other Keen not dead
					return;
				}
			}
		}
		Door_Open(doortag, 16);
	}
}


