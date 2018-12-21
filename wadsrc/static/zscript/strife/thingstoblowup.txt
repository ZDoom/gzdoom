extend class Actor
{

	void A_Bang4Cloud()
	{
		double xo = random[Bang4Cloud](0, 3) * (10. / 64);
		double yo = random[Bang4Cloud](0, 3) * (10. / 64);
		Spawn("Bang4Cloud", Vec3Offset(xo, yo, 0.), ALLOW_REPLACE);
	}
	
	void A_GiveQuestItem(int questitem)
	{
		// Give one of these quest items to every player in the game
		if (questitem >= 0)
		{
			String itemname = "QuestItem" .. questitem;
			class<Inventory> item = itemname;
			if (item != null)
			{
				for (int i = 0; i < MAXPLAYERS; ++i)
				{
					if (playeringame[i])
					{
						players[i].mo.GiveInventoryType(item);
					}
				}
			}
		}

		String msgid = "$TXT_QUEST_" .. questitem;
		String msg = StringTable.Localize(msgid);

		if (msg != msgid)	// if both are identical there was no message of this name in the stringtable.
		{
			Console.MidPrint ("SmallFont", msg);
		}
	}
	
}

// A Cloud used for various explosions --------------------------------------
// This actor has no direct equivalent in strife. To create this, Strife
// spawned a spark and then changed its state to that of this explosion
// cloud. Weird.

class Bang4Cloud : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+ZDOOMTRANS
		RenderStyle "Add";
		VSpeed 1;
	}
	States
	{
	Spawn:
		BNG4 BCDEFGHIJKLMN 3 Bright;
		Stop;
	}
}

// Piston -------------------------------------------------------------------

class Piston : Actor
{
	Default
	{
		Health 100;
		Speed 16;
		Radius 20;
		Height 76;
		Mass 10000000;
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+FLOORCLIP
		+INCOMBAT
		DeathSound "misc/explosion";
	}
	States
	{
	Spawn:
		PSTN AB 8;
		Loop;
	Death:
		PSTN A 4 Bright A_Scream;
		PSTN B 4 Bright A_NoBlocking;
		PSTN C 4 Bright A_GiveQuestItem(16);
		PSTN D 4 Bright A_Bang4Cloud;
		PSTN E 4 Bright A_TossGib;
		PSTN F 4 Bright;
		PSTN G 4 Bright A_Bang4Cloud;
		PSTN H 4;
		PSTN I -1;
		Stop;
	}

}

// Computer -----------------------------------------------------------------

class Computer : Actor
{
	Default
	{
		Health 80;
		Speed 27;
		Radius 26;
		Height 128;
		Mass 10000000;
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+FLOORCLIP
		+INCOMBAT
		DeathSound "misc/explosion";
	}
	States
	{
	Spawn:
		SECR ABCD 4 Bright;
		Loop;
	Death:
		SECR E 5 Bright A_Bang4Cloud;
		SECR F 5 Bright A_NoBlocking;
		SECR G 5 Bright A_GiveQuestItem(27);
		SECR H 5 Bright A_TossGib;
		SECR I 5 Bright A_Bang4Cloud;
		SECR J 5;
		SECR K 5 A_Bang4Cloud;
		SECR L 5;
		SECR M 5 A_Bang4Cloud;
		SECR N 5;
		SECR O 5 A_Bang4Cloud;
		SECR P -1;
		Stop;
	}
}


// Power Crystal ------------------------------------------------------------

class PowerCrystal : Actor
{
	Default
	{
		Health 50;
		Speed 14;
		Radius 20;
		Height 16;
		Mass 99999999;
		+SOLID
		+SHOOTABLE
		+NOGRAVITY
		+NOBLOOD
		+FLOORCLIP
		DeathSound "misc/explosion";
		ActiveSound "misc/reactor";
	}

	States
	{
	Spawn:
		CRYS A 16 A_LoopActiveSound;
		CRYS B 5 A_LoopActiveSound;
		CRYS CDEF 4 A_LoopActiveSound;
		Loop;
	Death:
		BOOM A 0 Bright A_Scream;
		BOOM A 1 Bright A_Explode512;
		BOOM B 3 Bright A_GiveQuestItem(14);
		BOOM C 2 Bright A_LightGoesOut;
		BOOM D 3 Bright A_Bang4Cloud;
		BOOM EF 3 Bright;
		BOOM G 3 Bright A_Bang4Cloud;
		BOOM H 1 Bright A_Explode512;
		BOOM I 3 Bright;
		BOOM JKL 3 Bright A_Bang4Cloud;
		BOOM MN 3 Bright;
		BOOM O 3 Bright A_Bang4Cloud;
		BOOM PQRST 3 Bright;
		BOOM U 3 Bright A_ExtraLightOff;
		BOOM VWXY 3 Bright;
		Stop;
	}
	
	// PowerCrystal -------------------------------------------------------------------

	void A_ExtraLightOff()
	{
		if (target != NULL && target.player != NULL)
		{
			target.player.extralight = 0;
		}
	}

	void A_Explode512()
	{
		A_Explode(512, 512);
		if (target != NULL && target.player != NULL)
		{
			target.player.extralight = 5;
		}
		A_SetRenderStyle(1, STYLE_Add);
	}

	void A_LightGoesOut()
	{
		sector sec = CurSector;

		sec.Flags |= Sector.SECF_SILENTMOVE;
		sec.lightlevel = 0;
		// Do this right with proper checks instead of just hacking the floor height.
		Floor.CreateFloor(sec, Floor.floorLowerToLowest, null, 65536.);
		

		for (int i = 0; i < 8; ++i)
		{
			Actor foo = Spawn("Rubble1", Pos, ALLOW_REPLACE);
			if (foo != NULL)
			{
				int t = random[LightOut](0, 7);
				foo.Vel.X = t - random[LightOut](0, 15);
				t = random[LightOut](0, 7);
				foo.Vel.Y = t - random[LightOut](0, 15);
				foo.Vel.Z = random[LightOut](7, 10);
			}
		}
	}
	
}
