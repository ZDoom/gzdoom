// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:  none
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//
//-----------------------------------------------------------------------------


#ifndef __P_SPEC__
#define __P_SPEC__


//jff 2/23/98 identify the special classes that can share sectors

typedef enum
{
	floor_special,
	ceiling_special,
	lighting_special,
} special_e;

// killough 3/7/98: Add generalized scroll effects

typedef struct {
	thinker_t thinker;		// Thinker structure for scrolling
	fixed_t dx, dy;			// (dx,dy) scroll speeds
	int affectee;			// Number of affected sidedef, sector, tag, or whatever
	int control;			// Control sector (-1 if none) used to control scrolling
	fixed_t last_height;	// Last known height of control sector
	fixed_t vdx, vdy;		// Accumulated velocity if accelerative
	int accel;				// Whether it's accelerative
	enum
	{
		sc_side,
		sc_floor,
		sc_ceiling,
		sc_carry,
		sc_carry_ceiling,	// killough 4/11/98: carry objects hanging on ceilings
	} type;					// Type of scroll effect
} scroll_t;

// phares 3/12/98: added new model of friction for ice/sludge effects

typedef struct {
	thinker_t thinker;		// Thinker structure for friction
	int friction;			// friction value (E800 = normal)
	int movefactor;			// inertia factor when adding to momentum
	int affectee;			// Number of affected sector
} friction_t;

// phares 3/20/98: added new model of Pushers for push/pull effects

typedef struct {
	thinker_t thinker;		// Thinker structure for Pusher
	enum
	{
		p_push,
		p_pull,
		p_wind,
		p_current,
	} type;
	mobj_t* source;			// Point source if point pusher
	int x_mag;				// X Strength
	int y_mag;				// Y Strength
	int magnitude;			// Vector strength for point pusher
	int radius;				// Effective radius for point pusher
	int x;					// X of point source if point pusher
	int y;					// Y of point source if point pusher
	int affectee;			// Number of affected sector
} pusher_t;

// [RH] Types of keys used by locked doors and scripts
typedef enum
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
} keytype_t;

BOOL P_CheckKeys (player_t *p, keytype_t lock, BOOL remote);

// Define values for map objects
#define MO_TELEPORTMAN			14


// [RH] If a deathmatch game, checks to see if noexit is enabled.
//		If so, it kills the player and returns false. Otherwise,
//		it returns true, and the player is allowed to live.
BOOL	CheckIfExitIsGood (mobj_t *self);

// at game start
void	P_InitPicAnims (void);

// at map load
void	P_SpawnSpecials (void);

// every tic
void	P_UpdateSpecials (void);

// when needed
BOOL	P_UseSpecialLine (mobj_t *thing, line_t *line, int side);
void	P_ShootSpecialLine (mobj_t *thing, line_t *line);
void	P_ShotCrossSpecialLine (mobj_t *thing, line_t *line);	// [RH]
void	P_CrossSpecialLine (int linenum, int side, mobj_t *thing);
BOOL	P_PushSpecialLine (mobj_t *thing, int side, line_t *line);	// [RH]

void	P_PlayerInSpecialSector (player_t *player);

int		twoSided (int sector, int line);

sector_t *getSector (int currentSector, int line, int side);

side_t	*getSide (int currentSector, int line, int side);

fixed_t	P_FindLowestFloorSurrounding (sector_t *sec);
fixed_t	P_FindHighestFloorSurrounding (sector_t *sec);

fixed_t	P_FindNextHighestFloor (sector_t *sec, int currentheight);
fixed_t P_FindNextLowestFloor (sector_t* sec, int currentheight);

fixed_t	P_FindLowestCeilingSurrounding (sector_t *sec);		// jff 2/04/98
fixed_t	P_FindHighestCeilingSurrounding (sector_t *sec);	// jff 2/04/98

fixed_t P_FindNextLowestCeiling (sector_t *sec, int currentheight);		// jff 2/04/98
fixed_t P_FindNextHighestCeiling (sector_t *sec, int currentheight);	// jff 2/04/98

fixed_t P_FindShortestTextureAround (int secnum);	// jff 2/04/98
fixed_t P_FindShortestUpperAround (int secnum);		// jff 2/04/98

sector_t* P_FindModelFloorSector (fixed_t floordestheight, int secnum);	//jff 02/04/98
sector_t* P_FindModelCeilingSector (fixed_t ceildestheight, int secnum);	//jff 02/04/98 

int		P_FindSectorFromTag (int tag, int start);
int		P_FindLineFromID (int id, int start);

int		P_FindMinSurroundingLight (sector_t *sector, int max);

sector_t *getNextSector (line_t *line, sector_t *sec);
sector_t *P_NextSpecialSector (sector_t *sec, int type, sector_t *back2);	// [RH]

void	T_Scroll (scroll_t *);		// killough 3/7/98: scroll effect thinker
void	T_Friction (friction_t *);	// phares 3/12/98: friction thinker
void	T_Pusher (pusher_t *);		// phares 3/20/98: Push thinker


//
// SPECIAL
//
int EV_DoDonut (int tag, fixed_t pillarspeed, fixed_t slimespeed);



//
// P_LIGHTS
//
typedef struct
{
	thinker_t	thinker;
	sector_t*	sector;
	int 		count;
	int 		maxlight;
	int 		minlight;
} fireflicker_t;



typedef struct
{
	thinker_t	thinker;
	sector_t*	sector;
	int 		count;
	int 		maxlight;
	int 		minlight;
	int 		maxtime;
	int 		mintime;
} lightflash_t;



typedef struct
{
	thinker_t	thinker;
	sector_t*	sector;
	int 		count;
	int 		minlight;
	int 		maxlight;
	int 		darktime;
	int 		brighttime;
} strobe_t;




typedef struct
{
	thinker_t	thinker;
	sector_t*	sector;
	int 		minlight;
	int 		maxlight;
	int 		direction;
} glow_t;

// [RH] Glow from Light_Glow and Light_Fade specials
typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			start;
	int			end;
	int			maxtics;
	int			tics;
	BOOL		oneshot;
} glow2_t;

// [RH] Phased light thinker
typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	byte		baselevel;
	byte		phase;
} phased_t;

#define GLOWSPEED				8
#define STROBEBRIGHT			5
#define FASTDARK				15
#define SLOWDARK				TICRATE

void	T_FireFlicker (fireflicker_t *flick);
void	P_SpawnFireFlicker (sector_t *sector);
void	T_LightFlash (lightflash_t *flash);
void	P_SpawnLightFlash (sector_t *sector, int min, int max);
void	T_StrobeFlash (strobe_t *flash);

void	P_SpawnStrobeFlash (sector_t *sector, int upper, int lower,
							int utics, int ltics, int inSync);

void	EV_StartLightFlashing (int tag, int upper, int lower);
void	EV_StartLightStrobing (int tag, int upper, int lower,
							   int utics, int ltics);
void	EV_TurnTagLightsOff (int tag);
void	EV_LightTurnOn (int tag, int bright);
void	EV_LightChange (int tag, int value);

void	T_Glow (glow_t *g);
void	P_SpawnGlowingLight (sector_t *sector);

void	T_Glow2 (glow2_t *g);
void	EV_StartLightGlowing (int tag, int upper, int lower, int tics);
void	EV_StartLightFading (int tag, int value, int tics);

void	T_PhasedLight (phased_t *l);
void	P_SpawnLightSequence (sector_t *sector);
void	P_SpawnLightPhased (sector_t *sector);


//
// P_SWITCH
//
typedef struct
{
	char		name1[9];
	char		name2[9];
	short		episode;
	
} switchlist_t;


typedef enum
{
	top,
	middle,
	bottom

} bwhere_e;


typedef struct button_s
{
	struct button_s *next;	// [RH] make buttons a singly-linked list
	line_t* 	line;
	bwhere_e	where;
	int 		btexture;
	int 		btimer;
	mobj_t* 	soundorg;

} button_t;




 // 1 second, in ticks. 
#define BUTTONTIME		TICRATE

extern button_t *buttonlist; 

void	P_ChangeSwitchTexture (line_t *line, int useAgain);

void	P_InitSwitchList(void);


//
// P_PLATS
//
typedef enum
{
	up,
	down,
	waiting,
	in_stasis

} plat_e;



typedef enum
{
	// [RH] Changed these
	platPerpetualRaise,
	platDownWaitUpStay,
	platUpWaitDownStay,
	platDownByValue,
	platUpByValue,
	platUpByValueStay,
	platRaiseAndStay,
	platToggle,
	platDownToNearestFloor,
	platDownToLowestCeiling

} plattype_e;



typedef struct plat_s
{
	thinker_t	thinker;

	// [RH] Added next and prev links
	struct plat_s *next, *prev;

	sector_t*	sector;
	fixed_t 	speed;
	fixed_t 	low;
	fixed_t 	high;
	int 		wait;
	int 		count;
	plat_e		status;
	plat_e		oldstatus;
	BOOL 		crush;
	int 		tag;
	plattype_e	type;
	
} plat_t;



extern plat_t *activeplats;

void	T_PlatRaise(plat_t *plat);

BOOL	EV_DoPlat (int tag, line_t *line, plattype_e type, int height,
				   int speed, int delay, int lip, int change);

void	P_AddActivePlat (plat_t *plat);
void	P_RemoveActivePlat (plat_t *plat);
void	EV_StopPlat (int tag);
void	P_ActivateInStasis (int tag);


//
// [RH]
// P_PILLAR
//
typedef enum
{
	pillarBuild,
	pillarOpen

} pillar_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	pillar_e	type;
	fixed_t		floorspeed;
	fixed_t		ceilingspeed;
	fixed_t		floortarget;
	fixed_t		ceilingtarget;
	int			crush;

} pillar_t;

BOOL EV_DoPillar (pillar_e type, int tag, fixed_t speed, fixed_t height,
				  fixed_t height2, int crush);

void T_Pillar (pillar_t *pillar);

//
// P_DOORS
//
typedef enum
{
	// [RH] Changed for new specials
	doorClose,
	doorOpen,
	doorRaise,
	doorRaiseIn5Mins,
	doorCloseWaitOpen,

} vldoor_e;

typedef struct
{
	thinker_t	thinker;
	vldoor_e	type;
	sector_t*	sector;
	fixed_t 	topheight;
	fixed_t 	speed;

	// 1 = up, 0 = waiting at top, -1 = down
	int 		direction;
	
	// tics to wait at the top
	int 		topwait;
	// (keep in case a door going down is reset)
	// when it reaches 0, start going down
	int 		topcountdown;
	
} vldoor_t;


BOOL	EV_DoDoor (vldoor_e type, line_t *line, mobj_t *thing,
				   int tag, int speed, int delay, keytype_t lock);

void	T_VerticalDoor (vldoor_t *door);
void	P_SpawnDoorCloseIn30 (sector_t *sec);

void	P_SpawnDoorRaiseIn5Mins (sector_t *sec);



//
// P_CEILNG
//

// [RH] Changed these
typedef enum
{
	ceilLowerByValue,
	ceilRaiseByValue,
	ceilMoveToValue,
	ceilLowerToHighestFloor,
	ceilLowerInstant,
	ceilRaiseInstant,
	ceilCrushAndRaise,
	ceilLowerAndCrush,
	ceilCrushRaiseAndStay,
	ceilRaiseToNearest,
	ceilLowerToLowest,
	ceilLowerToFloor,

	// The following are only used by Generic_Ceiling
	ceilRaiseToHighest,
	ceilLowerToHighest,
	ceilRaiseToLowest,
	ceilLowerToNearest,
	ceilRaiseToHighestFloor,
	ceilRaiseToFloor,
	ceilRaiseByTexture,
	ceilLowerByTexture,

	genCeilingChg0,
	genCeilingChgT,
	genCeilingChg

} ceiling_e;



typedef struct ceiling_s
{
	thinker_t	thinker;

	// [RH] Added next and prev links
	struct ceiling_s *next, *prev;

	ceiling_e	type;
	sector_t*	sector;
	fixed_t 	bottomheight;
	fixed_t 	topheight;
	fixed_t 	speed;
	fixed_t		speed1;		// [RH] dnspeed of crushers
	fixed_t		speed2;		// [RH] upspeed of crushers
	int 		crush;
	int			silent;		// [RH] 1=noise at stops, 2=no noise at all

	// 1 = up, 0 = waiting, -1 = down
	int 		direction;

	// [RH] Need these for BOOM-ish transfering ceilings
	int			texture;
	int			newspecial;

	// ID
	int 		tag;
	int 		olddirection;
	
} ceiling_t;





extern ceiling_t *activeceilings;

BOOL	EV_DoCeiling (ceiling_e type, line_t *line,
					  int tag, fixed_t speed, fixed_t speed2, fixed_t height,
					  int crush, int silent, int change);

void	T_MoveCeiling (ceiling_t *ceiling);
void	P_AddActiveCeiling (ceiling_t *c);
void	P_RemoveActiveCeiling (ceiling_t *c);
BOOL 	EV_CeilingCrushStop (int tag);
void	P_ActivateInStasisCeiling(int tag);


//
// P_FLOOR
//
// [RH] Changed these enums
typedef enum
{
	floorLowerToLowest,
	floorLowerToNearest,
	floorLowerToHighest,
	floorLowerByValue,
	floorRaiseByValue,
	floorRaiseToHighest,
	floorRaiseToNearest,
	floorRaiseAndCrush,
	floorCrushStop,
	floorLowerInstant,
	floorRaiseInstant,
	floorMoveToValue,
	floorRaiseToLowestCeiling,
	floorRaiseByTexture,

	floorLowerAndChange,
	floorRaiseAndChange,

	floorRaiseToLowest,
	floorRaiseToCeiling,
	floorLowerToLowestCeiling,
	floorLowerByTexture,
	floorLowerToCeiling,
	 
	donutRaise,

	buildStair,
	waitStair,
	resetStair,

	// Not to be used as parameters to EV_DoFloor()
	genFloorChg0,
	genFloorChgT,
	genFloorChg

} floor_e;

//jff 3/15/98 pure texture/type change for better generalized support
typedef enum
{
	trigChangeOnly,
	numChangeOnly,
} change_e;

// [RH] Changed to use Hexen-ish specials
typedef enum
{
	buildUp,
	buildDown
	
} stair_e;

typedef enum
{
	elevateUp,
	elevateDown,
	elevateCurrent,
	// [RH] For FloorAndCeiling_Raise/Lower
	elevateRaise,
	elevateLower
} elevator_e;

typedef struct
{
	thinker_t	thinker;
	floor_e 	type;
	int 		crush;
	sector_t*	sector;
	int 		direction;
	short 		newspecial;
	short		texture;
	fixed_t 	floordestheight;
	fixed_t 	speed;

	// [RH] New parameters use to reset and delayed stairs
	int			resetcount;
	int			orgheight;
	int			delay;
	int			pausetime;
	int			steptime;
	int			persteptime;

} floormove_t;

typedef struct
{
	thinker_t	thinker;
	elevator_e	type;
	sector_t*	sector;
	int			direction;
	fixed_t		floordestheight;
	fixed_t		ceilingdestheight;
	fixed_t		speed;
} elevator_t;

typedef struct {
	thinker_t   thinker;
	sector_t   *sector;
	int			x;
	int			amp;
	int			freq;
	int			timetodie;
	fixed_t		baseline;
	int			stage;
	int			ampfactor;
} waggle_t;


typedef enum
{
	ok,
	crushed,
	pastdest
	
} result_e;

result_e T_MovePlane (sector_t *sector, fixed_t speed, fixed_t dest,
					  int crush, int floorOrCeiling, int direction);

BOOL EV_DoElevator (line_t *line, ceiling_e type, fixed_t speed, fixed_t height, int tag);

BOOL EV_BuildStairs (int tag, stair_e type, line_t *line,
					 fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
					 int usespecials);

BOOL EV_DoFloor (floor_e floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change);
BOOL EV_FloorCrushStop (int tag);
BOOL EV_DoChange (line_t *line, change_e changetype, int tag);
BOOL EV_DoFloorWaggle (int tag, fixed_t amplitude, fixed_t speed, int delay, int count);

void T_Waggle (waggle_t *w);
void T_MoveFloor (floormove_t *floor);
void T_MoveElevator (elevator_t *elevator);


//
// P_TELEPT
//
BOOL EV_Teleport (int tid, int side, mobj_t *thing);
BOOL EV_SilentTeleport (int tid, line_t *line, int side, mobj_t *thing);
BOOL EV_SilentLineTeleport (line_t *line, int side, mobj_t *thing, int id,
							BOOL reverse);


int P_SectorActive (special_e t, sector_t *s);	// [RH] from BOOM


//
// [RH] ACS (see also p_acs.h)
//

void P_ClearScripts (void);

BOOL P_StartScript (mobj_t *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always);
void P_SuspendScript (int script, char *map);
void P_TerminateScript (int script, char *map);
void P_StartOpenScripts (void);
void P_DoDeferedScripts (void);

//
// [RH] p_quake.c
//
typedef struct quake_s {
	struct quake_s *next;
	mobj_t *quakespot;
	fixed_t tremorbox[4];
	fixed_t damagebox[4];
	int intensity;
	int countdown;
} quake_t;

extern quake_t *ActiveQuakes;

void P_RunQuakes (void);
BOOL P_StartQuake (int tid, int intensity, int duration, int damrad, int tremrad);

#endif
