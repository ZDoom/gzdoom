
// Static_Init types
enum
{
	Init_Gravity,
	Init_Color,
	Init_Damage,
	Init_TransferSky = 255
}

// Speeds for ceilings/crushers (x/8 units per tic)
//	These are the BOOM names.
enum
{
	C_SLOW		= 8,
	C_NORMAL	= 16,
	C_FAST		= 32,
	C_TURBO		= 64
}

define CEILWAIT			(150)

// Speeds for floors (x/8 units per tic)
enum
{
	F_SLOW		= 8,
	F_NORMAL	= 16,
	F_FAST		= 32,
	F_TURBO		= 64
}

// Speeds for doors (x/8 units per tic)
enum
{
	D_SLOW		= 16,
	D_NORMAL	= 32,
	D_FAST		= 64,
	D_TURBO		= 128
}

define VDOORWAIT		(150)


// Speeds for stairs (x/8 units per tic)
enum
{
	ST_SLOW		= 2,
	ST_NORMAL	= 4,
	ST_FAST		= 16,
	ST_TURBO	= 32
}

// Speeds for plats (Hexen plats stop 8 units above the floor)
enum
{
	P_SLOW		= 8,
	P_NORMAL	= 16,
	P_FAST		= 32,
	P_TURBO		= 64
}

define PLATWAIT			(105)

define ELEVATORSPEED	(32)

// Speeds for donut slime and pillar (x/8 units per tic)
define DORATE			(4)

// Texture scrollers operate at a rate of x/64 units per tic.
define SCROLL_UNIT		(64)

define WALK				(0)
define USE				(2)
define SHOOT			(6)
define MONST			(16)
define MONWALK			(4)
define REP				(1)
define FIRSTSIDE		(32)

enum
{
	NoKey,
	RCard,
	BCard,
	YCard,
	RSkull,
	BSkull,
	YSkull,

	AnyKey = 100,
	AllKeys = 101,
	CardIsSkull = 128
}


enum 
{
	Light_Phased = 1,
	LightSequenceStart = 2,
	LightSequenceSpecial1 = 3,
	LightSequenceSpecial2 = 4,

	Stairs_Special1 = 26,
	Stairs_Special2 = 27,
	
	Wind_East_Weak=40,
	Wind_East_Medium,
	Wind_East_Strong,
	Wind_North_Weak,
	Wind_North_Medium,
	Wind_North_Strong,
	Wind_South_Weak,
	Wind_South_Medium,
	Wind_South_Strong,
	Wind_West_Weak,
	Wind_West_Medium,
	Wind_West_Strong,

	// [RH] Equivalents for DOOM's sector specials
	dLight_Flicker = 65,
	dLight_StrobeFast = 66,
	dLight_StrobeSlow = 67,
	dLight_Strobe_Hurt = 68,
	dDamage_Hellslime = 69,
	dDamage_Nukage = 71,
	dLight_Glow = 72,
	dSector_DoorCloseIn30 = 74,
	dDamage_End = 75,
	dLight_StrobeSlowSync = 76,
	dLight_StrobeFastSync = 77,
	dSector_DoorRaiseIn5Mins = 78,
	dFriction_Low = 79,
	dDamage_SuperHellslime = 80,
	dLight_FireFlicker = 81,
	dDamage_LavaWimpy = 82,
	dDamage_LavaHefty = 83,
	dScroll_EastLavaDamage = 84,
	hDamage_Sludge = 85,
	Sector_Outside = 87,

	// And here are some for Strife
	sLight_Strobe_Hurt = 104,
	sDamage_Hellslime = 105,
	Damage_InstantDeath = 115,
	sDamage_SuperHellslime = 116,
	Scroll_StrifeCurrent = 118,

	// Caverns of Darkness healing sector
	Sector_Heal = 196,

	Light_OutdoorLightning = 197,
	Light_IndoorLightning1 = 198,
	Light_IndoorLightning2 = 199,

	Sky2 = 200,

	// Hexen-type scrollers
	Scroll_North_Slow = 201,
	Scroll_North_Medium,
	Scroll_North_Fast,
	Scroll_East_Slow,
	Scroll_East_Medium,
	Scroll_East_Fast,
	Scroll_South_Slow,
	Scroll_South_Medium,
	Scroll_South_Fast,
	Scroll_West_Slow,
	Scroll_West_Medium,
	Scroll_West_Fast,
	Scroll_NorthWest_Slow,
	Scroll_NorthWest_Medium,
	Scroll_NorthWest_Fast,
	Scroll_NorthEast_Slow,
	Scroll_NorthEast_Medium,
	Scroll_NorthEast_Fast,
	Scroll_SouthEast_Slow,
	Scroll_SouthEast_Medium,
	Scroll_SouthEast_Fast,
	Scroll_SouthWest_Slow,
	Scroll_SouthWest_Medium,
	Scroll_SouthWest_Fast,

	// Heretic-type scrollers
	Carry_East5 = 225,
	Carry_East10,
	Carry_East25,
	Carry_East30,
	Carry_East35,
	Carry_North5,
	Carry_North10,
	Carry_North25,
	Carry_North30,
	Carry_North35,
	Carry_South5,
	Carry_South10,
	Carry_South25,
	Carry_South30,
	Carry_South35,
	Carry_West5,
	Carry_West10,
	Carry_West25,
	Carry_West30,
	Carry_West35
}

// [RH] Equivalents for BOOM's generalized sector types
define DAMAGE_MASK (0x0300)
define SECRET_MASK (0x0400)
define FRICTION_MASK (0x0800)
define PUSH_MASK (0x1000)


enum
{
	ML_BLOCKING					= 0x00000001,
	ML_BLOCKMONSTERS			= 0x00000002,
	ML_TWOSIDED					= 0x00000004,
	ML_DONTPEGTOP				= 0x00000008,
	ML_DONTPEGBOTTOM			= 0x00000010,
	ML_SECRET					= 0x00000020,
	ML_SOUNDBLOCK				= 0x00000040,
	ML_DONTDRAW 				= 0x00000080,
	ML_MAPPED					= 0x00000100,

	// Extended flags
	ML_MONSTERSCANACTIVATE		= 0x00002000,
	ML_BLOCK_PLAYERS			= 0x00004000,
	ML_BLOCKEVERYTHING			= 0x00008000,
	ML_ZONEBOUNDARY				= 0x00010000,
	ML_RAILING					= 0x00020000,
	ML_BLOCK_FLOATERS			= 0x00040000,
	ML_CLIP_MIDTEX				= 0x00080000,
	ML_WRAP_MIDTEX				= 0x00100000,
	ML_3DMIDTEX					= 0x00200000,
	ML_CHECKSWITCHRANGE			= 0x00400000,
	ML_FIRSTSIDEONLY			= 0x00800000,
	ML_BLOCKPROJECTILE			= 0x01000000,
	ML_BLOCKUSE					= 0x02000000,
	
	// 
	ML_PASSTHROUGH				= -1,
	ML_TRANSLUCENT				= -2
}