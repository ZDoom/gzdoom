#include "actor.h"
#include "info.h"

/********** Decorations ***********/

#define _DECCOMMON(cls,ednum,rad,hi,ns,id) \
	class cls : public AActor { DECLARE_STATELESS_ACTOR (cls, AActor) static FState States[ns]; }; \
	IMPLEMENT_ACTOR (cls, Doom, ednum, id) \
		PROP_SpawnState(0) \
		PROP_RadiusFixed(rad) \
		PROP_HeightFixed(hi)

#define _DECSTARTSTATES(cls,ns) \
	FState cls::States[ns] =

#define DECID(cls,ednum,id,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns,id) \
	PROP_Flags (MF_SOLID) \
	END_DEFAULTS \
	_DECSTARTSTATES(cls,ns)

#define DEC(cls,ednum,rad,hi,ns) \
	DECID(cls,ednum,0,rad,hi,ns)

#define DECNSOLID(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns,0) \
	END_DEFAULTS \
	_DECSTARTSTATES(cls,ns)

#define DECHANG(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns,0) \
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY) \
	END_DEFAULTS \
	_DECSTARTSTATES(cls,ns)

#define DECHANGNS(cls,ednum,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns,0) \
	PROP_Flags (MF_SPAWNCEILING|MF_NOGRAVITY) \
	END_DEFAULTS \
	_DECSTARTSTATES(cls,ns)

#define DECNBLOCKID(cls,ednum,id,rad,hi,ns) \
	_DECCOMMON(cls,ednum,rad,hi,ns,id) \
	/*PROP_Flags (MF_NOBLOCKMAP)*/ \
	END_DEFAULTS \
	_DECSTARTSTATES(cls,ns)

#define SUBCLASS_NS(cls,super,ednum,rad,hi) \
	class cls : public super { DECLARE_STATELESS_ACTOR (cls, super) }; \
	IMPLEMENT_STATELESS_ACTOR (cls, Doom, ednum, 0) \
	PROP_RadiusFixed (rad) \
	PROP_HeightFixed (hi) \
	PROP_FlagsClear (MF_SOLID) \
	END_DEFAULTS

// Tech lamp ---------------------------------------------------------------

DEC (ATechLamp, 85, 16, 80, 4)
{
	S_BRIGHT (TLMP, 'A',  4, NULL, &States[1]),
	S_BRIGHT (TLMP, 'B',  4, NULL, &States[2]),
	S_BRIGHT (TLMP, 'C',  4, NULL, &States[3]),
	S_BRIGHT (TLMP, 'D',  4, NULL, &States[0])
};

// Tech lamp 2 -------------------------------------------------------------

DEC (ATechLamp2, 86, 16, 60, 4)
{
	S_BRIGHT (TLP2, 'A',  4, NULL, &States[1]),
	S_BRIGHT (TLP2, 'B',  4, NULL, &States[2]),
	S_BRIGHT (TLP2, 'C',  4, NULL, &States[3]),
	S_BRIGHT (TLP2, 'D',  4, NULL, &States[0])
};

// Column ------------------------------------------------------------------

DEC (AColumn, 2028, 16, 48, 1)
{
	S_BRIGHT (COLU, 'A', -1, NULL, NULL)
};

// Tall green column -------------------------------------------------------

DEC (ATallGreenColumn, 30, 16, 52, 1)
{
	S_NORMAL (COL1, 'A', -1, NULL, NULL)
};

// Short green column ------------------------------------------------------

DEC (AShortGreenColumn, 31, 16, 40, 1)
{
	S_NORMAL (COL2, 'A', -1, NULL, NULL)
};

// Tall red column ---------------------------------------------------------

DEC (ATallRedColumn, 32, 16, 52, 1)
{
	S_NORMAL (COL3, 'A', -1, NULL, NULL)
};

// Short red column --------------------------------------------------------

DEC (AShortRedColumn, 33, 16, 40, 1)
{
	S_NORMAL (COL4, 'A', -1, NULL, NULL)
};

// Skull column ------------------------------------------------------------

DEC (ASkullColumn, 37, 16, 40, 1)
{
	S_NORMAL (COL6, 'A', -1, NULL, NULL)
};

// Heart column ------------------------------------------------------------

DEC (AHeartColumn, 36, 16, 40, 2)
{
	S_NORMAL (COL5, 'A', 14, NULL, &States[1]),
	S_NORMAL (COL5, 'B', 14, NULL, &States[0])
};

// Evil eye ----------------------------------------------------------------

DEC (AEvilEye, 41, 16, 54, 4)
{
	S_BRIGHT (CEYE, 'A',  6, NULL, &States[1]),
	S_BRIGHT (CEYE, 'B',  6, NULL, &States[2]),
	S_BRIGHT (CEYE, 'C',  6, NULL, &States[3]),
	S_BRIGHT (CEYE, 'B',  6, NULL, &States[0])
};

// Floating skull ----------------------------------------------------------

DEC (AFloatingSkull, 42, 16, 26, 3)
{
	S_BRIGHT (FSKU, 'A',  6, NULL, &States[1]),
	S_BRIGHT (FSKU, 'B',  6, NULL, &States[2]),
	S_BRIGHT (FSKU, 'C',  6, NULL, &States[0])
};

// Torch tree --------------------------------------------------------------

DEC (ATorchTree, 43, 16, 56, 1)
{
	S_NORMAL (TRE1, 'A', -1, NULL, NULL)
};

// Blue torch --------------------------------------------------------------

DEC (ABlueTorch, 44, 16, 68, 4)
{
	S_BRIGHT (TBLU, 'A',  4, NULL, &States[1]),
	S_BRIGHT (TBLU, 'B',  4, NULL, &States[2]),
	S_BRIGHT (TBLU, 'C',  4, NULL, &States[3]),
	S_BRIGHT (TBLU, 'D',  4, NULL, &States[0])
};

// Green torch -------------------------------------------------------------

DEC (AGreenTorch, 45, 16, 68, 4)
{
	S_BRIGHT (TGRN, 'A',  4, NULL, &States[1]),
	S_BRIGHT (TGRN, 'B',  4, NULL, &States[2]),
	S_BRIGHT (TGRN, 'C',  4, NULL, &States[3]),
	S_BRIGHT (TGRN, 'D',  4, NULL, &States[0])
};

// Red torch ---------------------------------------------------------------

DEC (ARedTorch, 46, 16, 68, 4)
{
	S_BRIGHT (TRED, 'A',  4, NULL, &States[1]),
	S_BRIGHT (TRED, 'B',  4, NULL, &States[2]),
	S_BRIGHT (TRED, 'C',  4, NULL, &States[3]),
	S_BRIGHT (TRED, 'D',  4, NULL, &States[0])
};

// Short blue torch --------------------------------------------------------

DEC (AShortBlueTorch, 55, 16, 37, 4)
{
	S_BRIGHT (SMBT, 'A',  4, NULL, &States[1]),
	S_BRIGHT (SMBT, 'B',  4, NULL, &States[2]),
	S_BRIGHT (SMBT, 'C',  4, NULL, &States[3]),
	S_BRIGHT (SMBT, 'D',  4, NULL, &States[0])
};

// Short green torch -------------------------------------------------------

DEC (AShortGreenTorch, 56, 16, 37, 4)
{
	S_BRIGHT (SMGT, 'A',  4, NULL, &States[1]),
	S_BRIGHT (SMGT, 'B',  4, NULL, &States[2]),
	S_BRIGHT (SMGT, 'C',  4, NULL, &States[3]),
	S_BRIGHT (SMGT, 'D',  4, NULL, &States[0])
};

// Short red torch ---------------------------------------------------------

DEC (AShortRedTorch, 57, 16, 37, 4)
{
	S_BRIGHT (SMRT, 'A',  4, NULL, &States[1]),
	S_BRIGHT (SMRT, 'B',  4, NULL, &States[2]),
	S_BRIGHT (SMRT, 'C',  4, NULL, &States[3]),
	S_BRIGHT (SMRT, 'D',  4, NULL, &States[0])
};

// Stalagtite --------------------------------------------------------------

DEC (AStalagtite, 47, 16, 40, 1)
{
	S_NORMAL (SMIT, 'A', -1, NULL, NULL)
};

// Tech pillar -------------------------------------------------------------

DEC (ATechPillar, 48, 16, 128, 1)
{
	S_NORMAL (ELEC, 'A', -1, NULL, NULL)
};

// Candle stick ------------------------------------------------------------

DECNSOLID (ACandlestick, 34, 20, 14, 1)
{
	S_BRIGHT (CAND, 'A', -1, NULL, NULL)
};

// Candelabra --------------------------------------------------------------

DEC (ACandelabra, 35, 16, 60, 1)
{
	S_BRIGHT (CBRA, 'A', -1, NULL, NULL)
};

// Bloody twitch -----------------------------------------------------------

DECHANG (ABloodyTwitch, 49, 16, 68, 4)
{
	S_NORMAL (GOR1, 'A', 10, NULL, &States[1]),
	S_NORMAL (GOR1, 'B', 15, NULL, &States[2]),
	S_NORMAL (GOR1, 'C',  8, NULL, &States[3]),
	S_NORMAL (GOR1, 'B',  6, NULL, &States[0])
};

// Meat 2 ------------------------------------------------------------------

DECHANG (AMeat2, 50, 16, 84, 1)
{
	S_NORMAL (GOR2, 'A', -1, NULL, NULL)
};

// Meat 3 ------------------------------------------------------------------

DECHANG (AMeat3, 51, 16, 84, 1)
{
	S_NORMAL (GOR3, 'A', -1, NULL, NULL)
};

// Meat 4 ------------------------------------------------------------------

DECHANG (AMeat4, 52, 16, 68, 1)
{
	S_NORMAL (GOR4, 'A', -1, NULL, NULL)
};

// Meat 5 ------------------------------------------------------------------

DECHANG (AMeat5, 53, 16, 52, 1)
{
	S_NORMAL (GOR5, 'A', -1, NULL, NULL)
};

// Nonsolid meat -----------------------------------------------------------

SUBCLASS_NS (ANonsolidMeat2, AMeat2, 59, 20, 84)
SUBCLASS_NS (ANonsolidMeat3, AMeat3, 61, 20, 52)
SUBCLASS_NS (ANonsolidMeat4, AMeat4, 60, 20, 68)
SUBCLASS_NS (ANonsolidMeat5, AMeat5, 62, 20, 52)

// Nonsolid bloody twitch --------------------------------------------------

SUBCLASS_NS (ANonsolidTwitch, ABloodyTwitch, 63, 20, 68)

// Head on a stick ---------------------------------------------------------

DEC (AHeadOnAStick, 27, 16, 56, 1)
{
	S_NORMAL (POL4, 'A', -1, NULL, NULL)
};

// Heads (plural!) on a stick ----------------------------------------------

DEC (AHeadsOnAStick, 28, 16, 64, 1)
{
	S_NORMAL (POL2, 'A', -1, NULL, NULL)
};

// Head candles ------------------------------------------------------------

DEC (AHeadCandles, 29, 16, 42, 2)
{
	S_BRIGHT (POL3, 'A',  6, NULL, &States[1]),
	S_BRIGHT (POL3, 'B',  6, NULL, &States[0])
};

// Dead on a stick ---------------------------------------------------------

DEC (ADeadStick, 25, 16, 64, 1)
{
	S_NORMAL (POL1, 'A', -1, NULL, NULL)
};

// Still alive on a stick --------------------------------------------------

DEC (ALiveStick, 26, 16, 64, 2)
{
	S_NORMAL (POL6, 'A',  6, NULL, &States[1]),
	S_NORMAL (POL6, 'B',  8, NULL, &States[0])
};

// Big tree ----------------------------------------------------------------

DEC (ABigTree, 54, 32, 108, 1)
{
	S_NORMAL (TRE2, 'A', -1, NULL, NULL)
};

// Burning barrel ----------------------------------------------------------

DECID (ABurningBarrel, 70, 149, 16, 32, 3)
{
	S_BRIGHT (FCAN, 'A',  4, NULL, &States[1]),
	S_BRIGHT (FCAN, 'B',  4, NULL, &States[2]),
	S_BRIGHT (FCAN, 'C',  4, NULL, &States[0])
};

// Hanging with no guts ----------------------------------------------------

DECHANG (AHangNoGuts, 73, 16, 88, 1)
{
	S_NORMAL (HDB1, 'A', -1, NULL, NULL)
};

// Hanging from bottom with no brain ---------------------------------------

DECHANG (AHangBNoBrain, 74, 16, 88, 1)
{
	S_NORMAL (HDB2, 'A', -1, NULL, NULL)
};

// Hanging from top, looking down ------------------------------------------

DECHANG (AHangTLookingDown, 75, 16, 64, 1)
{
	S_NORMAL (HDB3, 'A', -1, NULL, NULL)
};

// Hanging from top, looking up --------------------------------------------

DECHANG (AHangTLookingUp, 77, 16, 64, 1)
{
	S_NORMAL (HDB5, 'A', -1, NULL, NULL)
};

// Hanging from top, skully ------------------------------------------------

DECHANG (AHangTSkull, 76, 16, 64, 1)
{
	S_NORMAL (HDB4, 'A', -1, NULL, NULL)
};

// Hanging from top without a brain ----------------------------------------

DECHANG (AHangTNoBrain, 78, 16, 64, 1)
{
	S_NORMAL (HDB6, 'A', -1, NULL, NULL)
};

// Colon gibs --------------------------------------------------------------

DECNBLOCKID (AColonGibs, 79, 147, 20, 4, 1)
{
	S_NORMAL (POB1, 'A', -1, NULL, NULL)
};

// Small pool o' blood -----------------------------------------------------

DECNBLOCKID (ASmallBloodPool, 80, 148, 20, 1, 1)
{
	S_NORMAL (POB2, 'A', -1, NULL, NULL)
};

// brain stem lying on the ground ------------------------------------------

DECNBLOCKID (ABrainStem, 81, 150, 20, 4, 1)
{
	S_NORMAL (BRS1, 'A', -1, NULL, NULL)
};
