#include "actor.h"
#include "info.h"

// Convenient macros --------------------------------------------------------

#define _DECCOMMON(cls,ednum,rad,hi,ns) \
	class cls : public AActor { DECLARE_STATELESS_ACTOR (cls, AActor) static FState States[ns]; }; \
	IMPLEMENT_ACTOR (cls, Hexen, ednum, 0) \
		PROP_SpawnState (0) \
		PROP_RadiusFixed (rad) \
		PROP_HeightFixed (hi)

#define _DECSTARTSTATES(cls,ns) \
	END_DEFAULTS  FState cls::States[ns] =

#define DEC(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_SOLID) \
	_DECSTARTSTATES(cls,ns)

#define DECNS(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) _DECSTARTSTATES(cls,ns)

#define DECHANG(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY) \
	_DECSTARTSTATES(cls,ns)

#define DECHANGNB(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_NOBLOCKMAP|MF_SPAWNCEILING|MF_NOGRAVITY) \
	_DECSTARTSTATES(cls,ns)

#define DECHANGNS(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_SPAWNCEILING|MF_NOGRAVITY) \
	_DECSTARTSTATES(cls,ns)

#define DECNBLOCK(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_NOBLOCKMAP) \
	_DECSTARTSTATES(cls,ns)

#define DECNBNG(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY) \
	_DECSTARTSTATES(cls,ns)

#define DECNBNGT(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns) \
		PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY) \
		PROP_RenderStyle (STYLE_Translucent) \
		PROP_Alpha (OPAQUE*8/10) \
	_DECSTARTSTATES(cls,ns)

// Definitions --------------------------------------------------------------

DEC (AZWingedStatue, 5, 10, 62, 1)
{
	S_NORMAL (STTW, 'A', -1, NULL, NULL)
};

DECNS (AZRock1, 6, 20, 16, 1)
{
	S_NORMAL (RCK1, 'A', -1, NULL, NULL)
};

DECNS (AZRock2, 7, 20, 16, 1)
{
	S_NORMAL (RCK2, 'A', -1, NULL, NULL)
};

DECNS (AZRock3, 9, 20, 16, 1)
{
	S_NORMAL (RCK3, 'A', -1, NULL, NULL)
};

DECNS (AZRock4, 15, 20, 16, 1)
{
	S_NORMAL (RCK4, 'A', -1, NULL, NULL)
};

DECHANGNS (AZChandelier, 17, 20, 60, 3)
{
	S_NORMAL (CDLR, 'A',  4, NULL, &States[1]),
	S_NORMAL (CDLR, 'B',  4, NULL, &States[2]),
	S_NORMAL (CDLR, 'C',  4, NULL, &States[0]),
};

DECHANGNS (AZChandelierUnlit, 8063, 20, 60, 1)
{
	S_NORMAL (CDLR, 'D', -1, NULL, NULL)
};

DEC (AZTreeDead, 24, 10, 96, 1)
{
	S_NORMAL (ZTRE, 'A', -1, NULL, NULL)
};

DEC (AZTree, 25, 15, 128, 1)
{
	S_NORMAL (ZTRE, 'A', -1, NULL, NULL)
};

DEC (AZTreeSwamp150, 26, 10, 150, 1)
{
	S_NORMAL (TRES, 'A', -1, NULL, NULL)
};

DEC (AZTreeSwamp120, 27, 10, 120, 1)
{
	S_NORMAL (TRE3, 'A', -1, NULL, NULL)
};

DEC (AZStumpBurned, 28, 12, 20, 1)
{
	S_NORMAL (STM1, 'A', -1, NULL, NULL)
};

DEC (AZStumpBare, 29, 12, 20, 1)
{
	S_NORMAL (STM2, 'A', -1, NULL, NULL)
};

DECNS (AZStumpSwamp1, 37, 20, 16, 1)
{
	S_NORMAL (STM3, 'A', -1, NULL, NULL)
};

DECNS (AZStumpSwamp2, 38, 20, 16, 1)
{
	S_NORMAL (STM4, 'A', -1, NULL, NULL)
};

DECNS (AZShroomLarge1, 39, 20, 16, 1)
{
	S_NORMAL (MSH1, 'A', -1, NULL, NULL)
};

DECNS (AZShroomLarge2, 40, 20, 16, 1)
{
	S_NORMAL (MSH2, 'A', -1, NULL, NULL)
};

DECNS (AZShroomLarge3, 41, 20, 16, 1)
{
	S_NORMAL (MSH3, 'A', -1, NULL, NULL)
};

DECNS (AZShroomSmall1, 42, 20, 16, 1)
{
	S_NORMAL (MSH4, 'A', -1, NULL, NULL)
};

DECNS (AZShroomSmall2, 44, 20, 16, 1)
{
	S_NORMAL (MSH5, 'A', -1, NULL, NULL)
};

DECNS (AZShroomSmall3, 45, 20, 16, 1)
{
	S_NORMAL (MSH6, 'A', -1, NULL, NULL)
};

DECNS (AZShroomSmall4, 46, 20, 16, 1)
{
	S_NORMAL (MSH7, 'A', -1, NULL, NULL)
};

DECNS (AZShroomSmall5, 47, 20, 16, 1)
{
	S_NORMAL (MSH8, 'A', -1, NULL, NULL)
};

DEC (AZStalagmitePillar, 48, 8, 138, 1)
{
	S_NORMAL (SGMP, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteLarge, 49, 8, 48, 1)
{
	S_NORMAL (SGM1, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteMedium, 50, 6, 40, 1)
{
	S_NORMAL (SGM2, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteSmall, 51, 8, 36, 1)
{
	S_NORMAL (SGM3, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteLarge, 52, 8, 66, 1)
{
	S_NORMAL (SLC1, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteMedium, 56, 6, 50, 1)
{
	S_NORMAL (SLC2, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteSmall, 57, 8, 40, 1)
{
	S_NORMAL (SLC3, 'A', -1, NULL, NULL)
};

DECHANGNS (AZMossCeiling1, 58, 20, 20, 1)
{
	S_NORMAL (MSS1, 'A', -1, NULL, NULL)
};

DECHANGNS (AZMossCeiling2, 59, 20, 24, 1)
{
	S_NORMAL (MSS2, 'A', -1, NULL, NULL)
};

DEC (AZSwampVine, 60, 8, 52, 1)
{
	S_NORMAL (SWMV, 'A', -1, NULL, NULL)
};

DEC (AZCorpseKabob, 61, 10, 92, 1)
{
	S_NORMAL (CPS1, 'A', -1, NULL, NULL)
};

DECNS (AZCorpseSleeping, 62, 20, 16, 1)
{
	S_NORMAL (CPS2, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneRIP, 63, 10, 46, 1)
{
	S_NORMAL (TMS1, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneShane, 64, 10, 46, 1)
{
	S_NORMAL (TMS2, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneBigCross, 65, 10, 46, 1)
{
	S_NORMAL (TMS3, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneBrianR, 66, 10, 52, 1)
{
	S_NORMAL (TMS4, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneCrossCircle, 67, 10, 52, 1)
{
	S_NORMAL (TMS5, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneSmallCross, 68, 8, 46, 1)
{
	S_NORMAL (TMS6, 'A', -1, NULL, NULL)
};

DEC (AZTombstoneBrianP, 69, 8, 46, 1)
{
	S_NORMAL (TMS7, 'A', -1, NULL, NULL)
};

DECHANG (AZCorpseHanging, 71, 6, 75, 1)
{
	S_NORMAL (CPS3, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleGreenTall, 72, 14, 108, 1)
{
	S_NORMAL (STT2, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleBlueTall, 73, 14, 108, 1)
{
	S_NORMAL (STT3, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleGreenShort, 74, 14, 62, 1)
{
	S_NORMAL (STT4, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleBlueShort, 76, 14, 62, 1)
{
	S_NORMAL (STT5, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleStripeTall, 8044, 14, 108, 1)
{
	S_NORMAL (GAR1, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleDarkRedTall, 8045, 14, 108, 1)
{
	S_NORMAL (GAR2, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleRedTall, 8046, 14, 108, 1)
{
	S_NORMAL (GAR3, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleTanTall, 8047, 14, 108, 1)
{
	S_NORMAL (GAR4, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleRustTall, 8048, 14, 108, 1)
{
	S_NORMAL (GAR5, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleDarkRedShort, 8049, 14, 62, 1)
{
	S_NORMAL (GAR6, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleRedShort, 8050, 14, 62, 1)
{
	S_NORMAL (GAR7, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleTanShort, 8051, 14, 62, 1)
{
	S_NORMAL (GAR8, 'A', -1, NULL, NULL)
};

DEC (AZStatueGargoyleRustShort, 8052, 14, 62, 1)
{
	S_NORMAL (GAR9, 'A', -1, NULL, NULL)
};

DEC (AZBannerTattered, 77, 8, 120, 1)
{
	S_NORMAL (BNR1, 'A', -1, NULL, NULL)
};

DEC (AZTreeLarge1, 78, 15, 180, 1)
{
	S_NORMAL (TRE4, 'A', -1, NULL, NULL)
};

DEC (AZTreeLarge2, 79, 15, 180, 1)
{
	S_NORMAL (TRE5, 'A', -1, NULL, NULL)
};

DEC (AZTreeGnarled1, 80, 22, 100, 1)
{
	S_NORMAL (TRE6, 'A', -1, NULL, NULL)
};

DEC (AZTreeGnarled2, 87, 22, 100, 1)
{
	S_NORMAL (TRE7, 'A', -1, NULL, NULL)
};

DEC (AZLog, 88, 20, 25, 1)
{
	S_NORMAL (LOGG, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteIceLarge, 89, 8, 66, 1)
{
	S_NORMAL (ICT1, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteIceMedium, 90, 5, 50, 1)
{
	S_NORMAL (ICT2, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteIceSmall, 91, 4, 32, 1)
{
	S_NORMAL (ICT3, 'A', -1, NULL, NULL)
};

DECHANG (AZStalactiteIceTiny, 92, 4, 8, 1)
{
	S_NORMAL (ICT4, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteIceLarge, 93, 8, 66, 1)
{
	S_NORMAL (ICM1, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteIceMedium, 94, 5, 50, 1)
{
	S_NORMAL (ICM2, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteIceSmall, 95, 4, 32, 1)
{
	S_NORMAL (ICM3, 'A', -1, NULL, NULL)
};

DEC (AZStalagmiteIceTiny, 96, 4, 8, 1)
{
	S_NORMAL (ICM4, 'A', -1, NULL, NULL)
};

DEC (AZRockBrown1, 97, 17, 72, 1)
{
	S_NORMAL (RKBL, 'A', -1, NULL, NULL)
};

DEC (AZRockBrown2, 98, 15, 50, 1)
{
	S_NORMAL (RKBS, 'A', -1, NULL, NULL)
};

DEC (AZRockBlack, 99, 20, 40, 1)
{
	S_NORMAL (RKBK, 'A', -1, NULL, NULL)
};

DECNS (AZRubble1, 100, 20, 16, 1)
{
	S_NORMAL (RBL1, 'A', -1, NULL, NULL)
};

DECNS (AZRubble2, 101, 20, 16, 1)
{
	S_NORMAL (RBL2, 'A', -1, NULL, NULL)
};

DECNS (AZRubble3, 102, 20, 16, 1)
{
	S_NORMAL (RBL3, 'A', -1, NULL, NULL)
};

DEC (AZVasePillar, 103, 12, 54, 1)
{
	S_NORMAL (VASE, 'A', -1, NULL, NULL)
};

DECHANG (AZCorpseLynched, 108, 11, 95, 1)
{
	S_NORMAL (CPS4, 'A', -1, NULL, NULL)
};

DECNBNG (AZCandle, 119, 20, 16, 3)
{
	S_BRIGHT (CNDL, 'A',  4, NULL, &States[1]),
	S_BRIGHT (CNDL, 'B',  4, NULL, &States[2]),
	S_BRIGHT (CNDL, 'C',  4, NULL, &States[0])
};

DEC (AZBarrel, 8100, 15, 32, 1)
{
	S_NORMAL (ZBAR, 'A', -1, NULL, NULL)
};

DECHANG (AZBucket, 8103, 8, 72, 1)
{
	S_NORMAL (BCKT, 'A', -1, NULL, NULL)
};

DEC (AFireThing, 8060, 5, 10, 9)
{
	S_BRIGHT (FSKL, 'A',  4, NULL, &States[1]),
	S_BRIGHT (FSKL, 'B',  3, NULL, &States[2]),
	S_BRIGHT (FSKL, 'C',  4, NULL, &States[3]),
	S_BRIGHT (FSKL, 'D',  3, NULL, &States[4]),
	S_BRIGHT (FSKL, 'E',  4, NULL, &States[5]),
	S_BRIGHT (FSKL, 'F',  3, NULL, &States[6]),
	S_BRIGHT (FSKL, 'G',  4, NULL, &States[7]),
	S_BRIGHT (FSKL, 'H',  3, NULL, &States[8]),
	S_BRIGHT (FSKL, 'I',  4, NULL, &States[0])
};

DEC (ABrassTorch, 8061, 6, 35, 13)
{
	S_BRIGHT (BRTR, 'A',  4, NULL, &States[1]),
	S_BRIGHT (BRTR, 'B',  4, NULL, &States[2]),
	S_BRIGHT (BRTR, 'C',  4, NULL, &States[3]),
	S_BRIGHT (BRTR, 'D',  4, NULL, &States[4]),
	S_BRIGHT (BRTR, 'E',  4, NULL, &States[5]),
	S_BRIGHT (BRTR, 'F',  4, NULL, &States[6]),
	S_BRIGHT (BRTR, 'G',  4, NULL, &States[7]),
	S_BRIGHT (BRTR, 'H',  4, NULL, &States[8]),
	S_BRIGHT (BRTR, 'I',  4, NULL, &States[9]),
	S_BRIGHT (BRTR, 'J',  4, NULL, &States[10]),
	S_BRIGHT (BRTR, 'K',  4, NULL, &States[11]),
	S_BRIGHT (BRTR, 'L',  4, NULL, &States[12]),
	S_BRIGHT (BRTR, 'M',  4, NULL, &States[0])
};

DECNBLOCK (AZBlueCandle, 8066, 20, 16, 5)
{
	S_BRIGHT (BCAN, 'A',  5, NULL, &States[1]),
	S_BRIGHT (BCAN, 'B',  5, NULL, &States[2]),
	S_BRIGHT (BCAN, 'C',  5, NULL, &States[3]),
	S_BRIGHT (BCAN, 'D',  5, NULL, &States[4]),
	S_BRIGHT (BCAN, 'E',  5, NULL, &States[0])
};

DEC (AZIronMaiden, 8067, 12, 60, 1)
{
	S_NORMAL (IRON, 'A', -1, NULL, NULL)
};

DECHANGNB (AZChainBit32, 8071, 4, 32, 1)
{
	S_NORMAL (CHNS, 'A', -1, NULL, NULL)
};

DECHANGNB (AZChainBit64, 8072, 4, 64, 1)
{
	S_NORMAL (CHNS, 'B', -1, NULL, NULL)
};

DECHANGNB (AZChainEndHeart, 8073, 4, 32, 1)
{
	S_NORMAL (CHNS, 'C', -1, NULL, NULL)
};

DECHANGNB (AZChainEndHook1, 8074, 4, 32, 1)
{
	S_NORMAL (CHNS, 'D', -1, NULL, NULL)
};

DECHANGNB (AZChainEndHook2, 8075, 4, 32, 1)
{
	S_NORMAL (CHNS, 'E', -1, NULL, NULL)
};

DECHANGNB (AZChainEndSpike, 8076, 4, 32, 1)
{
	S_NORMAL (CHNS, 'F', -1, NULL, NULL)
};

DECHANGNB (AZChainEndSkull, 8077, 4, 32, 1)
{
	S_NORMAL (CHNS, 'G', -1, NULL, NULL)
};

DECNBLOCK (ATableShit1, 8500, 20, 16, 1)
{
	S_NORMAL (TST1, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit2, 8501, 20, 16, 1)
{
	S_NORMAL (TST2, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit3, 8502, 20, 16, 1)
{
	S_NORMAL (TST3, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit4, 8503, 20, 16, 1)
{
	S_NORMAL (TST4, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit5, 8504, 20, 16, 1)
{
	S_NORMAL (TST5, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit6, 8505, 20, 16, 1)
{
	S_NORMAL (TST6, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit7, 8506, 20, 16, 1)
{
	S_NORMAL (TST7, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit8, 8507, 20, 16, 1)
{
	S_NORMAL (TST8, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit9, 8508, 20, 16, 1)
{
	S_NORMAL (TST9, 'A', -1, NULL, NULL)
};

DECNBLOCK (ATableShit10, 8509, 20, 16, 1)
{
	S_NORMAL (TST0, 'A', -1, NULL, NULL)
};

DECNBNGT (ATeleSmoke, 140, 20, 16, 26)
{
	S_NORMAL (TSMK, 'A',  4, NULL, &States[1]),
	S_NORMAL (TSMK, 'B',  3, NULL, &States[2]),
	S_NORMAL (TSMK, 'C',  4, NULL, &States[3]),
	S_NORMAL (TSMK, 'D',  3, NULL, &States[4]),
	S_NORMAL (TSMK, 'E',  4, NULL, &States[5]),
	S_NORMAL (TSMK, 'F',  3, NULL, &States[6]),
	S_NORMAL (TSMK, 'G',  4, NULL, &States[7]),
	S_NORMAL (TSMK, 'H',  3, NULL, &States[8]),
	S_NORMAL (TSMK, 'I',  4, NULL, &States[9]),
	S_NORMAL (TSMK, 'J',  3, NULL, &States[10]),
	S_NORMAL (TSMK, 'K',  4, NULL, &States[11]),
	S_NORMAL (TSMK, 'L',  3, NULL, &States[12]),
	S_NORMAL (TSMK, 'M',  4, NULL, &States[13]),
	S_NORMAL (TSMK, 'N',  3, NULL, &States[14]),
	S_NORMAL (TSMK, 'O',  4, NULL, &States[15]),
	S_NORMAL (TSMK, 'P',  3, NULL, &States[16]),
	S_NORMAL (TSMK, 'Q',  4, NULL, &States[17]),
	S_NORMAL (TSMK, 'R',  3, NULL, &States[18]),
	S_NORMAL (TSMK, 'S',  4, NULL, &States[19]),
	S_NORMAL (TSMK, 'T',  3, NULL, &States[20]),
	S_NORMAL (TSMK, 'U',  4, NULL, &States[21]),
	S_NORMAL (TSMK, 'V',  3, NULL, &States[22]),
	S_NORMAL (TSMK, 'W',  4, NULL, &States[23]),
	S_NORMAL (TSMK, 'X',  3, NULL, &States[24]),
	S_NORMAL (TSMK, 'Y',  4, NULL, &States[25]),
	S_NORMAL (TSMK, 'Z',  3, NULL, &States[0])
};

