/***************************************************************************/
//
// shown for respawning Doom and Strife items
//
/***************************************************************************/

class ItemFog : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
	}
	States
	{
	Spawn:
		IFOG ABABCDE 6 BRIGHT;
		Stop;
	}
}

// Pickup flash -------------------------------------------------------------

class PickupFlash : Actor
{
	default
	{
		+NOGRAVITY
	}
	States
	{
	Spawn:
		ACLO DCDCBCBABA 3;
		Stop;
	}
}

