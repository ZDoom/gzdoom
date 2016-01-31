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
#include "doomdata.h"
#include "r_state.h"

class FScanner;
struct level_info_t;

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
	DECLARE_CLASS (DScroller, DThinker)
	HAS_OBJECT_POINTERS
public:
	enum EScrollType
	{
		sc_side,
		sc_floor,
		sc_ceiling,
		sc_carry,
		sc_carry_ceiling,	// killough 4/11/98: carry objects hanging on ceilings
	};
	enum EScrollPos
	{
		scw_top=1,
		scw_mid=2,
		scw_bottom=4,
		scw_all=7,
	};
	
	DScroller (EScrollType type, fixed_t dx, fixed_t dy, int control, int affectee, int accel, int scrollpos = scw_all);
	DScroller (fixed_t dx, fixed_t dy, const line_t *l, int control, int accel, int scrollpos = scw_all);
	void Destroy();

	void Serialize (FArchive &arc);
	void Tick ();

	bool AffectsWall (int wallnum) const { return m_Type == sc_side && m_Affectee == wallnum; }
	int GetWallNum () const { return m_Type == sc_side ? m_Affectee : -1; }
	void SetRate (fixed_t dx, fixed_t dy) { m_dx = dx; m_dy = dy; }
	bool IsType (EScrollType type) const { return type == m_Type; }
	int GetAffectee () const { return m_Affectee; }
	int GetScrollParts() const { return m_Parts; }

protected:
	EScrollType m_Type;		// Type of scroll effect
	fixed_t m_dx, m_dy;		// (dx,dy) scroll speeds
	int m_Affectee;			// Number of affected sidedef, sector, tag, or whatever
	int m_Control;			// Control sector (-1 if none) used to control scrolling
	fixed_t m_LastHeight;	// Last known height of control sector
	fixed_t m_vdx, m_vdy;	// Accumulated velocity if accelerative
	int m_Accel;			// Whether it's accelerative
	int m_Parts;			// Which parts of a sidedef are being scrolled?
	TObjPtr<DInterpolation> m_Interpolations[3];

private:
	DScroller ();
};

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
enum { CARRYFACTOR = (3*FRACUNIT >> 5) };

// phares 3/20/98: added new model of Pushers for push/pull effects

class DPusher : public DThinker
{
	DECLARE_CLASS (DPusher, DThinker)
	HAS_OBJECT_POINTERS
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
	void Serialize (FArchive &arc);
	int CheckForSectorMatch (EPusher type, int tag);
	void ChangeValues (int magnitude, int angle)
	{
		angle_t ang = ((angle_t)(angle<<24)) >> ANGLETOFINESHIFT;
		m_Xmag = (magnitude * finecosine[ang]) >> FRACBITS;
		m_Ymag = (magnitude * finesine[ang]) >> FRACBITS;
		m_Magnitude = magnitude;
	}

	void Tick ();

protected:
	EPusher m_Type;
	TObjPtr<AActor> m_Source;// Point source if point pusher
	int m_Xmag;				// X Strength
	int m_Ymag;				// Y Strength
	int m_Magnitude;		// Vector strength for point pusher
	int m_Radius;			// Effective radius for point pusher
	int m_X;				// X of point source if point pusher
	int m_Y;				// Y of point source if point pusher
	int m_Affectee;			// Number of affected sector

	friend bool PIT_PushThing (AActor *thing);
};

bool PIT_PushThing (AActor *thing);

// Define values for map objects
#define MO_TELEPORTMAN			14

// Flags for P_SectorDamage
#define DAMAGE_PLAYERS				1
#define DAMAGE_NONPLAYERS			2
#define DAMAGE_IN_AIR				4
#define DAMAGE_SUBCLASSES_PROTECT	8


// [RH] If a deathmatch game, checks to see if noexit is enabled.
//		If so, it kills the player and returns false. Otherwise,
//		it returns true, and the player is allowed to live.
bool	CheckIfExitIsGood (AActor *self, level_info_t *info);

// at map load
void P_InitSectorSpecial(sector_t *sector, int special, bool nothinkers);
void	P_SpawnSpecials (void);

// every tic
void	P_UpdateSpecials (void);

// when needed
bool	P_ActivateLine (line_t *ld, AActor *mo, int side, int activationType);
bool	P_TestActivateLine (line_t *ld, AActor *mo, int side, int activationType);
bool	P_PredictLine (line_t *ld, AActor *mo, int side, int activationType);

void 	P_PlayerInSpecialSector (player_t *player, sector_t * sector=NULL);
void	P_PlayerOnSpecialFlat (player_t *player, int floorType);
void	P_SectorDamage(int tag, int amount, FName type, const PClass *protectClass, int flags);
void	P_SetSectorFriction (int tag, int amount, bool alterFlag);

inline fixed_t FrictionToMoveFactor(fixed_t friction)
{
	fixed_t movefactor;

	// [RH] Twiddled these values so that velocity on ice (with
	//		friction 0xf900) is the same as in Heretic/Hexen.
	if (friction >= ORIG_FRICTION)	// ice
//		movefactor = ((0x10092 - friction)*(0x70))/0x158;
		movefactor = ((0x10092 - friction) * 1024) / 4352 + 568;
	else
		movefactor = ((friction - 0xDB34)*(0xA))/0x80;

	// killough 8/28/98: prevent odd situations
	if (movefactor < 32)
		movefactor = 32;

	return movefactor;
}

void P_GiveSecret(AActor *actor, bool printmessage, bool playsound, int sectornum);

//
// getSide()
// Will return a side_t*
//	given the number of the current sector,
//	the line number, and the side (0/1) that you want.
//
inline side_t *getSide (int currentSector, int line, int side)
{
	return (sectors[currentSector].lines[line])->sidedef[side];
}

//
// getSector()
// Will return a sector_t*
//	given the number of the current sector,
//	the line number and the side (0/1) that you want.
//
inline sector_t *getSector (int currentSector, int line, int side)
{
	return (sectors[currentSector].lines[line])->sidedef[side]->sector;
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
inline sector_t *getNextSector (line_t *line, const sector_t *sec)
{
	if (!(line->flags & ML_TWOSIDED))
		return NULL;

	return line->frontsector == sec ?
		   (line->backsector != sec ? line->backsector : NULL) :
	       line->frontsector;
}


#include "p_tags.h"

//
// P_LIGHTS
//

class DLighting : public DSectorEffect
{
	DECLARE_CLASS (DLighting, DSectorEffect)
public:
	DLighting (sector_t *sector);
protected:
	DLighting ();
};

class DFireFlicker : public DLighting
{
	DECLARE_CLASS (DFireFlicker, DLighting)
public:
	DFireFlicker (sector_t *sector);
	DFireFlicker (sector_t *sector, int upper, int lower);
	void		Serialize (FArchive &arc);
	void		Tick ();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
private:
	DFireFlicker ();
};

class DFlicker : public DLighting
{
	DECLARE_CLASS (DFlicker, DLighting)
public:
	DFlicker (sector_t *sector, int upper, int lower);
	void		Serialize (FArchive &arc);
	void		Tick ();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
private:
	DFlicker ();
};

class DLightFlash : public DLighting
{
	DECLARE_CLASS (DLightFlash, DLighting)
public:
	DLightFlash (sector_t *sector);
	DLightFlash (sector_t *sector, int min, int max);
	void		Serialize (FArchive &arc);
	void		Tick ();
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
	DECLARE_CLASS (DStrobe, DLighting)
public:
	DStrobe (sector_t *sector, int utics, int ltics, bool inSync);
	DStrobe (sector_t *sector, int upper, int lower, int utics, int ltics);
	void		Serialize (FArchive &arc);
	void		Tick ();
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
	DECLARE_CLASS (DGlow, DLighting)
public:
	DGlow (sector_t *sector);
	void		Serialize (FArchive &arc);
	void		Tick ();
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
	DECLARE_CLASS (DGlow2, DLighting)
public:
	DGlow2 (sector_t *sector, int start, int end, int tics, bool oneshot);
	void		Serialize (FArchive &arc);
	void		Tick ();
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
	DECLARE_CLASS (DPhased, DLighting)
public:
	DPhased (sector_t *sector);
	DPhased (sector_t *sector, int baselevel, int phase);
	void		Serialize (FArchive &arc);
	void		Tick ();
protected:
	BYTE		m_BaseLevel;
	BYTE		m_Phase;
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
void	EV_LightTurnOnPartway (int tag, fixed_t frac);	// killough 10/98
void	EV_LightChange (int tag, int value);
void	EV_StopLightEffect (int tag);

void	P_SpawnGlowingLight (sector_t *sector);

void	EV_StartLightGlowing (int tag, int upper, int lower, int tics);
void	EV_StartLightFading (int tag, int value, int tics);


//
// P_SWITCH
//

#define BUTTONTIME TICRATE		// 1 second, in ticks. 

bool	P_ChangeSwitchTexture (side_t *side, int useAgain, BYTE special, bool *quest=NULL);
bool	P_CheckSwitchRange(AActor *user, line_t *line, int sideno);

//
// P_PLATS
//
class DPlat : public DMovingFloor
{
	DECLARE_CLASS (DPlat, DMovingFloor)
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
		platDownWaitUpStayStone,
		platUpWaitDownStay,
		platUpNearestWaitDownStay,
		platDownByValue,
		platUpByValue,
		platUpByValueStay,
		platRaiseAndStay,
		platToggle,
		platDownToNearestFloor,
		platDownToLowestCeiling,
		platRaiseAndStayLockout,
	};

	void Serialize (FArchive &arc);
	void Tick ();

	bool IsLift() const { return m_Type == platDownWaitUpStay || m_Type == platDownWaitUpStayStone; }

protected:
	DPlat (sector_t *sector);

	fixed_t 	m_Speed;
	fixed_t 	m_Low;
	fixed_t 	m_High;
	int 		m_Wait;
	int 		m_Count;
	EPlatState	m_Status;
	EPlatState	m_OldStatus;
	int			m_Crush;
	int 		m_Tag;
	EPlatType	m_Type;

	void PlayPlatSound (const char *sound);
	void Reactivate ();
	void Stop ();

private:
	DPlat ();

	friend bool	EV_DoPlat (int tag, line_t *line, EPlatType type,
						   int height, int speed, int delay, int lip, int change);
	friend void EV_StopPlat (int tag);
	friend void P_ActivateInStasis (int tag);
};

bool EV_DoPlat (int tag, line_t *line, DPlat::EPlatType type,
				int height, int speed, int delay, int lip, int change);
void EV_StopPlat (int tag);
void P_ActivateInStasis (int tag);

//
// [RH]
// P_PILLAR
//

class DPillar : public DMover
{
	DECLARE_CLASS (DPillar, DMover)
	HAS_OBJECT_POINTERS
public:
	enum EPillar
	{
		pillarBuild,
		pillarOpen

	};

	DPillar (sector_t *sector, EPillar type, fixed_t speed, fixed_t height,
			 fixed_t height2, int crush, bool hexencrush);

	void Serialize (FArchive &arc);
	void Tick ();
	void Destroy();

protected:
	EPillar		m_Type;
	fixed_t		m_FloorSpeed;
	fixed_t		m_CeilingSpeed;
	fixed_t		m_FloorTarget;
	fixed_t		m_CeilingTarget;
	int			m_Crush;
	bool		m_Hexencrush;
	TObjPtr<DInterpolation> m_Interp_Ceiling;
	TObjPtr<DInterpolation> m_Interp_Floor;

private:
	DPillar ();
};

bool EV_DoPillar (DPillar::EPillar type, line_t *line, int tag,
				  fixed_t speed, fixed_t height, fixed_t height2, int crush, bool hexencrush);

//
// P_DOORS
//
class DDoor : public DMovingCeiling
{
	DECLARE_CLASS (DDoor, DMovingCeiling)
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
	DDoor (sector_t *sec, EVlDoor type, fixed_t speed, int delay, int lightTag);

	void Serialize (FArchive &arc);
	void Tick ();
protected:
	EVlDoor		m_Type;
	fixed_t 	m_TopDist;
	fixed_t		m_BotDist, m_OldFloorDist;
	vertex_t	*m_BotSpot;
	fixed_t 	m_Speed;

	// 1 = up, 0 = waiting at top, -1 = down
	int 		m_Direction;
	
	// tics to wait at the top
	int 		m_TopWait;
	// (keep in case a door going down is reset)
	// when it reaches 0, start going down
	int 		m_TopCountdown;

	int			m_LightTag;

	void DoorSound (bool raise, class DSeqNode *curseq=NULL) const;

	friend bool	EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
						   int tag, int speed, int delay, int lock,
						   int lightTag, bool boomgen);
	friend void P_SpawnDoorCloseIn30 (sector_t *sec);
	friend void P_SpawnDoorRaiseIn5Mins (sector_t *sec);
private:
	DDoor ();

};

bool EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
				int tag, int speed, int delay, int lock,
				int lightTag, bool boomgen = false);
void P_SpawnDoorCloseIn30 (sector_t *sec);
void P_SpawnDoorRaiseIn5Mins (sector_t *sec);

class DAnimatedDoor : public DMovingCeiling
{
	DECLARE_CLASS (DAnimatedDoor, DMovingCeiling)
public:
	DAnimatedDoor (sector_t *sector);
	DAnimatedDoor (sector_t *sec, line_t *line, int speed, int delay, FDoorAnimation *anim);

	void Serialize (FArchive &arc);
	void Tick ();

	bool StartClosing ();
protected:
	line_t *m_Line1, *m_Line2;
	int m_Frame;
	FDoorAnimation *m_DoorAnim;
	int m_Timer;
	fixed_t m_BotDist;
	int m_Status;
	enum
	{
		Opening,
		Waiting,
		Closing,
		Dead
	};
	int m_Speed;
	int m_Delay;
	bool m_SetBlocking1, m_SetBlocking2;

	friend bool EV_SlidingDoor (line_t *line, AActor *thing, int tag, int speed, int delay);
private:
	DAnimatedDoor ();
};

bool EV_SlidingDoor (line_t *line, AActor *thing, int tag, int speed, int delay);

//
// P_CEILNG
//

// [RH] Changed these
class DCeiling : public DMovingCeiling
{
	DECLARE_CLASS (DCeiling, DMovingCeiling)
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
		ceilLowerAndCrushDist,
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

	void Serialize (FArchive &arc);
	void Tick ();

	static DCeiling *Create(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag,
						fixed_t speed, fixed_t speed2, fixed_t height,
						int crush, int silent, int change, bool hexencrush);

protected:
	ECeiling	m_Type;
	fixed_t 	m_BottomHeight;
	fixed_t 	m_TopHeight;
	fixed_t 	m_Speed;
	fixed_t		m_Speed1;		// [RH] dnspeed of crushers
	fixed_t		m_Speed2;		// [RH] upspeed of crushers
	int 		m_Crush;
	bool		m_Hexencrush;
	int			m_Silent;
	int 		m_Direction;	// 1 = up, 0 = waiting, -1 = down

	// [RH] Need these for BOOM-ish transferring ceilings
	FTextureID	m_Texture;
	secspecial_t m_NewSpecial;

	// ID
	int 		m_Tag;
	int 		m_OldDirection;

	void PlayCeilingSound ();

private:
	DCeiling ();

	friend bool EV_CeilingCrushStop (int tag);
	friend void P_ActivateInStasisCeiling (int tag);
};

bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
	int tag, fixed_t speed, fixed_t speed2, fixed_t height,
	int crush, int silent, int change, bool hexencrush);
bool EV_CeilingCrushStop (int tag);
void P_ActivateInStasisCeiling (int tag);



//
// P_FLOOR
//

class DFloor : public DMovingFloor
{
	DECLARE_CLASS (DFloor, DMovingFloor)
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
		floorRaiseAndCrushDoom,
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

	void Serialize (FArchive &arc);
	void Tick ();

protected:
	EFloor	 	m_Type;
	int 		m_Crush;
	bool		m_Hexencrush;
	int 		m_Direction;
	secspecial_t m_NewSpecial;
	FTextureID	m_Texture;
	fixed_t 	m_FloorDestDist;
	fixed_t 	m_Speed;

	// [RH] New parameters used to reset and delay stairs
	int			m_ResetCount;
	int			m_OrgDist;
	int			m_Delay;
	int			m_PauseTime;
	int			m_StepTime;
	int			m_PerStepTime;

	void StartFloorSound ();
	void SetFloorChangeType (sector_t *sec, int change);

	friend bool EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
		fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
		int usespecials);
	friend bool EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
		fixed_t speed, fixed_t height, int crush, int change, bool hexencrush, bool hereticlower);
	friend bool EV_FloorCrushStop (int tag);
	friend bool EV_DoDonut (int tag, line_t *line, fixed_t pillarspeed, fixed_t slimespeed);
private:
	DFloor ();
};

bool EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
	fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
	int usespecials);
bool EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
	fixed_t speed, fixed_t height, int crush, int change, bool hexencrush, bool hereticlower=false);
bool EV_FloorCrushStop (int tag);
bool EV_DoDonut (int tag, line_t *line, fixed_t pillarspeed, fixed_t slimespeed);

class DElevator : public DMover
{
	DECLARE_CLASS (DElevator, DMover)
	HAS_OBJECT_POINTERS
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

	void Destroy();
	void Serialize (FArchive &arc);
	void Tick ();

protected:
	EElevator	m_Type;
	int			m_Direction;
	fixed_t		m_FloorDestDist;
	fixed_t		m_CeilingDestDist;
	fixed_t		m_Speed;
	TObjPtr<DInterpolation> m_Interp_Ceiling;
	TObjPtr<DInterpolation> m_Interp_Floor;

	void StartFloorSound ();

	friend bool EV_DoElevator (line_t *line, DElevator::EElevator type, fixed_t speed,
		fixed_t height, int tag);
private:
	DElevator ();
};

bool EV_DoElevator (line_t *line, DElevator::EElevator type, fixed_t speed,
	fixed_t height, int tag);

class DWaggleBase : public DMover
{
	DECLARE_CLASS (DWaggleBase, DMover)
	HAS_OBJECT_POINTERS
public:
	DWaggleBase (sector_t *sec);

	void Serialize (FArchive &arc);

protected:
	fixed_t m_OriginalDist;
	fixed_t m_Accumulator;
	fixed_t m_AccDelta;
	fixed_t m_TargetScale;
	fixed_t m_Scale;
	fixed_t m_ScaleDelta;
	int m_Ticker;
	int m_State;
	TObjPtr<DInterpolation> m_Interpolation;

	friend bool EV_StartWaggle (int tag, line_t *line, int height, int speed,
		int offset, int timer, bool ceiling);

	void DoWaggle (bool ceiling);
	void Destroy();
	DWaggleBase ();
};

bool EV_StartWaggle (int tag, line_t *line, int height, int speed,
	int offset, int timer, bool ceiling);

class DFloorWaggle : public DWaggleBase
{
	DECLARE_CLASS (DFloorWaggle, DWaggleBase)
public:
	DFloorWaggle (sector_t *sec);
	void Tick ();
private:
	DFloorWaggle ();
};

class DCeilingWaggle : public DWaggleBase
{
	DECLARE_CLASS (DCeilingWaggle, DWaggleBase)
public:
	DCeilingWaggle (sector_t *sec);
	void Tick ();
private:
	DCeilingWaggle ();
};

//jff 3/15/98 pure texture/type change for better generalized support
enum EChange
{
	trigChangeOnly,
	numChangeOnly,
};

bool EV_DoChange (line_t *line, EChange changetype, int tag);



//
// P_TELEPT
//
enum
{
	TELF_DESTFOG			= 1,
	TELF_SOURCEFOG			= 2,
	TELF_KEEPORIENTATION	= 4,
	TELF_KEEPVELOCITY		= 8,
	TELF_KEEPHEIGHT			= 16,
};

void P_SpawnTeleportFog(AActor *mobj, fixed_t x, fixed_t y, fixed_t z, bool beforeTele = true, bool setTarget = false); //Spawns teleport fog. Pass the actor to pluck TeleFogFromType and TeleFogToType. 'from' determines if this is the fog to spawn at the old position (true) or new (false).
inline void P_SpawnTeleportFog(AActor *mobj, const fixedvec3 &pos, bool beforeTele = true, bool setTarget = false)
{
	P_SpawnTeleportFog(mobj, pos.x, pos.y, pos.z, beforeTele, setTarget);
}
bool P_Teleport (AActor *thing, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int flags); // bool useFog, bool sourceFog, bool keepOrientation, bool haltVelocity = true, bool keepHeight = false
bool EV_Teleport (int tid, int tag, line_t *line, int side, AActor *thing, int flags);
bool EV_SilentLineTeleport (line_t *line, int side, AActor *thing, int id, INTBOOL reverse);
bool EV_TeleportOther (int other_tid, int dest_tid, bool fog);
bool EV_TeleportGroup (int group_tid, AActor *victim, int source_tid, int dest_tid, bool moveSource, bool fog);
bool EV_TeleportSector (int tag, int source_tid, int dest_tid, bool fog, int group_tid);


//
// [RH] ACS (see also p_acs.h)
//

#define ACS_BACKSIDE		1
#define ACS_ALWAYS			2
#define ACS_WANTRESULT		4
#define ACS_NET				8

int  P_StartScript (AActor *who, line_t *where, int script, const char *map, const int *args, int argcount, int flags);
void P_SuspendScript (int script, const char *map);
void P_TerminateScript (int script, const char *map);
void P_DoDeferedScripts (void);

//
// [RH] p_quake.c
//
bool P_StartQuakeXYZ(AActor *activator, int tid, int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, FSoundID quakesfx, int flags, double waveSpeedX, double waveSpeedY, double waveSpeedZ);
bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx);

#endif
