// p_lnspec.c: Code to handle line specials.
//
// Each function returns true if it caused something to happen
// or false if it couldn't perform the desired action.

#include "doomstat.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "g_level.h"
#include "v_palett.h"
#include "tables.h"

#define FUNC(a) static BOOL a (line_t *ln, mobj_t *it, int arg0, int arg1, \
							   int arg2, int arg3, int arg4)

#define SPEED(a)		((a)*(FRACUNIT/8))
#define TICS(a)			(((a)*TICRATE)/35)
#define OCTICS(a)		(((a)*TICRATE)/8)
#define	BYTEANGLE(a)	((angle_t)((a)<<24))


// Used by the teleporters to know if they were
// activated by walking across the backside of a line.
int TeleportSide;

// Set true if this special was activated from inside a script.
BOOL InScript;

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
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, false);
}

FUNC(LS_Polyobj_MoveTimes8)
// Polyobj_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, false);
}

FUNC(LS_Polyobj_DoorSwing)
// Polyobj_DoorSwing (po, speed, angle, delay)
{
	return EV_OpenPolyDoor (ln, arg0, arg1, BYTEANGLE(arg2), arg3, 0, PODOOR_SWING);
}

FUNC(LS_Polyobj_DoorSlide)
// Polyobj_DoorSlide (po, speed, angle, distance, delay)
{
	return EV_OpenPolyDoor (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg4, arg3*FRACUNIT, PODOOR_SLIDE);
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
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, true);
}

FUNC(LS_Polyobj_OR_MoveTimes8)
// Polyobj_OR_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, true);
}

FUNC(LS_Door_Close)
// Door_Close (tag, speed)
{
	return EV_DoDoor (doorClose, ln, it, arg0, SPEED(arg1), 0, 0);
}

FUNC(LS_Door_Open)
// Door_Open (tag, speed)
{
	return EV_DoDoor (doorOpen, ln, it, arg0, SPEED(arg1), 0, 0);
}

FUNC(LS_Door_Raise)
// Door_Raise (tag, speed, delay)
{
	return EV_DoDoor (doorRaise, ln, it, arg0, SPEED(arg1), TICS(arg2), 0);
}

FUNC(LS_Door_LockedRaise)
// Door_LocedRaise (tag, speed, delay, lock)
{
	return EV_DoDoor (arg2 ? doorRaise : doorOpen, ln, it,
					  arg0, SPEED(arg1), TICS(arg2), arg3);
}

FUNC(LS_Door_CloseWaitOpen)
// Door_CloseWaitOpen (tag, speed, delay)
{
	return EV_DoDoor (doorCloseWaitOpen, ln, it, arg0, SPEED(arg1), OCTICS(arg2), 0);
}

FUNC(LS_Generic_Door)
// Generic_Door (tag, speed, kind, delay, lock)
{
	vldoor_e type;

	switch (arg2) {
		case 0: type = doorRaise;			break;
		case 1: type = doorOpen;			break;
		case 2: type = doorCloseWaitOpen;	break;
		case 3: type = doorClose;			break;
		default: return false;
	}
	return EV_DoDoor (type, ln, it, arg0, SPEED(arg1), OCTICS(arg3), arg4);
}

FUNC(LS_Floor_LowerByValue)
// Floor_LowerByValue (tag, speed, height)
{
	return EV_DoFloor (floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0);
}

FUNC(LS_Floor_LowerToLowest)
// Floor_LowerToLowest (tag, speed)
{
	return EV_DoFloor (floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_LowerToHighest)
// Floor_LowerToHighest (tag, speed, adjust)
{
	return EV_DoFloor (floorLowerToHighest, ln, arg0, SPEED(arg1), (arg2-128)*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_LowerToNearest)
// Floor_LowerToNearest (tag, speed)
{
	return EV_DoFloor (floorLowerToNearest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByValue)
// Floor_RaiseByValue (tag, speed, height)
{
	return EV_DoFloor (floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0);
}

FUNC(LS_Floor_RaiseToHighest)
// Floor_RaiseToHighest (tag, speed)
{
	return EV_DoFloor (floorRaiseToHighest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseToNearest)
// Floor_RaiseToNearest (tag, speed)
{
	return EV_DoFloor (floorRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseAndCrush)
// Floor_RaiseAndCrush (tag, speed, crush)
{
	return EV_DoFloor (floorRaiseAndCrush, ln, arg0, SPEED(arg1), 0, arg2, 0);
}

FUNC(LS_Floor_RaiseByValueTimes8)
// FLoor_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0);
}

FUNC(LS_Floor_LowerByValueTimes8)
// Floor_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0);
}

FUNC(LS_Floor_CrushStop)
// Floor_CrushStop (tag)
{
	return EV_FloorCrushStop (arg0);
}

FUNC(LS_Floor_LowerInstant)
// Floor_LowerInstant (tag, unused, height)
{
	return EV_DoFloor (floorLowerInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0);
}

FUNC(LS_Floor_RaiseInstant)
// Floor_RaiseInstant (tag, unused, height)
{
	return EV_DoFloor (floorRaiseInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0);
}

FUNC(LS_Floor_MoveToValueTimes8)
// Floor_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoFloor (floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*FRACUNIT*8*(arg3?-1:1), 0, 0);
}

FUNC(LS_Floor_RaiseToLowestCeiling)
// Floor_RaiseToLowestCeiling (tag, speed)
{
	return EV_DoFloor (floorRaiseToLowestCeiling, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByTexture)
// Floor_RaiseByTexture (tag, speed)
{
	return EV_DoFloor (floorRaiseByTexture, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByValueTxTy)
// Floor_RaiseByValueTxTy (tag, speed, height)
{
	return EV_DoFloor (floorRaiseAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_LowerToLowestTxTy)
// Floor_LowerToLowestTxTy (tag, speed)
{
	return EV_DoFloor (floorLowerAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_Waggle)
// Floor_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartFloorWaggle (arg0, arg1, arg2, arg3, arg4);
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
	return EV_DoDonut (arg0, SPEED(arg1), SPEED(arg2));
}

FUNC(LS_Generic_Floor)
// Generic_Floor (tag, speed, height, target, change/model/direct/crush)
{
	floor_e type;

	if (arg4 & 8) {
		switch (arg3) {
			case 1: type = floorRaiseToHighest;			break;
			case 2: type = floorRaiseToLowest;			break;
			case 3: type = floorRaiseToNearest;			break;
			case 4: type = floorRaiseToLowestCeiling;	break;
			case 5: type = floorRaiseToCeiling;			break;
			case 6: type = floorRaiseByTexture;			break;
			default:type = floorRaiseByValue;			break;
		}
	} else {
		switch (arg3) {
			case 1: type = floorLowerToHighest;			break;
			case 2: type = floorLowerToLowest;			break;
			case 3: type = floorLowerToNearest;			break;
			case 4: type = floorLowerToLowestCeiling;	break;
			case 5: type = floorLowerToCeiling;			break;
			case 6: type = floorLowerByTexture;			break;
			default:type = floorLowerByValue;			break;
		}
	}

	return EV_DoFloor (type, ln, arg0, SPEED(arg1), arg2*FRACUNIT,
					   (arg4 & 16) ? 20 : -1, arg4 & 7);
					   
}

FUNC(LS_Stairs_BuildDown)
// Stair_BuildDown (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildUp)
// Stairs_BuildUp (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildDownSync)
// Stairs_BuildDownSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpSync)
// Stairs_BuildUpSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpDoom)
// Stairs_BuildUpDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 0);
}

FUNC(LS_Generic_Stairs)
// Generic_Stairs (tag, speed, step, dir/igntxt, reset)
{
	stair_e type = (arg3 & 1) ? buildUp : buildDown;
	BOOL res = EV_BuildStairs (arg0, type, ln,
							   arg2 * FRACUNIT, SPEED(arg1), 0, arg4, arg3 & 2, 0);

	if (res && ln && (ln->flags & ML_REPEAT_SPECIAL) && ln->special == Generic_Stairs)
		// Toggle direction of next activation of repeatable stairs
		ln->args[3] ^= 1;

	return res;
}

FUNC(LS_Pillar_Build)
// Pillar_Build (tag, speed, height)
{
	return EV_DoPillar (pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, -1);
}

FUNC(LS_Pillar_BuildAndCrush)
// Pillar_BuildAndCrush (tag, speed, height, crush)
{
	return EV_DoPillar (pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, arg3);
}

FUNC(LS_Pillar_Open)
// Pillar_Open (tag, speed, f_height, c_height)
{
	return EV_DoPillar (pillarOpen, arg0, SPEED(arg1), arg2*FRACUNIT, arg3*FRACUNIT, -1);
}

FUNC(LS_Ceiling_LowerByValue)
// Ceiling_LowerByValue (tag, speed, height)
{
	return EV_DoCeiling (ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT, 0, 0, 0);
}

FUNC(LS_Ceiling_RaiseByValue)
// Ceiling_RaiseByValue (tag, speed, height)
{
	return EV_DoCeiling (ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT, 0, 0, 0);
}

FUNC(LS_Ceiling_LowerByValueTimes8)
// Ceiling_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT*8, 0, 0, 0);
}

FUNC(LS_Ceiling_RaiseByValueTimes8)
// Ceiling_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT*8, 0, 0, 0);
}

FUNC(LS_Ceiling_CrushAndRaise)
// Ceiling_CrushAndRaise (tag, speed, crush)
{
	return EV_DoCeiling (ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_LowerAndCrush)
// Ceiling_LowerAndCrush (tag, speed, crush)
{
	return EV_DoCeiling (ceilLowerAndCrush, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_CrushStop)
// Ceiling_CrushStop (tag)
{
	return EV_CeilingCrushStop (arg0);
}

FUNC(LS_Ceiling_CrushRaiseAndStay)
// Ceiling_CrushRaiseAndStay (tag, speed, crush)
{
	return EV_DoCeiling (ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_MoveToValueTimes8)
// Ceiling_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoCeiling (ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*FRACUNIT*8*((arg3) ? -1 : 1), 0, 0, 0);
}

FUNC(LS_Ceiling_LowerToHighestFloor)
// Ceiling_LowerToHighestFloor (tag, speed)
{
	return EV_DoCeiling (ceilLowerToHighestFloor, ln, arg0, SPEED(arg1), 0, 0, 0, 0, 0);
}

FUNC(LS_Ceiling_LowerInstant)
// Ceiling_LowerInstant (tag, unused, height)
{
	return EV_DoCeiling (ceilLowerInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, 0, 0, 0);
}

FUNC(LS_Ceiling_RaiseInstant)
// Ceiling_RaiseInstant (tag, unused, height)
{
	return EV_DoCeiling (ceilRaiseInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, 0, 0, 0);
}

FUNC(LS_Ceiling_CrushRaiseAndStayA)
// Ceiling_CrushRaiseAndStayA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0);
}

FUNC(LS_Ceiling_CrushRaiseAndStaySilA)
// Ceiling_CrushRaiseAndStaySilA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0);
}

FUNC(LS_Ceiling_CrushAndRaiseA)
// Ceiling_CrushAndRaiseA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0);
}

FUNC(LS_Ceiling_CrushAndRaiseSilentA)
// Ceiling_CrushAndRaiseSilentA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0);
}

FUNC(LS_Ceiling_RaiseToNearest)
// Ceiling_RaiseToNearest (tag, speed)
{
	return EV_DoCeiling (ceilRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, 0, 0, 0);
}

FUNC(LS_Ceiling_LowerToLowest)
// Ceiling_LowerToLowest (tag, speed)
{
	return EV_DoCeiling (ceilLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0, 0, 0);
}

FUNC(LS_Ceiling_LowerToFloor)
// Ceiling_LowerToFloor (tag, speed)
{
	return EV_DoCeiling (ceilLowerToFloor, ln, arg0, SPEED(arg1), 0, 0, 0, 0, 0);
}

FUNC(LS_Generic_Ceiling)
// Generic_Ceiling (tag, speed, height, target, change/model/direct/crush)
{
	ceiling_e type;

	if (arg4 & 8) {
		switch (arg3) {
			case 1:  type = ceilRaiseToHighest;			break;
			case 2:  type = ceilRaiseToLowest;			break;
			case 3:  type = ceilRaiseToNearest;			break;
			case 4:  type = ceilRaiseToHighestFloor;	break;
			case 5:  type = ceilRaiseToFloor;			break;
			case 6:  type = ceilRaiseByTexture;			break;
			default: type = ceilRaiseByValue;			break;
		}
	} else {
		switch (arg3) {
			case 1:  type = ceilLowerToHighest;			break;
			case 2:  type = ceilLowerToLowest;			break;
			case 3:  type = ceilLowerToNearest;			break;
			case 4:  type = ceilLowerToHighestFloor;	break;
			case 5:  type = ceilLowerToFloor;			break;
			case 6:  type = ceilLowerByTexture;			break;
			default: type = ceilLowerByValue;			break;
		}
	}

	return EV_DoCeiling (type, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT,
						 (arg4 & 16) ? 20 : -1, 0, arg4 & 7);
	return false;
}

FUNC(LS_Generic_Crusher)
// Generic_Crusher (tag, dnspeed, upspeed, silent, damage)
{
	return EV_DoCeiling (ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0);
}

FUNC(LS_Plat_PerpetualRaise)
// Plat_PerpetualRaise (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_PerpetualRaiseLip)
// Plat_PerpetualRaiseLip (tag, speed, delay, lip)
{
	return EV_DoPlat (arg0, ln, platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_Stop)
// Plat_Stop (tag)
{
	EV_StopPlat (arg0);
	return true;
}

FUNC(LS_Plat_DownWaitUpStay)
// Plat_DownWaitUpStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, platDownWaitUpStay, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_DownWaitUpStayLip)
// Plat_DownWaitUpStayLip (tag, speed, delay, lip)
{
	return EV_DoPlat (arg0, ln, platDownWaitUpStay, 0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_DownByValue)
// Plat_DownByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, platDownByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpByValue)
// Plat_UpByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, platUpByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpWaitDownStay)
// Plat_UpWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, platUpWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_RaiseAndStayTx0)
// Plat_RaiseAndStayTx0 (tag, speed)
{
	return EV_DoPlat (arg0, ln, platRaiseAndStay, 0, SPEED(arg1), 0, 0, 1);
}

FUNC(LS_Plat_UpByValueStayTx)
// Plat_UpByValueStayTx (tag, speed, height)
{
	return EV_DoPlat (arg0, ln, platUpByValueStay, FRACUNIT*arg2*8, SPEED(arg1), 0, 0, 2);
}

FUNC(LS_Plat_ToggleCeiling)
// Plat_ToggleCeiling (tag)
{
	return EV_DoPlat (arg0, ln, platToggle, 0, 0, 0, 0, 0);
}

FUNC(LS_Generic_Lift)
// Generic_Lift (tag, speed, delay, target, height)
{
	plattype_e type;

	switch (arg3) {
		case 1:
			type = platDownWaitUpStay;
			break;
		case 2:
			type = platDownToNearestFloor;
			break;
		case 3:
			type = platDownToLowestCeiling;
			break;
		case 4:
			type = platPerpetualRaise;
			break;
		default:
			type = platUpByValue;
			break;
	}

	return EV_DoPlat (arg0, ln, type, arg4*8*FRACUNIT, SPEED(arg1), OCTICS(arg2), 0, 0);
}

FUNC(LS_Exit_Normal)
// Exit_Normal (position)
{
	if (CheckIfExitIsGood (it)) {
		G_ExitLevel (0);
		return true;
	}
	return false;
}

FUNC(LS_Exit_Secret)
// Exit_Secret (position)
{
	if (CheckIfExitIsGood (it)) {
		G_SecretExitLevel (0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_NewMap)
// Teleport_NewMap (map, position)
{
	if (!TeleportSide) {
		level_info_t *info = FindLevelByNum (arg0);

		if (info && CheckIfExitIsGood (it)) {
			strncpy (level.nextmap, info->mapname, 8);
			G_ExitLevel (arg1);
			return true;
		}
	}
	return false;
}

FUNC(LS_Teleport)
// Teleport (tid)
{
	return EV_Teleport (arg0, TeleportSide, it);
}

FUNC(LS_Teleport_NoFog)
// Teleport_NoFog (tid)
{
	return EV_SilentTeleport (arg0, ln, TeleportSide, it);
}

FUNC(LS_Teleport_EndGame)
// Teleport_EndGame ()
{
	if (!TeleportSide) {
		if (CheckIfExitIsGood (it)) {
			strncpy (level.nextmap, "EndGameC", 8);
			G_ExitLevel (0);
			return true;
		}
	}
	return false;
}

FUNC(LS_Teleport_Line)
// Teleport_Line (thisid, destid, reversed)
{
	return EV_SilentLineTeleport (ln, TeleportSide, it, arg1, arg2);
}

FUNC(LS_ThrustThing)
// ThrustThing (angle, force)
{
	if (it) {
		angle_t angle = BYTEANGLE(arg0) >> ANGLETOFINESHIFT;

		it->momx = arg1 * finecosine[angle];
		it->momy = arg1 * finesine[angle];
		return true;
	}
	return false;
}

FUNC(LS_DamageThing)
// DamageThing (damage)
{
	if (it) {
		if (arg0)
			P_DamageMobj (it, NULL, NULL, arg0, MOD_UNKNOWN);
		else
			P_DamageMobj (it, NULL, NULL, 10000, MOD_UNKNOWN);
	}

	return it ? true : false;
}

BOOL P_GiveBody (player_t *, int);

FUNC(LS_HealThing)
// HealThing (amount)
{
	if (it) {
		if (it->player) {
			P_GiveBody (it->player, arg0);
		} else {
			it->health += arg0;
			if (mobjinfo[it->type].spawnhealth > it->health)
				it->health = mobjinfo[it->type].spawnhealth;
		}
	}

	return it ? true : false;
}

FUNC(LS_Thing_Activate)
// Thing_Activate (tid)
{
	mobj_t *mobj = P_FindMobjByTid (NULL, arg0);

	while (mobj) {
		mobj_t *temp = P_FindMobjByTid (mobj, arg0);

		P_ActivateMobj (mobj);
		mobj = temp;
	}

	return true;
}

FUNC(LS_Thing_Deactivate)
// Thing_Deactivate (tid)
{
	mobj_t *mobj = P_FindMobjByTid (NULL, arg0);

	while (mobj) {
		mobj_t *temp = P_FindMobjByTid (mobj, arg0);

		P_DeactivateMobj (mobj);
		mobj = temp;
	}

	return true;
}

FUNC(LS_Thing_Remove)
// Thing_Remove (tid)
{
	mobj_t *mobj = P_FindMobjByTid (NULL, arg0);

	while (mobj) {
		mobj_t *temp = P_FindMobjByTid (mobj, arg0);

		P_RemoveMobj (mobj);
		mobj = temp;
	}

	return true;
}

FUNC(LS_Thing_Destroy)
// Thing_Destroy (tid)
{
	mobj_t *mobj = P_FindMobjByTid (NULL, arg0);

	while (mobj) {
		mobj_t *temp = P_FindMobjByTid (mobj, arg0);

		if (mobj->flags & MF_SHOOTABLE)
			P_DamageMobj (mobj, NULL, it, mobj->health, MOD_UNKNOWN);

		mobj = temp;
	}

	return true;
}

FUNC(LS_Thing_Projectile)
// Thing_Projectile (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, arg1, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), false);
}

FUNC(LS_Thing_ProjectileGravity)
// Thing_ProjectileGravity (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, arg1, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), true);
}

FUNC(LS_Thing_Spawn)
// Thing_Spawn (tid, type, angle)
{
	return P_Thing_Spawn (arg0, arg1, BYTEANGLE(arg2), true);
}

FUNC(LS_Thing_SpawnNoFog)
// Thing_SpawnNoFog (tid, type, angle)
{
	return P_Thing_Spawn (arg0, arg1, BYTEANGLE(arg2), false);
}

FUNC(LS_Thing_SetGoal)
// Thing_SetGoal (tid, goal, delay)
{
	mobj_t *self = P_FindMobjByTid (NULL, arg0);
	mobj_t *goal = P_FindGoal (NULL, arg1, MT_PATHNODE);

	while (self) {
		if (self->flags & MF_SHOOTABLE) {
			self->goal = goal;
			if (!self->target)
				self->reactiontime = arg2 * TICRATE;
		}
		self = P_FindMobjByTid (self, arg0);
	}

	return true;
}

FUNC(LS_ACS_Execute)
// ACS_Execute (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) ) {
		return P_StartScript (it, ln, arg0, level.mapname, TeleportSide, arg2, arg3, arg4, 0);
	} else {
		return P_StartScript (it, ln, arg0, info->mapname, TeleportSide, arg2, arg3, arg4, 0);
	}
}

FUNC(LS_ACS_ExecuteAlways)
// ACS_ExecuteAlways (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) ) {
		return P_StartScript (it, ln, arg0, level.mapname, TeleportSide, arg2, arg3, arg4, 1);
	} else {
		return P_StartScript (it, ln, arg0, info->mapname, TeleportSide, arg2, arg3, arg4, 1);
	}
}

FUNC(LS_ACS_LockedExecute)
// ACS_LockedExecute (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it->player, arg4, 2))
		return false;
	else
		return LS_ACS_Execute (ln, it, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_Suspend)
// ACS_Suspend (script, map)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) ) {
		P_SuspendScript (arg0, level.mapname);
	} else {
		P_SuspendScript (arg0, info->mapname);
	}

	return true;
}

FUNC(LS_ACS_Terminate)
// ACS_Terminate (script, map)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) ) {
		P_TerminateScript (arg0, level.mapname);
	} else {
		P_TerminateScript (arg0, info->mapname);
	}

	return true;
}

FUNC(LS_FloorAndCeiling_LowerByValue)
// FloorAndCeiling_LowerByValue (tag, speed, height)
{
	return EV_DoElevator (ln, elevateLower, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_RaiseByValue)
// FloorAndCeiling_RaiseByValue (tag, speed, height)
{
	return EV_DoElevator (ln, elevateRaise, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_LowerRaise)
// FloorAndCeiling_LowerRaise (tag, fspeed, cspeed)
{
	return EV_DoCeiling (ceilRaiseToHighest, ln, arg0, SPEED(arg2), 0, 0, 0, 0, 0) ||
		   EV_DoFloor (floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Elevator_MoveToFloor)
// Elevator_MoveToFloor (tag, speed)
{
	return EV_DoElevator (ln, elevateCurrent, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_RaiseToNearest)
// Elevator_RaiseToNearest (tag, speed)
{
	return EV_DoElevator (ln, elevateUp, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_LowerToNearest)
// Elevator_LowerToNearest (tag, speed)
{
	return EV_DoElevator (ln, elevateDown, SPEED(arg1), 0, arg0);
}

FUNC(LS_Light_ForceLightning)
// Light_ForceLightning (tag)
{
	return false;
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
	EV_StartLightStrobing (arg0, -1, -1, TICS(arg1), TICS(arg2));
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

FUNC(LS_Radius_Quake)
// Radius_Quake (intensity, duration, damrad, tremrad, tid)
{
	return P_StartQuake (arg4, arg0, arg1, arg2, arg3);
}

FUNC(LS_UsePuzzleItem)
// UsePuzzleItem (item, script)
{
	return false;
}

FUNC(LS_Sector_ChangeSound)
// Sector_ChangeSound (tag, sound)
{
	int secNum;
	BOOL rtn;

	if (!arg0)
		return false;

	secNum = -1;
	rtn = false;
	while ((secNum = P_FindSectorFromTag (arg0,	secNum)) >= 0)
	{
		sectors[secNum].seqType = arg1;
		rtn = true;
	}
	return rtn;
}

FUNC(LS_Sector_SetWind)
// Sector_SetWind ()
{
	return false;
}

FUNC(LS_Sector_SetFriction)
// Sector_SetFriction ()
{
	return false;
}

FUNC(LS_Sector_SetCurrent)
// Sector_SetCurrent ()
{
	return false;
}

FUNC(LS_Scroll_Texture_Both)
{
	return false;
}

FUNC(LS_Scroll_Floor)
{
	return false;
}

FUNC(LS_Scroll_Ceiling)
{
	return false;
}

FUNC(LS_PointPush_SetForce)
// PointPush_SetForce ()
{
	return false;
}

FUNC(LS_Sector_SetDamage)
// Sector_SetDamage (tag, amount, mod)
{
	int secnum = -1;
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0) {
		sectors[secnum].damage = arg1;
		sectors[secnum].mod = arg2;
	}
	return true;
}

FUNC(LS_Sector_SetGravity)
// Sector_SetGravity (tag, intpart, fracpart)
{
	int secnum = -1;
	float gravity;

	if (arg2 > 99)
		arg2 = 99;
	gravity = (float)arg1 + (float)arg2 * 0.01f;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
		sectors[secnum].gravity = gravity;

	return true;
}

FUNC(LS_Sector_SetColor)
// Sector_SetColor (tag, r, g, b)
{
	int secnum = -1;
	
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].floorcolormap = GetSpecialLights (arg1, arg2, arg3,
			RPART(sectors[secnum].floorcolormap->fade),
			GPART(sectors[secnum].floorcolormap->fade),
			BPART(sectors[secnum].floorcolormap->fade));
		sectors[secnum].ceilingcolormap = GetSpecialLights (arg1, arg2, arg3,
			RPART(sectors[secnum].ceilingcolormap->fade),
			GPART(sectors[secnum].ceilingcolormap->fade),
			BPART(sectors[secnum].ceilingcolormap->fade));
	}

	return true;
}

FUNC(LS_Sector_SetFade)
// Sector_SetFade (tag, r, g, b)
{
	int secnum = -1;
	
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].floorcolormap = GetSpecialLights (
			RPART(sectors[secnum].floorcolormap->color),
			GPART(sectors[secnum].floorcolormap->color),
			BPART(sectors[secnum].floorcolormap->color),
			arg1, arg2, arg3);
		sectors[secnum].ceilingcolormap = GetSpecialLights (
			RPART(sectors[secnum].ceilingcolormap->color),
			GPART(sectors[secnum].ceilingcolormap->color),
			BPART(sectors[secnum].ceilingcolormap->color),
			arg1, arg2, arg3);
	}
	return true;
}

FUNC(LS_ChangeCamera)
// ChangeCamera (tid, who, revert?)
{
	mobj_t *camera = P_FindGoal (NULL, arg0, MT_CAMERA);

	if (!it || !it->player || arg1) {
		int i;

		for (i = 0; i < MAXPLAYERS; i++) {
			if (!playeringame[i])
				continue;

			if (camera) {
				players[i].camera = camera;
				if (arg2)
					players[i].cheats |= CF_REVERTPLEASE;
			} else {
				players[i].camera = players[i].mo;
				players[i].cheats &= ~CF_REVERTPLEASE;
			}
		}
	} else {
		if (camera) {
			it->player->camera = camera;
			if (arg2)
				it->player->cheats |= CF_REVERTPLEASE;
		} else {
			it->player->camera = it;
			it->player->cheats &= ~CF_REVERTPLEASE;
		}
	}

	return true;
}

FUNC(LS_SetPlayerProperty)
// SetPlayerProperty (who, set, which)
{
#define PROP_FROZEN		0
#define PROP_NOTARGET	1

	int mask = 0;

	if (!it->player && !arg0)
		return false;

	switch (arg2)
	{
		case PROP_FROZEN:
			mask = CF_FROZEN;
			break;
		case PROP_NOTARGET:
			mask = CF_NOTARGET;
			break;
	}

	if (arg1 == 0)
	{
		if (arg1)
			it->player->cheats |= mask;
		else
			it->player->cheats &= ~mask;
	}
	else
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			if (arg1)
				players[i].cheats |= mask;
			else
				players[i].cheats &= ~mask;
		}
	}

	return !!mask;
}

lnSpecFunc LineSpecials[256] =
{
	LS_NOP,
	LS_NOP,		// Polyobj_StartLine,
	LS_Polyobj_RotateLeft,
	LS_Polyobj_RotateRight,
	LS_Polyobj_Move,
	LS_NOP,		// Polyobj_ExplicitLine
	LS_Polyobj_MoveTimes8,
	LS_Polyobj_DoorSwing,
	LS_Polyobj_DoorSlide,
	LS_NOP,		// 9
	LS_Door_Close,
	LS_Door_Open,
	LS_Door_Raise,
	LS_Door_LockedRaise,
	LS_NOP,		// 14
	LS_NOP,		// 15
	LS_NOP,		// 16
	LS_NOP,		// 17
	LS_NOP,		// 18
	LS_NOP,		// 19
	LS_Floor_LowerByValue,
	LS_Floor_LowerToLowest,
	LS_Floor_LowerToNearest,
	LS_Floor_RaiseByValue,
	LS_Floor_RaiseToHighest,
	LS_Floor_RaiseToNearest,
	LS_Stairs_BuildDown,
	LS_Stairs_BuildUp,
	LS_Floor_RaiseAndCrush,
	LS_Pillar_Build,
	LS_Pillar_Open,
	LS_Stairs_BuildDownSync,
	LS_Stairs_BuildUpSync,
	LS_NOP,		// 33
	LS_NOP,		// 34
	LS_Floor_RaiseByValueTimes8,
	LS_Floor_LowerByValueTimes8,
	LS_NOP,		// 37
	LS_NOP,		// 38
	LS_NOP,		// 39
	LS_Ceiling_LowerByValue,
	LS_Ceiling_RaiseByValue,
	LS_Ceiling_CrushAndRaise,
	LS_Ceiling_LowerAndCrush,
	LS_Ceiling_CrushStop,
	LS_Ceiling_CrushRaiseAndStay,
	LS_Floor_CrushStop,
	LS_NOP,		// 47
	LS_NOP,		// 48
	LS_NOP,		// 49
	LS_NOP,		// 50
	LS_NOP,		// 51
	LS_NOP,		// 52
	LS_NOP,		// 53
	LS_NOP,		// 54
	LS_NOP,		// 55
	LS_NOP,		// 56
	LS_NOP,		// 57
	LS_NOP,		// 58
	LS_NOP,		// 59
	LS_Plat_PerpetualRaise,
	LS_Plat_Stop,
	LS_Plat_DownWaitUpStay,
	LS_Plat_DownByValue,
	LS_Plat_UpWaitDownStay,
	LS_Plat_UpByValue,
	LS_Floor_LowerInstant,
	LS_Floor_RaiseInstant,
	LS_Floor_MoveToValueTimes8,
	LS_Ceiling_MoveToValueTimes8,
	LS_Teleport,
	LS_Teleport_NoFog,
	LS_ThrustThing,
	LS_DamageThing,
	LS_Teleport_NewMap,
	LS_Teleport_EndGame,
	LS_NOP,		// 76
	LS_NOP,		// 77
	LS_NOP,		// 78
	LS_NOP,		// 79
	LS_ACS_Execute,
	LS_ACS_Suspend,
	LS_ACS_Terminate,
	LS_ACS_LockedExecute,
	LS_NOP,		// 84
	LS_NOP,		// 85
	LS_NOP,		// 86
	LS_NOP,		// 87
	LS_NOP,		// 88
	LS_NOP,		// 89
	LS_Polyobj_OR_RotateLeft,
	LS_Polyobj_OR_RotateRight,
	LS_Polyobj_OR_Move,
	LS_Polyobj_OR_MoveTimes8,
	LS_Pillar_BuildAndCrush,
	LS_FloorAndCeiling_LowerByValue,
	LS_FloorAndCeiling_RaiseByValue,
	LS_NOP,		// 97
	LS_NOP,		// 98
	LS_NOP,		// 99
	LS_NOP,		// Scroll_Texture_Left
	LS_NOP,		// Scroll_Texture_Right
	LS_NOP,		// Scroll_Texture_Up
	LS_NOP,		// Scroll_Texture_Down
	LS_NOP,		// 104
	LS_NOP,		// 105
	LS_NOP,		// 106
	LS_NOP,		// 107
	LS_NOP,		// 108
	LS_Light_ForceLightning,
	LS_Light_RaiseByValue,
	LS_Light_LowerByValue,
	LS_Light_ChangeToValue,
	LS_Light_Fade,
	LS_Light_Glow,
	LS_Light_Flicker,
	LS_Light_Strobe,
	LS_NOP,		// 117
	LS_NOP,		// 118
	LS_NOP,		// 119
	LS_Radius_Quake,
	LS_NOP,		// Line_SetIdentification
	LS_NOP,		// 122
	LS_NOP,		// 123
	LS_NOP,		// 124
	LS_NOP,		// 125
	LS_NOP,		// 126
	LS_NOP,		// 127
	LS_NOP,		// 128
	LS_UsePuzzleItem,
	LS_Thing_Activate,
	LS_Thing_Deactivate,
	LS_Thing_Remove,
	LS_Thing_Destroy,
	LS_Thing_Projectile,
	LS_Thing_Spawn,
	LS_Thing_ProjectileGravity,
	LS_Thing_SpawnNoFog,
	LS_Floor_Waggle,
	LS_NOP,		// 139
	LS_Sector_ChangeSound,
	LS_NOP,		// 141
	LS_NOP,		// 142
	LS_NOP,		// 143
	LS_NOP,		// 144
	LS_NOP,		// 145
	LS_NOP,		// 146
	LS_NOP,		// 147
	LS_NOP,		// 148
	LS_NOP,		// 149
	LS_NOP,		// 150
	LS_NOP,		// 151
	LS_NOP,		// 152
	LS_NOP,		// 153
	LS_NOP,		// 154
	LS_NOP,		// 155
	LS_NOP,		// 156
	LS_NOP,		// 157
	LS_NOP,		// 158
	LS_NOP,		// 159
	LS_NOP,		// 160
	LS_NOP,		// 161
	LS_NOP,		// 162
	LS_NOP,		// 163
	LS_NOP,		// 164
	LS_NOP,		// 165
	LS_NOP,		// 166
	LS_NOP,		// 167
	LS_NOP,		// 168
	LS_NOP,		// 169
	LS_NOP,		// 170
	LS_NOP,		// 171
	LS_NOP,		// 172
	LS_NOP,		// 173
	LS_NOP,		// 174
	LS_NOP,		// 175
	LS_NOP,		// 176
	LS_NOP,		// 177
	LS_NOP,		// 178
	LS_NOP,		// 179
	LS_NOP,		// 180
	LS_NOP,		// 181
	LS_NOP,		// 182
	LS_NOP,		// 183
	LS_NOP,		// 184
	LS_NOP,		// 185
	LS_NOP,		// 186
	LS_NOP,		// 187
	LS_NOP,		// 188
	LS_NOP,		// 189
	LS_NOP,		// Static_Init
	LS_SetPlayerProperty,
	LS_Ceiling_LowerToHighestFloor,
	LS_Ceiling_LowerInstant,
	LS_Ceiling_RaiseInstant,
	LS_Ceiling_CrushRaiseAndStayA,
	LS_Ceiling_CrushAndRaiseA,
	LS_Ceiling_CrushAndRaiseSilentA,
	LS_Ceiling_RaiseByValueTimes8,
	LS_Ceiling_LowerByValueTimes8,
	LS_Generic_Floor,
	LS_Generic_Ceiling,
	LS_Generic_Door,
	LS_Generic_Lift,
	LS_Generic_Stairs,
	LS_Generic_Crusher,
	LS_Plat_DownWaitUpStayLip,
	LS_Plat_PerpetualRaiseLip,
	LS_NOP,		// TranslucentLine
	LS_NOP,		// Transfer_Heights
	LS_NOP,		// Transfer_FloorLight
	LS_NOP,		// Transfer_CeilingLight
	LS_Sector_SetColor,
	LS_Sector_SetFade,
	LS_Sector_SetDamage,
	LS_Teleport_Line,
	LS_Sector_SetGravity,
	LS_Stairs_BuildUpDoom,
	LS_Sector_SetWind,
	LS_Sector_SetFriction,
	LS_Sector_SetCurrent,
	LS_Scroll_Texture_Both,
	LS_NOP,		// Scroll_Texture_Model
	LS_Scroll_Floor,
	LS_Scroll_Ceiling,
	LS_NOP,		// Scroll_Texture_Offsets
	LS_ACS_ExecuteAlways,
	LS_PointPush_SetForce,
	LS_Plat_RaiseAndStayTx0,
	LS_Thing_SetGoal,
	LS_Plat_UpByValueStayTx,
	LS_Plat_ToggleCeiling,
	LS_Light_StrobeDoom,
	LS_Light_MinNeighbor,
	LS_Light_MaxNeighbor,
	LS_Floor_TransferTrigger,
	LS_Floor_TransferNumeric,
	LS_ChangeCamera,
	LS_Floor_RaiseToLowestCeiling,
	LS_Floor_RaiseByValueTxTy,
	LS_Floor_RaiseByTexture,
	LS_Floor_LowerToLowestTxTy,
	LS_Floor_LowerToHighest,
	LS_Exit_Normal,
	LS_Exit_Secret,
	LS_Elevator_RaiseToNearest,
	LS_Elevator_MoveToFloor,
	LS_Elevator_LowerToNearest,
	LS_HealThing,
	LS_Door_CloseWaitOpen,
	LS_Floor_Donut,
	LS_FloorAndCeiling_LowerRaise,
	LS_Ceiling_RaiseToNearest,
	LS_Ceiling_LowerToLowest,
	LS_Ceiling_LowerToFloor,
	LS_Ceiling_CrushRaiseAndStaySilA
};
