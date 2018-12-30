//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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
struct FDoorAnimation;

struct FThinkerCollection
{
	int RefNum;
	DThinker *Obj;
};

enum class EScroll : int
{
		sc_side,
		sc_floor,
		sc_ceiling,
		sc_carry,
		sc_carry_ceiling,	// killough 4/11/98: carry objects hanging on ceilings
};

enum EScrollPos : int
{
	scw_top = 1,
	scw_mid = 2,
	scw_bottom = 4,
	scw_all = 7,
};

void P_CreateScroller(EScroll type, double dx, double dy, int control, int affectee, int accel, EScrollPos scrollpos = EScrollPos::scw_all);


//jff 2/23/98 identify the special classes that can share sectors

typedef enum
{
	floor_special,
	ceiling_special,
	lighting_special,
} special_e;

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
const double CARRYFACTOR = 3 / 32.;

// Flags for P_SectorDamage
#define DAMAGE_PLAYERS				1
#define DAMAGE_NONPLAYERS			2
#define DAMAGE_IN_AIR				4
#define DAMAGE_SUBCLASSES_PROTECT	8
#define DAMAGE_NO_ARMOR				16


// [RH] If a deathmatch game, checks to see if noexit is enabled.
//		If so, it kills the player and returns false. Otherwise,
//		it returns true, and the player is allowed to live.
bool	CheckIfExitIsGood (AActor *self, level_info_t *info);

class MapLoader;
// at map load
void	P_SpawnSpecials (MapLoader *ml);

// every tic
void	P_UpdateSpecials (void);

// when needed
bool	P_ActivateLine (line_t *ld, AActor *mo, int side, int activationType, DVector3 *optpos = NULL);
bool	P_TestActivateLine (line_t *ld, AActor *mo, int side, int activationType, DVector3 *optpos = NULL);
bool	P_PredictLine (line_t *ld, AActor *mo, int side, int activationType);

void 	P_PlayerInSpecialSector (player_t *player, sector_t * sector=NULL);
void	P_PlayerOnSpecialFlat (player_t *player, int floorType);
void	P_SectorDamage(int tag, int amount, FName type, PClassActor *protectClass, int flags);
void	P_SetSectorFriction (int tag, int amount, bool alterFlag);
double FrictionToMoveFactor(double friction);
void P_GiveSecret(AActor *actor, bool printmessage, bool playsound, int sectornum);

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

class DLighting : public DSectorEffect
{
	DECLARE_CLASS(DLighting, DSectorEffect)
public:
	DLighting(sector_t *sector);
protected:
	DLighting();
};

void	EV_StartLightFlickering (int tag, int upper, int lower);
void	EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics);
void	EV_StartLightStrobing (int tag, int utics, int ltics);
void	EV_TurnTagLightsOff (int tag);
void	EV_LightTurnOn (int tag, int bright);
void	EV_LightTurnOnPartway (int tag, double frac);	// killough 10/98
void	EV_LightChange (int tag, int value);
void	EV_StopLightEffect (int tag);

void	P_SpawnGlowingLight (sector_t *sector);

void	EV_StartLightGlowing (int tag, int upper, int lower, int tics);
void	EV_StartLightFading (int tag, int value, int tics);


//
// P_SWITCH
//

#define BUTTONTIME TICRATE		// 1 second, in ticks. 

bool	P_ChangeSwitchTexture (side_t *side, int useAgain, uint8_t special, bool *quest=NULL);
bool	P_CheckSwitchRange(AActor *user, line_t *line, int sideno, const DVector3 *optpos = NULL);

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

	void Serialize(FSerializer &arc);
	void Tick ();

	bool IsLift() const { return m_Type == platDownWaitUpStay || m_Type == platDownWaitUpStayStone; }
	DPlat(sector_t *sector);

protected:

	double	 	m_Speed;
	double	 	m_Low;
	double	 	m_High;
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
						   double height, double speed, int delay, int lip, int change);
	friend void EV_StopPlat (int tag, bool remove);
	friend void P_ActivateInStasis (int tag);
};

bool EV_DoPlat (int tag, line_t *line, DPlat::EPlatType type,
				double height, double speed, int delay, int lip, int change);
void EV_StopPlat (int tag, bool remove);
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

	DPillar (sector_t *sector, EPillar type, double speed, double height,
			 double height2, int crush, bool hexencrush);

	void Serialize(FSerializer &arc);
	void Tick ();
	void OnDestroy() override;

protected:
	EPillar		m_Type;
	double		m_FloorSpeed;
	double		m_CeilingSpeed;
	double		m_FloorTarget;
	double		m_CeilingTarget;
	int			m_Crush;
	bool		m_Hexencrush;
	TObjPtr<DInterpolation*> m_Interp_Ceiling;
	TObjPtr<DInterpolation*> m_Interp_Floor;

private:
	DPillar ();
};

bool EV_DoPillar (DPillar::EPillar type, line_t *line, int tag,
				  double speed, double height, double height2, int crush, bool hexencrush);

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
		doorWaitRaise,
		doorCloseWaitOpen,
		doorWaitClose,
	};

	DDoor (sector_t *sector);
	DDoor (sector_t *sec, EVlDoor type, double speed, int delay, int lightTag, int topcountdown);

	void Serialize(FSerializer &arc);
	void Tick ();
protected:
	EVlDoor		m_Type;
	double	 	m_TopDist;
	double		m_BotDist, m_OldFloorDist;
	vertex_t	*m_BotSpot;
	double	 	m_Speed;

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
						   int tag, double speed, int delay, int lock,
						   int lightTag, bool boomgen, int topcountdown);
private:
	DDoor ();

};

bool EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
				int tag, double speed, int delay, int lock,
				int lightTag, bool boomgen = false, int topcountdown = 0);


class DAnimatedDoor : public DMovingCeiling
{
	DECLARE_CLASS (DAnimatedDoor, DMovingCeiling)
public:

	enum EADType
	{
		adOpenClose,
		adClose
	};

	DAnimatedDoor (sector_t *sector);
	DAnimatedDoor (sector_t *sec, line_t *line, int speed, int delay, FDoorAnimation *anim, EADType type);

	void Serialize(FSerializer &arc);
	void Tick ();

	bool StartClosing ();
protected:
	line_t *m_Line1, *m_Line2;
	int m_Frame;
	FDoorAnimation *m_DoorAnim;
	int m_Timer;
	double m_BotDist;
	int m_Status;
	int m_Type;
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

	friend bool EV_SlidingDoor (line_t *line, AActor *thing, int tag, int speed, int delay, EADType type);
private:
	DAnimatedDoor ();
};

bool EV_SlidingDoor (line_t *line, AActor *thing, int tag, int speed, int delay, DAnimatedDoor::EADType type);

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
		ceil_placeholder,
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

	enum class ECrushMode
	{
		crushDoom = 0,
		crushHexen = 1,
		crushSlowdown = 2
	};


	DCeiling (sector_t *sec);
	DCeiling (sector_t *sec, double speed1, double speed2, int silent);

	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	ECeiling	m_Type;
	double	 	m_BottomHeight;
	double	 	m_TopHeight;
	double	 	m_Speed;
	double		m_Speed1;		// [RH] dnspeed of crushers
	double		m_Speed2;		// [RH] upspeed of crushers
	int 		m_Crush;
	ECrushMode	m_CrushMode;
	int			m_Silent;
	int 		m_Direction;	// 1 = up, 0 = waiting, -1 = down

	// [RH] Need these for BOOM-ish transferring ceilings
	FTextureID	m_Texture;
	secspecial_t m_NewSpecial{};

	// ID
	int 		m_Tag;
	int 		m_OldDirection;

	void PlayCeilingSound ();

private:
	DCeiling ();

	friend bool P_CreateCeiling(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, double speed, double speed2, double height, int crush, int silent, int change, DCeiling::ECrushMode hexencrush);
	friend bool EV_CeilingCrushStop (int tag, bool remove);
	friend void P_ActivateInStasisCeiling (int tag);
};

bool P_CreateCeiling(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, double speed, double speed2, double height, int crush, int silent, int change, DCeiling::ECrushMode hexencrush);
bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line, int tag, double speed, double speed2, double height, int crush, int silent, int change, DCeiling::ECrushMode hexencrush = DCeiling::ECrushMode::crushDoom);

bool EV_CeilingCrushStop (int tag, bool remove);
bool EV_StopCeiling(int tag, line_t *line);
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

	enum EStairType
	{
		stairUseSpecials = 1,
		stairSync = 2,
		stairCrush = 4,
	};

	DFloor (sector_t *sec);

	void Serialize(FSerializer &arc);
	void Tick ();

//protected:
	EFloor	 	m_Type;
	int 		m_Crush;
	bool		m_Hexencrush;
	bool		m_Instant;
	int 		m_Direction;
	secspecial_t m_NewSpecial{};
	FTextureID	m_Texture;
	double	 	m_FloorDestDist;
	double	 	m_Speed;

	// [RH] New parameters used to reset and delay stairs
	double		m_OrgDist;
	int			m_ResetCount;
	int			m_Delay;
	int			m_PauseTime;
	int			m_StepTime;
	int			m_PerStepTime;

	void StartFloorSound ();
	void SetFloorChangeType (sector_t *sec, int change);

	friend bool EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
		double stairsize, double speed, int delay, int reset, int igntxt,
		int usespecials);
	friend bool EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
		double speed, double height, int crush, int change, bool hexencrush, bool hereticlower);
	friend bool EV_DoDonut (int tag, line_t *line, double pillarspeed, double slimespeed);
private:
	DFloor ();
};

bool P_CreateFloor(sector_t *sec, DFloor::EFloor floortype, line_t *line,
	double speed, double height, int crush, int change, bool hexencrush, bool hereticlower);

bool EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
	double stairsize, double speed, int delay, int reset, int igntxt,
	int usespecials);
bool EV_DoFloor(DFloor::EFloor floortype, line_t *line, int tag,
	double speed, double height, int crush, int change, bool hexencrush, bool hereticlower = false);

bool EV_FloorCrushStop (int tag, line_t *line);
bool EV_StopFloor(int tag, line_t *line);
bool EV_DoDonut (int tag, line_t *line, double pillarspeed, double slimespeed);

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

	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	EElevator	m_Type;
	int			m_Direction;
	double		m_FloorDestDist;
	double		m_CeilingDestDist;
	double		m_Speed;
	TObjPtr<DInterpolation*> m_Interp_Ceiling;
	TObjPtr<DInterpolation*> m_Interp_Floor;

	void StartFloorSound ();

	friend bool EV_DoElevator (line_t *line, DElevator::EElevator type, double speed, double height, int tag);
private:
	DElevator ();
};

bool EV_DoElevator (line_t *line, DElevator::EElevator type, double speed, double height, int tag);

class DWaggleBase : public DMover
{
	DECLARE_CLASS (DWaggleBase, DMover)
	HAS_OBJECT_POINTERS
public:
	DWaggleBase (sector_t *sec);

	void Serialize(FSerializer &arc);

protected:
	double m_OriginalDist;
	double m_Accumulator;
	double m_AccDelta;
	double m_TargetScale;
	double m_Scale;
	double m_ScaleDelta;
	int m_Ticker;
	int m_State;

	friend bool EV_StartWaggle (int tag, line_t *line, int height, int speed,
		int offset, int timer, bool ceiling);

	void DoWaggle (bool ceiling);
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
	TELF_ROTATEBOOM			= 32,
	TELF_ROTATEBOOMINVERSE	= 64,
};

//Spawns teleport fog. Pass the actor to pluck TeleFogFromType and TeleFogToType. 'from' determines if this is the fog to spawn at the old position (true) or new (false).
void P_SpawnTeleportFog(AActor *mobj, const DVector3 &pos, bool beforeTele = true, bool setTarget = false);

bool P_Teleport(AActor *thing, DVector3 pos, DAngle angle, int flags);
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
bool P_StartQuakeXYZ(AActor *activator, int tid, int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, FSoundID quakesfx, int flags, double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint, double rollIntensity, double rollWave);
bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx);

#endif
