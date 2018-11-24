class ZWingedStatue : Actor
{
	Default
	{
		Radius 10;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		STTW A -1;
		Stop;
	}
}
	
class ZRock1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RCK1 A -1;
		Stop;
	}
}

class ZRock2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RCK2 A -1;
		Stop;
	}
}

class ZRock3 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RCK3 A -1;
		Stop;
	}
}

class ZRock4 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		RCK4 A -1;
		Stop;
	}
}

class ZChandelier : Actor
{
	Default
	{
		Radius 20;
		Height 60;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		CDLR ABC 4;
		Loop;
	}
}

class ZChandelierUnlit : Actor
{
	Default
	{
		Radius 20;
		Height 60;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		CDLR D -1;
		Stop;
	}
}

class ZTreeDead : Actor
{
	Default
	{
		Radius 10;
		Height 96;
		+SOLID
	}
	States
	{
	Spawn:
		ZTRE A -1;
		Stop;
	}
}

class ZTree : Actor
{
	Default
	{
		Radius 15;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		ZTRE A -1;
		Stop;
	}
}

class ZTreeSwamp150 : Actor
{
	Default
	{
		Radius 10;
		Height 150;
		+SOLID
	}
	States
	{
	Spawn:
		TRES A -1;
		Stop;
	}
}

class ZTreeSwamp120 : Actor
{
	Default
	{
		Radius 10;
		Height 120;
		+SOLID
	}
	States
	{
	Spawn:
		TRE3 A -1;
		Stop;
	}
}

class ZStumpBurned : Actor
{
	Default
	{
		Radius 12;
		Height 20;
		+SOLID
	}
	States
	{
	Spawn:
		STM1 A -1;
		Stop;
	}
}

class ZStumpBare : Actor
{
	Default
	{
		Radius 12;
		Height 20;
		+SOLID
	}
	States
	{
	Spawn:
		STM2 A -1;
		Stop;
	}
}

class ZStumpSwamp1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		STM3 A -1;
		Stop;
	}
}

class ZStumpSwamp2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		STM4 A -1;
		Stop;
	}
}

class ZShroomLarge1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH1 A -1;
		Stop;
	}
}

class ZShroomLarge2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH2 A -1;
		Stop;
	}
}

class ZShroomLarge3 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH3 A -1;
		Stop;
	}
}

class ZShroomSmall1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH4 A -1;
		Stop;
	}
}

class ZShroomSmall2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH5 A -1;
		Stop;
	}
}

class ZShroomSmall3 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH6 A -1;
		Stop;
	}
}

class ZShroomSmall4 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH7 A -1;
		Stop;
	}
}

class ZShroomSmall5 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		MSH8 A -1;
		Stop;
	}
}

class ZStalagmitePillar : Actor
{
	Default
	{
		Radius 8;
		Height 138;
		+SOLID
	}
	States
	{
	Spawn:
		SGMP A -1;
		Stop;
	}
}

class ZStalagmiteLarge : Actor
{
	Default
	{
		Radius 8;
		Height 48;
		+SOLID
	}
	States
	{
	Spawn:
		SGM1 A -1;
		Stop;
	}
}

class ZStalagmiteMedium : Actor
{
	Default
	{
		Radius 6;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		SGM2 A -1;
		Stop;
	}
}

class ZStalagmiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 36;
		+SOLID
	}
	States
	{
	Spawn:
		SGM3 A -1;
		Stop;
	}
}

class ZStalactiteLarge : Actor
{
	Default
	{
		Radius 8;
		Height 66;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SLC1 A -1;
		Stop;
	}
}

class ZStalactiteMedium : Actor
{
	Default
	{
		Radius 6;
		Height 50;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SLC2 A -1;
		Stop;
	}
}

class ZStalactiteSmall : Actor
{
	Default
	{
		Radius 8;
		Height 40;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		SLC3 A -1;
		Stop;
	}
}

class ZMossCeiling1 : Actor
{
	Default
	{
		Radius 20;
		Height 20;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		MSS1 A -1;
		Stop;
	}
}

class ZMossCeiling2 : Actor
{
	Default
	{
		Radius 20;
		Height 24;
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		MSS2 A -1;
		Stop;
	}
}

class ZSwampVine : Actor
{
	Default
	{
		Radius 8;
		Height 52;
		+SOLID
	}
	States
	{
	Spawn:
		SWMV A -1;
		Stop;
	}
}

class ZCorpseKabob : Actor
{
	Default
	{
		Radius 10;
		Height 92;
		+SOLID
	}
	States
	{
	Spawn:
		CPS1 A -1;
		Stop;
	}
}

class ZCorpseSleeping : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		CPS2 A -1;
		Stop;
	}
}

class ZTombstoneRIP : Actor
{
	Default
	{
		Radius 10;
		Height 46;
		+SOLID
	}
	States
	{
	Spawn:
		TMS1 A -1;
		Stop;
	}
}

class ZTombstoneShane : Actor
{
	Default
	{
		Radius 10;
		Height 46;
		+SOLID
	}
	States
	{
	Spawn:
		TMS2 A -1;
		Stop;
	}
}

class ZTombstoneBigCross : Actor
{
	Default
	{
		Radius 10;
		Height 46;
		+SOLID
	}
	States
	{
	Spawn:
		TMS3 A -1;
		Stop;
	}
}

class ZTombstoneBrianR : Actor
{
	Default
	{
		Radius 10;
		Height 52;
		+SOLID
	}
	States
	{
	Spawn:
		TMS4 A -1;
		Stop;
	}
}

class ZTombstoneCrossCircle : Actor
{
	Default
	{
		Radius 10;
		Height 52;
		+SOLID
	}
	States
	{
	Spawn:
		TMS5 A -1;
		Stop;
	}
}

class ZTombstoneSmallCross : Actor
{
	Default
	{
		Radius 8;
		Height 46;
		+SOLID
	}
	States
	{
	Spawn:
		TMS6 A -1;
		Stop;
	}
}

class ZTombstoneBrianP : Actor
{
	Default
	{
		Radius 8;
		Height 46;
		+SOLID
	}
	States
	{
	Spawn:
		TMS7 A -1;
		Stop;
	}
}

class ZCorpseHanging : Actor
{
	Default
	{
		Radius 6;
		Height 75;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		CPS3 A -1;
		Stop;
	}
}

class ZStatueGargoyleGreenTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		STT2 A -1;
		Stop;
	}
}

class ZStatueGargoyleBlueTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		STT3 A -1;
		Stop;
	}
}

class ZStatueGargoyleGreenShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		STT4 A -1;
		Stop;
	}
}

class ZStatueGargoyleBlueShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		STT5 A -1;
		Stop;
	}
}

class ZStatueGargoyleStripeTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		GAR1 A -1;
		Stop;
	}
}

class ZStatueGargoyleDarkRedTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		GAR2 A -1;
		Stop;
	}
}

class ZStatueGargoyleRedTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		GAR3 A -1;
		Stop;
	}
}

class ZStatueGargoyleTanTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		GAR4 A -1;
		Stop;
	}
}

class ZStatueGargoyleRustTall : Actor
{
	Default
	{
		Radius 14;
		Height 108;
		+SOLID
	}
	States
	{
	Spawn:
		GAR5 A -1;
		Stop;
	}
}

class ZStatueGargoyleDarkRedShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		GAR6 A -1;
		Stop;
	}
}

class ZStatueGargoyleRedShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		GAR7 A -1;
		Stop;
	}
}

class ZStatueGargoyleTanShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		GAR8 A -1;
		Stop;
	}
}

class ZStatueGargoyleRustShort : Actor
{
	Default
	{
		Radius 14;
		Height 62;
		+SOLID
	}
	States
	{
	Spawn:
		GAR9 A -1;
		Stop;
	}
}

class ZBannerTattered : Actor
{
	Default
	{
		Radius 8;
		Height 120;
		+SOLID
	}
	States
	{
	Spawn:
		BNR1 A -1;
		Stop;
	}
}

class ZTreeLarge1 : Actor
{
	Default
	{
		Radius 15;
		Height 180;
		+SOLID
	}
	States
	{
	Spawn:
		TRE4 A -1;
		Stop;
	}
}

class ZTreeLarge2 : Actor
{
	Default
	{
		Radius 15;
		Height 180;
		+SOLID
	}
	States
	{
	Spawn:
		TRE5 A -1;
		Stop;
	}
}

class ZTreeGnarled1 : Actor
{
	Default
	{
		Radius 22;
		Height 100;
		+SOLID
	}
	States
	{
	Spawn:
		TRE6 A -1;
		Stop;
	}
}

class ZTreeGnarled2 : Actor
{
	Default
	{
		Radius 22;
		Height 100;
		+SOLID
	}
	States
	{
	Spawn:
		TRE7 A -1;
		Stop;
	}
}

class ZLog : Actor
{
	Default
	{
		Radius 20;
		Height 25;
		+SOLID
	}
	States
	{
	Spawn:
		LOGG A -1;
		Stop;
	}
}

class ZStalactiteIceLarge : Actor
{
	Default
	{
		Radius 8;
		Height 66;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		ICT1 A -1;
		Stop;
	}
}

class ZStalactiteIceMedium : Actor
{
	Default
	{
		Radius 5;
		Height 50;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		ICT2 A -1;
		Stop;
	}
}

class ZStalactiteIceSmall : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		ICT3 A -1;
		Stop;
	}
}

class ZStalactiteIceTiny : Actor
{
	Default
	{
		Radius 4;
		Height 8;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		ICT4 A -1;
		Stop;
	}
}

class ZStalagmiteIceLarge : Actor
{
	Default
	{
		Radius 8;
		Height 66;
		+SOLID
	}
	States
	{
	Spawn:
		ICM1 A -1;
		Stop;
	}
}

class ZStalagmiteIceMedium : Actor
{
	Default
	{
		Radius 5;
		Height 50;
		+SOLID
	}
	States
	{
	Spawn:
		ICM2 A -1;
		Stop;
	}
}

class ZStalagmiteIceSmall : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		ICM3 A -1;
		Stop;
	}
}

class ZStalagmiteIceTiny : Actor
{
	Default
	{
		Radius 4;
		Height 8;
		+SOLID
	}
	States
	{
	Spawn:
		ICM4 A -1;
		Stop;
	}
}

class ZRockBrown1 : Actor
{
	Default
	{
		Radius 17;
		Height 72;
		+SOLID
	}
	States
	{
	Spawn:
		RKBL A -1;
		Stop;
	}
}

class ZRockBrown2 : Actor
{
	Default
	{
		Radius 15;
		Height 50;
		+SOLID
	}
	States
	{
	Spawn:
		RKBS A -1;
		Stop;
	}
}

class ZRockBlack : Actor
{
	Default
	{
		Radius 20;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		RKBK A -1;
		Stop;
	}
}

class ZRubble1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RBL1 A -1;
		Stop;
	}
}

class ZRubble2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RBL2 A -1;
		Stop;
	}
}

class ZRubble3 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		RBL3 A -1;
		Stop;
	}
}

class ZVasePillar : Actor
{
	Default
	{
		Radius 12;
		Height 54;
		+SOLID
	}
	States
	{
	Spawn:
		VASE A -1;
		Stop;
	}
}

class ZCorpseLynched : Actor
{
	Default
	{
		Radius 11;
		Height 95;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		CPS4 A -1;
		Stop;
	}
}

class ZCandle : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CNDL ABC 4 Bright;
		Loop;
	}
}

class ZBarrel : Actor
{
	Default
	{
		Radius 15;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		ZBAR A -1;
		Stop;
	}
}

class ZBucket : Actor
{
	Default
	{
		Radius 8;
		Height 72;
		+SOLID
		+SPAWNCEILING
		+NOGRAVITY
	}
	States
	{
	Spawn:
		BCKT A -1;
		Stop;
	}
}

class FireThing : Actor
{
	Default
	{
		Radius 5;
		Height 10;
		+SOLID
	}
	States
	{
	Spawn:
		FSKL A 4 Bright;
		FSKL B 3 Bright;
		FSKL C 4 Bright;
		FSKL D 3 Bright;
		FSKL E 4 Bright;
		FSKL F 3 Bright;
		FSKL G 4 Bright;
		FSKL H 3 Bright;
		FSKL I 4 Bright;
		Loop;
	}
}

class BrassTorch : Actor
{
	Default
	{
		Radius 6;
		Height 35;
		+SOLID
	}
	States
	{
	Spawn:
		BRTR ABCDEFGHIJKLM 4 Bright;
		Loop;
	}
}

class ZBlueCandle : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		BCAN ABCDE 5 Bright;
		Loop;
	}
}

class ZIronMaiden : Actor
{
	Default
	{
		Radius 12;
		Height 60;
		+SOLID
	}
	States
	{
	Spawn:
		IRON A -1;
		Stop;
	}
}

class ZChainBit32 : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS A -1;
		Stop;
	}
}

class ZChainBit64 : Actor
{
	Default
	{
		Radius 4;
		Height 64;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS B -1;
		Stop;
	}
}

class ZChainEndHeart : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS C -1;
		Stop;
	}
}

class ZChainEndHook1 : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS D -1;
		Stop;
	}
}

class ZChainEndHook2 : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS E -1;
		Stop;
	}
}

class ZChainEndSpike : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS F -1;
		Stop;
	}
}

class ZChainEndSkull : Actor
{
	Default
	{
		Radius 4;
		Height 32;
		+SPAWNCEILING
		+NOGRAVITY
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		CHNS G -1;
		Stop;
	}
}

class TableShit1 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST1 A -1;
		Stop;
	}
}

class TableShit2 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST2 A -1;
		Stop;
	}
}

class TableShit3 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST3 A -1;
		Stop;
	}
}

class TableShit4 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST4 A -1;
		Stop;
	}
}

class TableShit5 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST5 A -1;
		Stop;
	}
}

class TableShit6 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST6 A -1;
		Stop;
	}
}

class TableShit7 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST7 A -1;
		Stop;
	}
}

class TableShit8 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST8 A -1;
		Stop;
	}
}

class TableShit9 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST9 A -1;
		Stop;
	}
}

class TableShit10 : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TST0 A -1;
		Stop;
	}
}

class TeleSmoke : Actor
{
	Default
	{
		Radius 20;
		Height 16;
		+NOGRAVITY
		+NOBLOCKMAP
		RenderStyle "Translucent";
		Alpha 0.6;
	}
	States
	{
	Spawn:
		TSMK A 4;
		TSMK B 3;
		TSMK C 4;
		TSMK D 3;
		TSMK E 4;
		TSMK F 3;
		TSMK G 4;
		TSMK H 3;
		TSMK I 4;
		TSMK J 3;
		TSMK K 4;
		TSMK L 3;
		TSMK M 4;
		TSMK N 3;
		TSMK O 4;
		TSMK P 3;
		TSMK Q 4;
		TSMK R 3;
		TSMK S 4;
		TSMK T 3;
		TSMK U 4;
		TSMK V 3;
		TSMK W 4;
		TSMK X 3;
		TSMK Y 4;
		TSMK Z 3;
		Loop;
	}
}

