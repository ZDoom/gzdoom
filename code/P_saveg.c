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
//		Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------


#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "m_random.h"

#include "v_palett.h"

byte *save_p;


// Pads save_p to a 4-byte boundary
//	so that the load/save works on SGI&Gecko.
#define PADSAVEP()		save_p += (4 - ((int) save_p & 3)) & 3



//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
	int 		i;
	int 		j;
	player_t*	dest;
				
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (!playeringame[i])
			continue;
		
		PADSAVEP();

		dest = (player_t *)save_p;
		memcpy (dest,&players[i],sizeof(player_t));
		save_p += sizeof(player_t);
		for (j=0 ; j<NUMPSPRITES ; j++)
		{
			if (dest->psprites[j].state)
			{
				dest->psprites[j].state 
					= (state_t *)(dest->psprites[j].state-states);
			}
		}
	}
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
	int 		i;
	int 		j;
		
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (!playeringame[i])
			continue;
		
		PADSAVEP();

		memcpy (&players[i],save_p, sizeof(player_t));
		save_p += sizeof(player_t);
		
		// will be set when unarc thinker
		players[i].mo = NULL;	
		players[i].message = NULL;
		players[i].attacker = NULL;

		for (j=0 ; j<NUMPSPRITES ; j++)
		{
			if (players[i]. psprites[j].state)
			{
				players[i]. psprites[j].state 
					= &states[ (int)players[i].psprites[j].state ];
			}
		}
	}
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
	int 				i;
	int 				j;
	sector_t*			sec;
	line_t* 			li;
	side_t* 			si;
	short*				put;
		
	put = (short *)save_p;
	
	// do sectors
	for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
	{
		*put++ = (short)(sec->floorheight >> FRACBITS);
		*put++ = (short)(sec->ceilingheight >> FRACBITS);
		*put++ = sec->floorpic;
		*put++ = sec->ceilingpic;
		*put++ = sec->lightlevel;
		*put++ = sec->special;			// needed?
		*put++ = sec->tag;				// needed?
	}

	
	// do lines
	for (i=0, li = lines ; i<numlines ; i++,li++)
	{
		*put++ = li->flags;
		*put++ = li->special;
		*put++ = li->tag;
		for (j=0 ; j<2 ; j++)
		{
			if (li->sidenum[j] == -1)
				continue;
			
			si = &sides[li->sidenum[j]];

			*put++ = (short)(si->textureoffset >> FRACBITS);
			*put++ = (short)(si->rowoffset >> FRACBITS);
			*put++ = si->toptexture;
			*put++ = si->bottomtexture;
			*put++ = si->midtexture;	
		}
	}
		
	save_p = (byte *)put;
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
	int 				i;
	int 				j;
	sector_t*			sec;
	line_t* 			li;
	side_t* 			si;
	short*				get;
		
	get = (short *)save_p;
	
	// do sectors
	for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
	{
		sec->floorheight = *get++ << FRACBITS;
		sec->ceilingheight = *get++ << FRACBITS;
		sec->floorpic = *get++;
		sec->ceilingpic = *get++;
		sec->lightlevel = *get++;
		sec->special = *get++;
		sec->tag = *get++;
		sec->ceilingdata = 0;	//jff 2/22/98 now three thinker fields not two
		sec->floordata = 0;
		sec->lightingdata = 0;
		sec->soundtarget = 0;
	}
	
	// do lines
	for (i=0, li = lines ; i<numlines ; i++,li++)
	{
		li->flags = *get++;
		li->special = *get++;
		li->tag = *get++;
		for (j=0 ; j<2 ; j++)
		{
			if (li->sidenum[j] == -1)
				continue;
			si = &sides[li->sidenum[j]];
			si->textureoffset = *get++ << FRACBITS;
			si->rowoffset = *get++ << FRACBITS;
			si->toptexture = *get++;
			si->bottomtexture = *get++;
			si->midtexture = *get++;
		}
	}
	save_p = (byte *)get;		
}





//
// Thinkers
//
typedef enum
{
	tc_end,
	tc_mobj

} thinkerclass_t;



//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
	thinker_t*			th;
	mobj_t* 			mobj;
		
	// save off the current thinkers
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			*save_p++ = tc_mobj;
			PADSAVEP();
			mobj = (mobj_t *)save_p;
			memcpy (mobj, th, sizeof(*mobj));
			save_p += sizeof(*mobj);
			mobj->state = (state_t *)(mobj->state - states);
			
			if (mobj->player)
				mobj->player = (player_t *)((mobj->player-players) + 1);
			continue;
		}
				
		// I_Error ("P_ArchiveThinkers: Unknown thinker function");
	}

	// add a terminating marker
	*save_p++ = tc_end; 
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
	byte				tclass;
	thinker_t*			currentthinker;
	thinker_t*			next;
	mobj_t* 			mobj;
	
	// remove all the current thinkers
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		next = currentthinker->next;
		
		if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
			P_RemoveMobj ((mobj_t *)currentthinker);
		else
			Z_Free (currentthinker);

		currentthinker = next;
	}
	P_InitThinkers ();
		
	// read in saved thinkers
	while (1)
	{
		tclass = *save_p++;
		switch (tclass)
		{
		  case tc_end:
			return; 	// end of list
						
		  case tc_mobj:
			PADSAVEP();
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
			memcpy (mobj, save_p, sizeof(*mobj));
			save_p += sizeof(*mobj);
			mobj->state = &states[(int)mobj->state];
			mobj->target = NULL;
			if (mobj->player)
			{
				mobj->player = &players[(int)mobj->player-1];
				mobj->player->mo = mobj;
			}
			P_SetThingPosition (mobj);
			mobj->info = &mobjinfo[mobj->type];
			mobj->floorz = mobj->subsector->sector->floorheight;
			mobj->ceilingz = mobj->subsector->sector->ceilingheight;
			mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
			P_AddThinker (&mobj->thinker);
			break;
						
		  default:
			I_Error ("Unknown tclass %i in savegame",tclass);
		}
		
	}

}


//
// P_ArchiveSpecials
//
enum
{
	tc_ceiling,
	tc_door,
	tc_floor,
	tc_plat,
	tc_flash,
	tc_strobe,
	tc_glow,
	tc_fireflicker,
	tc_elevator,
	tc_scroll,
	tc_endspecials
} specials_e;	



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
// T_FireFlicker (fireflicker_t: sector_t * swizze),	[RH] Was missed in original code
// T_MoveElevator, (plat_t: sector_t *), - active list	// jff 2/22/98
// T_Scroll												// killough 3/7/98
//
void P_ArchiveSpecials (void)
{
	thinker_t*			th;
	ceiling_t*			ceiling;
	vldoor_t*			door;
	floormove_t*		floor;
	plat_t* 			plat;
	lightflash_t*		flash;
	strobe_t*			strobe;
	glow_t* 			glow;
	int 				i;
		
	// save off the current thinkers
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function.acv == (actionf_v)NULL)
		{
			for (i = 0; i < MaxCeilings;i++)
				if (activeceilings[i] == (ceiling_t *)th)
					break;
			
			if (i<MaxCeilings)
			{
				*save_p++ = tc_ceiling;
				PADSAVEP();
				ceiling = (ceiling_t *)save_p;
				memcpy (ceiling, th, sizeof(*ceiling));
				save_p += sizeof(*ceiling);
				ceiling->sector = (sector_t *)(ceiling->sector - sectors);
			}
			continue;
		}

		if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
		{
			*save_p++ = tc_ceiling;
			PADSAVEP();
			ceiling = (ceiling_t *)save_p;
			memcpy (ceiling, th, sizeof(*ceiling));
			save_p += sizeof(*ceiling);
			ceiling->sector = (sector_t *)(ceiling->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_VerticalDoor)
		{
			*save_p++ = tc_door;
			PADSAVEP();
			door = (vldoor_t *)save_p;
			memcpy (door, th, sizeof(*door));
			save_p += sizeof(*door);
			door->sector = (sector_t *)(door->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_MoveFloor)
		{
			*save_p++ = tc_floor;
			PADSAVEP();
			floor = (floormove_t *)save_p;
			memcpy (floor, th, sizeof(*floor));
			save_p += sizeof(*floor);
			floor->sector = (sector_t *)(floor->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_PlatRaise)
		{
			*save_p++ = tc_plat;
			PADSAVEP();
			plat = (plat_t *)save_p;
			memcpy (plat, th, sizeof(*plat));
			save_p += sizeof(*plat);
			plat->sector = (sector_t *)(plat->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_LightFlash)
		{
			*save_p++ = tc_flash;
			PADSAVEP();
			flash = (lightflash_t *)save_p;
			memcpy (flash, th, sizeof(*flash));
			save_p += sizeof(*flash);
			flash->sector = (sector_t *)(flash->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
		{
			*save_p++ = tc_strobe;
			PADSAVEP();
			strobe = (strobe_t *)save_p;
			memcpy (strobe, th, sizeof(*strobe));
			save_p += sizeof(*strobe);
			strobe->sector = (sector_t *)(strobe->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_Glow)
		{
			*save_p++ = tc_glow;
			PADSAVEP();
			glow = (glow_t *)save_p;
			memcpy (glow, th, sizeof(*glow));
			save_p += sizeof(*glow);
			glow->sector = (sector_t *)(glow->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_FireFlicker)
		{
			fireflicker_t *flick;

			// [RH] Save FireFlicker thinkers. The Linux code missed these.
			*save_p++ = tc_fireflicker;
			PADSAVEP();
			flick = (fireflicker_t *)save_p;
			memcpy (flick, th, sizeof(*flick));
			save_p += sizeof(*flick);
			flick->sector = (sector_t *)(flick->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_MoveElevator)
		{		//jff 2/22/98 new case for elevators

			elevator_t *elevator;		//jff 2/22/98

			*save_p++ = tc_elevator;
			PADSAVEP();
			elevator = (elevator_t *)save_p;
			memcpy (elevator, th, sizeof(*elevator));
			save_p += sizeof(*elevator);
			elevator->sector = (sector_t *)(elevator->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_Scroll)
		{		// killough 3/7/98: Scroll effect thinkers

			*save_p++ = tc_scroll;
			memcpy (save_p, th, sizeof(scroll_t));
			save_p += sizeof(scroll_t);
			continue;
		}

	}

	// add a terminating marker
	*save_p++ = tc_endspecials; 

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
	byte				tclass;
	ceiling_t*			ceiling;
	vldoor_t*			door;
	floormove_t*		floor;
	plat_t* 			plat;
	lightflash_t*		flash;
	strobe_t*			strobe;
	glow_t* 			glow;
	fireflicker_t*		flick;
		
		
	// read in saved thinkers
	while (1)
	{
		tclass = *save_p++;
		switch (tclass)
		{
		  case tc_endspecials:
			return; 	// end of list

		  case tc_ceiling:
			PADSAVEP();
			ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
			memcpy (ceiling, save_p, sizeof(*ceiling));
			save_p += sizeof(*ceiling);
			ceiling->sector = &sectors[(int)ceiling->sector];
			ceiling->sector->ceilingdata = ceiling;	//jff 2/22/98

			if (ceiling->thinker.function.acp1)
				ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;

			P_AddThinker (&ceiling->thinker);
			P_AddActiveCeiling(ceiling);
			break;

		  case tc_door:
			PADSAVEP();
			door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
			memcpy (door, save_p, sizeof(*door));
			save_p += sizeof(*door);
			door->sector = &sectors[(int)door->sector];
			door->sector->ceilingdata = door;	//jff 2/22/98
			door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
			P_AddThinker (&door->thinker);
			break;

		  case tc_floor:
			PADSAVEP();
			floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
			memcpy (floor, save_p, sizeof(*floor));
			save_p += sizeof(*floor);
			floor->sector = &sectors[(int)floor->sector];
			floor->sector->floordata = floor;	//jff 2/22/98
			floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
			P_AddThinker (&floor->thinker);
			break;

		  case tc_plat:
			PADSAVEP();
			plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
			memcpy (plat, save_p, sizeof(*plat));
			save_p += sizeof(*plat);
			plat->sector = &sectors[(int)plat->sector];
			plat->sector->floordata = plat;		//jff 2/22/98

			if (plat->thinker.function.acp1)
				plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;

			P_AddThinker (&plat->thinker);
			P_AddActivePlat(plat);
			break;

		  case tc_flash:
			PADSAVEP();
			flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
			memcpy (flash, save_p, sizeof(*flash));
			save_p += sizeof(*flash);
			flash->sector = &sectors[(int)flash->sector];
			flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
			P_AddThinker (&flash->thinker);
			break;

		  case tc_strobe:
			PADSAVEP();
			strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
			memcpy (strobe, save_p, sizeof(*strobe));
			save_p += sizeof(*strobe);
			strobe->sector = &sectors[(int)strobe->sector];
			strobe->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
			P_AddThinker (&strobe->thinker);
			break;

		  case tc_glow:
			PADSAVEP();
			glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
			memcpy (glow, save_p, sizeof(*glow));
			save_p += sizeof(*glow);
			glow->sector = &sectors[(int)glow->sector];
			glow->thinker.function.acp1 = (actionf_p1)T_Glow;
			P_AddThinker (&glow->thinker);
			break;

		  case tc_fireflicker:		// [RH] Restore fireflicker thinkers
			PADSAVEP();
			flick = Z_Malloc (sizeof(*flick), PU_LEVEL, NULL);
			memcpy (flick, save_p, sizeof(*flick));
			save_p += sizeof(*flick);
			flick->sector = &sectors[(int)flick->sector];
			flick->thinker.function.acp1 = (actionf_p1)T_FireFlicker;
			P_AddThinker (&flick->thinker);
			break;

		  //jff 2/22/98 new case for elevators
		  case tc_elevator:
			PADSAVEP();
			{
			  elevator_t *elevator = Z_Malloc (sizeof(*elevator), PU_LEVEL, NULL);
			  memcpy (elevator, save_p, sizeof(*elevator));
			  save_p += sizeof(*elevator);
			  elevator->sector = &sectors[(int)elevator->sector];
			  elevator->sector->floordata = elevator; //jff 2/22/98
			  elevator->sector->ceilingdata = elevator; //jff 2/22/98
			  elevator->thinker.function.acp1 = (actionf_p1) T_MoveElevator;
			  P_AddThinker (&elevator->thinker);
			  break;
			}

		  case tc_scroll:       // killough 3/7/98: scroll effect thinkers
			{
			  scroll_t *scroll = Z_Malloc (sizeof(scroll_t), PU_LEVEL, NULL);
			  memcpy (scroll, save_p, sizeof(scroll_t));
			  save_p += sizeof(scroll_t);
			  scroll->thinker.function.acp1 = (actionf_p1) T_Scroll;
			  P_AddThinker(&scroll->thinker);
			  break;
			}

		  default:
			I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
					 "in savegame",tclass);
		}
		
	}

}

// [RH] Save the state of the random number generator
void P_ArchiveRNGState (void)
{
	PADSAVEP();
	memcpy (save_p, &rng, sizeof(rng));
	save_p += sizeof(rng);
}

// [RH] Load the state of the random number generator
void P_UnArchiveRNGState (void)
{
	PADSAVEP();
	memcpy (&rng, save_p, sizeof(rng));
	save_p += sizeof(rng);
}

// [RH] Save level local info
void P_ArchiveLevelLocals (void)
{
	PADSAVEP();
	memcpy (save_p, &level, sizeof(level));
	save_p += sizeof(level);

	if (gamemode != commercial) {
		// Also save list of visited levels
		int epsd, map;
		char name[8] = "E M ";
		level_info_t *info;

		for (epsd = '1'; epsd <= '3'; epsd++) {
			for (map = '1'; map <= '9'; map++) {
				name[1] = epsd;
				name[3] = map;
				if ( (info = FindLevelInfo (name)) ) {
					if (info->level_name && info->flags & LEVEL_VISITED) {
						memcpy (save_p, info->mapname, 8);
						save_p += 8;
					}
				}
			}
		}
	}
	*save_p++ = 0;
}

// [RH] Restore the level local info
void P_UnArchiveLevelLocals (void)
{
	level_info_t *info = level.info;

	PADSAVEP();
	memcpy (&level, save_p, sizeof(level));
	save_p += sizeof(level);
	level.info = info;

	while (*save_p) {
		level_info_t *info;

		if ( (info = FindLevelInfo (save_p)) ) {
			if (info->level_name)
				info->flags |= LEVEL_VISITED;
		}
		save_p += 8;
	}
	save_p++;

	// In case testfade was used, we need to rebuild the colormaps
	RefreshPalettes();
}