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
//		Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------



#include "m_alloc.h"

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"


// [RH] Active plats are now linked together in a list instead of
//		stored in an array. Similar to what Lee Killough did with
//		BOOM except I link the plats themselves together.
plat_t *activeplats;



//
// Move a plat up and down
//
void T_PlatRaise(plat_t* plat)
{
	result_e res;
		
	switch (plat->status)
	{
	  case up:
		res = T_MovePlane(plat->sector,
						  plat->speed,
						  plat->high,
						  plat->crush,0,1);
										
		if (plat->type == platUpByValueStay
			|| plat->type == platRaiseAndStay)
		{
			if (!(level.time&7))
				S_StartSound((mobj_t *)&plat->sector->soundorg, "plats/pt1_mid", 119);
		}
		
								
		if (res == crushed && (!plat->crush))
		{
			plat->count = plat->wait;
			plat->status = down;
			S_StartSound((mobj_t *)&plat->sector->soundorg, "plats/pt1_strt", 100);
		}
		else
		{
			if (res == pastdest)
			{
				if (plat->type != platToggle) {
					plat->count = plat->wait;
					plat->status = waiting;
					S_StartSound((mobj_t *)&plat->sector->soundorg, "plats/pt1_stop", 100);

					switch(plat->type)
					{
						case platDownWaitUpStay:
						case platRaiseAndStay:
						case platUpByValueStay:
						case platDownToNearestFloor:
						case platDownToLowestCeiling:
							P_RemoveActivePlat(plat);
							break;
						default:
							break;
					}
				} else {
					plat->oldstatus = plat->status;//jff 3/14/98 after action wait  
					plat->status = in_stasis;      //for reactivation of toggle
				}
			}
		}
		break;
		
	  case down:
		res = T_MovePlane (plat->sector, plat->speed, plat->low, -1, 0, -1);

		if (res == pastdest)
		{
			// if not an instant toggle, start waiting, make plat stop sound
			if (plat->type != platToggle)	//jff 3/14/98 toggle up down
			{								// is silent, instant, no waiting
				plat->count = plat->wait;
				plat->status = waiting;
				S_StartSound((mobj_t *)&plat->sector->soundorg, "plats/pt1_stop", 100);

				switch (plat->type) {
					case platUpWaitDownStay:
					case platUpByValue:
						P_RemoveActivePlat (plat);
						break;
					default:
						break;
				}
			} else {	// instant toggles go into stasis awaiting next activation
				plat->oldstatus = plat->status;	//jff 3/14/98 after action wait  
				plat->status = in_stasis;		//for reactivation of toggle
			}
		}

		//jff 1/26/98 remove the plat if it bounced so it can be tried again
		//only affects plats that raise and bounce
		//killough 1/31/98: relax compatibility to demo_compatibility

		// remove the plat if it's a pure raise type
		if (!olddemo)
		{
			switch (plat->type)
			{
				case platUpByValueStay:
				case platRaiseAndStay:
					P_RemoveActivePlat(plat);
				default:
					break;
			}
		}

		break;
		
	  case waiting:
		if (!--plat->count)
		{
			if (plat->sector->floorheight == plat->low)
				plat->status = up;
			else
				plat->status = down;
			S_StartSound((mobj_t *)&plat->sector->soundorg, "plats/pt1_strt", 100);
		}
		break;

	  case in_stasis:
		break;
	}
}


//
// Do Platforms
//	[RH] Changed amount to height and added delay,
//		 lip, change, tag, and speed parameters.
//
BOOL EV_DoPlat (int tag, line_t *line, plattype_e type, int height,
				int speed, int delay, int lip, int change)
{
	plat_t* 	plat;
	int 		secnum;
	sector_t	*sec;
	int 		rtn = false;
	BOOL		manual = false;

	// [RH] If tag is zero, use the sector on the back side
	//		of the activating line (if any).
	if (!tag && !olddemo) {
		if (!line || !(sec = line->backsector))
			return false;
		secnum = sec - sectors;
		manual = true;
		goto manual_plat;
	}

	//	Activate all <type> plats that are in_stasis
	switch (type)
	{
	  case platToggle:
		rtn = true;
	  case platPerpetualRaise:
		P_ActivateInStasis (tag);
		break;
	
	  default:
		break;
	}
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_plat:
		if (P_SectorActive (floor_special, sec))	//jff 2/23/98 multiple thinkers
			continue;
		
		// Find lowest & highest floors around sector
		rtn = true;
		plat = Z_Malloc( sizeof(*plat), PU_LEVSPEC, 0);
		P_AddThinker(&plat->thinker);
				
		plat->type = type;
		plat->sector = sec;
		plat->sector->floordata = plat;	//jff 2/23/98 multiple thinkers
		plat->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
		plat->crush = -1;
		plat->tag = tag;
		plat->speed = speed;
		plat->wait = delay;

		//jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
		//going down forever -- default low to plat height when triggered
		plat->low = sec->floorheight;

		if (change) {
			if (line)
				sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
			if (change == 1)
				sec->special = 0;	// Stop damage and other stuff, if any
		}

		switch (type)
		{
			case platRaiseAndStay:
				plat->high = P_FindNextHighestFloor (sec, sec->floorheight);
				plat->status = up;
				S_StartSound ((mobj_t *)&sec->soundorg, "plats/pt1_mid", 119);
				break;

			case platUpByValue:
			case platUpByValueStay:
				plat->high = sec->floorheight + height;
				plat->status = up;
				S_StartSound ((mobj_t *)&sec->soundorg, "plats/pt1_mid", 119);
				break;
			
			case platDownByValue:
				plat->low = sec->floorheight - height;
				plat->status = down;
				S_StartSound ((mobj_t *)&sec->soundorg, "plats/pt1_mid", 119);
				break;

			case platDownWaitUpStay:
				plat->low = P_FindLowestFloorSurrounding (sec) + lip*FRACUNIT;

				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;

				plat->high = sec->floorheight;
				plat->status = down;
				S_StartSound ((mobj_t *)&sec->soundorg, "plats/pt1_strt", 100);
				break;
			
			case platUpWaitDownStay:
				plat->high = P_FindHighestFloorSurrounding (sec);

				if (plat->high < sec->floorheight)
					plat->high = sec->floorheight;

				plat->status = up;
				S_StartSound ((mobj_t *)&sec->soundorg, "plats/pt1_strt", 100);
				break;

			case platPerpetualRaise:
				plat->low = P_FindLowestFloorSurrounding (sec) + lip*FRACUNIT;

				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;

				plat->high = P_FindHighestFloorSurrounding (sec);

				if (plat->high < sec->floorheight)
					plat->high = sec->floorheight;

				plat->status = P_Random (pr_doplat) & 1;

				S_StartSound((mobj_t *)&sec->soundorg, "plats/pt1_strt", 100);
				break;

			case platToggle:	//jff 3/14/98 add new type to support instant toggle
				plat->crush = 10;	//jff 3/14/98 crush anything in the way

				// set up toggling between ceiling, floor inclusive
				plat->low = sec->ceilingheight;
				plat->high = sec->floorheight;
				plat->status = down;
				break;

			case platDownToNearestFloor:
				plat->low = P_FindNextLowestFloor (sec, sec->floorheight) + lip*FRACUNIT;
				plat->status = down;
				plat->high = sec->floorheight;
				S_StartSound((mobj_t *)&sec->soundorg, "plats/pt1_strt", 100);
				break;

			case platDownToLowestCeiling:
		        plat->low = P_FindLowestCeilingSurrounding(sec);
				plat->high = sec->floorheight;

				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;

				plat->status = down;
				S_StartSound((mobj_t *)&sec->soundorg, "plats/pt1_strt", 100);
				break;

			default:
				break;
		}
		P_AddActivePlat (plat);
		if (manual)
			return rtn;
	}
	return rtn;
}


// [RH] Rewritten to use list
void P_ActivateInStasis (int tag)
{
	plat_t *scan = activeplats;

	while (scan) {
		if (scan->tag == tag && scan->status == in_stasis) {
			if (scan->type == platToggle)	//jff 3/14/98 reactivate toggle type
				scan->status = scan->oldstatus == up ? down : up;
			else
				scan->status = scan->oldstatus;
			scan->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
		}
		scan = scan->next;
	}
}

// [RH] Passed a tag instead of a line and rewritten to use list
void EV_StopPlat (int tag)
{
	plat_t *scan = activeplats;

	while (scan) {
		if (scan->status != in_stasis && scan->tag == tag) {
			scan->oldstatus = scan->status;
			scan->status = in_stasis;
			scan->thinker.function.acv = (actionf_v)NULL;
		}
	}
}

// [RH] Rewritten to use list
void P_AddActivePlat (plat_t *plat)
{
	if (activeplats)
		activeplats->prev = plat;
	plat->next = activeplats;
	plat->prev = NULL;
	activeplats = plat;
}

// [RH] Rewritten to use list
void P_RemoveActivePlat (plat_t *plat)
{
	plat_t *scan = activeplats;

	while (scan) {
		if (scan == plat) {
			scan->sector->floordata = NULL;
			if (scan == activeplats) {
				activeplats = scan->next;
			} else {
				if (scan->prev)
					scan->prev->next = scan->next;
				if (scan->next)
					scan->next->prev = scan->prev;
			}
			P_RemoveThinker (&scan->thinker);
			break;
		}
		scan = scan->next;
	}
}

