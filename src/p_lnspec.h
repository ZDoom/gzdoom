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

typedef enum {
	Polyobj_StartLine = 1,
	Polyobj_RotateLeft = 2,
	Polyobj_RotateRight = 3,
	Polyobj_Move = 4,
	Polyobj_ExplicitLine = 5,
	Polyobj_MoveTimes8 = 6,
	Polyobj_DoorSwing = 7,
	Polyobj_DoorSlide = 8,

	Line_Horizon = 9,	// [RH] draw one-sided wall at horizon

	Door_Close = 10,
	Door_Open = 11,
	Door_Raise = 12,
	Door_LockedRaise = 13,
	Door_Animated = 14,

	Autosave = 15,	// [RH] Save the game *now*
	Transfer_WallLight = 16,
	Thing_Raise = 17,
	StartConversation = 18,
	Thing_Stop = 19,

	Floor_LowerByValue = 20,
	Floor_LowerToLowest = 21,
	Floor_LowerToNearest = 22,
	Floor_RaiseByValue = 23,
	Floor_RaiseToHighest = 24,
	Floor_RaiseToNearest = 25,

	Stairs_BuildDown = 26,
	Stairs_BuildUp = 27,

	Floor_RaiseAndCrush = 28,

	Pillar_Build = 29,
	Pillar_Open = 30,

	Stairs_BuildDownSync = 31,
	Stairs_BuildUpSync = 32,

	ForceField = 33,	// [RH] Strife's forcefield special (148)
	ClearForceField = 34,	// [RH] Remove Strife's forcefield from tagged sectors

	Floor_RaiseByValueTimes8 = 35,
	Floor_LowerByValueTimes8 = 36,
	Floor_MoveToValue = 37,

	Ceiling_Waggle = 38,	// [RH] Complement of Floor_Waggle
	Teleport_ZombieChanger = 39,	// [RH] Needed for Strife

	Ceiling_LowerByValue = 40,
	Ceiling_RaiseByValue = 41,
	Ceiling_CrushAndRaise = 42,
	Ceiling_LowerAndCrush = 43,
	Ceiling_CrushStop = 44,
	Ceiling_CrushRaiseAndStay = 45,

	Floor_CrushStop = 46,
	Ceiling_MoveToValue = 47,
	Sector_Attach3dMidtex = 48,

	GlassBreak = 49,
	ExtraFloor_LightOnly = 50,

	Plat_PerpetualRaise = 60,
	Plat_Stop = 61,
	Plat_DownWaitUpStay = 62,
	Plat_DownByValue = 63,
	Plat_UpWaitDownStay = 64,
	Plat_UpByValue = 65,

	Floor_LowerInstant = 66,
	Floor_RaiseInstant = 67,
	Floor_MoveToValueTimes8 = 68,

	Ceiling_MoveToValueTimes8 = 69,

	Teleport = 70,
	Teleport_NoFog = 71,

	ThrustThing = 72,
	DamageThing = 73,

	Teleport_NewMap = 74,
	Teleport_EndGame = 75,
	TeleportOther = 76,
	TeleportGroup = 77,
	TeleportInSector = 78,

	ACS_Execute = 80,
	ACS_Suspend = 81,
	ACS_Terminate = 82,
	ACS_LockedExecute = 83,
	ACS_ExecuteWithResult = 84,
	ACS_LockedExecuteDoor = 85,

	Polyobj_OR_RotateLeft = 90,
	Polyobj_OR_RotateRight = 91,
	Polyobj_OR_Move = 92,
	Polyobj_OR_MoveTimes8 = 93,

	Pillar_BuildAndCrush = 94,

	FloorAndCeiling_LowerByValue = 95,
	FloorAndCeiling_RaiseByValue = 96,

	Scroll_Texture_Left = 100,
	Scroll_Texture_Right = 101,
	Scroll_Texture_Up = 102,
	Scroll_Texture_Down = 103,

	Light_ForceLightning = 109,
	Light_RaiseByValue = 110,
	Light_LowerByValue = 111,
	Light_ChangeToValue = 112,
	Light_Fade = 113,
	Light_Glow = 114,
	Light_Flicker = 115,
	Light_Strobe = 116,
	Light_Stop = 117,

	Thing_Damage = 119,

	Radius_Quake = 120,	// Earthquake

	Line_SetIdentification = 121,
#if 0 // Skull Tag specials that might be added later
	Thing_SetGravity = 122,
	Thing_ReverseGravity = 123,
	Thing_RevertGravity = 124,
#endif
	Thing_Move = 125,
#if 0 // Skull Tag special I doubt I will add
	Thing_SetSprite = 126,
#endif
	Thing_SetSpecial = 127,
	ThrustThingZ = 128,

	UsePuzzleItem = 129,

	Thing_Activate = 130,
	Thing_Deactivate = 131,
	Thing_Remove = 132,
	Thing_Destroy = 133,
	Thing_Projectile = 134,
	Thing_Spawn = 135,
	Thing_ProjectileGravity = 136,
	Thing_SpawnNoFog = 137,

	Floor_Waggle = 138,

	Thing_SpawnFacing = 139,

	Sector_ChangeSound = 140,

	// GZDoom/Vavoom specials (put here so that they don't get accidentally redefined)
	Sector_SetPlaneReflection = 159,
	Sector_Set3DFloor = 160,
	Sector_SetContents = 161,

// [RH] Begin new specials for ZDoom
	Sector_SetCeilingScale2 = 170,
	Sector_SetFloorScale2 = 171,
	
	Plat_UpNearestWaitDownStay = 172,
	NoiseAlert = 173,
	SendToCommunicator = 174,
	Thing_ProjectileIntercept = 175,
	Thing_ChangeTID = 176,
	Thing_Hate = 177,
	Thing_ProjectileAimed = 178,
	ChangeSkill = 179,
	Thing_SetTranslation = 180,
	Plane_Align = 181,

	Line_Mirror = 182,
	Line_AlignCeiling = 183,
	Line_AlignFloor = 184,

	Sector_SetRotation = 185,
	Sector_SetCeilingOffset = 186,
	Sector_SetFloorOffset = 187,
	Sector_SetCeilingScale = 188,
	Sector_SetFloorScale = 189,

	Static_Init = 190,

	SetPlayerProperty = 191,

	Ceiling_LowerToHighestFloor = 192,
	Ceiling_LowerInstant = 193,
	Ceiling_RaiseInstant = 194,
	Ceiling_CrushRaiseAndStayA = 195,
	Ceiling_CrushAndRaiseA = 196,
	Ceiling_CrushAndRaiseSilentA = 197,
	Ceiling_RaiseByValueTimes8 = 198,
	Ceiling_LowerByValueTimes8 = 199,

	Generic_Floor = 200,
	Generic_Ceiling = 201,
	Generic_Door = 202,
	Generic_Lift = 203,
	Generic_Stairs = 204,
	Generic_Crusher = 205,

	Plat_DownWaitUpStayLip = 206,
	Plat_PerpetualRaiseLip = 207,

	TranslucentLine = 208,
	Transfer_Heights = 209,
	Transfer_FloorLight = 210,
	Transfer_CeilingLight = 211,

	Sector_SetColor = 212,
	Sector_SetFade = 213,
	Sector_SetDamage = 214,

	Teleport_Line = 215,

	Sector_SetGravity = 216,

	Stairs_BuildUpDoom = 217,

	Sector_SetWind = 218,
	Sector_SetFriction = 219,
	Sector_SetCurrent = 220,

	Scroll_Texture_Both = 221,
	Scroll_Texture_Model = 222,
	Scroll_Floor = 223,
	Scroll_Ceiling = 224,
	Scroll_Texture_Offsets = 225,

	ACS_ExecuteAlways = 226,

	PointPush_SetForce = 227,

	Plat_RaiseAndStayTx0 = 228,

	Thing_SetGoal = 229,

	Plat_UpByValueStayTx = 230,
	Plat_ToggleCeiling = 231,

	Light_StrobeDoom = 232,
	Light_MinNeighbor = 233,
	Light_MaxNeighbor = 234,

	Floor_TransferTrigger = 235,
	Floor_TransferNumeric = 236,

	ChangeCamera = 237,

	Floor_RaiseToLowestCeiling = 238,
	Floor_RaiseByValueTxTy = 239,
	Floor_RaiseByTexture = 240,
	Floor_LowerToLowestTxTy = 241,
	Floor_LowerToHighest = 242,

	Exit_Normal = 243,
	Exit_Secret = 244,

	Elevator_RaiseToNearest = 245,
	Elevator_MoveToFloor = 246,
	Elevator_LowerToNearest = 247,

	HealThing = 248,
	Door_CloseWaitOpen = 249,

	Floor_Donut = 250,

	FloorAndCeiling_LowerRaise = 251,

	Ceiling_RaiseToNearest = 252,
	Ceiling_LowerToLowest = 253,
	Ceiling_LowerToFloor = 254,
	Ceiling_CrushRaiseAndStaySilA = 255
} linespecial_t;

typedef enum {
	Init_Gravity = 0,
	Init_Color = 1,
	Init_Damage = 2,
	NUM_STATIC_INITS,
	Init_TransferSky = 255
} staticinit_t;

typedef enum {
	Light_Phased = 1,
	LightSequenceStart = 2,
	LightSequenceSpecial1 = 3,
	LightSequenceSpecial2 = 4,

	Stairs_Special1 = 26,
	Stairs_Special2 = 27,

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

struct line_s;
class AActor;

FName MODtoDamageType (int mod);

typedef int (*lnSpecFunc)(struct line_s	*line,
						  class AActor	*activator,
						  bool			backSide,
						  int			arg1,
						  int			arg2,
						  int			arg3,
						  int			arg4,
						  int			arg5);

extern lnSpecFunc LineSpecials[256];

#endif //__P_LNSPEC_H__
