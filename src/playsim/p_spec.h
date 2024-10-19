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


class MapLoader;

// every tic
void	P_UpdateSpecials (FLevelLocals *);

// when needed
bool	P_ActivateLine (line_t *ld, AActor *mo, int side, int activationType, DVector3 *optpos = NULL);
bool	P_TestActivateLine (line_t *ld, AActor *mo, int side, int activationType, DVector3 *optpos = NULL);
bool	P_PredictLine (line_t *ld, AActor *mo, int side, int activationType);

void 	P_PlayerInSpecialSector (player_t *player, sector_t * sector=NULL);
void	P_PlayerOnSpecialFlat (player_t *player, int floorType);
void	P_SectorDamage(FLevelLocals *Level, int tag, int amount, FName type, PClassActor *protectClass, int flags);
void	P_SetSectorFriction (FLevelLocals *level, int tag, int amount, bool alterFlag);
double FrictionToMoveFactor(double friction);
void P_GiveSecret(FLevelLocals *Level, AActor *actor, bool printmessage, bool playsound, int sectornum);

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
// P_SWITCH
//

#define BUTTONTIME TICRATE		// 1 second, in ticks. 

bool	P_ChangeSwitchTexture (side_t *side, int useAgain, uint8_t special, bool *quest=NULL);
bool	P_CheckSwitchRange(AActor *user, line_t *line, int sideno, const DVector3 *optpos = NULL);

#include "a_plats.h"
#include "a_pillar.h"
#include "a_doors.h"
#include "a_ceiling.h"
#include "a_floor.h"
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
	TELF_FDCOMPAT			= 128,
};

//Spawns teleport fog. Pass the actor to pluck TeleFogFromType and TeleFogToType. 'from' determines if this is the fog to spawn at the old position (true) or new (false).
void P_SpawnTeleportFog(AActor *mobj, const DVector3 &pos, bool beforeTele = true, bool setTarget = false);
bool P_Teleport(AActor *thing, DVector3 pos, DAngle angle, int flags);


//
// [RH] ACS (see also p_acs.h)
//

#define ACS_BACKSIDE		1
#define ACS_ALWAYS			2
#define ACS_WANTRESULT		4
#define ACS_NET				8

int  P_StartScript (FLevelLocals *Level, AActor *who, line_t *where, int script, const char *map, const int *args, int argcount, int flags);
void P_SuspendScript (FLevelLocals *Level, int script, const char *map);
void P_TerminateScript (FLevelLocals *Level, int script, const char *map);

//
// [RH] p_quake.c
//
bool P_StartQuakeXYZ(FLevelLocals *Level, AActor *activator, int tid, double intensityX, double intensityY, double intensityZ, int duration, double damrad, double tremrad, FSoundID quakesfx, int flags, double waveSpeedX, double waveSpeedY, double waveSpeedZ, double falloff, int highpoint, double rollIntensity, double rollWave, double damageMultiplier, double thrustMultiplier, int damage);
bool P_StartQuake(FLevelLocals *Level, AActor *activator, int tid, double intensity, int duration, double damrad, double tremrad, FSoundID quakesfx);

#endif
