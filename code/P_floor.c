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
// $Log:$
//
// DESCRIPTION:
//		Floor animation: raising stairs.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "tables.h"

extern fixed_t FloatBobOffsets[64];

//
// FLOORS
//

//
// Move a plane (floor or ceiling) and check for crushing
// [RH] Made crush specify the actual amount of crushing damage inflictable.
//		(Use -1 to prevent it from trying to crush)
//
result_e
T_MovePlane
( sector_t* 	sector,
  fixed_t		speed,
  fixed_t		dest,
  int			crush,
  int			floorOrCeiling,
  int			direction )
{
	BOOL	 	flag;
	fixed_t 	lastpos;
	fixed_t		destheight;	//jff 02/04/98 used to keep floors/ceilings
							// from moving thru each other

	switch (floorOrCeiling)
	{
	  case 0:
		// FLOOR
		switch (direction)
		{
		  case -1:
			// DOWN
			if (sector->floorheight - speed < dest)
			{
				lastpos = sector->floorheight;
				sector->floorheight = dest;
				flag = P_ChangeSector (sector, crush);
				if (flag == true)
				{
					sector->floorheight = lastpos;
					P_ChangeSector (sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = sector->floorheight;
				sector->floorheight -= speed;
				flag = P_ChangeSector (sector, crush);
				if (flag == true)
				{
					sector->floorheight = lastpos;
					P_ChangeSector (sector, crush);
					return crushed;
				}
			}
			break;
												
		  case 1:
			// UP
			// jff 02/04/98 keep floor from moving thru ceilings
			destheight = (dest < sector->ceilingheight) ? dest : sector->ceilingheight;
			if (sector->floorheight + speed > destheight)
			{
				lastpos = sector->floorheight;
				sector->floorheight = destheight;
				flag = P_ChangeSector (sector, crush);
				if (flag == true)
				{
					sector->floorheight = lastpos;
					P_ChangeSector (sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				lastpos = sector->floorheight;
				sector->floorheight += speed;
				flag = P_ChangeSector (sector, crush);
				if (flag == true)
				{
					if (crush == true)
						return crushed;
					sector->floorheight = lastpos;
					P_ChangeSector (sector, crush);
					return crushed;
				}
			}
			break;
		}
		break;
																		
	  case 1:
		// CEILING
		switch(direction)
		{
		  case -1:
			// DOWN
			// jff 02/04/98 keep ceiling from moving thru floors
			destheight = (dest > sector->floorheight) ? dest : sector->floorheight;
			if (sector->ceilingheight - speed < destheight)
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight = destheight;
				flag = P_ChangeSector (sector, crush);

				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector (sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				lastpos = sector->ceilingheight;
				sector->ceilingheight -= speed;
				flag = P_ChangeSector (sector, crush);

				if (flag == true)
				{
					if (crush == true)
						return crushed;
					sector->ceilingheight = lastpos;
					P_ChangeSector (sector, crush);
					return crushed;
				}
			}
			break;
												
		  case 1:
			// UP
			if (sector->ceilingheight + speed > dest)
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight = dest;
				flag = P_ChangeSector (sector,crush);
				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector (sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight += speed;
				flag = P_ChangeSector (sector, crush);
// UNUSED
#if 0
				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector (sector, crush);
					return crushed;
				}
#endif
			}
			break;
		}
		break;
				
	}
	return ok;
}


//
// MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
//
void T_MoveFloor (floormove_t *floor)
{
	result_e res;

	// [RH] Handle reseting stairs
	if (floor->type == buildStair || floor->type == waitStair) {
		if (floor->resetcount) {
			if (--floor->resetcount == 0) {
				floor->type = resetStair;
				floor->direction = (floor->direction > 0) ? -1 : 1;
				floor->floordestheight = floor->orgheight;
			}
		}
		if (floor->pausetime) {
			floor->pausetime--;
			return;
		} else if (floor->steptime) {
			if (--floor->steptime == 0) {
				floor->pausetime = floor->delay;
				floor->steptime = floor->persteptime;
			}
		}
	}

	if (floor->type == waitStair)
		return;

	res = T_MovePlane(floor->sector,
					  floor->speed,
					  floor->floordestheight,
					  floor->crush,0,floor->direction);
	
	if (res == pastdest)
	{
		SN_StopSequence ((mobj_t *)&floor->sector->soundorg);

		if (floor->type == buildStair)
			floor->type = waitStair;

		if (floor->type != waitStair || floor->resetcount == 0)
		{
			if (floor->direction == 1)
			{
				switch(floor->type)
				{
				  case donutRaise:
				  case genFloorChgT:
				  case genFloorChg0:
					floor->sector->special = floor->newspecial;
					//fall thru
				  case genFloorChg:
					floor->sector->floorpic = floor->texture;
					break;
				  default:
					break;
				}
			}
			else if (floor->direction == -1)
			{
				switch(floor->type)
				{
				  case floorLowerAndChange:
				  case genFloorChgT:
				  case genFloorChg0:
					floor->sector->special = floor->newspecial;
					//fall thru
				  case genFloorChg:
					floor->sector->floorpic = floor->texture;
					break;
				  default:
					break;
				}
			}

			floor->sector->floordata = NULL; //jff 2/22/98
			P_RemoveThinker (&floor->thinker);

			//jff 2/26/98 implement stair retrigger lockout while still building
			// note this only applies to the retriggerable generalized stairs

			if (floor->sector->stairlock == -2)		// if this sector is stairlocked
			{
				sector_t *sec = floor->sector;
				sec->stairlock = -1;				// thinker done, promote lock to -1

				while (sec->prevsec != -1 && sectors[sec->prevsec].stairlock != -2)
					sec = &sectors[sec->prevsec];	// search for a non-done thinker
				if (sec->prevsec == -1)				// if all thinkers previous are done
				{
					sec = floor->sector;			// search forward
					while (sec->nextsec != -1 && sectors[sec->nextsec].stairlock != -2) 
						sec = &sectors[sec->nextsec];
					if (sec->nextsec == -1)			// if all thinkers ahead are done too
					{
						while (sec->prevsec != -1)	// clear all locks
						{
							sec->stairlock = 0;
							sec = &sectors[sec->prevsec];
						}
						sec->stairlock = 0;
					}
				}
			}
		}
	}
}

//
// T_MoveElevator()
//
// Move an elevator to it's destination (up or down)
// Called once per tick for each moving floor.
//
// Passed an elevator_t structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return.
//
// jff 02/22/98 added to support parallel floor/ceiling motion
//
void T_MoveElevator (elevator_t* elevator)
{
	result_e res;

	if (elevator->direction<0)		// moving down
	{
		res = T_MovePlane			//jff 4/7/98 reverse order of ceiling/floor
		(
			elevator->sector,
			elevator->speed,
			elevator->ceilingdestheight,
			-1,
			1,						// move floor
			elevator->direction
		);
		if (res==ok || res==pastdest) // jff 4/7/98 don't move ceil if blocked
			T_MovePlane
			(
				elevator->sector,
				elevator->speed,
				elevator->floordestheight,
				-1,
				0,					// move ceiling
				elevator->direction
			);
	}
	else // up
	{
		res = T_MovePlane			//jff 4/7/98 reverse order of ceiling/floor
		(
			elevator->sector,
			elevator->speed,
			elevator->floordestheight,
			-1,
			0,						 // move ceiling
			elevator->direction
		);
		if (res==ok || res==pastdest) // jff 4/7/98 don't move floor if blocked
			T_MovePlane
			(
				elevator->sector,
				elevator->speed,
				elevator->ceilingdestheight,
				-1,
				1,					// move floor
				elevator->direction
			);
	}

	if (res == pastdest)			// if destination height acheived
	{
		// make floor stop sound
		SN_StopSequence ((mobj_t *)&elevator->sector->soundorg);

		elevator->sector->floordata = NULL;		//jff 2/22/98
		elevator->sector->ceilingdata = NULL;	//jff 2/22/98
		P_RemoveThinker(&elevator->thinker);	// remove elevator from actives
	}
}

static void StartFloorSound (sector_t *sec)
{
	if (sec->seqType >= 0)
	{
		SN_StartSequence ((mobj_t *)&sec->soundorg, sec->seqType, SEQ_PLATFORM);
	}
	else
	{
		SN_StartSequenceName ((mobj_t *)&sec->soundorg, "Floor");
	}
}

//
// HANDLE FLOOR TYPES
// [RH] Added tag, speed, height, crush, change params.
//
BOOL EV_DoFloor (floor_e floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change)
{
	int 				secnum;
	BOOL 				rtn;
	sector_t*			sec;
	floormove_t*		floor;
	BOOL				manual = false;

	rtn = false;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = sec-sectors;
		manual = true;
		goto manual_floor;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_floor:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (sec->floordata)
			continue;
		
		// new floor thinker
		rtn = true;
		floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
		P_AddThinker (&floor->thinker);
		sec->floordata = floor;		//jff 2/22/98
		floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
		floor->type = floortype;
		floor->crush = -1;
		floor->speed = speed;
		floor->sector = sec;
		floor->resetcount = 0;					// [RH]
		floor->orgheight = sec->floorheight;	// [RH]

		StartFloorSound (sec);

		switch(floortype)
		{
		  case floorLowerToHighest:
			floor->direction = -1;
			floor->floordestheight = P_FindHighestFloorSurrounding(sec);
			// [RH] DOOM's turboLower type did this. I've just extended it
			//		to be applicable to all LowerToHighest types.
			if (floor->floordestheight != sec->floorheight)
				floor->floordestheight += height;
			break;

		  case floorLowerToLowest:
			floor->direction = -1;
			floor->floordestheight = P_FindLowestFloorSurrounding(sec);
			break;

		  case floorLowerToNearest:
			//jff 02/03/30 support lowering floor to next lowest floor
			floor->direction = -1;
			floor->floordestheight =
				P_FindNextLowestFloor (sec, sec->floorheight);
			break;

		  case floorLowerInstant:
			floor->speed = height;
		  case floorLowerByValue:
			floor->direction = -1;
			floor->sector = sec;
			floor->floordestheight = sec->floorheight - height;
			break;

		  case floorRaiseInstant:
			floor->speed = height;
		  case floorRaiseByValue:
			floor->direction = 1;
			floor->floordestheight = sec->floorheight + height;
			break;

		  case floorMoveToValue:
			floor->direction = (height - sec->floorheight) > 0 ? 1 : -1;
			floor->floordestheight = height;
			break;

		  case floorRaiseAndCrush:
			floor->crush = crush;
		  case floorRaiseToLowestCeiling:
			floor->direction = 1;
			floor->floordestheight = 
				P_FindLowestCeilingSurrounding(sec);
			if (floor->floordestheight > sec->ceilingheight)
				floor->floordestheight = sec->ceilingheight;
			if (floortype == floorRaiseAndCrush)
				floor->floordestheight -= 8 * FRACUNIT;
			break;

		  case floorRaiseToHighest:
			floor->direction = 1;
			floor->floordestheight = P_FindHighestFloorSurrounding(sec);
			break;

		  case floorRaiseToNearest:
			floor->direction = 1;
			floor->floordestheight = P_FindNextHighestFloor(sec,sec->floorheight);
			break;

		  case floorRaiseToLowest:
			floor->direction = 1;
			floor->floordestheight = P_FindLowestFloorSurrounding(sec);
			break;

		  case floorRaiseToCeiling:
			floor->direction = 1;
			floor->floordestheight = sec->ceilingheight;
			break;

		  case floorLowerToLowestCeiling:
			floor->direction = -1;
			floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
			break;

		  case floorLowerByTexture:
			floor->direction = -1;
			floor->floordestheight = sec->floorheight -
				P_FindShortestTextureAround (secnum);
			break;

		  case floorLowerToCeiling:
			// [RH] Essentially instantly raises the floor to the ceiling
			floor->direction = -1;
			floor->floordestheight = sec->ceilingheight;
			break;

		  case floorRaiseByTexture:
			floor->direction = 1;
			// [RH] Use P_FindShortestTextureAround from BOOM to do this
			//		since the code is identical to what was here. (Oddly
			//		enough, BOOM preserved the code here even though it
			//		also had this function.)
			floor->floordestheight = sec->floorheight +
				P_FindShortestTextureAround (secnum);
			break;

		  case floorRaiseAndChange:
			floor->direction = 1;
			floor->floordestheight = sec->floorheight +	height;
			sec->floorpic = line->frontsector->floorpic;
			sec->special = line->frontsector->special;
			break;
		  
		  case floorLowerAndChange:
			floor->direction = -1;
			floor->floordestheight = 
				P_FindLowestFloorSurrounding (sec);
			floor->texture = sec->floorpic;
			// jff 1/24/98 make sure floor->newspecial gets initialized
			// in case no surrounding sector is at floordestheight
			// --> should not affect compatibility <--
			floor->newspecial = sec->special; 

			//jff 5/23/98 use model subroutine to unify fixes and handling
			sec = P_FindModelFloorSector (floor->floordestheight,sec-sectors);
			if (sec)
			{
				floor->texture = sec->floorpic;
				floor->newspecial = sec->special;
			}
			break;

		  default:
			break;
		}

		if (change & 3) {
			// [RH] Need to do some transferring
			if (change & 4) {
				// Numeric model change
				sector_t *sec;

				sec = (floortype == floorRaiseToLowestCeiling ||
					   floortype == floorLowerToLowestCeiling ||
					   floortype == floorRaiseToCeiling ||
					   floortype == floorLowerToCeiling) ?
					  P_FindModelCeilingSector (floor->floordestheight, secnum) :
					  P_FindModelFloorSector (floor->floordestheight, secnum);

				if (sec) {
					floor->texture = sec->floorpic;
					switch (change & 3) {
						case 1:
							floor->newspecial = 0;
							floor->type = genFloorChg0;
							break;
						case 2:
							floor->newspecial = sec->special;
							floor->type = genFloorChgT;
							break;
						case 3:
							floor->type = genFloorChg;
							break;
					}
				}
			} else if (line) {
				// Trigger model change
				floor->texture = line->frontsector->floorpic;

				switch (change & 3) {
					case 1:
						floor->newspecial = 0;
						floor->type = genFloorChg0;
						break;
					case 2:
						floor->newspecial = sec->special;
						floor->type = genFloorChgT;
						break;
					case 3:
						floor->type = genFloorChg;
						break;
				}
			}
		}
		if (manual)
			return rtn;
	}
	return rtn;
}

// [RH]
// EV_FloorCrushStop
// Stop a floor from crushing!
//
BOOL EV_FloorCrushStop (int tag)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = sectors + secnum;

		if (sec->floordata &&
			((thinker_t *)(sec->floordata))->function.acp1 == (actionf_p1) T_MoveFloor &&
			((floormove_t *)(sec->floordata))->type == floorRaiseAndCrush) {
			SN_StopSequence ((mobj_t *)&sec->soundorg);
			P_RemoveThinker ((thinker_t *)(sec->floordata));
			sec->floordata = NULL;
		}
	}
	return true;
}

//
// EV_DoChange()
//
// Handle pure change types. These change floor texture and sector type
// by trigger or numeric model without moving the floor.
//
// The linedef causing the change and the type of change is passed
// Returns true if any sector changes
//
// jff 3/15/98 added to better support generalized sector types
// [RH] Added tag parameter.
//
BOOL EV_DoChange (line_t *line, change_e changetype, int tag)
{
	int			secnum;
	BOOL		rtn;
	sector_t	*sec;
	sector_t	*secm;

	secnum = -1;
	rtn = false;
	// change all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
              
		rtn = true;

		// handle trigger or numeric change type
		switch(changetype)
		{
			case trigChangeOnly:
				if (line) { // [RH] if no line, no change
					sec->floorpic = line->frontsector->floorpic;
					sec->special = line->frontsector->special;
				}
				break;
			case numChangeOnly:
				secm = P_FindModelFloorSector (sec->floorheight,secnum);
				if (secm) // if no model, no change
				{
					sec->floorpic = secm->floorpic;
					sec->special = secm->special;
				}
				break;
			default:
				break;
		}
	}
	return rtn;
}


//
// BUILD A STAIRCASE!
// [RH] Added stairsize, srcspeed, delay, reset, igntxt, usespecials parameters
//		If usespecials is non-zero, then each sector in a stair is determined
//		by its special. If usespecials is 2, each sector stays in "sync" with
//		the others.
//
BOOL EV_BuildStairs (int tag, stair_e type, line_t *line,
					 fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
					 int usespecials)
{
	int 				secnum;
	int					osecnum;	//jff 3/4/98 save old loop index
	int 				height;
	int 				i;
	int 				newsecnum;
	int 				texture;
	int 				ok;
	int					persteptime;
	BOOL 				rtn = false;
	
	sector_t*			sec;
	sector_t*			tsec;
	sector_t*			prev = NULL;

	floormove_t*		floor;
	BOOL				manual = false;

	if (speed == 0)
		return false;

	persteptime = FixedDiv (stairsize, speed) >> FRACBITS;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = sec-sectors;
		manual = true;
		goto manual_stair;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_stair:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		//jff 2/26/98 add special lockout condition to wait for entire
		//staircase to build before retriggering
		if (sec->floordata || sec->stairlock) {
			if (!manual)
				continue;
			else
				return rtn;
		}
		
		// new floor thinker
		rtn = true;
		floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
		P_AddThinker (&floor->thinker);
		sec->floordata = floor;		//jff 2/22/98
		floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
		floor->direction = (type == buildUp) ? 1 : -1;
		floor->sector = sec;
		floor->type = buildStair;	//jff 3/31/98 do not leave uninited
		floor->resetcount = reset;	// [RH] Tics until reset (0 if never)
		floor->orgheight = sec->floorheight;	// [RH] Height to reset to
		// [RH] Set up delay values
		floor->delay = delay;
		floor->pausetime = 0;
		floor->steptime = floor->persteptime = persteptime;

		floor->crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1; //jff 2/27/98 fix uninitialized crush field

		floor->speed = speed;
		height = sec->floorheight + stairsize * floor->direction;
		floor->floordestheight = height;

		texture = sec->floorpic;
		osecnum = secnum;				//jff 3/4/98 preserve loop index
		
		// Find next sector to raise
		// 1.	Find 2-sided line with same sector side[0] (lowest numbered)
		// 2.	Other side is the next sector to raise
		// 3.	Unless already moving, or different texture, then stop building
		do
		{
			ok = 0;

			if (usespecials) {
				// [RH] Find the next sector by scanning for Stairs_Special?
				tsec = P_NextSpecialSector (sec,
						(sec->special & 0xff) == Stairs_Special1 ?
							Stairs_Special2 : Stairs_Special1, prev);

				if ( (ok = (tsec != NULL)) ) {
					height += floor->direction * stairsize;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->floordata || tsec->stairlock) {
						prev = sec;
						sec = tsec;
						continue;
					}
				}
				newsecnum = tsec - sectors;
			} else {
				for (i = 0; i < sec->linecount; i++)
				{
					if ( !((sec->lines[i])->flags & ML_TWOSIDED) )
						continue;

					tsec = (sec->lines[i])->frontsector;
					if (!tsec) continue;	//jff 5/7/98 if no backside, continue
					newsecnum = tsec-sectors;

					if (secnum != newsecnum)
						continue;

					tsec = (sec->lines[i])->backsector;
					newsecnum = tsec - sectors;

					if (!igntxt && tsec->floorpic != texture)
						continue;

					height += floor->direction * stairsize;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->floordata || tsec->stairlock)
						continue;

					ok = true;
					break;
				}
			}

			if (ok) {
				// jff 2/26/98
				// link the stair chain in both directions
				// lock the stair sector until building complete
				sec->nextsec = newsecnum; // link step to next
				tsec->prevsec = secnum;   // link next back
				tsec->nextsec = -1;       // set next forward link as end
				tsec->stairlock = -2;     // lock the step

				prev = sec;
				sec = tsec;
				secnum = newsecnum;

				// create and initialize a thinker for the next step
				floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
				P_AddThinker (&floor->thinker);

				StartFloorSound (sec);

				sec->floordata = floor;	//jff 2/22/98
				floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
				floor->direction = (type == buildUp) ? 1 : -1;
				floor->sector = sec;
				floor->floordestheight = height;
				// [RH] Set up delay values
				floor->delay = delay;
				floor->pausetime = 0;
				floor->steptime = floor->persteptime = persteptime;

				if (usespecials == 2) {
					// [RH]
					fixed_t rise = (floor->floordestheight - sec->floorheight)
									* floor->direction;
					floor->speed = FixedDiv (FixedMul (speed, rise), stairsize);
				} else {
					floor->speed = speed;
				}
				floor->type = buildStair;	//jff 3/31/98 do not leave uninited
				//jff 2/27/98 fix uninitialized crush field
				floor->crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1;
				floor->resetcount = reset;	// [RH] Tics until reset (0 if never)
				floor->orgheight = sec->floorheight;	// [RH] Height to reset to
			}
		} while(ok);
		if (manual)
			return rtn;
		secnum = osecnum;	//jff 3/4/98 restore loop index
	}
	return rtn;
}

// [RH] Added pillarspeed and slimespeed parameters
int EV_DoDonut (int tag, fixed_t pillarspeed, fixed_t slimespeed)
{
	sector_t*			s1;
	sector_t*			s2;
	sector_t*			s3;
	int 				secnum;
	int 				rtn;
	int 				i;
	floormove_t*		floor;
		
	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromTag(tag,secnum)) >= 0)
	{
		s1 = &sectors[secnum];					// s1 is pillar's sector
				
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (s1->floordata)
			continue;
						
		rtn = 1;
		s2 = getNextSector (s1->lines[0],s1);	// s2 is pool's sector
		if (!s2)								// note lowest numbered line around
			continue;							// pillar must be two-sided

		for (i = 0; i < s2->linecount; i++)
		{
			if ((!s2->lines[i]->flags & ML_TWOSIDED) ||
				(s2->lines[i]->backsector == s1))
				continue;
			s3 = s2->lines[i]->backsector;
			
			//	Spawn rising slime
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
			P_AddThinker (&floor->thinker);
			s2->floordata = floor;	//jff 2/22/98
			floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
			floor->type = donutRaise;
			floor->crush = -1;
			floor->direction = 1;
			floor->sector = s2;
			floor->speed = slimespeed;
			floor->texture = s3->floorpic;
			floor->newspecial = 0;
			floor->floordestheight = s3->floorheight;
			StartFloorSound (s2);
			
			//	Spawn lowering donut-hole
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
			P_AddThinker (&floor->thinker);
			s1->floordata = floor;	//jff 2/22/98
			floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
			floor->type = floorLowerToNearest;
			floor->crush = -1;
			floor->direction = -1;
			floor->sector = s1;
			floor->speed = pillarspeed;
			floor->floordestheight = s3->floorheight;
			StartFloorSound (s1);
			break;
		}
	}
	return rtn;
}

//
// EV_DoElevator
//
// Handle elevator linedef types
//
// Passed the linedef that triggered the elevator and the elevator action
//
// jff 2/22/98 new type to move floor and ceiling in parallel
// [RH] Added speed, tag, and height parameters and new types.
//
BOOL EV_DoElevator (line_t *line, elevator_e elevtype,
					fixed_t speed, fixed_t height, int tag)
{
	int			secnum;
	BOOL		rtn;
	sector_t*	sec;
	elevator_t*	elevator;

	if (!line && (elevtype == elevateCurrent))
		return false;

	secnum = -1;
	rtn = false;
	// act on all sectors with the same tag as the triggering linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

		// If either floor or ceiling is already activated, skip it
		if (sec->floordata || sec->ceilingdata) //jff 2/22/98
			continue;

		// create and initialize new elevator thinker
		rtn = true;
		elevator = Z_Malloc (sizeof(*elevator), PU_LEVSPEC, 0);
		P_AddThinker (&elevator->thinker);
		sec->floordata = elevator; //jff 2/22/98
		sec->ceilingdata = elevator; //jff 2/22/98
		elevator->thinker.function.acp1 = (actionf_p1) T_MoveElevator;
		elevator->type = elevtype;
		elevator->sector = sec;
		elevator->speed = speed;
		StartFloorSound (sec);

		// set up the fields according to the type of elevator action
		switch (elevtype)
		{
			// elevator down to next floor
			case elevateDown:
				elevator->direction = -1;
				elevator->floordestheight =
					P_FindNextLowestFloor(sec,sec->floorheight);
				elevator->ceilingdestheight =
					elevator->floordestheight + sec->ceilingheight - sec->floorheight;
				break;

			// elevator up to next floor
			case elevateUp:
				elevator->direction = 1;
				elevator->floordestheight =
					P_FindNextHighestFloor(sec,sec->floorheight);
				elevator->ceilingdestheight =
					elevator->floordestheight + sec->ceilingheight - sec->floorheight;
				break;

			// elevator to floor height of activating switch's front sector
			case elevateCurrent:
				elevator->floordestheight = line->frontsector->floorheight;
				elevator->ceilingdestheight =
					elevator->floordestheight + sec->ceilingheight - sec->floorheight;
				elevator->direction =
					elevator->floordestheight>sec->floorheight?  1 : -1;
				break;

			// [RH] elevate up by a specific amount
			case elevateRaise:
				elevator->direction = 1;
				elevator->floordestheight = sec->floorheight + height;
				elevator->ceilingdestheight = sec->ceilingheight + height;
				break;

			// [RH] elevate down by a specific amount
			case elevateLower:
				elevator->direction = -1;
				elevator->floordestheight = sec->floorheight - height;
				elevator->ceilingdestheight = sec->ceilingheight - height;
				break;
		}
	}
	return rtn;
}


//==========================================================================
//
// T_FloorWaggle
//
//==========================================================================

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

void T_FloorWaggle (floorWaggle_t *waggle)
{
	switch(waggle->state)
	{
		case WGLSTATE_EXPAND:
			if((waggle->scale += waggle->scaleDelta)
				>= waggle->targetScale)
			{
				waggle->scale = waggle->targetScale;
				waggle->state = WGLSTATE_STABLE;
			}
			break;
		case WGLSTATE_REDUCE:
			if((waggle->scale -= waggle->scaleDelta) <= 0)
			{ // Remove
				waggle->sector->floorheight = waggle->originalHeight;
				P_ChangeSector(waggle->sector, true);
				waggle->sector->floordata = NULL;
				P_RemoveThinker(&waggle->thinker);
				return;
			}
			break;
		case WGLSTATE_STABLE:
			if(waggle->ticker != -1)
			{
				if(!--waggle->ticker)
				{
					waggle->state = WGLSTATE_REDUCE;
				}
			}
			break;
	}
	waggle->accumulator += waggle->accDelta;
	waggle->sector->floorheight = waggle->originalHeight
		+FixedMul(FloatBobOffsets[(waggle->accumulator>>FRACBITS)&63],
		waggle->scale);
	P_ChangeSector(waggle->sector, true);
}

//==========================================================================
//
// EV_StartFloorWaggle
//
//==========================================================================

BOOL EV_StartFloorWaggle (int tag, int height, int speed, int offset,
	int timer)
{
	int sectorIndex;
	sector_t *sector;
	floorWaggle_t *waggle;
	BOOL retCode;

	retCode = false;
	sectorIndex = -1;
	while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
	{
		sector = &sectors[sectorIndex];
		if (sector->floordata)
		{ // Already busy with another thinker
			continue;
		}
		retCode = true;
		waggle = Z_Malloc(sizeof(*waggle), PU_LEVSPEC, 0);
		sector->floordata = waggle;
		waggle->thinker.function.acp1 = (actionf_p1) T_FloorWaggle;
		waggle->sector = sector;
		waggle->originalHeight = sector->floorheight;
		waggle->accumulator = offset*FRACUNIT;
		waggle->accDelta = speed<<10;
		waggle->scale = 0;
		waggle->targetScale = height<<10;
		waggle->scaleDelta = waggle->targetScale
			/(35+((3*35)*height)/255);
		waggle->ticker = timer ? timer*TICRATE : -1;
		waggle->state = WGLSTATE_EXPAND;
		P_AddThinker(&waggle->thinker);
	}
	return retCode;
}
