class HereticPlayer : PlayerPawn
{
	Default
	{
		Health 100;
		Radius 16;
		Height 56;
		Mass 100;
		Painchance 255;
		Speed 1;
		Player.DisplayName "Corvus";
		Player.StartItem "GoldWand";
		Player.StartItem "Staff";
		Player.StartItem "GoldWandAmmo", 50;
		Player.WeaponSlot 1, "Staff", "Gauntlets";
		Player.WeaponSlot 2, "GoldWand";
		Player.WeaponSlot 3, "Crossbow";
		Player.WeaponSlot 4, "Blaster";
		Player.WeaponSlot 5, "SkullRod";
		Player.WeaponSlot 6, "PhoenixRod";
		Player.WeaponSlot 7, "Mace";

		Player.ColorRange 225, 240;
		Player.Colorset 0, "Green",			225, 240,  238;
		Player.Colorset 1, "Yellow",		114, 129,  127;
		Player.Colorset 2, "Red",			145, 160,  158;
		Player.Colorset 3, "Blue",			190, 205,  203;
		// Doom Legacy additions
		Player.Colorset 4, "Brown",			 67,  82,   80;
		Player.Colorset 5, "Light Gray",	  9,  24,   22;
		Player.Colorset 6, "Light Brown",	 74,  89,   87;
		Player.Colorset 7, "Light Red",		150, 165,  163;
		Player.Colorset 8, "Light Blue",	192, 207,  205;
		Player.Colorset 9, "Beige",			 95, 110,  108;
	}

	States
	{
	Spawn:
		PLAY A -1;
		Stop;
	See:
		PLAY ABCD 4;
		Loop;
	Melee:
	Missile:
		PLAY F 6 BRIGHT;
		PLAY E 12;
		Goto Spawn;
	Pain:
		PLAY G 4;
		PLAY G 4 A_Pain;
		Goto Spawn;
	Death:
		PLAY H 6 A_PlayerSkinCheck("AltSkinDeath");
		PLAY I 6 A_PlayerScream;
		PLAY JK 6;
		PLAY L 6 A_NoBlocking;
		PLAY MNO 6;
		PLAY P -1;
		Stop;
	XDeath:
		PLAY Q 0 A_PlayerSkinCheck("AltSkinXDeath");
		PLAY Q 5 A_PlayerScream;
		PLAY R 0 A_NoBlocking;
		PLAY R 5 A_SkullPop;
		PLAY STUVWX 5;
		PLAY Y -1;
		Stop;
	Burn:
		FDTH A 5 BRIGHT A_PlaySound("*burndeath");
		FDTH B 4 BRIGHT;
		FDTH C 5 BRIGHT;
		FDTH D 4 BRIGHT A_PlayerScream;
		FDTH E 5 BRIGHT;
		FDTH F 4 BRIGHT;
		FDTH G 5 BRIGHT A_PlaySound("*burndeath");
		FDTH H 4 BRIGHT;
		FDTH I 5 BRIGHT;
		FDTH J 4 BRIGHT;
		FDTH K 5 BRIGHT;
		FDTH L 4 BRIGHT;
		FDTH M 5 BRIGHT;
		FDTH N 4 BRIGHT;
		FDTH O 5 BRIGHT A_NoBlocking;
		FDTH P 4 BRIGHT;
		FDTH Q 5 BRIGHT;
		FDTH R 4 BRIGHT;
		ACLO E 35 A_CheckPlayerDone;
		Wait;
	AltSkinDeath:	
		PLAY H 10;
		PLAY I 10 A_PlayerScream;
		PLAY J 10 A_NoBlocking;
		PLAY KLM 10;
		PLAY N -1;
		Stop;
	AltSkinXDeath:
		PLAY O 5;
		PLAY P 5 A_XScream;
		PLAY Q 5 A_NoBlocking;
		PLAY RSTUV 5;
		PLAY W -1;
		Stop;
	}
}				

// The player's skull -------------------------------------------------------

class BloodySkull : PlayerChunk
{
	Default
	{
		Radius 4;
		Height 4;
		Gravity 0.125;
		+NOBLOCKMAP
		+DROPOFF
		+CANNOTPUSH
		+SKYEXPLODE
		+NOBLOCKMONST
		+NOSKIN
	}
	States
	{
	Spawn:
		BSKL A 0;
		BSKL ABCDE 5 A_CheckFloor("Hit");
		Goto Spawn+1;
	Hit:
		BSKL F 16 A_CheckPlayerDone;
		Wait;
	}
}

