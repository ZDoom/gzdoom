
// CTC flag spots. They are not functional and only here so that something gets spawned for them.

class SVEFlagSpotRed : Inventory
{
	States
	{
	Spawn:
		FLGR A 1 BRIGHT;
		FLGR A 0 BRIGHT; // A_FlagStandSpawn
		FLGR A 1 BRIGHT; // A_FlagStandCheck
		Wait;
	}
}

class SVEFlagSpotBlue : Inventory
{
	States
	{
	Spawn:
		FLGB A 1 BRIGHT;
		FLGB A 0 BRIGHT; // A_FlagStandSpawn
		FLGB A 1 BRIGHT; // A_FlagStandCheck
		Wait;
	}
}

class SVEBlueChalice : Inventory
{
	Default
	{
		+DROPPED
		+FLOORCLIP
		+INVENTORY.INVBAR
		Radius 10;
		Height 16;
		Tag "$TAG_OFFERINGCHALICE";
		Inventory.Icon "I_RELB";
		Inventory.PickupMessage "$TXT_OFFERINGCHALICE";
	}
	States
	{
	Spawn:
		RELB A -1 BRIGHT;
		Stop;
	}
}

class SVETalismanPowerup : Inventory
{
}

class SVETalismanRed : Inventory
{
	Default
	{
		+DROPPED
		+FLOORCLIP
		+INVENTORY.INVBAR
		Radius 10;
		Height 16;
		Inventory.MaxAmount 1;
		Inventory.Icon "I_FLGR";
		Tag "$TAG_TALISMANRED";
		Inventory.PickupMessage "$MSG_TALISMANRED";
	}
	States
	{
	Spawn:
		FLGR A -1 BRIGHT;
		Stop;
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		let useok = Super.TryPickup (toucher);
		if (useok)
		{
			if (toucher.FindInventory("SVETalismanRed") && 
				toucher.FindInventory("SVETalismanGreen") && 
				toucher.FindInventory("SVETalismanBlue"))
			{
				toucher.A_Print("$MSG_TALISMANPOWER");
				toucher.GiveInventoryType("SVETalismanPowerup");
			}
		}
		return useok;
	}
	
}

class SVETalismanBlue : SVETalismanRed
{
	Default
	{
		+INVENTORY.INVBAR
		Inventory.Icon "I_FLGB";
		Tag "$TAG_TALISMANBLUE";
		Inventory.PickupMessage "$MSG_TALISMANBLUE";
	}
	States
	{
	Spawn:
		FLGB A -1 BRIGHT;
		Stop;
	}
}

class SVETalismanGreen : SVETalismanRed
{
	Default
	{
		+INVENTORY.INVBAR
		Inventory.Icon "I_FLGG";
		Tag "$TAG_TALISMANGREEN";
		Inventory.PickupMessage "$MSG_TALISMANGREEN";
	}
	States
	{
	Spawn:
		FLGG A -1 BRIGHT;
		Stop;
	}
}

class SVEOreSpawner : Actor
{
	Default
	{
		+NOSECTOR
	}
	States
	{
	Spawn:
		TNT1 A 175 A_OreSpawner;
		loop;
	}
	
	//
	// A_OreSpawner
	//
	// [SVE] svillarreal
	//

	void A_OreSpawner()
	{
		if(deathmatch) return;

		bool inrange = false;
		for(int i = 0; i < MAXPLAYERS; i++)
		{
			if(!playeringame[i])
				continue;

			if(Distance2D(players[i].mo) < 2048)
			{
				inrange = true;
				break;
			}
		}
		if(!inrange) return;

		let it = ThinkerIterator.Create("DegninOre");
		Thinker ac;
		
		int numores = 0;
		while (ac = it.Next())
		{
			if (++numores == 3) return;
		}
		Spawn("DegninOre", Pos);
	}
}

class SVEOpenDoor225 : DummyStrifeItem 
{
	override bool TryPickup (in out Actor toucher)
	{
		Door_Open(225, 16);
		GoAwayAndDie ();
		return true;
	}
	
	override bool SpecialDropAction (Actor dropper)
	{
		Door_Open(225, 16);
		Destroy ();
		return true;
	}
	
}
