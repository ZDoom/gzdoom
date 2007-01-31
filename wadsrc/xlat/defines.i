include "xlat/specials.i"

// Not all specials are accessible from ACS, so define those here
special
	 50:ExtraFloor_LightOnly(),
	100:Scroll_Texture_Left(),
	101:Scroll_Texture_Right(),
	102:Scroll_Texture_Up(),
	103:Scroll_Texture_Down(),
	121:Line_SetIdentification(),
	181:Plane_Align(),
	190:Static_Init(),
	209:Transfer_Heights(),
	210:Transfer_FloorLight(),
	211:Transfer_CeilingLight(),
	219:Sector_SetFriction(),
	222:Scroll_Texture_Model(),
	225:Scroll_Texture_Offsets(),
	227:PointPush_SetForce()
;

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
