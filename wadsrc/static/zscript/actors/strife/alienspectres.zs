
// Alien Spectre 1 -----------------------------------------------------------

class AlienSpectre1 : SpectralMonster
{
	Default
	{
		Health 1000;
		Painchance 250;
		Speed 12;
		Radius 64;
		Height 64;
		FloatSpeed 5;
		Mass 1000;
		MinMissileChance 150;
		RenderStyle "Translucent";
		Alpha 0.666;
		SeeSound "alienspectre/sight";
		AttackSound "alienspectre/blade";
		PainSound "alienspectre/pain";
		DeathSound "alienspectre/death";
		ActiveSound "alienspectre/active";
		Obituary "$OB_ALIENSPECTRE";
		+NOGRAVITY
		+FLOAT
		+SHADOW
		+NOTDMATCH
		+DONTMORPH
		+NOBLOCKMONST
		+INCOMBAT
		+LOOKALLAROUND
		+NOICEDEATH
	}

	States
	{
	Spawn:
		ALN1 A 10 A_Look;
		ALN1 B 10 A_SentinelBob;
		Loop;
	See:
		ALN1 AB 4 Bright A_Chase;
		ALN1 C 4 Bright A_SentinelBob;
		ALN1 DEF 4 Bright A_Chase;
		ALN1 G 4 Bright A_SentinelBob;
		ALN1 HIJ 4 Bright A_Chase;
		ALN1 K 4 Bright A_SentinelBob;
		Loop;
	Melee:
		ALN1 J 4 Bright A_FaceTarget;
		ALN1 I 4 Bright A_CustomMeleeAttack((random[SpectreMelee](0,255)&9)*5);
		ALN1 H 4 Bright;
		Goto See;
	Missile:
		ALN1 J 4 Bright A_FaceTarget;
		ALN1 I 4 Bright A_SpotLightning;
		ALN1 H 4 Bright;
		Goto See+10;
	Pain:
		ALN1 J 2 A_Pain;
		Goto See+6;
	Death:
		AL1P A 6 Bright A_SpectreChunkSmall;
		AL1P B 6 Bright A_Scream;
		AL1P C 6 Bright A_SpectreChunkSmall;
		AL1P DE 6 Bright;
		AL1P F 6 Bright A_SpectreChunkSmall;
		AL1P G 6 Bright;
		AL1P H 6 Bright A_SpectreChunkSmall;
		AL1P IJK 6 Bright;
		AL1P LM 5 Bright;
		AL1P N 5 Bright A_SpectreChunkLarge;
		AL1P OPQ 5 Bright;
		AL1P R 5 Bright A_AlienSpectreDeath;
		Stop;
	}
	
	//============================================================================

	void A_AlienSpectreDeath ()
	{
		PlayerPawn player = null;
		int log = 0;

		A_NoBlocking(); // [RH] Need this for Sigil rewarding
		if (!CheckBossDeath ())
		{
			return;
		}
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].health > 0)
			{
				player = players[i].mo;
				break;
			}
		}
		if (player == null)
		{
			return;
		}
		
		class<Actor> cls = GetClass();
		if (cls == "AlienSpectre1")
		{
			Floor_LowerToLowest(999, 8);
			log = 95;
		}
		else if (cls == "AlienSpectre2")
		{
			Console.MidPrint(null, "$TXT_KILLED_BISHOP");
			log = 74;
			player.GiveInventoryType ("QuestItem21");
		}
		else if (cls == "AlienSpectre3")
		{
			Console.MidPrint(null, "$TXT_KILLED_ORACLE");
			// If there are any Oracles still alive, kill them.
			ThinkerIterator it = ThinkerIterator.Create("Oracle");
			Actor oracle;

			while ( (oracle = Actor(it.Next())) != null)
			{
				if (oracle.health > 0)
				{
					oracle.health = 0;
					oracle.Die (self, self);
				}
			}
			player.GiveInventoryType ("QuestItem23");
			if (player.FindInventory ("QuestItem21"))
			{ // If the Bishop is dead, set quest item 22
				player.GiveInventoryType ("QuestItem22");
			}
			if (player.FindInventory ("QuestItem24") == null)
			{	// Macil is calling us back...
				log = 87;
			}
			else
			{	// You wield the power of the complete Sigil.
				log = 85;
			}
			Door_Open(222, 64);
		}
		else if (cls == "AlienSpectre4")
		{
			Console.MidPrint(null, "$TXT_KILLED_MACIL");
			player.GiveInventoryType ("QuestItem24");
			if (player.FindInventory ("QuestItem25") == null)
			{	// Richter has taken over. Macil is a snake.
				log = 79;
			}
			else
			{	// Back to the factory for another Sigil!
				log = 106;
			}
		}
		else if (cls == "AlienSpectre5")
		{
			Console.MidPrint(null, "$TXT_KILLED_LOREMASTER");

			player.GiveInventoryType ("QuestItem26");
			if (!multiplayer)
			{
				player.GiveInventoryType ("UpgradeStamina");
				player.GiveInventoryType ("UpgradeAccuracy");
			}
			Sigil sigl = Sigil(player.FindInventory("Sigil"));
			if (sigl != null && sigl.health == 5)
			{	// You wield the power of the complete Sigil.
				log = 85;
			}
			else
			{	// Another Sigil piece. Woohoo!
				log = 83;
			}
			Floor_LowerToLowest(666, 8);
		}
		if (log > 0)
		{
			String voc = "svox/voc" .. log;
			A_StartSound(voc, CHAN_VOICE, CHANF_DEFAULT, 1., ATTN_NONE);
			player.player.SetLogNumber (log);
			player.player.SetSubtitleNumber (log, voc);
		}
	}
	
}


// Alien Spectre 2 -----------------------------------------------------------

class AlienSpectre2 : AlienSpectre1
{
	Default
	{
		Health 1200;
		Painchance 50;
		Radius 24;
		DropItem "Sigil2";
	}
	States
	{
	Missile:
		ALN1 F 4 A_FaceTarget;
		ALN1 I 4 A_SpawnProjectile("SpectralLightningH3", 32, 0);
		ALN1 E 4;
		Goto See+10;
	}
}

// Alien Spectre 3 ----------------------------------------------------------
// This is the Oracle's personal spectre, so it's a little different.

class AlienSpectre3 : AlienSpectre1
{
	Default
	{
		Health 1500;
		Painchance 50;
		Radius 24;
		+SPAWNCEILING
		DropItem "Sigil3";
		DamageFactor "SpectralLow", 0;
	}
	States
	{
	Spawn:
		ALN1 ABCDEFGHIJK 5;
		Loop;
	See:
		ALN1 AB 5 A_Chase;
		ALN1 C 5 A_SentinelBob;
		ALN1 DEF 5 A_Chase;
		ALN1 G 5 A_SentinelBob;
		ALN1 HIJ 5 A_Chase;
		ALN1 K 5 A_SentinelBob;
		Loop;
	Melee:
		ALN1 J 4 A_FaceTarget;
		ALN1 I 4 A_CustomMeleeAttack((random[SpectreMelee](0,255)&9)*5);
		ALN1 C 4;
		Goto See+2;
	Missile:
		ALN1 F 4 A_FaceTarget;
		ALN1 I 4 A_Spectre3Attack;
		ALN1 E 4;
		Goto See+10;
	Pain:
		ALN1 J 2 A_Pain;
		Goto See+6;
	}
}	


// Alien Spectre 4 -----------------------------------------------------------

class AlienSpectre4 : AlienSpectre1
{
	Default
	{
		Health 1700;
		Painchance 50;
		Radius 24;
		DropItem "Sigil4";
	}
	States
	{
	Missile:
		ALN1 F 4 A_FaceTarget;
		ALN1 I 4 A_SpawnProjectile("SpectralLightningBigV2", 32, 0);
		ALN1 E 4;
		Goto See+10;
	}
}


// Alien Spectre 5 -----------------------------------------------------------

class AlienSpectre5 : AlienSpectre1
{
	Default
	{
		Health 2000;
		Painchance 50;
		Radius 24;
		DropItem "Sigil5";
	}
	States
	{
	Missile:
		ALN1 F 4 A_FaceTarget;
		ALN1 I 4 A_SpawnProjectile("SpectralLightningBigBall2", 32, 0);
		ALN1 E 4;
		Goto See+10;
	}
}

// Small Alien Chunk --------------------------------------------------------

class AlienChunkSmall : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOCLIP
	}
	States
	{
	Spawn:
		NODE ABCDEFG 6 Bright;
		Stop;
	}
}

// Large Alien Chunk --------------------------------------------------------

class AlienChunkLarge : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOCLIP
	}
	States
	{
	Spawn:
		MTHD ABCDEFGHIJK 5 Bright;
		Stop;
	}
}

