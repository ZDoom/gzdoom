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
// DESCRIPTION:  Ceiling aninmation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------



#include "m_alloc.h"

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

//
// CEILINGS
//


// [RH] Active ceilings are now linked together in a list instead
//		of stored in an array. Similar to what Lee Killough did
//		with BOOM except I link the ceilings themselves together.
ceiling_t *activeceilings;


//
// T_MoveCeiling
//
void T_MoveCeiling (ceiling_t* ceiling)
{
	result_e res;
		
	switch(ceiling->direction)
	{
	  case 0:
		// IN STASIS
		break;
	  case 1:
		// UP
		res = T_MovePlane(ceiling->sector,
						  ceiling->speed,
						  ceiling->topheight,
						  -1,1,ceiling->direction);
		
		// Make moving sound
		if (!(level.time&7) && (ceiling->silent == 0))
			S_StartSound ((mobj_t *)&ceiling->sector->soundorg, "plats/pt1_mid", 119);
		
		if (res == pastdest)
		{
			switch (ceiling->type)
			{
			  case ceilCrushAndRaise:
				if (ceiling->silent == 1)
					S_StartSound((mobj_t *)&ceiling->sector->soundorg, "plats/pt1_stop", 100);
				ceiling->direction = -1;
				ceiling->speed = ceiling->speed1;
				break;
				
			  // movers with texture change, change the texture then get removed
			  case genCeilingChgT:
			  case genCeilingChg0:
				ceiling->sector->special = ceiling->newspecial;
			  case genCeilingChg:
				ceiling->sector->ceilingpic = ceiling->texture;
				// fall through
			  default:
				P_RemoveActiveCeiling(ceiling);
				break;
			}
			
		}
		break;
		
	  case -1:
		// DOWN
		res = T_MovePlane(ceiling->sector,
						  ceiling->speed,
						  ceiling->bottomheight,
						  ceiling->crush,1,ceiling->direction);
		
		// Make moving noise
		if (!(level.time&7) && !ceiling->silent)
			S_StartSound((mobj_t *)&ceiling->sector->soundorg, "plats/pt1_mid", 119);
		
		if (res == pastdest)
		{
			switch (ceiling->type)
			{
			  case ceilCrushAndRaise:
			  case ceilCrushRaiseAndStay:
				if (ceiling->silent == 1)
					S_StartSound ((mobj_t *)&ceiling->sector->soundorg, "plats/pt1_stop", 100);
				ceiling->speed = ceiling->speed2;
				ceiling->direction = 1;
				break;

			  // in the case of ceiling mover/changer, change the texture
			  // then remove the active ceiling
			  case genCeilingChgT:
			  case genCeilingChg0:
				ceiling->sector->special = ceiling->newspecial;
			  case genCeilingChg:
				ceiling->sector->ceilingpic = ceiling->texture;
				// fall through
			  default:
				P_RemoveActiveCeiling(ceiling);
				break;
			}
		}
		else // ( res != pastdest )
		{
			if (res == crushed)
			{
				switch (ceiling->type)
				{
				  case ceilCrushAndRaise:
				  case ceilLowerAndCrush:
					if (ceiling->speed1 == FRACUNIT && ceiling->speed2 == FRACUNIT)
						ceiling->speed = FRACUNIT / 8;
						break;

				  default:
					break;
				}
			}
		}
		break;
	}
}


//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
// [RH] Added tag, speed, speed2, height, crush, silent, change params
BOOL EV_DoCeiling (ceiling_e type, line_t *line,
				   int tag, fixed_t speed, fixed_t speed2, fixed_t height,
				   int crush, int silent, int change)
{
	int 		secnum;
	BOOL 		rtn;
	sector_t*	sec;
	ceiling_t*	ceiling;
	BOOL		manual = false;
	fixed_t		targheight;
		
	rtn = false;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = sec-sectors;
		manual = true;
		// [RH] Hack to let manual crushers be retriggerable, too
		tag ^= secnum | 0x1000000;
		P_ActivateInStasisCeiling (tag);
		goto manual_ceiling;
	}
	
	//	Reactivate in-stasis ceilings...for certain types.
	// This restarts a crusher after it has been stopped
	if (type == ceilCrushAndRaise)
	{
		P_ActivateInStasisCeiling (tag);
	}

	secnum = -1;
	// affects all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
manual_ceiling:
		// if ceiling already moving, don't start a second function on it
		if (P_SectorActive (ceiling_special,sec))	//jff 2/22/98
			continue;
		
		// new door thinker
		rtn = 1;
		ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVSPEC, 0);
		P_AddThinker (&ceiling->thinker);
		sec->ceilingdata = ceiling;					//jff 2/22/98
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = -1;
		ceiling->speed = ceiling->speed1 = speed;
		ceiling->speed2 = speed2;
		ceiling->silent = silent;
		
		switch(type)
		{
		  case ceilCrushAndRaise:
		  case ceilCrushRaiseAndStay:
			ceiling->topheight = sec->ceilingheight;
		  case ceilLowerAndCrush:
			ceiling->crush = crush;
			targheight = ceiling->bottomheight = sec->floorheight + 8*FRACUNIT;
			ceiling->direction = -1;
			break;

		  case ceilRaiseToHighest:
			targheight = ceiling->topheight = P_FindHighestCeilingSurrounding (sec);
			ceiling->direction = 1;
			break;

		  case ceilLowerByValue:
			targheight = ceiling->bottomheight = sec->ceilingheight - height;
			ceiling->direction = -1;
			break;

		  case ceilRaiseByValue:
			targheight = ceiling->topheight = sec->ceilingheight + height;
			ceiling->direction = 1;
			break;

		  case ceilMoveToValue:
			{
			  int diff = height - sec->ceilingheight;

			  if (diff < 0) {
				  targheight = ceiling->bottomheight = height;
				  ceiling->direction = -1;
			  } else {
				  targheight = ceiling->topheight = height;
				  ceiling->direction = 1;
			  }
			}
			break;

		  case ceilLowerToHighestFloor:
			targheight = ceiling->bottomheight = P_FindHighestFloorSurrounding (sec);
			ceiling->direction = -1;
			break;

		  case ceilRaiseToHighestFloor:
			targheight = ceiling->topheight = P_FindHighestFloorSurrounding (sec);
			ceiling->direction = 1;
			break;

		  case ceilLowerInstant:
			targheight = ceiling->bottomheight = sec->ceilingheight - height;
			ceiling->direction = -1;
			ceiling->speed = height;
			break;

		  case ceilRaiseInstant:
			targheight = ceiling->topheight = sec->ceilingheight + height;
			ceiling->direction = 1;
			ceiling->speed = height;
			break;

		  case ceilLowerToNearest:
			targheight = ceiling->bottomheight =
				P_FindNextLowestCeiling (sec, sec->ceilingheight);
			ceiling->direction = 1;
			break;

		  case ceilRaiseToNearest:
			targheight = ceiling->topheight =
				P_FindNextHighestCeiling (sec, sec->ceilingheight);
			ceiling->direction = 1;
			break;

		  case ceilLowerToLowest:
			targheight = ceiling->bottomheight = P_FindLowestCeilingSurrounding (sec);
			ceiling->direction = -1;
			break;

		  case ceilRaiseToLowest:
			targheight = ceiling->topheight = P_FindLowestCeilingSurrounding (sec);
			ceiling->direction = -1;
			break;

		  case ceilLowerToFloor:
			targheight = ceiling->bottomheight = sec->floorheight;
			ceiling->direction = -1;
			break;

		  case ceilRaiseToFloor:
			targheight = ceiling->topheight = sec->floorheight;
			ceiling->direction = 1;
			break;

		  case ceilLowerToHighest:
			targheight = ceiling->bottomheight = P_FindHighestCeilingSurrounding (sec);
			ceiling->direction = -1;
			break;

		  case ceilLowerByTexture:
			targheight = ceiling->bottomheight =
				sec->ceilingheight - P_FindShortestUpperAround (secnum);
			ceiling->direction = -1;
			break;

		  case ceilRaiseByTexture:
			targheight = ceiling->topheight =
				sec->ceilingheight + P_FindShortestUpperAround (secnum);
			ceiling->direction = 1;
			break;
		  
		}
				
		ceiling->tag = tag;
		ceiling->type = type;

		// set texture/type change properties
		if (change & 3)		// if a texture change is indicated
		{
			if (change & 4)	// if a numeric model change
			{
				sector_t *sec;

				//jff 5/23/98 find model with floor at target height if target
				//is a floor type
				sec = (type == ceilRaiseToHighest ||
					   type == ceilRaiseToFloor ||
					   type == ceilLowerToHighest ||
					   type == ceilLowerToFloor) ?
					P_FindModelFloorSector (targheight, secnum) :
					P_FindModelCeilingSector (targheight, secnum);
				if (sec)
				{
					ceiling->texture = sec->ceilingpic;
					switch (change & 3)
					{
						case 1:		// type is zeroed
							ceiling->newspecial = 0;
							ceiling->type = genCeilingChg0;
							break;
						case 2:		// type is copied
							ceiling->newspecial = sec->special;
							ceiling->type = genCeilingChgT;
							break;
						case 3:		// type is left alone
							ceiling->type = genCeilingChg;
							break;
					}
				}
			}
			else if (line)	// else if a trigger model change
			{

				ceiling->texture = line->frontsector->ceilingpic;
				switch (change & 3)
				{
					case 1:		// type is zeroed
						ceiling->newspecial = 0;
						ceiling->type = genCeilingChg0;
						break;
					case 2:		// type is copied
						ceiling->newspecial = line->frontsector->special;
						ceiling->type = genCeilingChgT;
						break;
					case 3:		// type is left alone
						ceiling->type = genCeilingChg;
						break;
				}
			}
		}

		P_AddActiveCeiling(ceiling);

		if (manual)
			return rtn;
	}
	return rtn;
}


//
// Add an active ceiling
// [RH] Rewritten to use list
//
void P_AddActiveCeiling (ceiling_t *c)
{
	c->next = activeceilings;
	c->prev = NULL;
	activeceilings = c;
}



//
// Remove a ceiling's thinker
// [RH] Rewritten to use list
//
void P_RemoveActiveCeiling (ceiling_t *c)
{
	ceiling_t *scan = activeceilings;

	while (scan) {
		if (scan == c) {
			c->sector->ceilingdata = NULL;
			if (c == activeceilings) {
				activeceilings = c->next;
			} else {
				if (c->prev)
					c->prev->next = c->next;
				if (c->next)
					c->next->prev = c->prev;
			}
			P_RemoveThinker (&c->thinker);
			break;
		}
		scan = scan->next;
	}
}



//
// Restart a ceiling that's in-stasis
// [RH] Passed a tag instead of a line and rewritten to use list
//
void P_ActivateInStasisCeiling (int tag)
{
	ceiling_t *scan = activeceilings;

	while (scan) {
		if (scan->tag == tag && scan->direction == 0) {
			scan->direction = scan->olddirection;
			scan->thinker.function.acp1 = (actionf_p1) T_MoveCeiling;
		}
		scan = scan->next;
	}
}



//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
// [RH] Passed a tag instead of a line and rewritten to use list
//
BOOL EV_CeilingCrushStop (int tag)
{
	BOOL rtn = false;
	ceiling_t *scan = activeceilings;

	while (scan) {
		if (scan->tag == tag && scan->direction != 0) {
			scan->olddirection = scan->direction;
			scan->thinker.function.acv = (actionf_v) NULL;
			scan->direction = 0;		// in-stasis;
			rtn = true;
		}
		scan = scan->next;
	}

	return rtn;
}
