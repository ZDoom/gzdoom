/*
** p_lnspec.cpp
** Handles line specials
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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
** Each function returns true if it caused something to happen
** or false if it could not perform the desired action.
*/

#include "doomstat.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "g_level.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "a_lightning.h"
#include "a_keys.h"
#include "gi.h"
#include "p_conversation.h"
#include "p_3dmidtex.h"
#include "d_net.h"
#include "d_event.h"
#include "gstrings.h"
#include "po_man.h"
#include "d_player.h"
#include "r_utility.h"
#include "fragglescript/t_fs.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "vm.h"
#include "p_destructible.h"

// Remaps EE sector change types to Generic_Floor values. According to the Eternity Wiki:
/*
    0 : No texture or type change. ( = 0)
    1 : Copy texture, zero type; trigger model. ( = 1)
    2 : Copy texture, zero type; numeric model. ( = 1+4)
    3 : Copy texture, preserve type; trigger model. ( = 3)
    4 : Copy texture, preserve type; numeric model. ( = 3+4)
    5 : Copy texture and type; trigger model.  ( = 2)
    6 : Copy texture and type; numeric model.  ( = 2+4)
*/
static const uint8_t ChangeMap[8] = { 0, 1, 5, 3, 7, 2, 6, 0 };


#define FUNC(a) static int a (line_t *ln, AActor *it, bool backSide, \
	int arg0, int arg1, int arg2, int arg3, int arg4)

#define SPEED(a)		((a) / 8.)
#define TICS(a)			(((a)*TICRATE)/35)
#define OCTICS(a)		(((a)*TICRATE)/8)
#define BYTEANGLE(a)	((a) * (360./256.))
#define CRUSH(a)		((a) > 0? (a) : -1)
#define CHANGE(a)		(((a) >= 0 && (a)<=7)? ChangeMap[a]:0)

static bool CRUSHTYPE(int a)
{
	return ((a) == 1 ? false : (a) == 2 ? true : gameinfo.gametype == GAME_Hexen);
}

static DCeiling::ECrushMode CRUSHTYPE(int a, bool withslowdown)
{
	static DCeiling::ECrushMode map[] = { DCeiling::ECrushMode::crushDoom, DCeiling::ECrushMode::crushHexen, DCeiling::ECrushMode::crushSlowdown };
	if (a >= 1 && a <= 3) return map[a - 1];
	if (gameinfo.gametype == GAME_Hexen) return DCeiling::ECrushMode::crushHexen;
	return withslowdown? DCeiling::ECrushMode::crushSlowdown : DCeiling::ECrushMode::crushDoom;
}

static FRandom pr_glass ("GlassBreak");

// There are aliases for the ACS specials that take names instead of numbers.
// This table maps them onto the real number-based specials.
uint8_t NamedACSToNormalACS[7] =
{
	ACS_Execute,
	ACS_Suspend,
	ACS_Terminate,
	ACS_LockedExecute,
	ACS_LockedExecuteDoor,
	ACS_ExecuteWithResult,
	ACS_ExecuteAlways,
};

FName MODtoDamageType (int mod)
{
	switch (mod)
	{
	default:	return NAME_None;			break;
	case 9:		return NAME_BFGSplash;		break;
	case 12:	return NAME_Drowning;		break;
	case 13:	return NAME_Slime;			break;
	case 14:	return NAME_Fire;			break;
	case 15:	return NAME_Crush;			break;
	case 16:	return NAME_Telefrag;		break;
	case 17:	return NAME_Falling;		break;
	case 18:	return NAME_Suicide;		break;
	case 20:	return NAME_Exit;			break;
	case 22:	return NAME_Melee;			break;
	case 23:	return NAME_Railgun;		break;
	case 24:	return NAME_Ice;			break;
	case 25:	return NAME_Disintegrate;	break;
	case 26:	return NAME_Poison;			break;
	case 27:	return NAME_Electric;		break;
	case 1000:	return NAME_Massacre;		break;
	}
}

FUNC(LS_NOP)
{
	return false;
}

FUNC(LS_Polyobj_RotateLeft)
// Polyobj_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, 1, false);
}

FUNC(LS_Polyobj_RotateRight)
// Polyobj_rotateRight (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, -1, false);
}

FUNC(LS_Polyobj_Move)
// Polyobj_Move (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3, false);
}

FUNC(LS_Polyobj_MoveTimes8)
// Polyobj_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * 8, false);
}

FUNC(LS_Polyobj_MoveTo)
// Polyobj_MoveTo (po, speed, x, y)
{
	return EV_MovePolyTo (ln, arg0, SPEED(arg1), DVector2(arg2, arg3), false);
}

FUNC(LS_Polyobj_MoveToSpot)
// Polyobj_MoveToSpot (po, speed, tid)
{
	FActorIterator iterator (arg2);
	AActor *spot = iterator.Next();
	if (spot == NULL) return false;
	return EV_MovePolyTo (ln, arg0, SPEED(arg1), spot->Pos(), false);
}

FUNC(LS_Polyobj_DoorSwing)
// Polyobj_DoorSwing (po, speed, angle, delay)
{
	return EV_OpenPolyDoor (ln, arg0, arg1, BYTEANGLE(arg2), arg3, 0, PODOOR_SWING);
}

FUNC(LS_Polyobj_DoorSlide)
// Polyobj_DoorSlide (po, speed, angle, distance, delay)
{
	return EV_OpenPolyDoor (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg4, arg3, PODOOR_SLIDE);
}

FUNC(LS_Polyobj_OR_RotateLeft)
// Polyobj_OR_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, 1, true);
}

FUNC(LS_Polyobj_OR_RotateRight)
// Polyobj_OR_RotateRight (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, -1, true);
}

FUNC(LS_Polyobj_OR_Move)
// Polyobj_OR_Move (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3, true);
}

FUNC(LS_Polyobj_OR_MoveTimes8)
// Polyobj_OR_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * 8, true);
}

FUNC(LS_Polyobj_OR_MoveTo)
// Polyobj_OR_MoveTo (po, speed, x, y)
{
	return EV_MovePolyTo (ln, arg0, SPEED(arg1), DVector2(arg2, arg3), true);
}

FUNC(LS_Polyobj_OR_MoveToSpot)
// Polyobj_OR_MoveToSpot (po, speed, tid)
{
	FActorIterator iterator (arg2);
	AActor *spot = iterator.Next();
	if (spot == NULL) return false;
	return EV_MovePolyTo (ln, arg0, SPEED(arg1), spot->Pos(), true);
}

FUNC(LS_Polyobj_Stop)
// Polyobj_Stop (po)
{
	return EV_StopPoly (arg0);
}

FUNC(LS_Door_Close)
// Door_Close (tag, speed, lighttag)
{
	return EV_DoDoor (DDoor::doorClose, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Open)
// Door_Open (tag, speed, lighttag)
{
	return EV_DoDoor (DDoor::doorOpen, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Raise)
// Door_Raise (tag, speed, delay, lighttag)
{
	return EV_DoDoor (DDoor::doorRaise, ln, it, arg0, SPEED(arg1), TICS(arg2), 0, arg3);
}

FUNC(LS_Door_LockedRaise)
// Door_LockedRaise (tag, speed, delay, lock, lighttag)
{
#if 0
	// In Hexen this originally created a thinker running for nearly 4 years.
	// Let's not do this unless it becomes necessary because this can hang tagwait.
	return EV_DoDoor (arg2 || (level.flags2 & LEVEL2_HEXENHACK) ? DDoor::doorRaise : DDoor::doorOpen, ln, it,
#else
	return EV_DoDoor (arg2 ? DDoor::doorRaise : DDoor::doorOpen, ln, it,
#endif
					  arg0, SPEED(arg1), TICS(arg2), arg3, arg4);
}

FUNC(LS_Door_CloseWaitOpen)
// Door_CloseWaitOpen (tag, speed, delay, lighttag)
{
	return EV_DoDoor (DDoor::doorCloseWaitOpen, ln, it, arg0, SPEED(arg1), OCTICS(arg2), 0, arg3);
}

FUNC(LS_Door_WaitRaise)
// Door_WaitRaise(tag, speed, delay, wait, lighttag)
{
	return EV_DoDoor(DDoor::doorWaitRaise, ln, it, arg0, SPEED(arg1), TICS(arg2), 0, arg4, false, TICS(arg3));
}

FUNC(LS_Door_WaitClose)
// Door_WaitRaise(tag, speed, wait, lighttag)
{
	return EV_DoDoor(DDoor::doorWaitClose, ln, it, arg0, SPEED(arg1), 0, 0, arg3, false, TICS(arg2));
}

FUNC(LS_Door_Animated)
// Door_Animated (tag, speed, delay, lock)
{
	if (arg3 != 0 && !P_CheckKeys (it, arg3, arg0 != 0))
		return false;

	return EV_SlidingDoor (ln, it, arg0, arg1, arg2, DAnimatedDoor::adOpenClose);
}

FUNC(LS_Door_AnimatedClose)
// Door_AnimatedClose (tag, speed)
{
	return EV_SlidingDoor(ln, it, arg0, arg1, -1, DAnimatedDoor::adClose);
}

FUNC(LS_Generic_Door)
// Generic_Door (tag, speed, kind, delay, lock)
{
	int tag, lightTag;
	DDoor::EVlDoor type;
	bool boomgen = false;

	switch (arg2 & 63)
	{
		case 0: type = DDoor::doorRaise;			break;
		case 1: type = DDoor::doorOpen;				break;
		case 2: type = DDoor::doorCloseWaitOpen;	break;
		case 3: type = DDoor::doorClose;			break;
		default: return false;
	}
	// Boom doesn't allow manual generalized doors to be activated while they move
	if (arg2 & 64) boomgen = true;
	if (arg2 & 128)
	{
		// New for 2.0.58: Finally support BOOM's local door light effect
		tag = 0;
		lightTag = arg0;
	}
	else
	{
		tag = arg0;
		lightTag = 0;
	}
	return EV_DoDoor (type, ln, it, tag, SPEED(arg1), OCTICS(arg3), arg4, lightTag, boomgen);
}

FUNC(LS_Floor_LowerByValue)
// Floor_LowerByValue (tag, speed, height, change)
{
	return EV_DoFloor (DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), arg2, -1, CHANGE(arg3), false);
}

FUNC(LS_Floor_LowerToLowest)
// Floor_LowerToLowest (tag, speed, change)
{
	return EV_DoFloor (DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, -1, CHANGE(arg2), false);
}

FUNC(LS_Floor_LowerToHighest)
// Floor_LowerToHighest (tag, speed, adjust, hereticlower)
{
	return EV_DoFloor (DFloor::floorLowerToHighest, ln, arg0, SPEED(arg1), (arg2-128), -1, 0, false, arg3==1);
}

FUNC(LS_Floor_LowerToHighestEE)
// Floor_LowerToHighestEE (tag, speed, change)
{
	return EV_DoFloor (DFloor::floorLowerToHighest, ln, arg0, SPEED(arg1), 0, -1, CHANGE(arg2), false);
}

FUNC(LS_Floor_LowerToNearest)
// Floor_LowerToNearest (tag, speed, change)
{
	return EV_DoFloor (DFloor::floorLowerToNearest, ln, arg0, SPEED(arg1), 0, -1, CHANGE(arg2), false);
}

FUNC(LS_Floor_RaiseByValue)
// Floor_RaiseByValue (tag, speed, height, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), arg2, CRUSH(arg4), CHANGE(arg3), true);
}

FUNC(LS_Floor_RaiseToHighest)
// Floor_RaiseToHighest (tag, speed, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseToHighest, ln, arg0, SPEED(arg1), 0, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseToNearest)
// Floor_RaiseToNearest (tag, speed, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseToNearest, ln, arg0, SPEED(arg1), 0, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseToLowest)
// Floor_RaiseToLowest (tag, change, crush)
{
	// This is merely done for completeness as it's a rather pointless addition.
	return EV_DoFloor (DFloor::floorRaiseToLowest, ln, arg0, 2., 0, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseAndCrush)
// Floor_RaiseAndCrush (tag, speed, crush, crushmode)
{
	return EV_DoFloor (DFloor::floorRaiseAndCrush, ln, arg0, SPEED(arg1), 0, arg2, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Floor_RaiseAndCrushDoom)
// Floor_RaiseAndCrushDoom (tag, speed, crush, crushmode)
{
	return EV_DoFloor (DFloor::floorRaiseAndCrushDoom, ln, arg0, SPEED(arg1), 0, arg2, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Floor_RaiseByValueTimes8)
// FLoor_RaiseByValueTimes8 (tag, speed, height, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), arg2*8, CRUSH(arg4), CHANGE(arg3), true);
}

FUNC(LS_Floor_LowerByValueTimes8)
// Floor_LowerByValueTimes8 (tag, speed, height, change)
{
	return EV_DoFloor (DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), arg2*8, -1, CHANGE(arg3), false);
}

FUNC(LS_Floor_CrushStop)
// Floor_CrushStop (tag)
{
	return EV_FloorCrushStop (arg0, ln);
}

FUNC(LS_Floor_LowerInstant)
// Floor_LowerInstant (tag, unused, height, change)
{
	return EV_DoFloor (DFloor::floorLowerInstant, ln, arg0, 0., arg2*8, -1, CHANGE(arg3), false);
}

FUNC(LS_Floor_RaiseInstant)
// Floor_RaiseInstant (tag, unused, height, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseInstant, ln, arg0, 0., arg2*8, CRUSH(arg4), CHANGE(arg3), true);
}

FUNC(LS_Floor_ToCeilingInstant)
// Floor_ToCeilingInstant (tag, change, crush, gap)
{
	return EV_DoFloor (DFloor::floorLowerToCeiling, ln, arg0, 0, arg3, CRUSH(arg2), CHANGE(arg1), true);
}

FUNC(LS_Floor_MoveToValueTimes8)
// Floor_MoveToValueTimes8 (tag, speed, height, negative, change)
{
	return EV_DoFloor (DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*8*(arg3?-1:1), -1, CHANGE(arg4), false);
}

FUNC(LS_Floor_MoveToValue)
// Floor_MoveToValue (tag, speed, height, negative, change)
{
	return EV_DoFloor (DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*(arg3?-1:1), -1, CHANGE(arg4), false);
}

FUNC(LS_Floor_MoveToValueAndCrush)
// Floor_MoveToValueAndCrush (tag, speed, height, crush, crushmode)
{
	return EV_DoFloor(DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
		arg2, CRUSH(arg3) -1, 0, CRUSHTYPE(arg4), false);
}

FUNC(LS_Floor_RaiseToLowestCeiling)
// Floor_RaiseToLowestCeiling (tag, speed, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseToLowestCeiling, ln, arg0, SPEED(arg1), 0, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_LowerToLowestCeiling)
// Floor_LowerToLowestCeiling (tag, speed, change)
{
	return EV_DoFloor (DFloor::floorLowerToLowestCeiling, ln, arg0, SPEED(arg1), arg4, -1, CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseByTexture)
// Floor_RaiseByTexture (tag, speed, change, crush)
{
	return EV_DoFloor (DFloor::floorRaiseByTexture, ln, arg0, SPEED(arg1), 0, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_LowerByTexture)
// Floor_LowerByTexture (tag, speed, change, crush)
{
	return EV_DoFloor (DFloor::floorLowerByTexture, ln, arg0, SPEED(arg1), 0, -1, CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseToCeiling)
// Floor_RaiseToCeiling (tag, speed, change, crush, gap)
{
	return EV_DoFloor (DFloor::floorRaiseToCeiling, ln, arg0, SPEED(arg1), arg4, CRUSH(arg3), CHANGE(arg2), true);
}

FUNC(LS_Floor_RaiseByValueTxTy)
// Floor_RaiseByValueTxTy (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorRaiseAndChange, ln, arg0, SPEED(arg1), arg2, -1, 0, false);
}

FUNC(LS_Floor_LowerToLowestTxTy)
// Floor_LowerToLowestTxTy (tag, speed)
{
	return EV_DoFloor (DFloor::floorLowerAndChange, ln, arg0, SPEED(arg1), arg2, -1, 0, false);
}

FUNC(LS_Floor_Waggle)
// Floor_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (arg0, ln, arg1, arg2, arg3, arg4, false);
}

FUNC(LS_Ceiling_Waggle)
// Ceiling_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (arg0, ln, arg1, arg2, arg3, arg4, true);
}

FUNC(LS_Floor_TransferTrigger)
// Floor_TransferTrigger (tag)
{
	return EV_DoChange (ln, trigChangeOnly, arg0);
}

FUNC(LS_Floor_TransferNumeric)
// Floor_TransferNumeric (tag)
{
	return EV_DoChange (ln, numChangeOnly, arg0);
}

FUNC(LS_Floor_Donut)
// Floor_Donut (pillartag, pillarspeed, slimespeed)
{
	return EV_DoDonut (arg0, ln, SPEED(arg1), SPEED(arg2));
}

FUNC(LS_Generic_Floor)
// Generic_Floor (tag, speed, height, target, change/model/direct/crush)
{
	DFloor::EFloor type;

	if (arg4 & 8)
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorRaiseToHighest;			break;
			case 2: type = DFloor::floorRaiseToLowest;			break;
			case 3: type = DFloor::floorRaiseToNearest;			break;
			case 4: type = DFloor::floorRaiseToLowestCeiling;	break;
			case 5: type = DFloor::floorRaiseToCeiling;			break;
			case 6: type = DFloor::floorRaiseByTexture;			break;
			default:type = DFloor::floorRaiseByValue;			break;
		}
	}
	else
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorLowerToHighest;			break;
			case 2: type = DFloor::floorLowerToLowest;			break;
			case 3: type = DFloor::floorLowerToNearest;			break;
			case 4: type = DFloor::floorLowerToLowestCeiling;	break;
			case 5: type = DFloor::floorLowerToCeiling;			break;
			case 6: type = DFloor::floorLowerByTexture;			break;
			default:type = DFloor::floorLowerByValue;			break;
		}
	}

	return EV_DoFloor (type, ln, arg0, SPEED(arg1), arg2,
					   (arg4 & 16) ? 20 : -1, arg4 & 7, false);
					   
}

FUNC(LS_Floor_Stop)
// Floor_Stop (tag)
{
	return EV_StopFloor(arg0, ln);
}


FUNC(LS_Stairs_BuildDown)
// Stair_BuildDown (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2, SPEED(arg1), TICS(arg3), arg4, 0, DFloor::stairUseSpecials);
}

FUNC(LS_Stairs_BuildUp)
// Stairs_BuildUp (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2, SPEED(arg1), TICS(arg3), arg4, 0, DFloor::stairUseSpecials);
}

FUNC(LS_Stairs_BuildDownSync)
// Stairs_BuildDownSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2, SPEED(arg1), 0, arg3, 0, DFloor::stairUseSpecials|DFloor::stairSync);
}

FUNC(LS_Stairs_BuildUpSync)
// Stairs_BuildUpSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2, SPEED(arg1), 0, arg3, 0, DFloor::stairUseSpecials|DFloor::stairSync);
}

FUNC(LS_Stairs_BuildUpDoom)
// Stairs_BuildUpDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2, SPEED(arg1), TICS(arg3), arg4, 0, 0);
}

FUNC(LS_Stairs_BuildUpDoomCrush)
// Stairs_BuildUpDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs(arg0, DFloor::buildUp, ln,
		arg2, SPEED(arg1), TICS(arg3), arg4, 0, DFloor::stairCrush);
}

FUNC(LS_Stairs_BuildDownDoom)
// Stair_BuildDownDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2, SPEED(arg1), TICS(arg3), arg4, 0, 0);
}

FUNC(LS_Stairs_BuildDownDoomSync)
// Stairs_BuildDownDoomSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2, SPEED(arg1), 0, arg3, 0, DFloor::stairSync);
}

FUNC(LS_Stairs_BuildUpDoomSync)
// Stairs_BuildUpDoomSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2, SPEED(arg1), 0, arg3, 0, DFloor::stairSync);
}


FUNC(LS_Generic_Stairs)
// Generic_Stairs (tag, speed, step, dir/igntxt, reset)
{
	DFloor::EStair type = (arg3 & 1) ? DFloor::buildUp : DFloor::buildDown;
	bool res = EV_BuildStairs (arg0, type, ln,
							   arg2, SPEED(arg1), 0, arg4, arg3 & 2, 0);

	if (res && ln && (ln->flags & ML_REPEAT_SPECIAL) && ln->special == Generic_Stairs)
		// Toggle direction of next activation of repeatable stairs
		ln->args[3] ^= 1;

	return res;
}

FUNC(LS_Pillar_Build)
// Pillar_Build (tag, speed, height)
{
	return EV_DoPillar (DPillar::pillarBuild, ln, arg0, SPEED(arg1), arg2, 0, -1, false);
}

FUNC(LS_Pillar_BuildAndCrush)
// Pillar_BuildAndCrush (tag, speed, height, crush, crushtype)
{
	return EV_DoPillar (DPillar::pillarBuild, ln, arg0, SPEED(arg1), arg2, 0, arg3, CRUSHTYPE(arg4));
}

FUNC(LS_Pillar_Open)
// Pillar_Open (tag, speed, f_height, c_height)
{
	return EV_DoPillar (DPillar::pillarOpen, ln, arg0, SPEED(arg1), arg2, arg3, -1, false);
}

FUNC(LS_Ceiling_LowerByValue)
// Ceiling_LowerByValue (tag, speed, height, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2, CRUSH(arg4), 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_RaiseByValue)
// Ceiling_RaiseByValue (tag, speed, height, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2, CRUSH(arg4), 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_LowerByValueTimes8)
// Ceiling_LowerByValueTimes8 (tag, speed, height, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2*8, -1, 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_RaiseByValueTimes8)
// Ceiling_RaiseByValueTimes8 (tag, speed, height, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2*8, -1, 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_CrushAndRaise)
// Ceiling_CrushAndRaise (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 8, arg2, 0, 0, CRUSHTYPE(arg3, false));
}

FUNC(LS_Ceiling_LowerAndCrush)
// Ceiling_LowerAndCrush (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilLowerAndCrush, ln, arg0, SPEED(arg1), SPEED(arg1), 8, arg2, 0, 0, CRUSHTYPE(arg3, arg1 == 8));
}

FUNC(LS_Ceiling_LowerAndCrushDist)
// Ceiling_LowerAndCrush (tag, speed, crush, dist, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilLowerAndCrush, ln, arg0, SPEED(arg1), SPEED(arg1), arg3, arg2, 0, 0, CRUSHTYPE(arg4, arg1 == 8));
}

FUNC(LS_Ceiling_CrushStop)
// Ceiling_CrushStop (tag, remove)
{
	bool remove;
	switch (arg1)
	{
	case 1:
		remove = false;
		break;
	case 2:
		remove = true;
		break;
	default:
		remove = gameinfo.gametype == GAME_Hexen;
		break;
	}
	return EV_CeilingCrushStop (arg0, remove);
}

FUNC(LS_Ceiling_CrushRaiseAndStay)
// Ceiling_CrushRaiseAndStay (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 8, arg2, 0, 0, CRUSHTYPE(arg3, false));
}

FUNC(LS_Ceiling_MoveToValueTimes8)
// Ceiling_MoveToValueTimes8 (tag, speed, height, negative, change)
{
	return EV_DoCeiling (DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*8*((arg3) ? -1 : 1), -1, 0, CHANGE(arg4));
}

FUNC(LS_Ceiling_MoveToValue)
// Ceiling_MoveToValue (tag, speed, height, negative, change)
{
	return EV_DoCeiling (DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*((arg3) ? -1 : 1), -1, 0, CHANGE(arg4));
}

FUNC(LS_Ceiling_MoveToValueAndCrush)
// Ceiling_MoveToValueAndCrush (tag, speed, height, crush, crushmode)
{
	return EV_DoCeiling (DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2, CRUSH(arg3), 0, 0, CRUSHTYPE(arg4, arg1 == 8));
}

FUNC(LS_Ceiling_LowerToHighestFloor)
// Ceiling_LowerToHighestFloor (tag, speed, change, crush, gap)
{
	return EV_DoCeiling (DCeiling::ceilLowerToHighestFloor, ln, arg0, SPEED(arg1), 0, arg4, CRUSH(arg3), 0, CHANGE(arg2));
}

FUNC(LS_Ceiling_LowerInstant)
// Ceiling_LowerInstant (tag, unused, height, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerInstant, ln, arg0, 0, 0, arg2*8, CRUSH(arg4), 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_RaiseInstant)
// Ceiling_RaiseInstant (tag, unused, height, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseInstant, ln, arg0, 0, 0, arg2*8, -1, 0, CHANGE(arg3));
}

FUNC(LS_Ceiling_CrushRaiseAndStayA)
// Ceiling_CrushRaiseAndStayA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0, CRUSHTYPE(arg4, false));
}

FUNC(LS_Ceiling_CrushRaiseAndStaySilA)
// Ceiling_CrushRaiseAndStaySilA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0, CRUSHTYPE(arg4, false));
}

FUNC(LS_Ceiling_CrushAndRaiseA)
// Ceiling_CrushAndRaiseA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0, CRUSHTYPE(arg4, arg1 == 8 && arg2 == 8));
}

FUNC(LS_Ceiling_CrushAndRaiseDist)
// Ceiling_CrushAndRaiseDist (tag, dist, speed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg2), SPEED(arg2), arg1, arg3, 0, 0, CRUSHTYPE(arg4, arg2 == 8));
}

FUNC(LS_Ceiling_CrushAndRaiseSilentA)
// Ceiling_CrushAndRaiseSilentA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0, CRUSHTYPE(arg4, arg1 == 8 && arg2 == 8));
}

FUNC(LS_Ceiling_CrushAndRaiseSilentDist)
// Ceiling_CrushAndRaiseSilentDist (tag, dist, upspeed, damage, crushtype)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg2), SPEED(arg2), arg1, arg3, 1, 0, CRUSHTYPE(arg4, arg2 == 8));
}

FUNC(LS_Ceiling_RaiseToNearest)
// Ceiling_RaiseToNearest (tag, speed, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, -1, CHANGE(arg2), 0);
}

FUNC(LS_Ceiling_RaiseToHighest)
// Ceiling_RaiseToHighest (tag, speed, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToHighest, ln, arg0, SPEED(arg1), 0, 0, -1, CHANGE(arg2), 0);
}

FUNC(LS_Ceiling_RaiseToLowest)
// Ceiling_RaiseToLowest (tag, speed, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToLowest, ln, arg0, SPEED(arg1), 0, 0, -1, CHANGE(arg2), 0);
}

FUNC(LS_Ceiling_RaiseToHighestFloor)
// Ceiling_RaiseToHighestFloor (tag, speed, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToHighestFloor, ln, arg0, SPEED(arg1), 0, 0, -1, CHANGE(arg2), 0);
}

FUNC(LS_Ceiling_RaiseByTexture)
// Ceiling_RaiseByTexture (tag, speed, change)
{
	return EV_DoCeiling (DCeiling::ceilRaiseByTexture, ln, arg0, SPEED(arg1), 0, 0, -1, CHANGE(arg2), 0);
}

FUNC(LS_Ceiling_LowerToLowest)
// Ceiling_LowerToLowest (tag, speed, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, CRUSH(arg3), 0, CHANGE(arg2));
}

FUNC(LS_Ceiling_LowerToNearest)
// Ceiling_LowerToNearest (tag, speed, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerToNearest, ln, arg0, SPEED(arg1), 0, 0, CRUSH(arg3), 0, CHANGE(arg2));
}

FUNC(LS_Ceiling_ToHighestInstant)
// Ceiling_ToHighestInstant (tag, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerToHighest, ln, arg0, 2, 0, 0, CRUSH(arg2), 0, CHANGE(arg1));
}

FUNC(LS_Ceiling_ToFloorInstant)
// Ceiling_ToFloorInstant (tag, change, crush, gap)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToFloor, ln, arg0, 2, 0, arg3, CRUSH(arg2), 0, CHANGE(arg1));
}

FUNC(LS_Ceiling_LowerToFloor)
// Ceiling_LowerToFloor (tag, speed, change, crush, gap)
{
	return EV_DoCeiling (DCeiling::ceilLowerToFloor, ln, arg0, SPEED(arg1), 0, arg4, CRUSH(arg3), 0, CHANGE(arg2));
}

FUNC(LS_Ceiling_LowerByTexture)
// Ceiling_LowerByTexture (tag, speed, change, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerByTexture, ln, arg0, SPEED(arg1), 0, 0, CRUSH(arg3), 0, CHANGE(arg2));
}

FUNC(LS_Ceiling_Stop)
// Ceiling_Stop (tag)
{
	return EV_StopCeiling(arg0, ln);
}


FUNC(LS_Generic_Ceiling)
// Generic_Ceiling (tag, speed, height, target, change/model/direct/crush)
{
	DCeiling::ECeiling type;

	if (arg4 & 8) {
		switch (arg3) {
			case 1:  type = DCeiling::ceilRaiseToHighest;		break;
			case 2:  type = DCeiling::ceilRaiseToLowest;		break;
			case 3:  type = DCeiling::ceilRaiseToNearest;		break;
			case 4:  type = DCeiling::ceilRaiseToHighestFloor;	break;
			case 5:  type = DCeiling::ceilRaiseToFloor;			break;
			case 6:  type = DCeiling::ceilRaiseByTexture;		break;
			default: type = DCeiling::ceilRaiseByValue;			break;
		}
	} else {
		switch (arg3) {
			case 1:  type = DCeiling::ceilLowerToHighest;		break;
			case 2:  type = DCeiling::ceilLowerToLowest;		break;
			case 3:  type = DCeiling::ceilLowerToNearest;		break;
			case 4:  type = DCeiling::ceilLowerToHighestFloor;	break;
			case 5:  type = DCeiling::ceilLowerToFloor;			break;
			case 6:  type = DCeiling::ceilLowerByTexture;		break;
			default: type = DCeiling::ceilLowerByValue;			break;
		}
	}

	return EV_DoCeiling (type, ln, arg0, SPEED(arg1), SPEED(arg1), arg2,
						 (arg4 & 16) ? 20 : -1, 0, arg4 & 7);
}

FUNC(LS_Generic_Crusher)
// Generic_Crusher (tag, dnspeed, upspeed, silent, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0, (arg1 <= 24 && arg2 <= 24)? DCeiling::ECrushMode::crushSlowdown : DCeiling::ECrushMode::crushDoom);
}

FUNC(LS_Generic_Crusher2)
// Generic_Crusher2 (tag, dnspeed, upspeed, silent, damage)
{
	// same as above but uses Hexen's crushing method.
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0, DCeiling::ECrushMode::crushHexen);
}

FUNC(LS_Plat_PerpetualRaise)
// Plat_PerpetualRaise (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_PerpetualRaiseLip)
// Plat_PerpetualRaiseLip (tag, speed, delay, lip)
{
	return EV_DoPlat (arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_Stop)
// Plat_Stop (tag, remove?)
{
	bool remove;
	switch (arg3)
	{
	case 1:
		remove = false;
		break;
	case 2:
		remove = true;
		break;
	default:
		remove = gameinfo.gametype == GAME_Hexen;
		break;
	}
	EV_StopPlat(arg0, remove);
	return true;
}

FUNC(LS_Plat_DownWaitUpStay)
// Plat_DownWaitUpStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platDownWaitUpStay, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_DownWaitUpStayLip)
// Plat_DownWaitUpStayLip (tag, speed, delay, lip, floor-sound?)
{
	return EV_DoPlat (arg0, ln,
		arg4 ? DPlat::platDownWaitUpStayStone : DPlat::platDownWaitUpStay,
		0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_DownByValue)
// Plat_DownByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platDownByValue, arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpByValue)
// Plat_UpByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpByValue, arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpWaitDownStay)
// Plat_UpWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpNearestWaitDownStay)
// Plat_UpNearestWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpNearestWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_RaiseAndStayTx0)
// Plat_RaiseAndStayTx0 (tag, speed, lockout)
{
	DPlat::EPlatType type;

	switch (arg3)
	{
		case 1:
			type = DPlat::platRaiseAndStay;
			break;
		case 2:
			type = DPlat::platRaiseAndStayLockout;
			break;
		default:
			type = gameinfo.gametype == GAME_Heretic? DPlat::platRaiseAndStayLockout : DPlat::platRaiseAndStay;
			break;
	}


	return EV_DoPlat (arg0, ln, type, 0, SPEED(arg1), 0, 0, 1);
}

FUNC(LS_Plat_UpByValueStayTx)
// Plat_UpByValueStayTx (tag, speed, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpByValueStay, arg2*8, SPEED(arg1), 0, 0, 2);
}

FUNC(LS_Plat_ToggleCeiling)
// Plat_ToggleCeiling (tag)
{
	return EV_DoPlat (arg0, ln, DPlat::platToggle, 0, 0, 0, 0, 0);
}

FUNC(LS_Generic_Lift)
// Generic_Lift (tag, speed, delay, target, height)
{
	DPlat::EPlatType type;

	switch (arg3)
	{
		case 1:
			type = DPlat::platDownWaitUpStay;
			break;
		case 2:
			type = DPlat::platDownToNearestFloor;
			break;
		case 3:
			type = DPlat::platDownToLowestCeiling;
			break;
		case 4:
			type = DPlat::platPerpetualRaise;
			break;
		default:
			type = DPlat::platUpByValue;
			break;
	}

	return EV_DoPlat (arg0, ln, type, arg4*8, SPEED(arg1), OCTICS(arg2), 0, 0);
}

FUNC(LS_Exit_Normal)
// Exit_Normal (position)
{
	if (CheckIfExitIsGood (it, FindLevelInfo(G_GetExitMap())))
	{
		G_ExitLevel (arg0, false);
		return true;
	}
	return false;
}

FUNC(LS_Exit_Secret)
// Exit_Secret (position)
{
	if (CheckIfExitIsGood (it, FindLevelInfo(G_GetSecretExitMap())))
	{
		G_SecretExitLevel (arg0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_NewMap)
// Teleport_NewMap (map, position, keepFacing?)
{
	if (backSide == 0 || gameinfo.gametype == GAME_Strife)
	{
		level_info_t *info = FindLevelByNum (arg0);

		if (info && CheckIfExitIsGood (it, info))
		{
			G_ChangeLevel(info->MapName, arg1, arg2 ? CHANGELEVEL_KEEPFACING : 0);
			return true;
		}
	}
	return false;
}

FUNC(LS_Teleport)
// Teleport (tid, sectortag, bNoSourceFog)
{
	int flags = TELF_DESTFOG;
	if (!arg2)
	{
		flags |= TELF_SOURCEFOG;
	}
	return EV_Teleport (arg0, arg1, ln, backSide, it, flags);
}

FUNC( LS_Teleport_NoStop )
// Teleport_NoStop (tid, sectortag, bNoSourceFog)
{
	int flags = TELF_DESTFOG | TELF_KEEPVELOCITY;
	if (!arg2)
	{
		flags |= TELF_SOURCEFOG;
	}
	return EV_Teleport( arg0, arg1, ln, backSide, it, flags);
}

FUNC(LS_Teleport_NoFog)
// Teleport_NoFog (tid, useang, sectortag, keepheight)
{
	int flags = 0;
	switch (arg1)
	{
	case 0:
		flags |= TELF_KEEPORIENTATION;
		break;

	default:
	case 1:
		break;

	case 2:
		if (ln != NULL) flags |= TELF_KEEPORIENTATION | TELF_ROTATEBOOM;	// adjust to exit thing like Boom (i.e. with incorrect reversed angle)
		break;

	case 3:
		if (ln != NULL) flags |= TELF_KEEPORIENTATION | TELF_ROTATEBOOMINVERSE;	// adjust to exit thing correctly
		break;
	}

	if (arg3)
	{
		flags |= TELF_KEEPHEIGHT;
	}
	return EV_Teleport (arg0, arg2, ln, backSide, it, flags);
}

FUNC(LS_Teleport_ZombieChanger)
// Teleport_ZombieChanger (tid, sectortag)
{
	// This is practically useless outside of Strife, but oh well.
	if (it != NULL)
	{
		EV_Teleport (arg0, arg1, ln, backSide, it, 0);
		if (it->health >= 0) it->SetState (it->FindState(NAME_Pain));
		return true;
	}
	return false;
}

FUNC(LS_TeleportOther)
// TeleportOther (other_tid, dest_tid, fog?)
{
	return EV_TeleportOther (arg0, arg1, arg2?true:false);
}

FUNC(LS_TeleportGroup)
// TeleportGroup (group_tid, source_tid, dest_tid, move_source?, fog?)
{
	return EV_TeleportGroup (arg0, it, arg1, arg2, arg3?true:false, arg4?true:false);
}

FUNC(LS_TeleportInSector)
// TeleportInSector (tag, source_tid, dest_tid, bFog, group_tid)
{
	return EV_TeleportSector (arg0, arg1, arg2, arg3?true:false, arg4);
}

FUNC(LS_Teleport_EndGame)
// Teleport_EndGame ()
{
	if (!backSide && CheckIfExitIsGood (it, NULL))
	{
		G_ChangeLevel(NULL, 0, 0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_Line)
// Teleport_Line (thisid, destid, reversed)
{
	return EV_SilentLineTeleport (ln, backSide, it, arg1, arg2);
}

static void ThrustThingHelper(AActor *it, DAngle angle, double force, INTBOOL nolimit)
{
	it->Thrust(angle, force);
	if (!nolimit)
	{
		it->Vel.X = clamp(it->Vel.X, -MAXMOVE, MAXMOVE);
		it->Vel.Y = clamp(it->Vel.Y, -MAXMOVE, MAXMOVE);
	}
}

FUNC(LS_ThrustThing)
// ThrustThing (angle, force, nolimit, tid)
{
	if (arg3 != 0)
	{
		FActorIterator iterator (arg3);
		while ((it = iterator.Next()) != NULL)
		{
			ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		}
		return true;
	}
	else if (it)
	{
		if (level.flags2 & LEVEL2_HEXENHACK && backSide)
		{
			return false;
		}
		ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		return true;
	}
	return false;
}

FUNC(LS_ThrustThingZ)	// [BC]
// ThrustThingZ (tid, zthrust, down/up, set)
{
	AActor *victim;
	double thrust = arg1/4.;

	// [BC] Up is default
	if (arg2)
		thrust = -thrust;

	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);

		while ( (victim = iterator.Next ()) )
		{
			if (!arg3)
				victim->Vel.Z = thrust;
			else
				victim->Vel.Z += thrust;
		}
		return true;
	}
	else if (it)
	{
		if (!arg3)
			it->Vel.Z = thrust;
		else
			it->Vel.Z += thrust;
		return true;
	}
	return false;
}

FUNC(LS_Thing_SetSpecial)	// [BC]
// Thing_SetSpecial (tid, special, arg1, arg2, arg3)
// [RH] Use the SetThingSpecial ACS command instead.
// It can set all args and not just the first three.
{
	if (arg0 == 0)
	{
		if (it != NULL)
		{
			it->special = arg1;
			it->args[0] = arg2;
			it->args[1] = arg3;
			it->args[2] = arg4;
		}
	}
	else
	{
		AActor *actor;
		FActorIterator iterator (arg0);

		while ( (actor = iterator.Next ()) )
		{
			actor->special = arg1;
			actor->args[0] = arg2;
			actor->args[1] = arg3;
			actor->args[2] = arg4;
		}
	}
	return true;
}

FUNC(LS_Thing_ChangeTID)
// Thing_ChangeTID (oldtid, newtid)
{
	if (arg0 == 0)
	{
		if (it != NULL && !(it->ObjectFlags & OF_EuthanizeMe))
		{
			it->RemoveFromHash ();
			it->tid = arg1;
			it->AddToHash ();
		}
	}
	else
	{
		FActorIterator iterator (arg0);
		AActor *actor, *next;

		next = iterator.Next ();
		while (next != NULL)
		{
			actor = next;
			next = iterator.Next ();

			if (!(actor->ObjectFlags & OF_EuthanizeMe))
			{
				actor->RemoveFromHash ();
				actor->tid = arg1;
				actor->AddToHash ();
			}
		}
	}
	return true;
}

FUNC(LS_DamageThing)
// DamageThing (damage, mod)
{
	if (it)
	{
		if (arg0 < 0)
		{ // Negative damages mean healing
			if (it->player)
			{
				P_GiveBody (it, -arg0);
			}
			else
			{
				it->health -= arg0;
				if (it->SpawnHealth() < it->health)
					it->health = it->SpawnHealth();
			}
		}
		else if (arg0 > 0)
		{
			P_DamageMobj (it, NULL, NULL, arg0, MODtoDamageType (arg1));
		}
		else
		{ // If zero damage, guarantee a kill
			P_DamageMobj (it, NULL, NULL, TELEFRAG_DAMAGE, MODtoDamageType (arg1));
		}
	}

	return it ? true : false;
}

FUNC(LS_HealThing)
// HealThing (amount, max)
{
	if (it)
	{
		int max = arg1;

		if (max == 0 || it->player == NULL)
		{
			P_GiveBody(it, arg0);
			return true;
		}
		else if (max == 1)
		{
			max = deh.MaxSoulsphere;
		}

		// If health is already above max, do nothing
		if (it->health < max)
		{
			it->health += arg0;
			if (it->health > max && max > 0)
			{
				it->health = max;
			}
			if (it->player)
			{
				it->player->health = it->health;
			}
		}
	}

	return it ? true : false;
}

// So that things activated/deactivated by ACS or DECORATE *and* by 
// the BUMPSPECIAL or USESPECIAL flags work correctly both ways.
void DoActivateThing(AActor * thing, AActor * activator)
{
	if (thing->activationtype & THINGSPEC_Activate)
	{
		thing->activationtype &= ~THINGSPEC_Activate; // Clear flag
		if (thing->activationtype & THINGSPEC_Switch) // Set other flag if switching
			thing->activationtype |= THINGSPEC_Deactivate;
	}
	thing->CallActivate (activator);
}

void DoDeactivateThing(AActor * thing, AActor * activator)
{
	if (thing->activationtype & THINGSPEC_Deactivate)
	{
		thing->activationtype &= ~THINGSPEC_Deactivate; // Clear flag
		if (thing->activationtype & THINGSPEC_Switch) // Set other flag if switching
			thing->activationtype |= THINGSPEC_Activate;
	}
	thing->CallDeactivate (activator);
}

FUNC(LS_Thing_Activate)
// Thing_Activate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;

		actor = iterator.Next ();
		while (actor)
		{
			// Actor might remove itself as part of activation, so get next
			// one before activating it.
			AActor *temp = iterator.Next ();
			DoActivateThing(actor, it);
			actor = temp;
			count++;
		}

		return count != 0;
	}
	else if (it != NULL)
	{
		DoActivateThing(it, it);
		return true;
	}
	return false;
}

FUNC(LS_Thing_Deactivate)
// Thing_Deactivate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;
	
		actor = iterator.Next ();
		while (actor)
		{
			// Actor might removes itself as part of deactivation, so get next
			// one before we activate it.
			AActor *temp = iterator.Next ();
			DoDeactivateThing(actor, it);
			actor = temp;
			count++;
		}
	
		return count != 0;
	}
	else if (it != NULL)
	{
		DoDeactivateThing(it, it);
		return true;
	}
	return false;
}

FUNC(LS_Thing_Remove)
// Thing_Remove (tid)
{
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		AActor *actor;

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();

			P_RemoveThing(actor);
			actor = temp;
		}
	}
	else if (it != NULL)
	{
		P_RemoveThing(it);
	}

	return true;
}

FUNC(LS_Thing_Destroy)
// Thing_Destroy (tid, extreme, tag)
{
	AActor *actor;

	if (arg0 == 0 && arg2 == 0)
	{
		P_Massacre ();
	}
	else if (arg0 == 0)
	{
		TThinkerIterator<AActor> iterator;
		
		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();
			if (actor->flags & MF_SHOOTABLE && tagManager.SectorHasTag(actor->Sector, arg2))
				P_DamageMobj (actor, NULL, it, arg1 ? TELEFRAG_DAMAGE : actor->health, NAME_None);
			actor = temp;
		}
	}
	else
	{
		FActorIterator iterator (arg0);

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();
			if (actor->flags & MF_SHOOTABLE && (arg2 == 0 || tagManager.SectorHasTag(actor->Sector, arg2)))
				P_DamageMobj (actor, NULL, it, arg1 ? TELEFRAG_DAMAGE : actor->health, NAME_None);
			actor = temp;
		}
	}
	return true;
}

FUNC(LS_Thing_Damage)
// Thing_Damage (tid, amount, MOD)
{
	P_Thing_Damage (arg0, it, arg1, MODtoDamageType (arg2));
	return true;
}

FUNC(LS_Thing_Projectile)
// Thing_Projectile (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), SPEED(arg3),
		SPEED(arg4), 0, NULL, 0, 0, false);
}

FUNC(LS_Thing_ProjectileGravity)
// Thing_ProjectileGravity (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), SPEED(arg3),
		SPEED(arg4), 0, NULL, 1, 0, false);
}

FUNC(LS_Thing_Hate)
// Thing_Hate (hater, hatee, group/"xray"?)
{
	FActorIterator haterIt (arg0);
	AActor *hater, *hatee = NULL;
	FActorIterator hateeIt (arg1);
	bool nothingToHate = false;

	if (arg1 != 0)
	{
		while ((hatee = hateeIt.Next ()))
		{
			if (hatee->flags & MF_SHOOTABLE	&&	// can't hate nonshootable things
				hatee->health > 0 &&			// can't hate dead things
				!(hatee->flags2 & MF2_DORMANT))	// can't target dormant things
			{
				break;
			}
		}
		if (hatee == NULL)
		{ // Nothing to hate
			nothingToHate = true;
		}
	}

	if (arg0 == 0)
	{
		if (it != NULL && it->player != NULL)
		{
			// Players cannot have their attitudes set
			return false;
		}
		else
		{
			hater = it;
		}
	}
	else
	{
		while ((hater = haterIt.Next ()))
		{
			if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
			{
				break;
			}
		}
	}
	while (hater != NULL)
	{
		// Can't hate if can't attack.
		if (hater->SeeState != NULL)
		{
			// If hating a group of things, record the TID and NULL
			// the target (if its TID doesn't match). A_Look will
			// find an appropriate thing to go chase after.
			if (arg2 != 0)
			{
				hater->TIDtoHate = arg1;
				hater->LastLookActor = NULL;

				// If the TID to hate is 0, then don't forget the target and
				// lastenemy fields.
				if (arg1 != 0)
				{
					if (hater->target != NULL && hater->target->tid != arg1)
					{
						hater->target = NULL;
					}
					if (hater->lastenemy != NULL && hater->lastenemy->tid != arg1)
					{
						hater->lastenemy = NULL;
					}
				}
			}
			// Hate types for arg2:
			//
			// 0 - Just hate one specific actor
			// 1 - Hate actors with given TID and attack players when shot
			// 2 - Same as 1, but will go after enemies without seeing them first
			// 3 - Hunt actors with given TID and also players
			// 4 - Same as 3, but will go after monsters without seeing them first
			// 5 - Hate actors with given TID and ignore player attacks
			// 6 - Same as 5, but will go after enemies without seeing them first

			// Note here: If you use Thing_Hate (tid, 0, 2), you can make
			// a monster go after a player without seeing him first.
			if (arg2 == 2 || arg2 == 4 || arg2 == 6)
			{
				hater->flags3 |= MF3_NOSIGHTCHECK;
			}
			else
			{
				hater->flags3 &= ~MF3_NOSIGHTCHECK;
			}
			if (arg2 == 3 || arg2 == 4)
			{
				hater->flags3 |= MF3_HUNTPLAYERS;
			}
			else
			{
				hater->flags3 &= ~MF3_HUNTPLAYERS;
			}
			if (arg2 == 5 || arg2 == 6)
			{
				hater->flags4 |= MF4_NOHATEPLAYERS;
			}
			else
			{
				hater->flags4 &= ~MF4_NOHATEPLAYERS;
			}

			if (arg1 == 0)
			{
				hatee = it;
			}
			else if (nothingToHate)
			{
				hatee = NULL;
			}
			else if (arg2 != 0)
			{
				do
				{
					hatee = hateeIt.Next ();
				}
				while ( hatee == NULL ||
						hatee == hater ||					// can't hate self
						!(hatee->flags & MF_SHOOTABLE) ||	// can't hate nonshootable things
						hatee->health <= 0 ||				// can't hate dead things
						(hatee->flags2 & MF2_DORMANT));	
			}

			if (hatee != NULL && hatee != hater && (arg2 == 0 || (hater->goal != NULL && hater->target != hater->goal)))
			{
				if (hater->target)
				{
					hater->lastenemy = hater->target;
				}
				hater->target = hatee;
				if (!(hater->flags2 & MF2_DORMANT))
				{
					if (hater->health > 0) hater->SetState (hater->SeeState);
				}
			}
		}
		if (arg0 != 0)
		{
			while ((hater = haterIt.Next ()))
			{
				if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
				{
					break;
				}
			}
		}
		else
		{
			hater = NULL;
		}
	}
	return true;
}

FUNC(LS_Thing_ProjectileAimed)
// Thing_ProjectileAimed (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0., SPEED(arg2), 0, arg3, it, 0, arg4, false);
}

FUNC(LS_Thing_ProjectileIntercept)
// Thing_ProjectileIntercept (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0., SPEED(arg2), 0, arg3, it, 0, arg4, true);
}

// [BC] added newtid for next two
FUNC(LS_Thing_Spawn)
// Thing_Spawn (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), true, arg3);
}

FUNC(LS_Thing_SpawnNoFog)
// Thing_SpawnNoFog (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), false, arg3);
}

FUNC(LS_Thing_SpawnFacing)
// Thing_SpawnFacing (tid, type, nofog, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, 1000000., arg2 ? false : true, arg3);
}

FUNC(LS_Thing_Raise)
// Thing_Raise(tid, nocheck)
{
	AActor * target;
	bool ok = false;

	if (arg0==0)
	{
		ok = P_Thing_Raise (it, it, arg1);
	}
	else
	{
		TActorIterator<AActor> iterator (arg0);

		while ( (target = iterator.Next ()) )
		{
			ok |= P_Thing_Raise(target, target, arg1);
		}
	}
	return ok;
}

FUNC(LS_Thing_Stop)
// Thing_Stop(tid)
{
	AActor * target;
	bool ok = false;

	if (arg0==0)
	{
		if (it != NULL)
		{
			it->Vel.Zero();
			if (it->player != NULL) it->player->Vel.Zero();
			ok = true;
		}
	}
	else
	{
		TActorIterator<AActor> iterator (arg0);

		while ( (target = iterator.Next ()) )
		{
			target->Vel.Zero();
			if (target->player != NULL) target->player->Vel.Zero();
			ok = true;
		}
	}
	return ok;
}


FUNC(LS_Thing_SetGoal)
// Thing_SetGoal (tid, goal, delay, chasegoal)
{
	TActorIterator<AActor> selfiterator (arg0);
	NActorIterator goaliterator (NAME_PatrolPoint, arg1);
	AActor *self;
	AActor *goal = goaliterator.Next ();
	bool ok = false;

	while ( (self = selfiterator.Next ()) )
	{
		ok = true;
		if (self->flags & MF_SHOOTABLE)
		{
			if (self->target == self->goal)
			{ // Targeting a goal already? -> don't target it anymore.
			  // A_Look will set it to the goal, presuming no real targets
			  // come into view by then.
				self->target = NULL;
			}
			self->goal = goal;
			if (arg3 == 0)
			{
				self->flags5 &= ~MF5_CHASEGOAL;
			}
			else
			{
				self->flags5 |= MF5_CHASEGOAL;
			}
			if (self->target == NULL)
			{
				self->reactiontime = arg2 * TICRATE;
			}
		}
	}

	return ok;
}

FUNC(LS_Thing_Move)		// [BC]
// Thing_Move (tid, mapspot, nofog)
{
	return P_Thing_Move (arg0, it, arg1, arg2 ? false : true);
}

enum
{
	TRANSLATION_ICE = 0x100007
};

FUNC(LS_Thing_SetTranslation)
// Thing_SetTranslation (tid, range)
{
	TActorIterator<AActor> iterator (arg0);
	int range;
	AActor *target;
	bool ok = false;

	if (arg1 == -1 && it != NULL)
	{
		range = it->Translation;
	}
	else if (arg1 >= 1 && arg1 < MAX_ACS_TRANSLATIONS)
	{
		range = TRANSLATION(TRANSLATION_LevelScripted, (arg1-1));
	}
	else if (arg1 == TRANSLATION_ICE)
	{
		range = TRANSLATION(TRANSLATION_Standard, 7);
	}
	else
	{
		range = 0;
	}

	if (arg0 == 0)
	{
		if (it != NULL)
		{
			ok = true;
			it->Translation = range==0? it->GetDefault()->Translation : range;
		}
	}
	else
	{
		while ( (target = iterator.Next ()) )
		{
			ok = true;
			target->Translation = range==0? target->GetDefault()->Translation : range;
		}
	}

	return ok;
}

FUNC(LS_ACS_Execute)
// ACS_Execute (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;
	const char *mapname = NULL;
	int args[3] = { arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0);

	if (arg1 == 0)
	{
		mapname = level.MapName;
	}
	else if ((info = FindLevelByNum(arg1)) != NULL)
	{
		mapname = info->MapName;
	}
	else
	{
		return false;
	}
	return P_StartScript(it, ln, arg0, mapname, args, 3, flags);
}

FUNC(LS_ACS_ExecuteAlways)
// ACS_ExecuteAlways (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;
	const char *mapname = NULL;
	int args[3] = { arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0) | ACS_ALWAYS;

	if (arg1 == 0)
	{
		mapname = level.MapName;
	}
	else if ((info = FindLevelByNum(arg1)) != NULL)
	{
		mapname = info->MapName;
	}
	else
	{
		return false;
	}
	return P_StartScript(it, ln, arg0, mapname, args, 3, flags);
}

FUNC(LS_ACS_LockedExecute)
// ACS_LockedExecute (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, true))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_LockedExecuteDoor)
// ACS_LockedExecuteDoor (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, false))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_ExecuteWithResult)
// ACS_ExecuteWithResult (script, s_arg1, s_arg2, s_arg3, s_arg4)
{
	// This is like ACS_ExecuteAlways, except the script is always run on
	// the current map, and the return value is whatever the script sets
	// with SetResultValue.
	int args[4] = { arg1, arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0) | ACS_ALWAYS | ACS_WANTRESULT;

	return P_StartScript (it, ln, arg0, level.MapName, args, 4, flags);
}

FUNC(LS_ACS_Suspend)
// ACS_Suspend (script, map)
{
	level_info_t *info;

	if (arg1 == 0)
		P_SuspendScript (arg0, level.MapName);
	else if ((info = FindLevelByNum (arg1)) )
		P_SuspendScript (arg0, info->MapName);

	return true;
}

FUNC(LS_ACS_Terminate)
// ACS_Terminate (script, map)
{
	level_info_t *info;

	if (arg1 == 0)
		P_TerminateScript (arg0, level.MapName);
	else if ((info = FindLevelByNum (arg1)) )
		P_TerminateScript (arg0, info->MapName);

	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FUNC(LS_FS_Execute)
// FS_Execute(script#,firstsideonly,lock,msgtype)
{
	if (arg1 && ln && backSide) return false;
	if (arg2!=0 && !P_CheckKeys(it, arg2, !!arg3)) return false;
	return T_RunScript(arg0,it);
}



FUNC(LS_FloorAndCeiling_LowerByValue)
// FloorAndCeiling_LowerByValue (tag, speed, height)
{
	return EV_DoElevator (ln, DElevator::elevateLower, SPEED(arg1), arg2, arg0);
}

FUNC(LS_FloorAndCeiling_RaiseByValue)
// FloorAndCeiling_RaiseByValue (tag, speed, height)
{
	return EV_DoElevator (ln, DElevator::elevateRaise, SPEED(arg1), arg2, arg0);
}

FUNC(LS_FloorAndCeiling_LowerRaise)
// FloorAndCeiling_LowerRaise (tag, fspeed, cspeed, boomemu)
{
	bool res = EV_DoCeiling (DCeiling::ceilRaiseToHighest, ln, arg0, SPEED(arg2), 0, 0, 0, 0, 0);
	// The switch based Boom equivalents of FloorandCeiling_LowerRaise do incorrect checks
	// which cause the floor only to move when the ceiling fails to do so.
	// To avoid problems with maps that have incorrect args this only uses a 
	// more or less unintuitive value for the fourth arg to trigger Boom's broken behavior
	if (arg3 != 1998 || !res)	// (1998 for the year in which Boom was released... :P)
	{
		res |= EV_DoFloor (DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, -1, 0, false);
	}
	return res;
}

FUNC(LS_Elevator_MoveToFloor)
// Elevator_MoveToFloor (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateCurrent, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_RaiseToNearest)
// Elevator_RaiseToNearest (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateUp, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_LowerToNearest)
// Elevator_LowerToNearest (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateDown, SPEED(arg1), 0, arg0);
}

FUNC(LS_Light_ForceLightning)
// Light_ForceLightning (mode)
{
	P_ForceLightning (arg0);
	return true;
}

FUNC(LS_Light_RaiseByValue)
// Light_RaiseByValue (tag, value)
{
	EV_LightChange (arg0, arg1);
	return true;
}

FUNC(LS_Light_LowerByValue)
// Light_LowerByValue (tag, value)
{
	EV_LightChange (arg0, -arg1);
	return true;
}

FUNC(LS_Light_ChangeToValue)
// Light_ChangeToValue (tag, value)
{
	EV_LightTurnOn (arg0, arg1);
	return true;
}

FUNC(LS_Light_Fade)
// Light_Fade (tag, value, tics);
{
	EV_StartLightFading (arg0, arg1, TICS(arg2));
	return true;
}

FUNC(LS_Light_Glow)
// Light_Glow (tag, upper, lower, tics)
{
	EV_StartLightGlowing (arg0, arg1, arg2, TICS(arg3));
	return true;
}

FUNC(LS_Light_Flicker)
// Light_Flicker (tag, upper, lower)
{
	EV_StartLightFlickering (arg0, arg1, arg2);
	return true;
}

FUNC(LS_Light_Strobe)
// Light_Strobe (tag, upper, lower, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, arg1, arg2, TICS(arg3), TICS(arg4));
	return true;
}

FUNC(LS_Light_StrobeDoom)
// Light_StrobeDoom (tag, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, TICS(arg1), TICS(arg2));
	return true;
}

FUNC(LS_Light_MinNeighbor)
// Light_MinNeighbor (tag)
{
	EV_TurnTagLightsOff (arg0);
	return true;
}

FUNC(LS_Light_MaxNeighbor)
// Light_MaxNeighbor (tag)
{
	EV_LightTurnOn (arg0, -1);
	return true;
}

FUNC(LS_Light_Stop)
// Light_Stop (tag)
{
	EV_StopLightEffect (arg0);
	return true;
}

FUNC(LS_Radius_Quake)
// Radius_Quake (intensity, duration, damrad, tremrad, tid)
{
	return P_StartQuake (it, arg4, arg0, arg1, arg2*64, arg3*64, "world/quake");
}

FUNC(LS_UsePuzzleItem)
// UsePuzzleItem (item, script)
{
	AActor *item;

	if (!it) return false;

	// Check player's inventory for puzzle item
	for (item = it->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf (NAME_PuzzleItem))
		{
			if (item->IntVar(NAME_PuzzleItemNumber) == arg0)
			{
				if (it->UseInventory (item))
				{
					return true;
				}
				break;
			}
		}
	}

	// [RH] Say "hmm" if you don't have the puzzle item
	S_Sound (it, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	return false;
}

FUNC(LS_Sector_ChangeSound)
// Sector_ChangeSound (tag, sound)
{
	int secNum;
	bool rtn;

	if (!arg0)
		return false;

	rtn = false;
	FSectorTagIterator itr(arg0);
	while ((secNum = itr.Next()) >= 0)
	{
		level.sectors[secNum].seqType = arg1;
		rtn = true;
	}
	return rtn;
}

FUNC(LS_Sector_ChangeFlags)
// Sector_ChangeFlags (tag, set, clear)
{
	int secNum;
	bool rtn;

	if (!arg0)
		return false;

	rtn = false;
	FSectorTagIterator itr(arg0);
	// exclude protected flags
	arg1 &= ~SECF_NOMODIFY;
	arg2 &= ~SECF_NOMODIFY;
	while ((secNum = itr.Next()) >= 0)
	{
		level.sectors[secNum].Flags = (level.sectors[secNum].Flags | arg1) & ~arg2;
		rtn = true;
	}
	return rtn;
}



void AdjustPusher(int tag, int magnitude, int angle, bool wind);

FUNC(LS_Sector_SetWind)
// Sector_SetWind (tag, amount, angle)
{
	if (arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, true);
	return true;
}

FUNC(LS_Sector_SetCurrent)
// Sector_SetCurrent (tag, amount, angle)
{
	if (arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, false);
	return true;
}

FUNC(LS_Sector_SetFriction)
// Sector_SetFriction (tag, amount)
{
	P_SetSectorFriction (arg0, arg1, true);
	return true;
}

FUNC(LS_Sector_SetTranslucent)
// Sector_SetTranslucent (tag, plane, amount, type)
{
	if (arg0 != 0)
	{
		int secnum;
		FSectorTagIterator itr(arg0);
		while ((secnum = itr.Next()) >= 0)
		{
			level.sectors[secnum].SetAlpha(arg1, clamp(arg2, 0, 255) / 255.);
			level.sectors[secnum].ChangeFlags(arg1, ~PLANEF_ADDITIVE, arg3? PLANEF_ADDITIVE:0);
		}
		return true;
	}
	return false;
}

FUNC(LS_Sector_SetLink)
// Sector_SetLink (controltag, linktag, floor/ceiling, movetype)
{
	if (arg0 != 0)	// control tag == 0 is for static initialization and must not be handled here
	{
		int control = P_FindFirstSectorFromTag(arg0);
		if (control >= 0)
		{
			return P_AddSectorLinks(&level.sectors[control], arg1, arg2, arg3);
		}
	}
	return false;
}

void SetWallScroller(int id, int sidechoice, double dx, double dy, EScrollPos Where);
void SetScroller(int tag, EScroll type, double dx, double dy);


FUNC(LS_Scroll_Texture_Both)
// Scroll_Texture_Both (id, left, right, up, down)
{
	if (arg0 == 0)
		return false;

	double dx = (arg1 - arg2) / 64.;
	double dy = (arg4 - arg3) / 64.;
	int sidechoice;

	if (arg0 < 0)
	{
		sidechoice = 1;
		arg0 = -arg0;
	}
	else
	{
		sidechoice = 0;
	}

	SetWallScroller (arg0, sidechoice, dx, dy, scw_all);

	return true;
}

FUNC(LS_Scroll_Wall)
// Scroll_Wall (id, x, y, side, flags)
{
	if (arg0 == 0)
		return false;

	SetWallScroller (arg0, !!arg3, arg1 / 65536., arg2 / 65536., EScrollPos(arg4));
	return true;
}

// NOTE: For the next two functions, x-move and y-move are
// 0-based, not 128-based as they are if they appear on lines.
// Note also that parameter ordering is different.

FUNC(LS_Scroll_Floor)
// Scroll_Floor (tag, x-move, y-move, s/c)
{
	double dx = arg1 / 32.;
	double dy = arg2 / 32.;

	if (arg3 == 0 || arg3 == 2)
	{
		SetScroller (arg0, EScroll::sc_floor, -dx, dy);
	}
	else
	{
		SetScroller (arg0, EScroll::sc_floor, 0, 0);
	}
	if (arg3 > 0)
	{
		SetScroller (arg0, EScroll::sc_carry, dx, dy);
	}
	else
	{
		SetScroller (arg0, EScroll::sc_carry, 0, 0);
	}
	return true;
}

FUNC(LS_Scroll_Ceiling)
// Scroll_Ceiling (tag, x-move, y-move, 0)
{
	double dx = arg1 / 32.;
	double dy = arg2 / 32.;

	SetScroller (arg0, EScroll::sc_ceiling, -dx, dy);
	return true;
}

FUNC(LS_PointPush_SetForce)
// PointPush_SetForce ()
{
	return false;
}

FUNC(LS_Sector_SetDamage)
// Sector_SetDamage (tag, amount, mod, interval, leaky)
{
	// The sector still stores the mod in its old format because
	// adding an FName to the sector_t structure might cause
	// problems by adding an unwanted constructor.
	// Since it doesn't really matter whether the type is translated
	// here or in P_PlayerInSpecialSector I think it's the best solution.
	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		if (arg3 <= 0)	// emulate old and hacky method to handle leakiness.
		{
			if (arg1 < 20)
			{
				arg4 = 0;
				arg3 = 32;
			}
			else if (arg1 < 50)
			{
				arg4 = 5;
				arg3 = 32;
			}
			else
			{
				arg4 = 256;
				arg3 = 1;
			}
		}
		level.sectors[secnum].damageamount = (short)arg1;
		level.sectors[secnum].damagetype = MODtoDamageType(arg2);
		level.sectors[secnum].damageinterval = (short)arg3;
		level.sectors[secnum].leakydamage = (short)arg4;
	}
	return true;
}

FUNC(LS_Sector_SetGravity)
// Sector_SetGravity (tag, intpart, fracpart)
{
	double gravity;

	if (arg2 > 99)
		arg2 = 99;
	gravity = (double)arg1 + (double)arg2 * 0.01;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
		level.sectors[secnum].gravity = gravity;

	return true;
}

FUNC(LS_Sector_SetColor)
// Sector_SetColor (tag, r, g, b, desaturate)
{
	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		level.sectors[secnum].SetColor(PalEntry(255, arg1, arg2, arg3), arg4);
	}

	return true;
}

FUNC(LS_Sector_SetFade)
// Sector_SetFade (tag, r, g, b)
{
	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		level.sectors[secnum].SetFade(PalEntry(255, arg1, arg2, arg3));
	}
	return true;
}

FUNC(LS_Sector_SetCeilingPanning)
// Sector_SetCeilingPanning (tag, x-int, x-frac, y-int, y-frac)
{
	double xofs = arg1 + arg2 / 100.;
	double yofs = arg3 + arg4 / 100.;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		level.sectors[secnum].SetXOffset(sector_t::ceiling, xofs);
		level.sectors[secnum].SetYOffset(sector_t::ceiling, yofs);
	}
	return true;
}

FUNC(LS_Sector_SetFloorPanning)
// Sector_SetFloorPanning (tag, x-int, x-frac, y-int, y-frac)
{
	double xofs = arg1 + arg2 / 100.;
	double yofs = arg3 + arg4 / 100.;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		level.sectors[secnum].SetXOffset(sector_t::floor, xofs);
		level.sectors[secnum].SetYOffset(sector_t::floor, yofs);
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale)
// Sector_SetFloorScale (tag, x-int, x-frac, y-int, y-frac)
{
	double xscale = arg1 + arg2 / 100.;
	double yscale = arg3 + arg4 / 100.;

	if (xscale)
		xscale = 1. / xscale;
	if (yscale)
		yscale = 1. / yscale;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		if (xscale)
			level.sectors[secnum].SetXScale(sector_t::floor, xscale);
		if (yscale)
			level.sectors[secnum].SetYScale(sector_t::floor, yscale);
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale)
// Sector_SetCeilingScale (tag, x-int, x-frac, y-int, y-frac)
{
	double xscale = arg1 + arg2 / 100.;
	double yscale = arg3 + arg4 / 100.;

	if (xscale)
		xscale = 1. / xscale;
	if (yscale)
		yscale = 1. / yscale;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		if (xscale)
			level.sectors[secnum].SetXScale(sector_t::ceiling, xscale);
		if (yscale)
			level.sectors[secnum].SetYScale(sector_t::ceiling, yscale);
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	double xscale = arg1 / 65536., yscale = arg2 / 65536.;

	if (xscale)
		xscale = 1. / xscale;
	if (yscale)
		yscale = 1. / yscale;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		if (arg1)
			level.sectors[secnum].SetXScale(sector_t::floor, xscale);
		if (arg2)
			level.sectors[secnum].SetYScale(sector_t::floor, yscale);
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	double xscale = arg1 / 65536., yscale = arg2 / 65536.;

	if (xscale)
		xscale = 1. / xscale;
	if (yscale)
		yscale = 1. / yscale;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		if (arg1)
			level.sectors[secnum].SetXScale(sector_t::ceiling, xscale);
		if (arg2)
			level.sectors[secnum].SetYScale(sector_t::ceiling, yscale);
	}
	return true;
}

FUNC(LS_Sector_SetRotation)
// Sector_SetRotation (tag, floor-angle, ceiling-angle)
{
	DAngle ceiling = (double)arg2;
	DAngle floor = (double)arg1;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		level.sectors[secnum].SetAngle(sector_t::floor, floor);
		level.sectors[secnum].SetAngle(sector_t::ceiling, ceiling);
	}
	return true;
}

FUNC(LS_Line_AlignCeiling)
// Line_AlignCeiling (lineid, side)
{
	bool ret = 0;

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		ret |= P_AlignFlat (line, !!arg1, 1);
	}
	return ret;
}

FUNC(LS_Line_AlignFloor)
// Line_AlignFloor (lineid, side)
{
	bool ret = 0;

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		ret |= P_AlignFlat (line, !!arg1, 0);
	}
	return ret;
}

FUNC(LS_Line_SetTextureOffset)
// Line_SetTextureOffset (id, x, y, side, flags)
{
	const int NO_CHANGE = 32767 << 16;
	double farg1 = arg1 / 65536.;
	double farg2 = arg2 / 65536.;

	if (arg0 == 0 || arg3 < 0 || arg3 > 1)
		return false;

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		side_t *side = level.lines[line].sidedef[arg3];
		if (side != NULL)
		{

			if ((arg4&8)==0)
			{
				// set
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureXOffset(side_t::top, farg1);
					if (arg4&2) side->SetTextureXOffset(side_t::mid, farg1);
					if (arg4&4) side->SetTextureXOffset(side_t::bottom, farg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureYOffset(side_t::top, farg2);
					if (arg4&2) side->SetTextureYOffset(side_t::mid, farg2);
					if (arg4&4) side->SetTextureYOffset(side_t::bottom, farg2);
				}
			}
			else
			{
				// add
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->AddTextureXOffset(side_t::top, farg1);
					if (arg4&2) side->AddTextureXOffset(side_t::mid, farg1);
					if (arg4&4) side->AddTextureXOffset(side_t::bottom, farg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->AddTextureYOffset(side_t::top, farg2);
					if (arg4&2) side->AddTextureYOffset(side_t::mid, farg2);
					if (arg4&4) side->AddTextureYOffset(side_t::bottom, farg2);
				}
			}
		}
	}
	return true;
}

FUNC(LS_Line_SetTextureScale)
// Line_SetTextureScale (id, x, y, side, flags)
{
	const int NO_CHANGE = 32767 << 16;
	double farg1 = arg1 / 65536.;
	double farg2 = arg2 / 65536.;

	if (arg0 == 0 || arg3 < 0 || arg3 > 1)
		return false;

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		side_t *side = level.lines[line].sidedef[arg3];
		if (side != NULL)
		{
			if ((arg4&8)==0)
			{
				// set
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureXScale(side_t::top, farg1);
					if (arg4&2) side->SetTextureXScale(side_t::mid, farg1);
					if (arg4&4) side->SetTextureXScale(side_t::bottom, farg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureYScale(side_t::top, farg2);
					if (arg4&2) side->SetTextureYScale(side_t::mid, farg2);
					if (arg4&4) side->SetTextureYScale(side_t::bottom, farg2);
				}
			}
			else
			{
				// add
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->MultiplyTextureXScale(side_t::top, farg1);
					if (arg4&2) side->MultiplyTextureXScale(side_t::mid, farg1);
					if (arg4&4) side->MultiplyTextureXScale(side_t::bottom, farg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->MultiplyTextureYScale(side_t::top, farg2);
					if (arg4&2) side->MultiplyTextureYScale(side_t::mid, farg2);
					if (arg4&4) side->MultiplyTextureYScale(side_t::bottom, farg2);
				}
			}
		}
	}
	return true;
}

FUNC(LS_Line_SetBlocking)
// Line_SetBlocking (id, setflags, clearflags)
{
	static const int flagtrans[] =
	{
		ML_BLOCKING,
		ML_BLOCKMONSTERS,
		ML_BLOCK_PLAYERS,
		ML_BLOCK_FLOATERS,
		ML_BLOCKPROJECTILE,
		ML_BLOCKEVERYTHING,
		ML_RAILING,
		ML_BLOCKUSE,
		ML_BLOCKSIGHT,
		ML_BLOCKHITSCAN,
		ML_SOUNDBLOCK,
		-1
	};

	if (arg0 == 0) return false;

	int setflags = 0;
	int clearflags = 0;

	for(int i = 0; flagtrans[i] != -1; i++, arg1 >>= 1, arg2 >>= 1)
	{
		if (arg1 & 1) setflags |= flagtrans[i];
		if (arg2 & 1) clearflags |= flagtrans[i];
	}

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		level.lines[line].flags = (level.lines[line].flags & ~clearflags) | setflags;
	}
	return true;
}

FUNC(LS_Line_SetAutomapFlags)
// Line_SetAutomapFlags (id, setflags, clearflags)
{
	static const int flagtrans[] =
	{
		ML_SECRET,
		ML_DONTDRAW,
		ML_MAPPED,
		ML_REVEALED,
		-1
	};

	if (arg0 == 0) return false;

	int setflags = 0;
	int clearflags = 0;

	for (int i = 0; flagtrans[i] != -1; i++, arg1 >>= 1, arg2 >>= 1)
	{
		if (arg1 & 1) setflags |= flagtrans[i];
		if (arg2 & 1) clearflags |= flagtrans[i];
	}

	FLineIdIterator itr(arg0);
	int line;
	while ((line = itr.Next()) >= 0)
	{
		level.lines[line].flags = (level.lines[line].flags & ~clearflags) | setflags;
	}

	return true;
}

FUNC(LS_Line_SetAutomapStyle)
// Line_SetAutomapStyle (id, style)
{
	if (arg1 < AMLS_COUNT && arg1 >= 0)
	{
		FLineIdIterator itr(arg0);
		int line;
		while ((line = itr.Next()) >= 0)
		{
			level.lines[line].automapstyle = (AutomapLineStyle) arg1;
		}

		return true;
	}
	return false;
}


FUNC(LS_ChangeCamera)
// ChangeCamera (tid, who, revert?)
{
	AActor *camera;
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		camera = iterator.Next ();
	}
	else
	{
		camera = NULL;
	}

	if (!it || !it->player || arg1)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			AActor *oldcamera = players[i].camera;
			if (camera)
			{
				players[i].camera = camera;
				if (arg2)
					players[i].cheats |= CF_REVERTPLEASE;
			}
			else
			{
				players[i].camera = players[i].mo;
				players[i].cheats &= ~CF_REVERTPLEASE;
			}
			if (oldcamera != players[i].camera)
			{
				R_ClearPastViewer (players[i].camera);
			}
		}
	}
	else
	{
		AActor *oldcamera = it->player->camera;
		if (camera)
		{
			it->player->camera = camera;
			if (arg2)
				it->player->cheats |= CF_REVERTPLEASE;
		}
		else
		{
			it->player->camera = it;
			it->player->cheats &= ~CF_REVERTPLEASE;
		}
		if (oldcamera != it->player->camera)
		{
			R_ClearPastViewer (it->player->camera);
		}
	}

	return true;
}

enum
{
	PROP_FROZEN,
	PROP_NOTARGET,
	PROP_INSTANTWEAPONSWITCH,
	PROP_FLY,
	PROP_TOTALLYFROZEN,

	PROP_INVULNERABILITY,
	PROP_STRENGTH,
	PROP_INVISIBILITY,
	PROP_RADIATIONSUIT,
	PROP_ALLMAP,
	PROP_INFRARED,
	PROP_WEAPONLEVEL2,
	PROP_FLIGHT,
	PROP_UNUSED1,
	PROP_UNUSED2,
	PROP_SPEED,
	PROP_BUDDHA,
};

FUNC(LS_SetPlayerProperty)
// SetPlayerProperty (who, value, which)
// who == 0: set activator's property
// who == 1: set every player's property
{
	int mask = 0;

	if ((!it || !it->player) && !arg0)
		return false;

	// Add or remove a power
	if (arg2 >= PROP_INVULNERABILITY && arg2 <= PROP_SPEED)
	{
		static ENamedName powers[14] =
		{
			NAME_PowerInvulnerable,
			NAME_PowerStrength,
			NAME_PowerInvisibility,
			NAME_PowerIronFeet,
			NAME_None,
			NAME_PowerLightAmp,
			NAME_PowerWeaponLevel2,
			NAME_PowerFlight,
			NAME_None,
			NAME_None,
			NAME_PowerSpeed,
			NAME_PowerInfiniteAmmo,
			NAME_PowerDoubleFiringSpeed,
			NAME_PowerBuddha
		};
		int power = arg2 - PROP_INVULNERABILITY;

		if (power > 4 && powers[power] == NAME_None)
		{
			return false;
		}

		if (arg0 == 0)
		{
			if (arg1)
			{ // Give power to activator
				if (power != 4)
				{
					auto item = it->GiveInventoryType(PClass::FindActor(powers[power]));
					if (item != NULL && power == 0 && arg1 == 1) 
					{
						item->ColorVar(NAME_BlendColor) = MakeSpecialColormap(INVERSECOLORMAP);
					}
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags2 |= LEVEL2_ALLMAP;
				}
			}
			else
			{ // Take power from activator
				if (power != 4)
				{
					auto item = it->FindInventory(powers[power], true);
					if (item != NULL)
					{
						item->Destroy ();
					}
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags2 &= ~LEVEL2_ALLMAP;
				}
			}
		}
		else
		{
			int i;

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].mo == NULL)
					continue;

				if (arg1)
				{ // Give power
					if (power != 4)
					{
						auto item = players[i].mo->GiveInventoryType ((PClass::FindActor(powers[power])));
						if (item != NULL && power == 0 && arg1 == 1) 
						{
							item->ColorVar(NAME_BlendColor) = MakeSpecialColormap(INVERSECOLORMAP);
						}
					}
					else if (i == consoleplayer)
					{
						level.flags2 |= LEVEL2_ALLMAP;
					}
				}
				else
				{ // Take power
					if (power != 4)
					{
						auto item = players[i].mo->FindInventory (PClass::FindActor(powers[power]));
						if (item != NULL)
						{
							item->Destroy ();
						}
					}
					else if (i == consoleplayer)
					{
						level.flags2 &= ~LEVEL2_ALLMAP;
					}
				}
			}
		}
		return true;
	}

	// Set or clear a flag
	switch (arg2)
	{
	case PROP_BUDDHA:
		mask = CF_BUDDHA;
		break;
	case PROP_FROZEN:
		mask = CF_FROZEN;
		break;
	case PROP_NOTARGET:
		mask = CF_NOTARGET;
		break;
	case PROP_INSTANTWEAPONSWITCH:
		mask = CF_INSTANTWEAPSWITCH;
		break;
	case PROP_FLY:
		//mask = CF_FLY;
		break;
	case PROP_TOTALLYFROZEN:
		mask = CF_TOTALLYFROZEN;
		break;
	}

	if (arg0 == 0)
	{
		if (arg1)
		{
			it->player->cheats |= mask;
			if (arg2 == PROP_FLY)
			{
				it->flags7 |= MF7_FLYCHEAT;
				it->flags2 |= MF2_FLY;
				it->flags |= MF_NOGRAVITY;
			}
		}
		else
		{
			it->player->cheats &= ~mask;
			if (arg2 == PROP_FLY)
			{
				it->flags7 &= ~MF7_FLYCHEAT;
				it->flags2 &= ~MF2_FLY;
				it->flags &= ~MF_NOGRAVITY;
			}
		}
	}
	else
	{
		int i;

		if ((ib_compatflags & BCOMPATF_LINKFROZENPROPS) && (mask & (CF_FROZEN | CF_TOTALLYFROZEN)))
		{ // Clearing one of these properties clears both of them (if the compat flag is set.)
			mask = CF_FROZEN | CF_TOTALLYFROZEN;
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			if (arg1)
			{
				players[i].cheats |= mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 |= MF2_FLY;
					players[i].mo->flags |= MF_NOGRAVITY;
				}
			}
			else
			{
				players[i].cheats &= ~mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 &= ~MF2_FLY;
					players[i].mo->flags &= ~MF_NOGRAVITY;
				}
			}
		}
	}

	return !!mask;
}

FUNC(LS_TranslucentLine)
// TranslucentLine (id, amount, type)
{
	FLineIdIterator itr(arg0);
	int linenum;
	while ((linenum = itr.Next()) >= 0)
	{
		level.lines[linenum].alpha = clamp(arg1, 0, 255) / 255.;
		if (arg2 == 0)
		{
			level.lines[linenum].flags &= ~ML_ADDTRANS;
		}
		else if (arg2 == 1)
		{
			level.lines[linenum].flags |= ML_ADDTRANS;
		}
		else
		{
			Printf ("Unknown translucency type used with TranslucentLine\n");
		}
	}

	return true;
}

FUNC(LS_Autosave)
{
	if (gameaction != ga_savegame)
	{
		level.flags2 &= ~LEVEL2_NOAUTOSAVEHINT;
		Net_WriteByte (DEM_CHECKAUTOSAVE);
	}
	return true;
}

FUNC(LS_ChangeSkill)
{
	if ((unsigned)arg0 >= AllSkills.Size())
	{
		NextSkill = -1;
	}
	else
	{
		NextSkill = arg0;
	}
	return true;
}

FUNC(LS_NoiseAlert)
// NoiseAlert (TID of target, TID of emitter)
{
	AActor *target, *emitter;

	if (arg0 == 0)
	{
		target = it;
	}
	else
	{
		FActorIterator iter (arg0);
		target = iter.Next();
	}

	if (arg1 == 0)
	{
		emitter = it;
	}
	else if (arg1 == arg0)
	{
		emitter = target;
	}
	else
	{
		FActorIterator iter (arg1);
		emitter = iter.Next();
	}

	P_NoiseAlert (emitter, target);
	return true;
}

FUNC(LS_SendToCommunicator)
// SendToCommunicator (voc #, front-only, identify, nolog)
{
	// This obviously isn't going to work for co-op.
	if (arg1 && backSide)
		return false;

	if (it != NULL && it->player != NULL && it->FindInventory(NAME_Communicator))
	{
		char name[32];									   
		mysnprintf (name, countof(name), "svox/voc%d", arg0);

		if (!arg3)
		{
			it->player->SetLogNumber (arg0);
		}

		if (it->CheckLocalView (consoleplayer))
		{
			S_StopSound (CHAN_VOICE);
			S_Sound (CHAN_VOICE, name, 1, ATTN_NORM);

			// Get the message from the LANGUAGE lump.
			FString msg;
			msg.Format("TXT_COMM%d", arg2);
			const char *str = GStrings[msg];
			if (str != NULL)
			{
				Printf (PRINT_CHAT, "%s\n", str);
			}
		}
		return true;
	}
	return false;
}

FUNC(LS_ForceField)
// ForceField ()
{
	if (it != NULL)
	{
		P_DamageMobj (it, NULL, NULL, 16, NAME_None);
		it->Thrust(it->Angles.Yaw + 180, 7.8125);
	}
	return true;
}

FUNC(LS_ClearForceField)
// ClearForceField (tag)
{
	bool rtn = false;

	FSectorTagIterator itr(arg0);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		sector_t *sec = &level.sectors[secnum];
		rtn = true;

		sec->RemoveForceField();
	}
	return rtn;
}

FUNC(LS_GlassBreak)
// GlassBreak (bNoJunk, junkID)
{
	bool switched;
	bool quest1, quest2;

	ln->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
	switched = P_ChangeSwitchTexture (ln->sidedef[0], false, 0, &quest1);
	ln->special = 0;
	if (ln->sidedef[1] != NULL)
	{
		switched |= P_ChangeSwitchTexture (ln->sidedef[1], false, 0, &quest2);
	}
	else
	{
		quest2 = quest1;
	}
	if (switched)
	{
		if (!arg0)
		{ // Break some glass

			DVector2 linemid((ln->v1->fX() + ln->v2->fX()) / 2, (ln->v1->fY() + ln->v2->fY()) / 2);

			// remove dependence on sector size and always spawn 2 map units in front of the line.
			DVector2 normal(ln->Delta().Y, -ln->Delta().X);
			linemid += normal.Unit() * 2;
			/* old code:
			x += (ln->frontsector->centerspot.x - x) / 5;
			y += (ln->frontsector->centerspot.y - y) / 5;
			*/

			auto type = SpawnableThings.CheckKey(arg1);
			for (int i = 0; i < 7; ++i)
			{
				AActor *glass = nullptr;
				if (arg1 > 0)
				{
					if (type != nullptr)
					{
						glass = Spawn(*type, DVector3(linemid, ONFLOORZ), ALLOW_REPLACE);
						glass->AddZ(24.);
					}
				}
				else
				{
					glass = Spawn("GlassJunk", DVector3(linemid, ONFLOORZ), ALLOW_REPLACE);
					glass->AddZ(24.);
					glass->SetState(glass->SpawnState + (pr_glass() % glass->health));
				}
				if (glass != nullptr)
				{
					glass->Angles.Yaw = pr_glass() * (360 / 256.);
					glass->VelFromAngle(pr_glass() & 3);
					glass->Vel.Z = (pr_glass() & 7);
					// [RH] Let the shards stick around longer than they did in Strife.
					glass->tics += pr_glass();
				}
			}
		}
		if (quest1 || quest2)
		{ // Up stats and signal this mission is complete
			if (it == NULL)
			{
				for (int i = 0; i < MAXPLAYERS; ++i)
				{
					if (playeringame[i])
					{
						it = players[i].mo;
						break;
					}
				}
			}
			if (it != NULL)
			{
				it->GiveInventoryType (PClass::FindActor("QuestItem29"));
				it->GiveInventoryType (PClass::FindActor("UpgradeAccuracy"));
				it->GiveInventoryType (PClass::FindActor("UpgradeStamina"));
			}
		}
	}
	// We already changed the switch texture, so don't make the main code switch it back.
	return false;
}

FUNC(LS_StartConversation)
// StartConversation (tid, facetalker)
{
	FActorIterator iterator (arg0);

	AActor *target = iterator.Next();

	// Nothing to talk to
	if (target == NULL)
	{
		return false;
	}
	
	// Only living players are allowed to start conversations
	if (it == NULL || it->player == NULL || it->player->mo != it || it->health<=0)
	{
		return false;
	}

	// Dead things can't talk.
	if (target->health <= 0)
	{
		return false;
	}
	// Fighting things don't talk either.
	if (target->flags4 & MF4_INCOMBAT)
	{
		return false;
	}
	if (target->Conversation != NULL)
	{
		// Give the NPC a chance to play a brief animation
		target->ConversationAnimation (0);
		P_StartConversation (target, it, !!arg1, true);
		return true;
	}
	return false;
}

FUNC(LS_Thing_SetConversation)
// Thing_SetConversation (tid, dlg_id)
{
	int dlg_index = -1;
	FStrifeDialogueNode *node = NULL;

	if (arg1 != 0)
	{
		dlg_index = GetConversation(arg1);	
		if (dlg_index == -1) return false;
		node = StrifeDialogues[dlg_index];
	}

	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		while ((it = iterator.Next()) != NULL)
		{
			it->ConversationRoot = dlg_index;
			it->Conversation = node;
		}
	}
	else if (it)
	{
		it->ConversationRoot = dlg_index;
		it->Conversation = node;
	}
	return true;
}

FUNC(LS_Line_SetPortalTarget)
// Line_SetPortalTarget(thisid, destid)
{
	return P_ChangePortal(ln, arg0, arg1);
}

FUNC(LS_Sector_SetPlaneReflection)
// Sector_SetPlaneReflection (tag, floor, ceiling)
{
	int secnum;
	FSectorTagIterator itr(arg0);

	while ((secnum = itr.Next()) >= 0)
	{
		sector_t * s = &level.sectors[secnum];
		if (!s->floorplane.isSlope()) s->reflect[sector_t::floor] = arg1 / 255.f;
		if (!s->ceilingplane.isSlope()) level.sectors[secnum].reflect[sector_t::ceiling] = arg2 / 255.f;
	}

	return true;
}


FUNC(LS_SetGlobalFogParameter)
// SetGlobalFogParameter (type, value)
{
	switch (arg0)
	{
	case 0:
		level.fogdensity = arg1 >> 1;
		return true;

	case 1:
		level.outsidefogdensity = arg1 >> 1;
		return true;

	case 2:
		level.skyfog = arg1;
		return true;

	default:
		return false;
	}
}

FUNC(LS_Sector_SetFloorGlow)
// Sector_SetFloorGlow(tag, height, r, g, b)
{
	int secnum;
	PalEntry color(arg2, arg3, arg4);
	if (arg1 < 0) color = -1;	// negative height invalidates the glow.
	FSectorTagIterator itr(arg0);

	while ((secnum = itr.Next()) >= 0)
	{
		sector_t * s = &level.sectors[secnum];
		s->SetGlowColor(sector_t::floor, color);
		s->SetGlowHeight(sector_t::floor, float(arg1));
	}
	return true;
}

FUNC(LS_Sector_SetCeilingGlow)
// Sector_SetCeilingGlow(tag, height, r, g, b)
{
	int secnum;
	PalEntry color(arg2, arg3, arg4);
	if (arg1 < 0) color = -1;	// negative height invalidates the glow.
	FSectorTagIterator itr(arg0);

	while ((secnum = itr.Next()) >= 0)
	{
		sector_t * s = &level.sectors[secnum];
		s->SetGlowColor(sector_t::ceiling, color);
		s->SetGlowHeight(sector_t::ceiling, float(arg1));
	}
	return true;
}

FUNC(LS_Line_SetHealth)
// Line_SetHealth(id, health)
{
	FLineIdIterator itr(arg0);
	int l;

	if (arg1 < 0)
		arg1 = 0;

	while ((l = itr.Next()) >= 0)
	{
		line_t* line = &level.lines[l];
		line->health = arg1;
		if (line->healthgroup)
			P_SetHealthGroupHealth(line->healthgroup, arg1);
	}
	return true;
}

FUNC(LS_Sector_SetHealth)
// Sector_SetHealth(id, part, health)
{
	FSectorTagIterator itr(arg0);
	int s;

	if (arg2 < 0)
		arg2 = 0;

	while ((s = itr.Next()) >= 0)
	{
		sector_t* sector = &level.sectors[s];
		if (arg1 == SECPART_Ceiling)
		{
			sector->healthceiling = arg2;
			if (sector->healthceilinggroup)
				P_SetHealthGroupHealth(sector->healthceilinggroup, arg2);
		}
		else if (arg1 == SECPART_Floor)
		{
			sector->healthfloor = arg2;
			if (sector->healthfloorgroup)
				P_SetHealthGroupHealth(sector->healthfloorgroup, arg2);
		}
		else if (arg1 == SECPART_3D)
		{
			sector->health3d = arg2;
			if (sector->health3dgroup)
				P_SetHealthGroupHealth(sector->health3dgroup, arg2);
		}
	}
	return true;
}

static lnSpecFunc LineSpecials[] =
{
	/*   0 */ LS_NOP,
	/*   1 */ LS_NOP,		// Polyobj_StartLine,
	/*   2 */ LS_Polyobj_RotateLeft,
	/*   3 */ LS_Polyobj_RotateRight,
	/*   4 */ LS_Polyobj_Move,
	/*   5 */ LS_NOP,		// Polyobj_ExplicitLine
	/*   6 */ LS_Polyobj_MoveTimes8,
	/*   7 */ LS_Polyobj_DoorSwing,
	/*   8 */ LS_Polyobj_DoorSlide,
	/*   9 */ LS_NOP,		// Line_Horizon
	/*  10 */ LS_Door_Close,
	/*  11 */ LS_Door_Open,
	/*  12 */ LS_Door_Raise,
	/*  13 */ LS_Door_LockedRaise,
	/*  14 */ LS_Door_Animated,
	/*  15 */ LS_Autosave,
	/*  16 */ LS_NOP,		// Transfer_WallLight
	/*  17 */ LS_Thing_Raise,
	/*  18 */ LS_StartConversation,
	/*  19 */ LS_Thing_Stop,
	/*  20 */ LS_Floor_LowerByValue,
	/*  21 */ LS_Floor_LowerToLowest,
	/*  22 */ LS_Floor_LowerToNearest,
	/*  23 */ LS_Floor_RaiseByValue,
	/*  24 */ LS_Floor_RaiseToHighest,
	/*  25 */ LS_Floor_RaiseToNearest,
	/*  26 */ LS_Stairs_BuildDown,
	/*  27 */ LS_Stairs_BuildUp,
	/*  28 */ LS_Floor_RaiseAndCrush,
	/*  29 */ LS_Pillar_Build,
	/*  30 */ LS_Pillar_Open,
	/*  31 */ LS_Stairs_BuildDownSync,
	/*  32 */ LS_Stairs_BuildUpSync,
	/*  33 */ LS_ForceField,
	/*  34 */ LS_ClearForceField,
	/*  35 */ LS_Floor_RaiseByValueTimes8,
	/*  36 */ LS_Floor_LowerByValueTimes8,
	/*  37 */ LS_Floor_MoveToValue,
	/*  38 */ LS_Ceiling_Waggle,
	/*  39 */ LS_Teleport_ZombieChanger,
	/*  40 */ LS_Ceiling_LowerByValue,
	/*  41 */ LS_Ceiling_RaiseByValue,
	/*  42 */ LS_Ceiling_CrushAndRaise,
	/*  43 */ LS_Ceiling_LowerAndCrush,
	/*  44 */ LS_Ceiling_CrushStop,
	/*  45 */ LS_Ceiling_CrushRaiseAndStay,
	/*  46 */ LS_Floor_CrushStop,
	/*  47 */ LS_Ceiling_MoveToValue,
	/*  48 */ LS_NOP,		// Sector_Attach3dMidtex
	/*  49 */ LS_GlassBreak,
	/*  50 */ LS_NOP,		// ExtraFloor_LightOnly
	/*  51 */ LS_Sector_SetLink,
	/*  52 */ LS_Scroll_Wall,
	/*  53 */ LS_Line_SetTextureOffset,
	/*  54 */ LS_Sector_ChangeFlags,
	/*  55 */ LS_Line_SetBlocking,
	/*  56 */ LS_Line_SetTextureScale,
	/*  57 */ LS_NOP,		// Sector_SetPortal
	/*  58 */ LS_NOP,		// Sector_CopyScroller
	/*  59 */ LS_Polyobj_OR_MoveToSpot,
	/*  60 */ LS_Plat_PerpetualRaise,
	/*  61 */ LS_Plat_Stop,
	/*  62 */ LS_Plat_DownWaitUpStay,
	/*  63 */ LS_Plat_DownByValue,
	/*  64 */ LS_Plat_UpWaitDownStay,
	/*  65 */ LS_Plat_UpByValue,
	/*  66 */ LS_Floor_LowerInstant,
	/*  67 */ LS_Floor_RaiseInstant,
	/*  68 */ LS_Floor_MoveToValueTimes8,
	/*  69 */ LS_Ceiling_MoveToValueTimes8,
	/*  70 */ LS_Teleport,
	/*  71 */ LS_Teleport_NoFog,
	/*  72 */ LS_ThrustThing,
	/*  73 */ LS_DamageThing,
	/*  74 */ LS_Teleport_NewMap,
	/*  75 */ LS_Teleport_EndGame,
	/*  76 */ LS_TeleportOther,
	/*  77 */ LS_TeleportGroup,
	/*  78 */ LS_TeleportInSector,
	/*  79 */ LS_Thing_SetConversation,
	/*  80 */ LS_ACS_Execute,
	/*  81 */ LS_ACS_Suspend,
	/*  82 */ LS_ACS_Terminate,
	/*  83 */ LS_ACS_LockedExecute,
	/*  84 */ LS_ACS_ExecuteWithResult,
	/*  85 */ LS_ACS_LockedExecuteDoor,
	/*  86 */ LS_Polyobj_MoveToSpot,
	/*  87 */ LS_Polyobj_Stop,
	/*  88 */ LS_Polyobj_MoveTo,
	/*  89 */ LS_Polyobj_OR_MoveTo,
	/*  90 */ LS_Polyobj_OR_RotateLeft,
	/*  91 */ LS_Polyobj_OR_RotateRight,
	/*  92 */ LS_Polyobj_OR_Move,
	/*  93 */ LS_Polyobj_OR_MoveTimes8,
	/*  94 */ LS_Pillar_BuildAndCrush,
	/*  95 */ LS_FloorAndCeiling_LowerByValue,
	/*  96 */ LS_FloorAndCeiling_RaiseByValue,
	/*  97 */ LS_Ceiling_LowerAndCrushDist,
	/*  98 */ LS_Sector_SetTranslucent,
	/*  99 */ LS_Floor_RaiseAndCrushDoom,
	/* 100 */ LS_NOP,		// Scroll_Texture_Left
	/* 101 */ LS_NOP,		// Scroll_Texture_Right
	/* 102 */ LS_NOP,		// Scroll_Texture_Up
	/* 103 */ LS_NOP,		// Scroll_Texture_Down
	/* 104 */ LS_Ceiling_CrushAndRaiseSilentDist,
	/* 105 */ LS_Door_WaitRaise,
	/* 106 */ LS_Door_WaitClose,
	/* 107 */ LS_Line_SetPortalTarget,
	/* 108 */ LS_NOP,
	/* 109 */ LS_Light_ForceLightning,
	/* 110 */ LS_Light_RaiseByValue,
	/* 111 */ LS_Light_LowerByValue,
	/* 112 */ LS_Light_ChangeToValue,
	/* 113 */ LS_Light_Fade,
	/* 114 */ LS_Light_Glow,
	/* 115 */ LS_Light_Flicker,
	/* 116 */ LS_Light_Strobe,
	/* 117 */ LS_Light_Stop,
	/* 118 */ LS_NOP,		// Plane_Copy
	/* 119 */ LS_Thing_Damage,
	/* 120 */ LS_Radius_Quake,
	/* 121 */ LS_NOP,		// Line_SetIdentification
	/* 122 */ LS_NOP,
	/* 123 */ LS_NOP,
	/* 124 */ LS_NOP,
	/* 125 */ LS_Thing_Move,
	/* 126 */ LS_NOP,
	/* 127 */ LS_Thing_SetSpecial,
	/* 128 */ LS_ThrustThingZ,
	/* 129 */ LS_UsePuzzleItem,
	/* 130 */ LS_Thing_Activate,
	/* 131 */ LS_Thing_Deactivate,
	/* 132 */ LS_Thing_Remove,
	/* 133 */ LS_Thing_Destroy,
	/* 134 */ LS_Thing_Projectile,
	/* 135 */ LS_Thing_Spawn,
	/* 136 */ LS_Thing_ProjectileGravity,
	/* 137 */ LS_Thing_SpawnNoFog,
	/* 138 */ LS_Floor_Waggle,
	/* 139 */ LS_Thing_SpawnFacing,
	/* 140 */ LS_Sector_ChangeSound,
	/* 141 */ LS_NOP,
	/* 142 */ LS_NOP,
	/* 143 */ LS_NOP,
	/* 144 */ LS_NOP,
	/* 145 */ LS_NOP,		// 145 Player_SetTeam
	/* 146 */ LS_NOP,
	/* 147 */ LS_NOP,
	/* 148 */ LS_NOP,
	/* 149 */ LS_NOP,
	/* 150 */ LS_Line_SetHealth,
	/* 151 */ LS_Sector_SetHealth,
	/* 152 */ LS_NOP,		// 152 Team_Score
	/* 153 */ LS_NOP,		// 153 Team_GivePoints
	/* 154 */ LS_Teleport_NoStop,
	/* 155 */ LS_NOP,
	/* 156 */ LS_NOP,
	/* 157 */ LS_SetGlobalFogParameter,
	/* 158 */ LS_FS_Execute,
	/* 159 */ LS_Sector_SetPlaneReflection,
	/* 160 */ LS_NOP,		// Sector_Set3DFloor
	/* 161 */ LS_NOP,		// Sector_SetContents
	/* 162 */ LS_NOP,		// Reserved Doom64 branch
	/* 163 */ LS_NOP,		// Reserved Doom64 branch
	/* 164 */ LS_NOP,		// Reserved Doom64 branch
	/* 165 */ LS_NOP,		// Reserved Doom64 branch
	/* 166 */ LS_NOP,		// Reserved Doom64 branch
	/* 167 */ LS_NOP,		// Reserved Doom64 branch
	/* 168 */ LS_Ceiling_CrushAndRaiseDist,
	/* 169 */ LS_Generic_Crusher2,
	/* 170 */ LS_Sector_SetCeilingScale2,
	/* 171 */ LS_Sector_SetFloorScale2,
	/* 172 */ LS_Plat_UpNearestWaitDownStay,
	/* 173 */ LS_NoiseAlert,
	/* 174 */ LS_SendToCommunicator,
	/* 175 */ LS_Thing_ProjectileIntercept,
	/* 176 */ LS_Thing_ChangeTID,
	/* 177 */ LS_Thing_Hate,
	/* 178 */ LS_Thing_ProjectileAimed,
	/* 179 */ LS_ChangeSkill,
	/* 180 */ LS_Thing_SetTranslation,
	/* 181 */ LS_NOP,		// Plane_Align
	/* 182 */ LS_NOP,		// Line_Mirror
	/* 183 */ LS_Line_AlignCeiling,
	/* 184 */ LS_Line_AlignFloor,
	/* 185 */ LS_Sector_SetRotation,
	/* 186 */ LS_Sector_SetCeilingPanning,
	/* 187 */ LS_Sector_SetFloorPanning,
	/* 188 */ LS_Sector_SetCeilingScale,
	/* 189 */ LS_Sector_SetFloorScale,
	/* 190 */ LS_NOP,		// Static_Init
	/* 191 */ LS_SetPlayerProperty,
	/* 192 */ LS_Ceiling_LowerToHighestFloor,
	/* 193 */ LS_Ceiling_LowerInstant,
	/* 194 */ LS_Ceiling_RaiseInstant,
	/* 195 */ LS_Ceiling_CrushRaiseAndStayA,
	/* 196 */ LS_Ceiling_CrushAndRaiseA,
	/* 197 */ LS_Ceiling_CrushAndRaiseSilentA,
	/* 198 */ LS_Ceiling_RaiseByValueTimes8,
	/* 199 */ LS_Ceiling_LowerByValueTimes8,
	/* 200 */ LS_Generic_Floor,
	/* 201 */ LS_Generic_Ceiling,
	/* 202 */ LS_Generic_Door,
	/* 203 */ LS_Generic_Lift,
	/* 204 */ LS_Generic_Stairs,
	/* 205 */ LS_Generic_Crusher,
	/* 206 */ LS_Plat_DownWaitUpStayLip,
	/* 207 */ LS_Plat_PerpetualRaiseLip,
	/* 208 */ LS_TranslucentLine,
	/* 209 */ LS_NOP,		// Transfer_Heights
	/* 210 */ LS_NOP,		// Transfer_FloorLight
	/* 211 */ LS_NOP,		// Transfer_CeilingLight
	/* 212 */ LS_Sector_SetColor,
	/* 213 */ LS_Sector_SetFade,
	/* 214 */ LS_Sector_SetDamage,
	/* 215 */ LS_Teleport_Line,
	/* 216 */ LS_Sector_SetGravity,
	/* 217 */ LS_Stairs_BuildUpDoom,
	/* 218 */ LS_Sector_SetWind,
	/* 219 */ LS_Sector_SetFriction,
	/* 220 */ LS_Sector_SetCurrent,
	/* 221 */ LS_Scroll_Texture_Both,
	/* 222 */ LS_NOP,		// Scroll_Texture_Model
	/* 223 */ LS_Scroll_Floor,
	/* 224 */ LS_Scroll_Ceiling,
	/* 225 */ LS_NOP,		// Scroll_Texture_Offsets
	/* 226 */ LS_ACS_ExecuteAlways,
	/* 227 */ LS_PointPush_SetForce,
	/* 228 */ LS_Plat_RaiseAndStayTx0,
	/* 229 */ LS_Thing_SetGoal,
	/* 230 */ LS_Plat_UpByValueStayTx,
	/* 231 */ LS_Plat_ToggleCeiling,
	/* 232 */ LS_Light_StrobeDoom,
	/* 233 */ LS_Light_MinNeighbor,
	/* 234 */ LS_Light_MaxNeighbor,
	/* 235 */ LS_Floor_TransferTrigger,
	/* 236 */ LS_Floor_TransferNumeric,
	/* 237 */ LS_ChangeCamera,
	/* 238 */ LS_Floor_RaiseToLowestCeiling,
	/* 239 */ LS_Floor_RaiseByValueTxTy,
	/* 240 */ LS_Floor_RaiseByTexture,
	/* 241 */ LS_Floor_LowerToLowestTxTy,
	/* 242 */ LS_Floor_LowerToHighest,
	/* 243 */ LS_Exit_Normal,
	/* 244 */ LS_Exit_Secret,
	/* 245 */ LS_Elevator_RaiseToNearest,
	/* 246 */ LS_Elevator_MoveToFloor,
	/* 247 */ LS_Elevator_LowerToNearest,
	/* 248 */ LS_HealThing,
	/* 249 */ LS_Door_CloseWaitOpen,
	/* 250 */ LS_Floor_Donut,
	/* 251 */ LS_FloorAndCeiling_LowerRaise,
	/* 252 */ LS_Ceiling_RaiseToNearest,
	/* 253 */ LS_Ceiling_LowerToLowest,
	/* 254 */ LS_Ceiling_LowerToFloor,
	/* 255 */ LS_Ceiling_CrushRaiseAndStaySilA,

	/* 256 */ LS_Floor_LowerToHighestEE,
	/* 257 */ LS_Floor_RaiseToLowest,
	/* 258 */ LS_Floor_LowerToLowestCeiling,
	/* 259 */ LS_Floor_RaiseToCeiling,
	/* 260 */ LS_Floor_ToCeilingInstant,
	/* 261 */ LS_Floor_LowerByTexture,
	/* 262 */ LS_Ceiling_RaiseToHighest,
	/* 263 */ LS_Ceiling_ToHighestInstant,
	/* 264 */ LS_Ceiling_LowerToNearest,
	/* 265 */ LS_Ceiling_RaiseToLowest,
	/* 266 */ LS_Ceiling_RaiseToHighestFloor,
	/* 267 */ LS_Ceiling_ToFloorInstant,
	/* 268 */ LS_Ceiling_RaiseByTexture,
	/* 269 */ LS_Ceiling_LowerByTexture,
	/* 270 */ LS_Stairs_BuildDownDoom,
	/* 271 */ LS_Stairs_BuildUpDoomSync,
	/* 272 */ LS_Stairs_BuildDownDoomSync,
	/* 273 */ LS_Stairs_BuildUpDoomCrush,
	/* 274 */ LS_Door_AnimatedClose,
	/* 275 */ LS_Floor_Stop,
	/* 276 */ LS_Ceiling_Stop,
	/* 277 */ LS_Sector_SetFloorGlow,
	/* 278 */ LS_Sector_SetCeilingGlow,
	/* 279 */ LS_Floor_MoveToValueAndCrush,
	/* 280 */ LS_Ceiling_MoveToValueAndCrush,
	/* 281 */ LS_Line_SetAutomapFlags,
	/* 282 */ LS_Line_SetAutomapStyle,


};

#define DEFINE_SPECIAL(name, num, min, max, mmax) {#name, num, min, max, mmax},
static FLineSpecial LineSpecialNames[] = {
#include "actionspecials.h"
};

static int lscmp (const void * a, const void * b)
{
	return stricmp( ((FLineSpecial*)a)->name, ((FLineSpecial*)b)->name);
}

static struct LineSpecialTable
{
	TArray<FLineSpecial *> LineSpecialsInfo;

	LineSpecialTable()
	{
		unsigned int max = 0;
		for (size_t i = 0; i < countof(LineSpecialNames); ++i)
		{
			if (LineSpecialNames[i].number > (int)max)
				max = LineSpecialNames[i].number;
		}
		LineSpecialsInfo.Resize(max + 1);
		for (unsigned i = 0; i <= max; i++)
		{
			LineSpecialsInfo[i] = NULL;
		}

		qsort(LineSpecialNames, countof(LineSpecialNames), sizeof(FLineSpecial), lscmp);
		for (size_t i = 0; i < countof(LineSpecialNames); ++i)
		{
			assert(LineSpecialsInfo[LineSpecialNames[i].number] == NULL);
			LineSpecialsInfo[LineSpecialNames[i].number] = &LineSpecialNames[i];
		}
	}
} LineSpec;

//==========================================================================
//
//
//
//==========================================================================

int P_GetMaxLineSpecial()
{
	return LineSpec.LineSpecialsInfo.Size() - 1;
}

//==========================================================================
//
//
//
//==========================================================================

FLineSpecial *P_GetLineSpecialInfo(int special)
{
	if ((unsigned) special < LineSpec.LineSpecialsInfo.Size())
	{
		return LineSpec.LineSpecialsInfo[special];
	}
	return NULL;
}

//==========================================================================
//
// P_FindLineSpecial
//
// Finds a line special and also returns the min and max argument count.
//
//==========================================================================

int P_FindLineSpecial (const char *string, int *min_args, int *max_args)
{
	int min = 0, max = countof(LineSpecialNames) - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, LineSpecialNames[mid].name);
		if (lexval == 0)
		{
			if (min_args != NULL) *min_args = LineSpecialNames[mid].min_args;
			if (max_args != NULL) *max_args = LineSpecialNames[mid].max_args;
			return LineSpecialNames[mid].number;
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return 0;
}


//==========================================================================
//
// P_ExecuteSpecial
//
//==========================================================================

int P_ExecuteSpecial(int			num,
					 struct line_t	*line,
					 class AActor	*activator,
					 bool			backSide,
					 int			arg1,
					 int			arg2,
					 int			arg3,
					 int			arg4,
					 int			arg5)
{
	if (num >= 0 && num < (int)countof(LineSpecials))
	{
		return LineSpecials[num](line, activator, backSide, arg1, arg2, arg3, arg4, arg5);
	}
	return 0;
}

//==========================================================================
//
// Execute a line special / script
//
//==========================================================================
DEFINE_ACTION_FUNCTION(FLevelLocals, ExecuteSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(special);
	PARAM_OBJECT(activator, AActor);
	PARAM_POINTER(linedef, line_t);
	PARAM_BOOL(lineside);
	PARAM_INT(arg1);
	PARAM_INT(arg2);
	PARAM_INT(arg3);
	PARAM_INT(arg4);
	PARAM_INT(arg5);

	bool res = !!P_ExecuteSpecial(special, linedef, activator, lineside, arg1, arg2, arg3, arg4, arg5);

	ACTION_RETURN_BOOL(res);
}

