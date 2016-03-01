/*
** p_lnspec.h
** New line and sector specials (Using Hexen as a base.)
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __P_LNSPEC_H__
#define __P_LNSPEC_H__

#include "doomtype.h"

#define DEFINE_SPECIAL(name, num, min, max, map) name = num,

typedef enum {
#include "actionspecials.h"
} linespecial_t;

struct FLineSpecial
{
	const char *name;
	int number;
	SBYTE min_args;
	SBYTE max_args;
	BYTE map_args;
};


typedef enum {
	Init_Gravity = 0,
	Init_Color = 1,
	Init_Damage = 2,
	Init_SectorLink = 3,
	NUM_STATIC_INITS,
	Init_EDSector = 253,
	Init_EDLine = 254,
	Init_TransferSky = 255
} staticinit_t;

typedef enum {
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

	
	Sector_Hidden = 195,
	Sector_Heal = 196, // Caverns of Darkness healing sector

	Light_OutdoorLightning = 197,
	Light_IndoorLightning1 = 198,
	Light_IndoorLightning2 = 199,

	Sky2 = 200,

	// Hexen-type scrollers
	Scroll_North_Slow = 201,
	Scroll_North_Medium = 202,
	Scroll_North_Fast = 203,
	Scroll_East_Slow = 204,
	Scroll_East_Medium = 205,
	Scroll_East_Fast = 206,
	Scroll_South_Slow = 207,
	Scroll_South_Medium = 208,
	Scroll_South_Fast = 209,
	Scroll_West_Slow = 210,
	Scroll_West_Medium = 211,
	Scroll_West_Fast = 212,
	Scroll_NorthWest_Slow = 213,
	Scroll_NorthWest_Medium = 214,
	Scroll_NorthWest_Fast = 215,
	Scroll_NorthEast_Slow = 216,
	Scroll_NorthEast_Medium = 217,
	Scroll_NorthEast_Fast = 218,
	Scroll_SouthEast_Slow = 219,
	Scroll_SouthEast_Medium = 220,
	Scroll_SouthEast_Fast = 221,
	Scroll_SouthWest_Slow = 222,
	Scroll_SouthWest_Medium = 223,
	Scroll_SouthWest_Fast = 224,

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

} sectorspecial_t;

// [RH] Equivalents for BOOM's generalized sector types

#define DAMAGE_MASK		0x0300
#define SECRET_MASK		0x0400
#define FRICTION_MASK	0x0800
#define PUSH_MASK		0x1000

struct line_t;
class AActor;

FName MODtoDamageType (int mod);

typedef int (*lnSpecFunc)(struct line_t	*line,
						  class AActor	*activator,
						  bool			backSide,
						  int			arg1,
						  int			arg2,
						  int			arg3,
						  int			arg4,
						  int			arg5);

extern BYTE NamedACSToNormalACS[7];
static inline bool P_IsACSSpecial(int specnum)
{
	return (specnum >= ACS_Execute && specnum <= ACS_LockedExecuteDoor) ||
			specnum == ACS_ExecuteAlways;
}

FLineSpecial *P_GetLineSpecialInfo(int num);
int P_GetMaxLineSpecial();
int P_FindLineSpecial (const char *string, int *min_args=NULL, int *max_args=NULL);
bool P_ActivateThingSpecial(AActor * thing, AActor * trigger, bool death=false);
int P_ExecuteSpecial(int			num,
					 struct line_t	*line,
					 class AActor	*activator,
					 bool			backSide,
					 int			arg1,
					 int			arg2,
					 int			arg3,
					 int			arg4,
					 int			arg5);

#endif //__P_LNSPEC_H__
