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

#include "dsectoreffect.h"

//jff 2/23/98 identify the special classes that can share sectors

typedef enum
{
	floor_special,
	ceiling_special,
	lighting_special,
} special_e;

// killough 3/7/98: Add generalized scroll effects

class DScroller : public DThinker
{
	DECLARE_SERIAL (DScroller, DThinker)
public:
	enum EScrollType
	{
		sc_side,
		sc_floor,
		sc_ceiling,
		sc_carry,
		sc_carry_ceiling	// killough 4/11/98: carry objects hanging on ceilings

	};
	
	DScroller (EScrollType type, fixed_t dx, fixed_t dy, int control, int affectee, int accel);
	DScroller (fixed_t dx, fixed_t dy, const line_t *l, int control, int accel);

	void RunThink ();

	bool AffectsWall (int wallnum) { return m_Type == sc_side && m_Affectee == wallnum; }
	int GetWallNum () { return m_Type == sc_side ? m_Affectee : -1; }
	void SetRate (fixed_t dx, fixed_t dy) { m_dx = dx; m_dy = dy; }

protected:
	EScrollType m_Type;		// Type of scroll effect
	fixed_t m_dx, m_dy;		// (dx,dy) scroll speeds
	int m_Affectee;			// Number of affected sidedef, sector, tag, or whatever
	int m_Control;			// Control sector (-1 if none) used to control scrolling
	fixed_t m_LastHeight;	// Last known height of control sector
	fixed_t m_vdx, m_vdy;	// Accumulated velocity if accelerative
	int m_Accel;			// Whether it's accelerative

private:
	DScroller ();
};

inline FArchive &operator<< (FArchive &arc, DScroller::EScrollType type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DScroller::EScrollType &out)
{
	BYTE in; arc >> in; out = (DScroller::EScrollType)in; return arc;
}

// phares 3/20/98: added new model of Pushers for push/pull effects

class DPusher : public DThinker
{
	DECLARE_SERIAL (DPusher, DThinker)
public:
	enum EPusher
	{
		p_push,
		p_pull,
		p_wind,
		p_current
	};

	DPusher ();
	DPusher (EPusher type, line_t *l, int magnitude, int angle, AActor *source, int affectee);
	int CheckForSectorMatch (EPusher type, int tag)
	{
		if (m_Type == type && sectors[m_Affectee].tag == tag)
			return m_Affectee;
		else
			return -1;
	}
	void ChangeValues (int magnitude, int angle)
	{
		angle_t ang = (angle<<24) >> ANGLETOFINESHIFT;
		m_Xmag = (magnitude * finecosine[ang]) >> FRACBITS;
		m_Ymag = (magnitude * finesine[ang]) >> FRACBITS;
		m_Magnitude = magnitude;
	}

	void RunThink ();

protected:
	EPusher m_Type;
	AActor *m_Source;		// Point source if point pusher
	int m_Xmag;				// X Strength
	int m_Ymag;				// Y Strength
	int m_Magnitude;		// Vector strength for point pusher
	int m_Radius;			// Effective radius for point pusher
	int m_X;				// X of point source if point pusher
	int m_Y;				// Y of point source if point pusher
	int m_Affectee;			// Number of affected sector

	friend BOOL PIT_PushThing (AActor *thing);
};

inline FArchive &operator<< (FArchive &arc, DPusher::EPusher type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DPusher::EPusher &out)
{
	BYTE in; arc >> in; out = (DPusher::EPusher)in; return arc;
}

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
BOOL	CheckIfExitIsGood (AActor *self);

// at game start
void	P_InitPicAnims (void);

// at map load
void	P_SpawnSpecials (void);

// every tic
void	P_UpdateSpecials (void);

// when needed
BOOL	P_ActivateLine (line_t *ld, AActor *mo, int side, int activationType);
BOOL	P_TestActivateLine (line_t *ld, AActor *mo, int side, int activationType);

void	P_PlayerInSpecialSector (player_t *player);

//
// getSide()
// Will return a side_t*
//	given the number of the current sector,
//	the line number, and the side (0/1) that you want.
//
inline side_t *getSide (int currentSector, int line, int side)
{
	return &sides[ (sectors[currentSector].lines[line])->sidenum[side] ];
}

//
// getSector()
// Will return a sector_t*
//	given the number of the current sector,
//	the line number and the side (0/1) that you want.
//
inline sector_t *getSector (int currentSector, int line, int side)
{
	return sides[ (sectors[currentSector].lines[line])->sidenum[side] ].sector;
}


//
// twoSided()
// Given the sector number and the line number,
//	it will tell you whether the line is two-sided or not.
//
inline int twoSided (int sector, int line)
{
	return (sectors[sector].lines[line])->flags & ML_TWOSIDED;
}

//
// getNextSector()
// Return sector_t * of sector next to current.
// NULL if not two-sided line
//
inline sector_t *getNextSector (line_t *line, sector_t *sec)
{
	if (!(line->flags & ML_TWOSIDED))
		return NULL;
	
	return (line->frontsector == sec) ? line->backsector : line->frontsector;
		
	return line->frontsector;
}


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

sector_t *P_NextSpecialSector (sector_t *sec, int type, sector_t *back2);	// [RH]



//
// P_LIGHTS
//

class DLighting : public DSectorEffect
{
	DECLARE_SERIAL (DLighting, DSectorEffect);
public:
	DLighting (sector_t *sector);
protected:
	DLighting ();
};

class DFireFlicker : public DLighting
{
	DECLARE_SERIAL (DFireFlicker, DLighting)
public:
	DFireFlicker (sector_t *sector);
	DFireFlicker (sector_t *sector, int upper, int lower);
	void		RunThink ();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
private:
	DFireFlicker ();
};

class DFlicker : public DLighting
{
	DECLARE_SERIAL (DFlicker, DLighting)
public:
	DFlicker (sector_t *sector, int upper, int lower);
	void		RunThink ();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
private:
	DFlicker ();
};

class DLightFlash : public DLighting
{
	DECLARE_SERIAL (DLightFlash, DLighting)
public:
	DLightFlash (sector_t *sector);
	DLightFlash (sector_t *sector, int min, int max);
	void		RunThink ();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
	int 		m_MaxTime;
	int 		m_MinTime;
private:
	DLightFlash ();
};

class DStrobe : public DLighting
{
	DECLARE_SERIAL (DStrobe, DLighting)
public:
	DStrobe (sector_t *sector, int utics, int ltics, bool inSync);
	DStrobe (sector_t *sector, int upper, int lower, int utics, int ltics);
	void		RunThink ();
protected:
	int 		m_Count;
	int 		m_MinLight;
	int 		m_MaxLight;
	int 		m_DarkTime;
	int 		m_BrightTime;
private:
	DStrobe ();
};

class DGlow : public DLighting
{
	DECLARE_SERIAL (DGlow, DLighting)
public:
	DGlow (sector_t *sector);
	void		RunThink ();
protected:
	int 		m_MinLight;
	int 		m_MaxLight;
	int 		m_Direction;
private:
	DGlow ();
};

// [RH] Glow from Light_Glow and Light_Fade specials
class DGlow2 : public DLighting
{
	DECLARE_SERIAL (DGlow2, DLighting)
public:
	DGlow2 (sector_t *sector, int start, int end, int tics, bool oneshot);
	void		RunThink ();
protected:
	int			m_Start;
	int			m_End;
	int			m_MaxTics;
	int			m_Tics;
	bool		m_OneShot;
private:
	DGlow2 ();
};

// [RH] Phased light thinker
class DPhased : public DLighting
{
	DECLARE_SERIAL (DPhased, DLighting)
public:
	DPhased (sector_t *sector);
	DPhased (sector_t *sector, int baselevel, int phase);
	void		RunThink ();
protected:
	byte		m_BaseLevel;
	byte		m_Phase;
private:
	DPhased ();
	DPhased (sector_t *sector, int baselevel);
	int PhaseHelper (sector_t *sector, int index, int light, sector_t *prev);
};

#define GLOWSPEED				8
#define STROBEBRIGHT			5
#define FASTDARK				15
#define SLOWDARK				TICRATE

void	EV_StartLightFlickering (int tag, int upper, int lower);
void	EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics);
void	EV_StartLightStrobing (int tag, int utics, int ltics);
void	EV_TurnTagLightsOff (int tag);
void	EV_LightTurnOn (int tag, int bright);
void	EV_LightChange (int tag, int value);

void	P_SpawnGlowingLight (sector_t *sector);

void	EV_StartLightGlowing (int tag, int upper, int lower, int tics);
void	EV_StartLightFading (int tag, int value, int tics);


//
// P_SWITCH
//
typedef struct
{
	char		name1[9];
	char		name2[9];
	short		episode;
	
} switchlist_t;


// 1 second, in ticks. 
#define BUTTONTIME		TICRATE

void	P_ChangeSwitchTexture (line_t *line, int useAgain);

void	P_InitSwitchList ();


//
// P_PLATS
//
class DPlat : public DMovingFloor
{
	DECLARE_SERIAL (DPlat, DMovingFloor);
public:
	enum EPlatState
	{
		up,
		down,
		waiting,
		in_stasis
	};

	enum EPlatType
	{
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
	};

	void RunThink ();

protected:
	DPlat (sector_t *sector);

	fixed_t 	m_Speed;
	fixed_t 	m_Low;
	fixed_t 	m_High;
	int 		m_Wait;
	int 		m_Count;
	EPlatState	m_Status;
	EPlatState	m_OldStatus;
	BOOL 		m_Crush;
	int 		m_Tag;
	EPlatType	m_Type;

	void PlayPlatSound (const char *sound);
	void Reactivate ();
	void Stop ();

private:
	DPlat ();

	friend BOOL	EV_DoPlat (int tag, line_t *line, EPlatType type,
						   int height, int speed, int delay, int lip, int change);
	friend void EV_StopPlat (int tag);
	friend void P_ActivateInStasis (int tag);
};

inline FArchive &operator<< (FArchive &arc, DPlat::EPlatType type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DPlat::EPlatType &out)
{
	BYTE in; arc >> in; out = (DPlat::EPlatType)in; return arc;
}
inline FArchive &operator<< (FArchive &arc, DPlat::EPlatState state)
{
	return arc << (BYTE)state;
}
inline FArchive &operator>> (FArchive &arc, DPlat::EPlatState &out)
{
	BYTE in; arc >> in; out = (DPlat::EPlatState)in; return arc;
}

//
// [RH]
// P_PILLAR
//

class DPillar : public DMover
{
	DECLARE_SERIAL (DPillar, DMover)
public:
	enum EPillar
	{
		pillarBuild,
		pillarOpen

	};

	DPillar (sector_t *sector, EPillar type, fixed_t speed, fixed_t height,
			 fixed_t height2, int crush);

	void RunThink ();

protected:
	EPillar		m_Type;
	fixed_t		m_FloorSpeed;
	fixed_t		m_CeilingSpeed;
	fixed_t		m_FloorTarget;
	fixed_t		m_CeilingTarget;
	int			m_Crush;

private:
	DPillar ();
};

inline FArchive &operator<< (FArchive &arc, DPillar::EPillar type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DPillar::EPillar &out)
{
	BYTE in; arc >> in; out = (DPillar::EPillar)in; return arc;
}

BOOL EV_DoPillar (DPillar::EPillar type, int tag, fixed_t speed, fixed_t height,
				  fixed_t height2, int crush);

//
// P_DOORS
//
class DDoor : public DMovingCeiling
{
	DECLARE_SERIAL (DDoor, DMovingCeiling)
public:
	enum EVlDoor
	{
		doorClose,
		doorOpen,
		doorRaise,
		doorRaiseIn5Mins,
		doorCloseWaitOpen,
	};

	DDoor (sector_t *sector);
	DDoor (sector_t *sec, EVlDoor type, fixed_t speed, int delay);

	void RunThink ();
protected:
	EVlDoor		m_Type;
	fixed_t 	m_TopHeight;
	fixed_t 	m_Speed;

	// 1 = up, 0 = waiting at top, -1 = down
	int 		m_Direction;
	
	// tics to wait at the top
	int 		m_TopWait;
	// (keep in case a door going down is reset)
	// when it reaches 0, start going down
	int 		m_TopCountdown;

	void DoorSound (bool raise) const;

	friend BOOL	EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
						   int tag, int speed, int delay, keytype_t lock);
	friend void P_SpawnDoorCloseIn30 (sector_t *sec);
	friend void P_SpawnDoorRaiseIn5Mins (sector_t *sec);
private:
	DDoor ();

};

inline FArchive &operator<< (FArchive &arc, DDoor::EVlDoor type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DDoor::EVlDoor &out)
{
	BYTE in; arc >> in; out = (DDoor::EVlDoor)in; return arc;
}


//
// P_CEILNG
//

// [RH] Changed these
class DCeiling : public DMovingCeiling
{
	DECLARE_SERIAL (DCeiling, DMovingCeiling)
public:
	enum ECeiling
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
	};

	DCeiling (sector_t *sec);
	DCeiling (sector_t *sec, fixed_t speed1, fixed_t speed2, int silent);

	void RunThink ();

protected:
	ECeiling	m_Type;
	fixed_t 	m_BottomHeight;
	fixed_t 	m_TopHeight;
	fixed_t 	m_Speed;
	fixed_t		m_Speed1;		// [RH] dnspeed of crushers
	fixed_t		m_Speed2;		// [RH] upspeed of crushers
	int 		m_Crush;
	int			m_Silent;
	int 		m_Direction;	// 1 = up, 0 = waiting, -1 = down

	// [RH] Need these for BOOM-ish transferring ceilings
	int			m_Texture;
	int			m_NewSpecial;

	// ID
	int 		m_Tag;
	int 		m_OldDirection;

	void PlayCeilingSound ();

private:
	DCeiling ();

	friend BOOL EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
		int tag, fixed_t speed, fixed_t speed2, fixed_t height,
		int crush, int silent, int change);
	friend BOOL EV_CeilingCrushStop (int tag);
	friend void P_ActivateInStasisCeiling (int tag);
};

inline FArchive &operator<< (FArchive &arc, DCeiling::ECeiling type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DCeiling::ECeiling &type)
{
	BYTE in; arc >> in; type = (DCeiling::ECeiling)in; return arc;
}


//
// P_FLOOR
//

class DFloor : public DMovingFloor
{
	DECLARE_SERIAL (DFloor, DMovingFloor)
public:
	enum EFloor
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
	};

	// [RH] Changed to use Hexen-ish specials
	enum EStair
	{
		buildUp,
		buildDown
	};

	DFloor (sector_t *sec);

	void RunThink ();

protected:
	EFloor	 	m_Type;
	int 		m_Crush;
	int 		m_Direction;
	short 		m_NewSpecial;
	short		m_Texture;
	fixed_t 	m_FloorDestHeight;
	fixed_t 	m_Speed;

	// [RH] New parameters used to reset and delay stairs
	int			m_ResetCount;
	int			m_OrgHeight;
	int			m_Delay;
	int			m_PauseTime;
	int			m_StepTime;
	int			m_PerStepTime;

	void StartFloorSound ();

	friend BOOL EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
		fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
		int usespecials);
	friend BOOL EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
		fixed_t speed, fixed_t height, int crush, int change);
	friend BOOL EV_FloorCrushStop (int tag);
	friend int EV_DoDonut (int tag, fixed_t pillarspeed, fixed_t slimespeed);
private:
	DFloor ();
};

inline FArchive &operator<< (FArchive &arc, DFloor::EFloor type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DFloor::EFloor &type)
{
	BYTE in; arc >> in; type = (DFloor::EFloor)in; return arc;
}

class DElevator : public DMover
{
	DECLARE_SERIAL (DElevator, DMover)
public:
	enum EElevator
	{
		elevateUp,
		elevateDown,
		elevateCurrent,
		// [RH] For FloorAndCeiling_Raise/Lower
		elevateRaise,
		elevateLower
	};

	DElevator (sector_t *sec);

	void RunThink ();

protected:
	EElevator	m_Type;
	int			m_Direction;
	fixed_t		m_FloorDestHeight;
	fixed_t		m_CeilingDestHeight;
	fixed_t		m_Speed;

	void StartFloorSound ();

	friend BOOL EV_DoElevator (line_t *line, DElevator::EElevator type, fixed_t speed,
		fixed_t height, int tag);
private:
	DElevator ();
};

inline FArchive &operator<< (FArchive &arc, DElevator::EElevator type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, DElevator::EElevator &out)
{
	BYTE in; arc >> in; out = (DElevator::EElevator)in; return arc;
}

class DFloorWaggle : public DMovingFloor
{
	DECLARE_SERIAL (DFloorWaggle, DMovingFloor);
public:
	DFloorWaggle (sector_t *sec);

	void RunThink ();

protected:
	fixed_t m_OriginalHeight;
	fixed_t m_Accumulator;
	fixed_t m_AccDelta;
	fixed_t m_TargetScale;
	fixed_t m_Scale;
	fixed_t m_ScaleDelta;
	int m_Ticker;
	int m_State;

	friend BOOL EV_StartFloorWaggle (int tag, int height, int speed,
		int offset, int timer);
private:
	DFloorWaggle ();
};

//jff 3/15/98 pure texture/type change for better generalized support
enum EChange
{
	trigChangeOnly,
	numChangeOnly,
};

BOOL EV_DoChange (line_t *line, EChange changetype, int tag);



//
// P_TELEPT
//
BOOL EV_Teleport (int tid, int side, AActor *thing);
BOOL EV_SilentTeleport (int tid, line_t *line, int side, AActor *thing);
BOOL EV_SilentLineTeleport (line_t *line, int side, AActor *thing, int id,
							BOOL reverse);


//
// [RH] ACS (see also p_acs.h)
//

BOOL P_StartScript (AActor *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always);
void P_SuspendScript (int script, char *map);
void P_TerminateScript (int script, char *map);
void P_StartOpenScripts (void);
void P_DoDeferedScripts (void);

//
// [RH] p_quake.c
//
BOOL P_StartQuake (int tid, int intensity, int duration, int damrad, int tremrad);

#endif
