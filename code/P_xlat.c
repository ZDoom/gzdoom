// p_xlat.c: Translate old linedefs to new linedefs

#include "doomtype.h"
#include "p_lnspec.h"
#include "doomdata.h"
#include "r_data.h"
#include "m_swap.h"
#include "p_spec.h"
#include "p_local.h"

// Speeds for ceilings/crushers (x/8 units per tic)
//	(Hexen crushers go up at half the speed that they go down)
#define C_SLOW			8
#define C_NORMAL		16
#define C_FAST			32
#define C_TURBO			64

#define CEILWAIT		150

// Speeds for floors (x/8 units per tic)
#define F_SLOW			8
#define F_NORMAL		16
#define F_FAST			32
#define F_TURBO			64

// Speeds for doors (x/8 units per tic)
#define D_SLOW			16
#define D_NORMAL		32
#define D_FAST			64
#define D_TURBO			128

#define VDOORWAIT		150

// Speeds for stairs (x/8 units per tic)
#define S_SLOW			2
#define S_NORMAL		4
#define S_FAST			16
#define S_TURBO			32

// Speeds for plats (Hexen plats stop 8 units above the floor)
#define P_SLOW			8
#define P_NORMAL		16
#define P_FAST			32
#define P_TURBO			64

#define PLATWAIT		105

#define ELEVATORSPEED	32

// Speeds for donut slime and pillar (x/8 units per tic)
#define DORATE			4

// Texture scrollers operate at a rate of x/64 units per tic.
#define SCROLL_UNIT		64


//Define masks, shifts, for fields in generalized linedef types
// (from BOOM's p_psec.h file)

#define GenFloorBase          0x6000
#define GenCeilingBase        0x4000
#define GenDoorBase           0x3c00
#define GenLockedBase         0x3800
#define GenLiftBase           0x3400
#define GenStairsBase         0x3000
#define GenCrusherBase        0x2F80

// define names for the TriggerType field of the general linedefs

typedef enum
{
  WalkOnce,
  WalkMany,
  SwitchOnce,
  SwitchMany,
  GunOnce,
  GunMany,
  PushOnce,
  PushMany,
} triggertype_e;


// Line special translation structure
typedef struct {
	byte	flags;
	byte	newspecial;
	byte	args[5];
} xlat_t;

#define TAG	123		// Special value that gets replaced with the line tag

#define WALK	(ML_ACTIVATECROSS>>8)
#define USE		(ML_ACTIVATEUSE>>8)
#define SHOOT	(ML_ACTIVATEPROJECTILEHIT>>8)
#define MONST	(ML_MONSTERSCANACTIVATE>>8)
#define MONWALK (ML_ACTIVATEMONSTERCROSS>>8)
#define REP		(ML_REPEATABLE>>8)

static const xlat_t SpecialTranslation[] = {
/*   0 */ { 0 },
/*   1 */ { USE|MONST|REP,	Door_Raise,					 { 0, D_SLOW, VDOORWAIT } },
/*   2 */ { WALK,			Door_Open,					 { TAG, D_SLOW } },
/*   3 */ { WALK,			Door_Close,					 { TAG, D_SLOW } },
/*   4 */ { WALK|MONST,		Door_Raise,					 { TAG, D_SLOW, VDOORWAIT } },
/*   5 */ { WALK,			Floor_RaiseToLowestCeiling,	 { TAG, F_SLOW } },
/*	 6 */ { WALK,			Ceiling_CrushAndRaiseA,		 { TAG, C_NORMAL, C_NORMAL, 10 } },
/*   7 */ { USE,			Stairs_BuildUpDoom,			 { TAG, S_SLOW, 8 } },
/*   8 */ { WALK,			Stairs_BuildUpDoom,			 { TAG, S_SLOW, 8 } },
/*	 9 */ { USE,			Floor_Donut,				 { TAG, DORATE, DORATE } },
/*  10 */ { WALK|MONST,		Plat_DownWaitUpStayLip,		 { TAG, P_FAST, PLATWAIT, 0 } },
/*  11 */ { USE,			Exit_Normal,				 { 0 } },
/*  12 */ { WALK,			Light_MaxNeighbor,			 { TAG } },
/*  13 */ { WALK,			Light_ChangeToValue,		 { TAG, 255 } },
/*  14 */ { USE,			Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 4 } },
/*  15 */ { USE,			Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 3 } },
/*  16 */ { WALK,			Door_CloseWaitOpen,			 { TAG, D_SLOW, 240 } },
/*  17 */ { WALK,			Light_StrobeDoom,			 { TAG, 5, 35 } },
/*  18 */ { USE,			Floor_RaiseToNearest,		 { TAG, F_SLOW } },
/*  19 */ { WALK,			Floor_LowerToHighest,		 { TAG, F_SLOW, 128 } },
/*  20 */ { USE,			Plat_RaiseAndStayTx0,		 { TAG, P_SLOW/2 } },
/*  21 */ { USE,			Plat_DownWaitUpStayLip,		 { TAG, P_FAST, PLATWAIT } },
/*  22 */ { WALK,			Plat_RaiseAndStayTx0,		 { TAG, P_SLOW/2 } },
/*  23 */ { USE,			Floor_LowerToLowest,		 { TAG, F_SLOW } },
/*  24 */ { SHOOT,			Floor_RaiseToLowestCeiling,  { TAG, F_SLOW } },
/*  25 */ { WALK,			Ceiling_CrushAndRaiseA,		 { TAG, C_SLOW, C_SLOW, 10 } },
/*  26 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_SLOW, VDOORWAIT, BCard | CardIsSkull } },
/*  27 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_SLOW, VDOORWAIT, YCard | CardIsSkull } },
/*  28 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_SLOW, VDOORWAIT, RCard | CardIsSkull } },
/*  29 */ { USE,			Door_Raise,					 { TAG, D_SLOW, VDOORWAIT } },
/*  30 */ { WALK,			Floor_RaiseByTexture,		 { TAG, F_SLOW } },
/*  31 */ { USE,			Door_Open,					 { 0, D_SLOW } },
/*  32 */ { USE|MONST,		Door_LockedRaise,			 { 0, D_SLOW, 0, BCard | CardIsSkull } },
/*  33 */ { USE|MONST,		Door_LockedRaise,			 { 0, D_SLOW, 0, RCard | CardIsSkull } },
/*  34 */ { USE|MONST,		Door_LockedRaise,			 { 0, D_SLOW, 0, YCard | CardIsSkull } },
/*  35 */ { WALK,			Light_ChangeToValue,		 { TAG, 35 } },
/*  36 */ { WALK,			Floor_LowerToHighest,		 { TAG, F_FAST, 136 } },
/*  37 */ { WALK,			Floor_LowerToLowestTxTy,	 { TAG, F_SLOW } },
/*  38 */ { WALK,			Floor_LowerToLowest,		 { TAG, F_SLOW } },
/*  39 */ { WALK|MONST,		Teleport,					 { TAG } },
/*  40 */ { WALK,			Generic_Ceiling,			 { TAG, C_SLOW, 0, 1, 8 } },
/*  41 */ { USE,			Ceiling_LowerToFloor,		 { TAG, C_SLOW } },
/*  42 */ { USE|REP,		Door_Close,					 { TAG, D_SLOW } },
/*  43 */ { USE|REP,		Ceiling_LowerToFloor,		 { TAG, C_SLOW } },
/*  44 */ { WALK,			Ceiling_LowerAndCrush,		 { TAG, C_SLOW, 0 } },
/*  45 */ { USE|REP,		Floor_LowerToHighest,		 { TAG, F_SLOW, 128 } },
/*  46 */ { SHOOT|REP|MONST,Door_Open,					 { TAG, D_SLOW } },
/*  47 */ { SHOOT,			Plat_RaiseAndStayTx0,		 { TAG, P_SLOW/2 } },
/*  48 */ { 0,				Scroll_Texture_Left,		 { SCROLL_UNIT } },
/*  49 */ { USE,			Ceiling_CrushAndRaiseA,		 { TAG, C_SLOW, C_SLOW, 10 } },
/*  50 */ { USE,			Door_Close,					 { TAG, D_SLOW } },
/*  51 */ { USE,			Exit_Secret,				 { 0 } },
/*  52 */ { WALK,			Exit_Normal,				 { 0 } },
/*  53 */ { WALK,			Plat_PerpetualRaiseLip,		 { TAG, P_SLOW, PLATWAIT, 0 } },
/*  54 */ { WALK,			Plat_Stop,					 { TAG } },
/*  55 */ { USE,			Floor_RaiseAndCrush,		 { TAG, F_SLOW, 10 } },
/*  56 */ { WALK,			Floor_RaiseAndCrush,		 { TAG, F_SLOW, 10 } },
/*  57 */ { WALK,			Ceiling_CrushStop,			 { TAG } },
/*  58 */ { WALK,			Floor_RaiseByValue,			 { TAG, F_SLOW, 24 } },
/*  59 */ { WALK,			Floor_RaiseByValueTxTy,		 { TAG, F_SLOW, 24 } },
/*  60 */ { USE|REP,		Floor_LowerToLowest,		 { TAG, F_SLOW } },
/*  61 */ { USE|REP,		Door_Open,					 { TAG, D_SLOW } },
/*  62 */ { USE|REP,		Plat_DownWaitUpStayLip,		 { TAG, P_FAST, PLATWAIT, 0 } },
/*  63 */ { USE|REP,		Door_Raise,					 { TAG, D_SLOW, VDOORWAIT } },
/*  64 */ { USE|REP,		Floor_RaiseToLowestCeiling,	 { TAG, F_SLOW } },
/*  65 */ { USE|REP,		Floor_RaiseAndCrush,		 { TAG, F_SLOW, 10 } },
/*  66 */ { USE|REP,		Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 3 } },
/*  67 */ { USE|REP,		Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 4 } },
/*  68 */ { USE|REP,		Plat_RaiseAndStayTx0,		 { TAG, P_SLOW/2 } },
/*  69 */ { USE|REP,		Floor_RaiseToNearest,		 { TAG, F_SLOW } },
/*  70 */ { USE|REP,		Floor_LowerToHighest,		 { TAG, F_FAST, 136 } },
/*  71 */ { USE,			Floor_LowerToHighest,		 { TAG, F_FAST, 136 } },
/*  72 */ { WALK|REP,		Ceiling_LowerAndCrush,		 { TAG, C_SLOW, 0 } },
/*  73 */ { WALK|REP,		Ceiling_CrushAndRaiseA,		 { TAG, C_SLOW, C_SLOW, 10 } },
/*  74 */ { WALK|REP,		Ceiling_CrushStop,			 { TAG } },
/*  75 */ { WALK|REP,		Door_Close,					 { TAG, D_SLOW } },
/*  76 */ { WALK|REP,		Door_CloseWaitOpen,			 { TAG, D_SLOW, 240 } },
/*  77 */ { WALK|REP,		Ceiling_CrushAndRaiseA,		 { TAG, C_NORMAL, C_NORMAL, 10 } },
/*  78 */ { USE|REP,		Floor_TransferNumeric,		 { TAG } },			// <- BOOM special
/*  79 */ { WALK|REP,		Light_ChangeToValue,		 { TAG, 35 } },
/*  80 */ { WALK|REP,		Light_MaxNeighbor,			 { TAG } },
/*  81 */ { WALK|REP,		Light_ChangeToValue,		 { TAG, 255 } },
/*  82 */ { WALK|REP,		Floor_LowerToLowest,		 { TAG, F_SLOW } },
/*  83 */ { WALK|REP,		Floor_LowerToHighest,		 { TAG, F_SLOW, 128 } },
/*  84 */ { WALK|REP,		Floor_LowerToLowestTxTy,	 { TAG, F_SLOW } },
/*  85 */ { 0,				Scroll_Texture_Right,		 { SCROLL_UNIT } }, // <- BOOM special
/*  86 */ { WALK|REP,		Door_Open,					 { TAG, D_SLOW } },
/*  87 */ { WALK|REP,		Plat_PerpetualRaiseLip,		 { TAG, P_SLOW, PLATWAIT, 0 } },
/*  88 */ { WALK|REP|MONST, Plat_DownWaitUpStayLip,		 { TAG, P_FAST, PLATWAIT, 0 } },
/*  89 */ { WALK|REP,		Plat_Stop,					 { TAG } },
/*  90 */ { WALK|REP,		Door_Raise,					 { TAG, D_SLOW, VDOORWAIT } },
/*  91 */ { WALK|REP,		Floor_RaiseToLowestCeiling,	 { TAG, F_SLOW } },
/*  92 */ { WALK|REP,		Floor_RaiseByValue,			 { TAG, F_SLOW, 24 } },
/*  93 */ { WALK|REP,		Floor_RaiseByValueTxTy,		 { TAG, F_SLOW, 24 } },
/*  94 */ { WALK|REP,		Floor_RaiseAndCrush,		 { TAG, F_SLOW, 10 } },
/*  95 */ { WALK|REP,		Plat_RaiseAndStayTx0,		 { TAG, P_SLOW/2 } },
/*  96 */ { WALK|REP,		Floor_RaiseByTexture,		 { TAG, F_SLOW } },
/*  97 */ { WALK|REP|MONST, Teleport,					 { TAG } },
/*  98 */ { WALK|REP,		Floor_LowerToHighest,		 { TAG, F_FAST, 136 } },
/*  99 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_FAST, 0, BCard | CardIsSkull } },
/* 100 */ { WALK,			Stairs_BuildUpDoom,			 { TAG, S_TURBO, 16, 0, 0 } },
/* 101 */ { USE,			Floor_RaiseToLowestCeiling,	 { TAG, F_SLOW } },
/* 102 */ { USE,			Floor_LowerToHighest,		 { TAG, F_SLOW, 128 } },
/* 103 */ { USE,			Door_Open,					 { TAG, D_SLOW } },
/* 104 */ { WALK,			Light_MinNeighbor,			 { TAG } },
/* 105 */ { WALK|REP,		Door_Raise,					 { TAG, D_FAST, VDOORWAIT } },
/* 106 */ { WALK|REP,		Door_Open,					 { TAG, D_FAST } },
/* 107 */ { WALK|REP,		Door_Close,					 { TAG, D_FAST } },
/* 108 */ { WALK,			Door_Raise,					 { TAG, D_FAST, VDOORWAIT } },
/* 109 */ { WALK,			Door_Open,					 { TAG, D_FAST } },
/* 110 */ { WALK,			Door_Close,					 { TAG, D_FAST } },
/* 111 */ { USE,			Door_Raise,					 { TAG, D_FAST, VDOORWAIT } },
/* 112 */ { USE,			Door_Open,					 { TAG, D_FAST } },
/* 113 */ { USE,			Door_Close,					 { TAG, D_FAST } },
/* 114 */ { USE|REP,		Door_Raise,					 { TAG, D_FAST, VDOORWAIT } },
/* 115 */ { USE|REP,		Door_Open,					 { TAG, D_FAST } },
/* 116 */ { USE|REP,		Door_Close,					 { TAG, D_FAST } },
/* 117 */ { USE|REP,		Door_Raise,					 { 0, D_FAST, VDOORWAIT } },
/* 118 */ { USE,			Door_Open,					 { 0, D_FAST } },
/* 119 */ { WALK,			Floor_RaiseToNearest,		 { TAG, F_SLOW } },
/* 120 */ { WALK|REP,		Plat_DownWaitUpStayLip,		 { TAG, P_TURBO, PLATWAIT, 0 } },
/* 121 */ { WALK,			Plat_DownWaitUpStayLip,		 { TAG, P_TURBO, PLATWAIT, 0 } },
/* 122 */ { USE,			Plat_DownWaitUpStayLip,		 { TAG, P_TURBO, PLATWAIT, 0 } },
/* 123 */ { USE|REP,		Plat_DownWaitUpStayLip,		 { TAG, P_TURBO, PLATWAIT, 0 } },
/* 124 */ { WALK,			Exit_Secret,				 { 0 } },
/* 125 */ { MONWALK,		Teleport,					 { TAG } },
/* 126 */ { MONWALK|REP,	Teleport,					 { TAG } },
/* 127 */ { USE,			Stairs_BuildUpDoom,			 { TAG, S_TURBO, 16, 0, 0 } },
/* 128 */ { WALK|REP,		Floor_RaiseToNearest,		 { TAG, F_SLOW } },
/* 129 */ { WALK|REP,		Floor_RaiseToNearest,		 { TAG, F_FAST } },
/* 130 */ { WALK,			Floor_RaiseToNearest,		 { TAG, F_FAST } },
/* 131 */ { USE,			Floor_RaiseToNearest,		 { TAG, F_FAST } },
/* 132 */ { USE|REP,		Floor_RaiseToNearest,		 { TAG, F_FAST } },
/* 133 */ { USE,			Door_LockedRaise,			 { TAG, D_FAST, 0, BCard | CardIsSkull } },
/* 134 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_FAST, 0, BCard | CardIsSkull } },
/* 135 */ { USE,			Door_LockedRaise,			 { TAG, D_FAST, 0, RCard | CardIsSkull } },
/* 136 */ { USE|REP,		Door_LockedRaise,			 { TAG, D_FAST, 0, YCard | CardIsSkull } },
/* 137 */ { USE,			Door_LockedRaise,			 { TAG, D_FAST, 0, YCard | CardIsSkull } },
/* 138 */ { USE|REP,		Light_ChangeToValue,		 { TAG, 255 } },
/* 139 */ { USE|REP,		Light_ChangeToValue,		 { TAG, 35 } },
/* 140 */ { USE,			Floor_RaiseByValueTimes8,	 { TAG, F_SLOW, 64 } },
/* 141 */ { WALK,			Ceiling_CrushAndRaiseSilentA,{ TAG, C_SLOW, C_SLOW, 10 } },

/****** The following are all new to BOOM ******/

/* 142 */ { WALK,			Floor_RaiseByValueTimes8,	 { TAG, F_SLOW, 64 } },
/* 143 */ { WALK,			Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 3 } },
/* 144 */ { WALK,			Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 4 } },
/* 145 */ { WALK,			Ceiling_LowerToFloor,		 { TAG, C_SLOW } },
/* 146 */ { WALK,			Floor_Donut,				 { TAG, DORATE, DORATE } },
/* 147 */ { WALK|REP,		Floor_RaiseByValueTimes8,	 { TAG, F_SLOW, 64 } },
/* 148 */ { WALK|REP,		Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 3 } },
/* 149 */ { WALK|REP,		Plat_UpByValueStayTx,		 { TAG, P_SLOW/2, 4 } },
/* 150 */ { WALK|REP,		Ceiling_CrushAndRaiseSilentA,{ TAG, C_SLOW, C_SLOW, 10 } },
/* 151 */ { WALK|REP,		FloorAndCeiling_LowerRaise,	 { TAG, F_SLOW, C_SLOW } },
/* 152 */ { WALK|REP,		Ceiling_LowerToFloor,		 { TAG, C_SLOW } },
/* 153 */ { WALK,			Floor_TransferTrigger,		 { TAG } },
/* 154 */ { WALK|REP,		Floor_TransferTrigger,		 { TAG } },
/* 155 */ { WALK|REP,		Floor_Donut,				 { TAG, DORATE, DORATE } },
/* 156 */ { WALK|REP,		Light_StrobeDoom,			 { TAG, 5, 35 } },
/* 157 */ { WALK|REP,		Light_MinNeighbor,			 { TAG } },
/* 158 */ { USE,			Floor_RaiseByTexture,		 { TAG, F_SLOW } },
/* 159 */ { USE,			Floor_LowerToLowestTxTy,	 { TAG, F_SLOW } },
/* 160 */ { USE,			Floor_RaiseByValueTxTy,		 { TAG, F_SLOW, 24 } },
/* 161 */ { USE,			Floor_RaiseByValue,			 { TAG, F_SLOW, 24 } },
/* 162 */ { USE,			Plat_PerpetualRaiseLip,		 { TAG, P_SLOW, PLATWAIT, 0 } },
/* 163 */ { USE,			Plat_Stop,					 { TAG } },
/* 164 */ { USE,			Ceiling_CrushAndRaiseA,		 { TAG, C_NORMAL, C_NORMAL, 10 } },
/* 165 */ { USE,			Ceiling_CrushAndRaiseSilentA,{ TAG, C_SLOW, C_SLOW, 10 } },
/* 166 */ { USE,			FloorAndCeiling_LowerRaise,	 { TAG, F_SLOW, C_SLOW } },
/* 167 */ { USE,			Ceiling_LowerAndCrush,		 { TAG, C_SLOW, 0 } },
/* 168 */ { USE,			Ceiling_CrushStop,			 { TAG } },
/* 169 */ { USE,			Light_MaxNeighbor,			 { TAG } },
/* 170 */ { USE,			Light_ChangeToValue,		 { TAG, 35 } },
/* 171 */ { USE,			Light_ChangeToValue,		 { TAG, 255 } },
/* 172 */ { USE,			Light_StrobeDoom,			 { TAG, 5, 35 } },
/* 173 */ { USE,			Light_MinNeighbor,			 { TAG } },
/* 174 */ { USE|MONST,		Teleport,					 { TAG } },
/* 175 */ { USE,			Door_CloseWaitOpen,			 { TAG, F_SLOW, 240 } },
/* 176 */ { USE|REP,		Floor_RaiseByTexture,		 { TAG, F_SLOW } },
/* 177 */ { USE|REP,		Floor_LowerToLowestTxTy,	 { TAG, F_SLOW } },
/* 178 */ { USE|REP,		Floor_RaiseByValueTimes8,	 { TAG, F_SLOW, 64 } },
/* 179 */ { USE|REP,		Floor_RaiseByValueTxTy,		 { TAG, F_SLOW, 24 } },
/* 180 */ { USE|REP,		Floor_RaiseByValue,			 { TAG, F_SLOW, 24 } },
/* 181 */ { USE|REP,		Plat_PerpetualRaiseLip,		 { TAG, P_SLOW, PLATWAIT, 0 } },
/* 182 */ { USE|REP,		Plat_Stop,					 { TAG } },
/* 183 */ { USE|REP,		Ceiling_CrushAndRaiseA,		 { TAG, C_NORMAL, C_NORMAL, 10 } },
/* 184 */ { USE|REP,		Ceiling_CrushAndRaiseA,		 { TAG, C_SLOW, C_SLOW, 10 } },
/* 185 */ { USE|REP,		Ceiling_CrushAndRaiseSilentA,{ TAG, C_SLOW, C_SLOW, 10 } },
/* 186 */ { USE|REP,		FloorAndCeiling_LowerRaise,	 { TAG, F_SLOW, C_SLOW } },
/* 187 */ { USE|REP,		Ceiling_LowerAndCrush,		 { TAG, C_SLOW, 0 } },
/* 188 */ { USE|REP,		Ceiling_CrushStop,			 { TAG } },
/* 189 */ { USE,			Floor_TransferTrigger,		 { TAG } },
/* 190 */ { USE|REP,		Floor_TransferTrigger,		 { TAG } },
/* 191 */ { USE|REP,		Floor_Donut,				 { TAG, DORATE, DORATE } },
/* 192 */ { USE|REP,		Light_MaxNeighbor,			 { TAG } },
/* 193 */ { USE|REP,		Light_StrobeDoom,			 { TAG, 5, 35 } },
/* 194 */ { USE|REP,		Light_MinNeighbor,			 { TAG } },
/* 195 */ { USE|REP|MONST,	Teleport,					 { TAG } },
/* 196 */ { USE|REP,		Door_CloseWaitOpen,			 { TAG, D_SLOW, 240 } },
/* 197 */ { SHOOT,			Exit_Normal,				 { 0 } },
/* 198 */ { SHOOT,			Exit_Secret,				 { 0 } },
/* 199 */ { WALK,			Ceiling_LowerToLowest,		 { TAG, C_SLOW } },
/* 200 */ { WALK,			Ceiling_LowerToHighestFloor, { TAG, C_SLOW } },
/* 201 */ { WALK|REP,		Ceiling_LowerToLowest,		 { TAG, C_SLOW } },
/* 202 */ { WALK|REP,		Ceiling_LowerToHighestFloor, { TAG, C_SLOW } },
/* 203 */ { USE,			Ceiling_LowerToLowest,		 { TAG, C_SLOW } },
/* 204 */ { USE,			Ceiling_LowerToHighestFloor, { TAG, C_SLOW } },
/* 205 */ { USE|REP,		Ceiling_LowerToLowest,		 { TAG, C_SLOW } },
/* 206 */ { USE|REP,		Ceiling_LowerToHighestFloor, { TAG, C_SLOW } },
/* 207 */ { WALK|MONST,		Teleport_NoFog,				 { TAG } },
/* 208 */ { WALK|REP|MONST,	Teleport_NoFog,				 { TAG } },
/* 209 */ { USE|MONST,		Teleport_NoFog,				 { TAG } },
/* 210 */ { USE|REP|MONST,	Teleport_NoFog,				 { TAG } },
/* 211 */ { USE|REP,		Plat_ToggleCeiling,			 { TAG } },
/* 212 */ { WALK|REP,		Plat_ToggleCeiling,			 { TAG } },
/* 213 */ { 0,				Transfer_FloorLight,		 { TAG } },
/* 214 */ { 0,				Scroll_Ceiling,				 { TAG, 6, 0, 0, 0 } },
/* 215 */ { 0,				Scroll_Floor,				 { TAG, 6, 0, 0, 0 } },
/* 216 */ { 0,				Scroll_Floor,				 { TAG, 6, 1, 0, 0 } },
/* 217 */ { 0,				Scroll_Floor,				 { TAG, 6, 2, 0, 0 } },
/* 218 */ { 0,				Scroll_Texture_Model,		 { TAG, 2 } },
/* 219 */ { WALK,			Floor_LowerToNearest,		 { TAG, F_SLOW } },
/* 220 */ { WALK|REP,		Floor_LowerToNearest,		 { TAG, F_SLOW } },
/* 221 */ { USE,			Floor_LowerToNearest,		 { TAG, F_SLOW } },
/* 222 */ { USE|REP,		Floor_LowerToNearest,		 { TAG, F_SLOW } },
/* 223 */ { 0,				Sector_SetFriction,			 { TAG, 0 } },
/* 224 */ { 0,				Sector_SetWind,				 { TAG, 0, 0, 1 } },
/* 225 */ { 0,				Sector_SetCurrent,			 { TAG, 0, 0, 1 } },
/* 226 */ { 0,				PointPush_SetForce,			 { TAG, 0, 0, 1 } },
/* 227 */ { WALK,			Elevator_RaiseToNearest,	 { TAG, ELEVATORSPEED } },
/* 228 */ { WALK|REP,		Elevator_RaiseToNearest,	 { TAG, ELEVATORSPEED } },
/* 229 */ { USE,			Elevator_RaiseToNearest,	 { TAG, ELEVATORSPEED } },
/* 230 */ { USE|REP,		Elevator_RaiseToNearest,	 { TAG, ELEVATORSPEED } },
/* 231 */ { WALK,			Elevator_LowerToNearest,	 { TAG, ELEVATORSPEED } },
/* 232 */ { WALK|REP,		Elevator_LowerToNearest,	 { TAG, ELEVATORSPEED } },
/* 233 */ { USE,			Elevator_LowerToNearest,	 { TAG, ELEVATORSPEED } },
/* 234 */ { USE|REP,		Elevator_LowerToNearest,	 { TAG, ELEVATORSPEED } },
/* 235 */ { WALK,			Elevator_MoveToFloor,		 { TAG, ELEVATORSPEED } },
/* 236 */ { WALK|REP,		Elevator_MoveToFloor,		 { TAG, ELEVATORSPEED } },
/* 237 */ { USE,			Elevator_MoveToFloor,		 { TAG, ELEVATORSPEED } },
/* 238 */ { USE|REP,		Elevator_MoveToFloor,		 { TAG, ELEVATORSPEED } },
/* 239 */ { WALK,			Floor_TransferNumeric,		 { TAG } },
/* 240 */ { WALK|REP,		Floor_TransferNumeric,		 { TAG } },
/* 241 */ { USE,			Floor_TransferNumeric,		 { TAG } },
/* 242 */ { 0,				Transfer_Heights,			 { TAG } },
/* 243 */ { WALK|MONST,		Teleport_Line,				 { TAG, TAG, 0 } },
/* 244 */ { WALK|REP|MONST,	Teleport_Line,				 { TAG, TAG, 0 } },
/* 245 */ { 0,				Scroll_Ceiling,				 { TAG, 5, 0, 0, 0 } },
/* 246 */ { 0,				Scroll_Floor,				 { TAG, 5, 0, 0, 0 } },
/* 247 */ { 0,				Scroll_Floor,				 { TAG, 5, 1, 0, 0 } },
/* 248 */ { 0,				Scroll_Floor,				 { TAG, 5, 2, 0, 0 } },
/* 249 */ { 0,				Scroll_Texture_Model,		 { TAG, 1 } },
/* 250 */ { 0,				Scroll_Ceiling,				 { TAG, 4, 0, 0, 0 } },
/* 251 */ { 0,				Scroll_Floor,				 { TAG, 4, 0, 0, 0 } },
/* 252 */ { 0,				Scroll_Floor,				 { TAG, 4, 1, 0, 0 } },
/* 253 */ { 0,				Scroll_Floor,				 { TAG, 4, 2, 0, 0 } },
/* 254 */ { 0,				Scroll_Texture_Model,		 { TAG, 0 } },
/* 255 */ { 0,				Scroll_Texture_Offsets },
/* 256 */ { WALK|REP,		Stairs_BuildUpDoom,			 { TAG, S_SLOW, 8, 0, 0 } },
/* 257 */ { WALK|REP,		Stairs_BuildUpDoom,			 { TAG, S_TURBO, 16, 0, 0 } },
/* 258 */ { USE|REP,		Stairs_BuildUpDoom,			 { TAG, S_SLOW, 8, 0, 0 } },
/* 259 */ { USE|REP,		Stairs_BuildUpDoom,			 { TAG, S_TURBO, 16, 0, 0 } },
/* 260 */ { 0,				TranslucentLine,			 { TAG, 128 } },
/* 261 */ { 0,				Transfer_CeilingLight,		 { TAG } },
/* 262 */ { WALK|MONST,		Teleport_Line,				 { TAG, TAG, 1 } },
/* 263 */ { WALK|REP|MONST,	Teleport_Line,				 { TAG, TAG, 1 } },
/* 264 */ { MONWALK,		Teleport_Line,				 { TAG, TAG, 1 } },
/* 265 */ { MONWALK|REP,	Teleport_Line,				 { TAG, TAG, 1 } },
/* 266 */ { MONWALK,		Teleport_Line,				 { TAG, TAG, 0 } },
/* 267 */ { MONWALK|REP,	Teleport_Line,				 { TAG, TAG, 0 } },
/* 268 */ { MONWALK,		Teleport_NoFog,				 { TAG } },
/* 269 */ { MONWALK|REP,	Teleport_NoFog,				 { TAG } }
};
#define NUM_SPECIALS 269

void P_TranslateLineDef (line_t *ld, maplinedef_t *mld)
{
	short special = SHORT(mld->special);
	short tag = SHORT(mld->tag);
	short flags = SHORT(mld->flags);
	int i;

	if (flags & ML_PASSUSEORG)	// Remap ML_PASSUSE flag from BOOM.
		flags = (flags & ~ML_PASSUSEORG) | ML_PASSUSE;

	flags = flags & 0x41ff;	// Ignore flags unknown in DOOM.

	if (special <= NUM_SPECIALS) {
		// This is a regular special; translate thru LUT
		flags = flags | (SpecialTranslation[special].flags << 8);
		ld->special = SpecialTranslation[special].newspecial;
		for (i = 0; i < 5; i++)
			ld->args[i] = SpecialTranslation[special].args[i] == TAG ? tag :
						  SpecialTranslation[special].args[i];
	} else if (special <= GenCrusherBase) {
		// This is an unknown special. Just zero it.
		ld->special = 0;
		memset (ld->args, 0, sizeof(ld->args));
	} else {
		// Anything else is a BOOM generalized linedef type
		switch (special & 0x0007) {
			case WalkMany:
				flags |= ML_REPEATABLE;
			case WalkOnce:
				flags |= ML_ACTIVATECROSS;
				break;

			case SwitchMany:
			case PushMany:
				flags |= ML_REPEATABLE;
			case SwitchOnce:
			case PushOnce:
				flags |= ML_ACTIVATEUSE;
				break;

			case GunMany:
				flags |= ML_REPEATABLE;
			case GunOnce:
				flags |= ML_ACTIVATEPROJECTILEHIT;
				break;
		}

		// We treat push triggers like switch triggers with zero tags.
		if ((special & 0x0007) == PushMany ||
			(special & 0x0007) == PushOnce)
			ld->args[0] = 0;
		else
			ld->args[0] = tag;

		if (special <= GenStairsBase) {
			// Generalized crusher (tag, dnspeed, upspeed, silent, damage)
			ld->special = Generic_Crusher;
			if (special & 0x0020)
				flags |= ML_MONSTERSCANACTIVATE;
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = C_SLOW;	break;
				case 0x0008:	ld->args[1] = C_NORMAL;	break;
				case 0x0010:	ld->args[1] = C_FAST;	break;
				case 0x0018:	ld->args[1] = C_TURBO;	break;
			}
			ld->args[2] = ld->args[1];
			ld->args[3] = (special & 0x0040) >> 6;
			ld->args[4] = 10;
		} else if (special <= GenLiftBase) {
			// Generalized stairs (tag, speed, step, dir/igntxt, reset)
			ld->special = Generic_Stairs;
			if (special & 0x0020)
				flags |= ML_MONSTERSCANACTIVATE;
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = S_SLOW;	break;
				case 0x0008:	ld->args[1] = S_NORMAL;	break;
				case 0x0010:	ld->args[1] = S_FAST;	break;
				case 0x0018:	ld->args[1] = S_TURBO;	break;
			}
			switch (special & 0x00c0) {
				case 0x0000:	ld->args[2] = 4;		break;
				case 0x0040:	ld->args[2] = 8;		break;
				case 0x0080:	ld->args[2] = 16;		break;
				case 0x00c0:	ld->args[2] = 24;		break;
			}
			ld->args[3] = (special & 0x0300) >> 8;
			ld->args[4] = 0;
		} else if (special <= GenLockedBase) {
			// Generalized lift (tag, speed, delay, target, height)
			ld->special = Generic_Lift;
			if (special & 0x0020)
				flags |= ML_MONSTERSCANACTIVATE;
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = P_SLOW*2;		break;
				case 0x0008:	ld->args[1] = P_NORMAL*2;	break;
				case 0x0010:	ld->args[1] = P_FAST*2;		break;
				case 0x0018:	ld->args[1] = P_TURBO*2;	break;
			}
			switch (special & 0x00c0) {
				case 0x0000:	ld->args[2] = 8;		break;
				case 0x0040:	ld->args[2] = 24;		break;
				case 0x0080:	ld->args[2] = 40;		break;
				case 0x00c0:	ld->args[2] = 80;		break;
			}
			ld->args[3] = ((special & 0x0300) >> 8) + 1;
			ld->args[4] = 0;
		} else if (special <= GenDoorBase) {
			// Generalized locked door (tag, speed, kind, delay, lock)
			ld->special = Generic_Door;
			if (special & 0x0080)
				flags |= ML_MONSTERSCANACTIVATE;
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = D_SLOW;	break;
				case 0x0008:	ld->args[1] = D_NORMAL;	break;
				case 0x0010:	ld->args[1] = D_FAST;	break;
				case 0x0018:	ld->args[1] = D_TURBO;	break;
			}
			ld->args[2] = (special & 0x0020) >> 5;
			ld->args[3] = 0;
			ld->args[4] = (special & 0x01c0) >> 6;
			if (ld->args[4] == 0)
				ld->args[4] = AnyKey;
			else if (ld->args[4] == 7)
				ld->args[4] = AllKeys;
			ld->args[4] |= (special & 0x0200) >> 2;
		} else if (special <= GenCeilingBase) {
			// Generalized door (tag, speed, kind, delay, lock)
			ld->special = Generic_Door;
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = D_SLOW;	break;
				case 0x0008:	ld->args[1] = D_NORMAL;	break;
				case 0x0010:	ld->args[1] = D_FAST;	break;
				case 0x0018:	ld->args[1] = D_TURBO;	break;
			}
			ld->args[2] = (special & 0x0060) >> 5;
			switch (special & 0x0300) {
				case 0x0000:	ld->args[3] = 8;		break;
				case 0x0100:	ld->args[3] = 32;		break;
				case 0x0200:	ld->args[3] = 72;		break;
				case 0x0300:	ld->args[3] = 240;		break;
			}
			ld->args[4] = 0;
		} else {
			// Generalized ceiling (tag, speed, height, target, change/model/direct/crush)
			// Generalized floor (tag, speed, height, target, change/model/direct/crush)
			if (special <= GenFloorBase)
				ld->special = Generic_Ceiling;
			else
				ld->special = Generic_Floor;
			
			switch (special & 0x0018) {
				case 0x0000:	ld->args[1] = F_SLOW;	break;
				case 0x0008:	ld->args[1] = F_NORMAL;	break;
				case 0x0010:	ld->args[1] = F_FAST;	break;
				case 0x0018:	ld->args[1] = F_TURBO;	break;
			}
			ld->args[3] = ((special & 0x0380) >> 7) + 1;
			if (ld->args[3] >= 7) {
				ld->args[2] = 24 + (ld->args[3] - 7) * 8;
				ld->args[3] = 0;
			} else {
				ld->args[2] = 0;
			}
			ld->args[4] = ((special & 0x0c00) >> 10) |
						  ((special & 0x0060) >> 2) |
						  ((special & 0x1000) >> 8);
		}
	}

	ld->flags = flags;

	// For purposes of maintaining BOOM compatibility, each
	// line also needs to have its ID set to the same as its tag.
	// An external conversion program would need to do this more
	// intelligently.

	ld->id = tag;
}

// The teleport specials that use things as destinations also require
// that their TIDs be set to the tags of their containing sectors. We
// do that after the rest of the level has been loaded.

void P_TranslateTeleportThings (void)
{
	int i, j;
	thinker_t *thinker;
	mobj_t *m;

	for (i = 0; i < numlines; i++) {
		if (lines[i].special == Teleport ||
			lines[i].special == Teleport_NoFog) {
			// The sector tag hash table hasn't been set up yet,
			// so we need to use this linear search.
			for (j = 0; j < numsectors; j++) {
				if (sectors[j].tag == lines[i].args[0]) {
					thinker = thinkercap.next;
					for (thinker = thinkercap.next;
						 thinker != &thinkercap;
						 thinker = thinker->next)
					{
						// not a mobj
						if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
							continue;

						m = (mobj_t *)thinker;
						
						// not a teleportman
						if (m->type != MT_TELEPORTMAN && m->type != MT_TELEPORTMAN2)
							continue;

						// wrong sector
						if (m->subsector->sector - sectors != j)
							continue;

						// It's a teleportman, so set it's tid to match
						// the sector's tag.
						m->tid = lines[i].args[0];
						P_AddMobjToHash (m);

						// We only bother with the first teleportman
						break;
					}
					if (thinker != &thinkercap)
						break;
				}
			}
		}
	}
}

int P_TranslateSectorSpecial (int special)
{
	// This supports phased lighting with specials 21-24
	return (special == 9) ? SECRET_MASK :
			((special & 0xfe0) << 3) |
			((special & 0x01f) + (((special & 0x1f) < 21) ? 64 : -20));
}