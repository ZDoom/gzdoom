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
// DESCRIPTION: Door animation code (opening/closing)
//		[RH] Removed sliding door code and simplified for Hexen-ish specials
//
//-----------------------------------------------------------------------------



#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"


// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"

#include "c_consol.h"


//
// VERTICAL DOORS
//

// [RH] DoorSound: Plays door sound depending on its direction and speed
static void DoorSound (vldoor_t *door, fixed_t speed, BOOL raise)
{
	char *snd;

	if (raise)
		snd = (speed >= FRACUNIT*8) ? "doors/dr2_open" : "doors/dr1_open";
	else
		snd = (speed >= FRACUNIT*8) ? "doors/dr2_clos" : "doors/dr1_clos";

	S_StartSound ((mobj_t *)&door->sector->soundorg, snd, 100);
}

//
// T_VerticalDoor
//
void T_VerticalDoor (vldoor_t *door)
{
	result_e res;
		
	switch (door->direction)
	{
	  case 0:
		// WAITING
		if (!--door->topcountdown)
		{
			switch(door->type)
			{
			  case doorRaise:
				door->direction = -1; // time to go back down
				DoorSound (door, door->speed, false);
				break;
				
			  case doorCloseWaitOpen:
				door->direction = 1;
				DoorSound (door, door->speed, true);
				break;
				
			  default:
				break;
			}
		}
		break;
		
	  case 2:
		//	INITIAL WAIT
		if (!--door->topcountdown)
		{
			switch(door->type)
			{
			  case doorRaiseIn5Mins:
				door->direction = 1;
				door->type = doorRaise;
				DoorSound (door, door->speed, true);
				break;
				
			  default:
				break;
			}
		}
		break;
		
	  case -1:
		// DOWN
		res = T_MovePlane(door->sector,
						  door->speed,
						  door->sector->floorheight,
						  -1,1,door->direction);
		if (res == pastdest)
		{
			switch(door->type)
			{
			  case doorRaise:
			  case doorClose:
				door->sector->ceilingdata = NULL;  //jff 2/22/98
				P_RemoveThinker (&door->thinker);  // unlink and free
				break;
				
			  case doorCloseWaitOpen:
				door->direction = 0;
				door->topcountdown = door->topwait;
				break;
				
			  default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch(door->type)
			{
			  case doorClose:				// DO NOT GO BACK UP!
				break;
				
			  default:
				door->direction = 1;
				DoorSound (door, door->speed, true);
				break;
			}
		}
		break;
		
	  case 1:
		// UP
		res = T_MovePlane(door->sector,
						  door->speed,
						  door->topheight,
						  -1,1,door->direction);
		
		if (res == pastdest)
		{
			switch(door->type)
			{
			  case doorRaise:
				door->direction = 0; // wait at top
				door->topcountdown = door->topwait;
				break;
				
			  case doorCloseWaitOpen:
			  case doorOpen:
				door->sector->ceilingdata = NULL;  //jff 2/22/98
				P_RemoveThinker (&door->thinker);  // unlink and free
				break;
				
			  default:
				break;
			}
		}
		break;
	}
}


// [RH] Merged EV_VerticalDoor and EV_DoLockedDoor into EV_DoDoor
//		and made them more general to support the new specials.

// [RH] SpawnDoor: Helper function for EV_DoDoor
static BOOL SpawnDoor (sector_t *sec, vldoor_e type, fixed_t speed, int delay)
{
	// new door thinker
	vldoor_t *door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
	P_AddThinker (&door->thinker);
	sec->ceilingdata = door;	//jff 2/22/98

	door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
	door->sector = sec;
	door->type = type;
	door->topwait = delay;
	door->speed = speed;

	switch(type)
	{
		case doorClose:
			door->topheight = P_FindLowestCeilingSurrounding (sec) - 4*FRACUNIT;
			door->topheight -= 4*FRACUNIT;
			door->direction = -1;
			DoorSound (door, speed, false);
			break;

		case doorOpen:
		case doorRaise:
			door->direction = 1;
			door->topheight = P_FindLowestCeilingSurrounding (sec) - 4*FRACUNIT;
			if (door->topheight != sec->ceilingheight)
				DoorSound (door, speed, true);
			break;

		case doorCloseWaitOpen:
			door->topheight = sec->ceilingheight;
			door->direction = -1;
			DoorSound (door, speed, false);
			break;
	}

	return true;
}

BOOL EV_DoDoor (vldoor_e type, line_t *line, mobj_t *thing,
				int tag, int speed, int delay, keytype_t lock)
{
	BOOL		rtn = false;
	int 		secnum;
	sector_t*	sec;

	if (lock && !P_CheckKeys (thing->player, lock, tag))
		return false;

	if (tag == 0) {		// [RH] manual door
		if (!line)
			return false;

		// if the wrong side of door is pushed, give oof sound
		if (line->sidenum[1]==-1)				// killough
		{
			S_StartSound (thing, "*grunt1", 78);
			return false;
		}

		// get the sector on the second side of activating linedef
		sec = sides[line->sidenum[1]].sector;
		secnum = sec-sectors;

		// if door already has a thinker, use it
		if (sec->ceilingdata)			//jff 2/22/98
		{
			vldoor_t *door = sec->ceilingdata;	//jff 2/22/98
			if (type == doorRaise) {
				// ONLY FOR "RAISE" DOORS, NOT "OPEN"s
				if (door->direction == -1) {
					door->direction = 1;	// go back up
				}
				else if ((line->flags & ML_ACTIVATIONMASK) != ML_ACTIVATEPUSH)
					// [RH] activate push doors don't go back down when you
					//		run into them (otherwise opening them would be
					//		a real pain).
				{
					if (!thing->player)
						return false;		// JDC: bad guys never close doors
					
					door->direction = -1;	// start going down immediately
				}
				return true;
			}
		}
		rtn = SpawnDoor (sec, type, speed, delay);

	} else {	// [RH] Remote door

		secnum = -1;
		while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
		{
			sec = &sectors[secnum];
			// if the ceiling already moving, don't start the door action
			if (P_SectorActive (ceiling_special,sec)) //jff 2/22/98
				continue;

			rtn |= SpawnDoor (sec, type, speed, delay);
		}
				
	}
	return rtn;
}


//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30 (sector_t* sec)
{
	vldoor_t *door;
		
	door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);

	P_AddThinker (&door->thinker);

	sec->ceilingdata = door;	//jff 2/22/98
	sec->special = 0;

	door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
	door->sector = sec;
	door->direction = 0;
	door->type = doorRaise;
	door->speed = FRACUNIT*2;
	door->topcountdown = 30 * TICRATE;
}

//
// Spawn a door that opens after 5 minutes
//
void P_SpawnDoorRaiseIn5Mins (sector_t *sec)
{
	vldoor_t *door;

	door = Z_Malloc ( sizeof(*door), PU_LEVSPEC, 0);
	
	P_AddThinker (&door->thinker);

	sec->ceilingdata = door;	//jff 2/22/98
	sec->special = 0;

	door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
	door->sector = sec;
	door->direction = 2;
	door->type = doorRaiseIn5Mins;
	door->speed = FRACUNIT * 2;
	door->topheight = P_FindLowestCeilingSurrounding(sec);
	door->topheight -= 4*FRACUNIT;
	door->topwait = (150*TICRATE)/35;
	door->topcountdown = 5 * 60 * TICRATE;
}
