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
#include "p_saveg.h"
#include "p_acs.h"
#include "s_sndseq.h"
#include "v_palett.h"

extern button_t *buttonlist;

byte *save_p;



//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
	int i, j;
	player_t *dest;
				
	CheckSaveGame (sizeof(player_t) * MAXPLAYERS); // killough
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		
		PADSAVEP();

		dest = (player_t *)save_p;
		memcpy (dest,&players[i],sizeof(player_t));
		save_p += sizeof(player_t);
		for (j = 0; j < NUMPSPRITES; j++)
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
	int i, j;
	userinfo_t userinfo;
		
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		
		PADSAVEP();

		memcpy (&userinfo, &players[i].userinfo, sizeof(userinfo));	// [RH] remember userinfo

		memcpy (&players[i], save_p, sizeof(player_t));
		save_p += sizeof(player_t);
		
		memcpy (&players[i].userinfo, &userinfo, sizeof(userinfo));
		players[i].mo = NULL;			// will be set when unarc thinker
		players[i].attacker = NULL;

		for (j = 0; j < NUMPSPRITES; j++)
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
	int 		i;
	int 		j;
	sector_t*	sec;
	line_t* 	li;
	side_t* 	si;
	short*		put;

	// killough 3/22/98: fix bug caused by hoisting save_p too early

	size_t size = (sizeof(unsigned int)*2+sizeof(float)+sizeof(short)*10)*numsectors
				  + (sizeof(short)*8)*numlines + 4;

	for (i = 0; i < numlines; i++)
	{
		if (lines[i].sidenum[0] != -1)
			size += sizeof(short)*5;
		if (lines[i].sidenum[1] != -1)
			size += sizeof(short)*5;
	}

	CheckSaveGame (size);	// killough

	PADSAVEP();				// killough 3/22/98

	// do sectors
	for (i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		put = (short *)save_p;
		put[0] = (short)(sec->floorheight >> FRACBITS);
		put[1] = (short)(sec->ceilingheight >> FRACBITS);
		put[2] = sec->floorpic;
		put[3] = sec->ceilingpic;
		put[4] = sec->lightlevel;
		put[5] = sec->special;
		put[6] = sec->tag;
		put[7] = sec->damage;	// [RH]
		put[8] = sec->mod;	// [RH]
		put[9] = sec->seqType;	// [RH]
		save_p = (byte *)(put + 10);	// [RH]
		PADSAVEP();	// [RH]
		*((float *)save_p)++ = sec->gravity;	// [RH]
		*((unsigned int *)save_p)++ = sec->colormap->color;	// [RH]
		*((unsigned int *)save_p)++ = sec->colormap->fade;	// [RH];
	}

	put = (short *)save_p;	// [RH]

	// do lines
	for (i = 0, li = lines; i < numlines; i++, li++)
	{
		put[0] = li->flags;
		put[1] = (li->special << 8) | li->lucency;	// [RH]
		put[2] = li->id;		// [RH]
		memcpy (put + 3, li->args, sizeof(li->args)); // [RH]
		put += 3 + sizeof(li->args)/sizeof(*put);

		for (j = 0; j < 2; j++)
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

	PADSAVEP();					// killough 3/22/98
	
	// do sectors
	for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
	{
		unsigned int color, fade;

		get = (short *)save_p;
		sec->floorheight = get[0] << FRACBITS;
		sec->ceilingheight = get[1] << FRACBITS;
		sec->floorpic = get[2];
		sec->ceilingpic = get[3];
		sec->lightlevel = get[4];
		sec->special = get[5];
		sec->tag = get[6];
		sec->damage = get[7];	// [RH]
		sec->mod = get[8];	// [RH]
		sec->seqType = get[9];	// [RH]
		save_p = (byte *)(get + 10);	// [RH]
		PADSAVEP();
		sec->gravity = *((float *)save_p)++;	// [RH]
		color = *((unsigned int *)save_p)++;	// [RH]
		fade = *((unsigned int *)save_p)++;	// [RH]

		sec->colormap = GetSpecialLights (
			RPART(color), GPART(color), BPART(color),
			RPART(fade), GPART(fade), BPART(fade));	// [RH]

		sec->ceilingdata = 0;	//jff 2/22/98 now three thinker fields not two
		sec->floordata = 0;
		sec->lightingdata = 0;
		sec->soundtarget = 0;
	}

	get = (short *)save_p;	// [RH]

	// do lines
	for (i=0, li = lines ; i<numlines ; i++,li++)
	{
		li->flags = *get++;
		li->special = *get >> 8;	// [RH]
		li->lucency = (*get++) & 255;	// [RH]
		li->id = *get++;	// [RH]
		memcpy (li->args, get, sizeof(li->args));	// [RH]
		get += sizeof(li->args) / sizeof(*get);

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
// 2/14/98 killough: substantially modified to fix savegame bugs

void P_ArchiveThinkers (void)
{
	thinker_t  *th;
	size_t		size = 0;

	CheckSaveGame (sizeof brain);	// killough 3/26/98: Save boss brain state
	memcpy (save_p, &brain, sizeof brain);
	save_p += sizeof brain;

	// killough 2/14/98:
	// count the number of thinkers, and mark each one with its index, using
	// the prev field as a placeholder, since it can be restored later.

	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
		if (th->function.acp1 == (actionf_p1) P_MobjThinker)
			th->prev = (thinker_t *) ++size;

	// check that enough room is available in savegame buffer
	CheckSaveGame (size*(sizeof(mobj_t)+4));	// killough 2/14/98

	// save off the current thinkers
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
		if (th->function.acp1 == (actionf_p1) P_MobjThinker)
		{
			mobj_t *mobj;

			*save_p++ = tc_mobj;
			PADSAVEP();
			mobj = (mobj_t *)save_p;
			memcpy (mobj, th, sizeof(*mobj));
			save_p += sizeof(*mobj);
			mobj->state = (state_t *)(mobj->state - states);

			// killough 2/14/98: convert pointers into indices.
			// Fixes many savegame problems, by properly saving
			// target and tracer fields. Note: we store NULL if
			// the thinker pointed to by these fields is not a
			// mobj thinker.

			if (mobj->target)
				mobj->target = mobj->target->thinker.function.acp1 ==
					(actionf_p1) P_MobjThinker ?
					(mobj_t *) mobj->target->thinker.prev : NULL;

			if (mobj->tracer)
				mobj->tracer = mobj->tracer->thinker.function.acp1 ==
					(actionf_p1) P_MobjThinker ?
					(mobj_t *) mobj->tracer->thinker.prev : NULL;

			if (mobj->goal)		// [RH] Save goal properly
				mobj->goal = mobj->goal->thinker.function.acp1 ==
					(actionf_p1) P_MobjThinker ?
					(mobj_t *) mobj->goal->thinker.prev : NULL;

			// killough 2/14/98: new field: save last known enemy. Prevents
			// monsters from going to sleep after killing monsters and not
			// seeing player anymore.

			if (mobj->lastenemy)
				mobj->lastenemy = mobj->lastenemy->thinker.function.acp1 ==
					(actionf_p1) P_MobjThinker ?
					(mobj_t *) mobj->lastenemy->thinker.prev : NULL;

			// killough 2/14/98: end changes

			if (mobj->player)
				mobj->player = (player_t *)((mobj->player-players) + 1);
		}

	// add a terminating marker
	*save_p++ = tc_end; 

	// [RH] Don't restore mobj prev fields until the end of
	//		P_ArchiveThinkers() since we need the indices for
	//		archiving ACS scripts, too.

	// killough 2/14/98: end changes
}



//
// P_UnArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs

// [RH] Moved this out of P_UnArchiveThinkers so that it's visible
//		within P_UnArchiveScripts() and P_UnArchiveSpecials().
static mobj_t    **mobj_p;		// killough 2/14/98: Translation table

// [RH] Added keepPlayers. When true, the player mobjs that were
//		spawned during level initialization are kept instead of
//		being replaced by the ones in the snapshot.
void P_UnArchiveThinkers (BOOL keepPlayers)
{
	thinker_t *th;
	size_t    size;			// killough 2/14/98: size of or index into table

	// killough 3/26/98: Load boss brain state
	memcpy (&brain, save_p, sizeof brain);
	save_p += sizeof brain;

	// remove all the current thinkers
	P_ClearTidHashes ();	// [RH] Clear the tid hash table

	for (th = thinkercap.next; th != &thinkercap; )
	{
		thinker_t *next = th->next;
		if (th->function.acp1 == (actionf_p1) P_MobjThinker) {
			if (!keepPlayers || !(((mobj_t *)th)->player))
				P_RemoveMobj ((mobj_t *) th);
		} else {
			// time to remove it
			th->next->prev = th->prev;
			th->prev->next = th->next;
			Z_Free (th);
		}
		th = next;
	}

//	if (!keepPlayers)
//		P_InitThinkers ();

	// killough 2/14/98: count number of thinkers by skipping through them
	{
		byte *sp = save_p;	// save pointer and skip header
		for (size = 1; *save_p++ == tc_mobj; size++)	// killough 2/14/98
		{					// skip all entries, adding up count
			PADSAVEP();
			save_p += sizeof(mobj_t);
		}

		if (*--save_p != tc_end)
			I_Error ("Unknown tclass %i in snapshot", *save_p);

		// first table entry special: 0 maps to NULL
		*(mobj_p = Z_Malloc(size * sizeof *mobj_p, PU_STATIC, 0)) = 0;	// table of pointers
		save_p = sp;		// restore save pointer
	}

	// read in saved thinkers
	for (size = 1; *save_p++ == tc_mobj; size++)		// killough 2/14/98
	{
		mobj_t *mobj = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);

		// killough 2/14/98 -- insert pointers to thinkers into table, in order:
		mobj_p[size] = mobj;

		PADSAVEP();
		memcpy (mobj, save_p, sizeof(mobj_t));
		save_p += sizeof(mobj_t);
		mobj->state = states + (int) mobj->state;

		if (mobj->player) {
			// [RH] If we're re-entering a level as part of movement in a hub,
			//		use the currently existing player mobj instead of the one
			//		in the snapshot. Note that this doesn't work with voodoo
			//		dolls, and fixing that isn't worth the trouble since
			//		scripting offers everything voodoo dolls did and more.
			if (keepPlayers) {
				Z_Free (mobj);
				mobj_p[size] = players[(int)mobj->player - 1].mo;
				continue;
			}
			(mobj->player = &players[(int) mobj->player - 1]) -> mo = mobj;
			mobj->player->camera = mobj;	// [RH] Reset the camera to the player's viewpoint
		}

		P_SetThingPosition (mobj);
		mobj->info = &mobjinfo[mobj->type];

		// killough 2/28/98:
		// Fix for falling down into a wall after savegame loaded:
		//      mobj->floorz = mobj->subsector->sector->floorheight;
		//      mobj->ceilingz = mobj->subsector->sector->ceilingheight;

		mobj->thinker.function.acp1 = (actionf_p1) P_MobjThinker;
		P_AddThinker (&mobj->thinker);

		P_AddMobjToHash (mobj);	// [RH] Add mobj to tid hash table
	}

	// killough 2/14/98: adjust target and tracer fields, plus
	// lastenemy field, to correctly point to mobj thinkers.
	// NULL entries automatically handled by first table entry.

	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function.acp1 != (actionf_p1) P_MobjThinker)
			continue;

		if (!keepPlayers || !((mobj_t *)th)->player) {
			((mobj_t *) th)->target =
			  mobj_p[(size_t)((mobj_t *)th)->target];

			((mobj_t *) th)->tracer =
			  mobj_p[(size_t)((mobj_t *)th)->tracer];

			((mobj_t *) th)->lastenemy =
			  mobj_p[(size_t)((mobj_t *)th)->lastenemy];

			((mobj_t *) th)->goal =		// [RH] restore goal
			  mobj_p[(size_t)((mobj_t *)th)->goal];
		}
	}

	// killough 3/26/98: Spawn icon landings:
	if (gamemode == commercial)
		P_SpawnBrainTargets();
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
	tc_elevator,	//jff 2/22/98 new elevator type thinker
	tc_scroll,		// killough 3/7/98: new scroll effect thinker
	tc_glow2,
	tc_waggle,
	tc_phased,
	tc_pillar,
	tc_friction,	// phares 3/18/98:  new friction effect thinker
	tc_pusher,		// phares 3/22/98:  new push/pull effect thinker
	tc_rotate_poly,
	tc_move_poly,
	tc_poly_door,
	tc_flicker,
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
// T_FireFlicker (fireflicker_t: sector_t * swizzle),	// [RH] Was missed in original code
// T_Flicker (flicker_t: sector_t * swizzle)			// [RH] Hexen flicker
// T_MoveElevator, (plat_t: sector_t *), - active list	// jff 2/22/98
// T_Scroll												// killough 3/7/98
// T_Glow2, (glow2_t: sector_t *)						// [RH]
// T_Waggle, (floorWaggle_t: sector_t *)				// [RH]
// T_PhasedLight, (phased_t: sector_t *)				// [RH]
// T_Pillar (pillar_t: sector_t *)						// [RH]
// T_Pusher	(pusher_t: mobj_t *)						// phares 3/22/98
// T_RotatePoly
// T_MovePoly
// T_PolyDoor
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
	button_t*			button;
	quake_t*			quake;
		
	size_t				size = 0;			// killough

	// save off the current thinkers (memory size calculation -- killough)

	// [RH] save buttonlist, too
	button = buttonlist;
	while (button) {
		size += sizeof(*button);
		button = button->next;
	}
	size++;

	// [RH] Also save earthquakes
	quake = ActiveQuakes;
	while (quake) {
		size += sizeof(*quake);
		quake = quake->next;
	}
	size++;

	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
		if (th->function.acv == (actionf_v)NULL)
		{
			// [RH] Search for active plat or ceiling list that's in-stasis
			for (ceiling = activeceilings; ceiling; ceiling = ceiling->next)
				if (ceiling == (ceiling_t *)th)
					break;

			if (ceiling) {
				size += 4+sizeof(ceiling_t);
				goto end;
			}

			for (plat = activeplats; plat; plat = plat->next)
				if (plat == (plat_t *)th)
					break;

			if (plat) {
				size += 4+sizeof(plat_t);
				goto end;
			}
			end:;
		}
		else
			size +=
				th->function.acp1==(actionf_p1)T_MoveCeiling  ? 4+sizeof(ceiling_t)	:
				th->function.acp1==(actionf_p1)T_VerticalDoor ? 4+sizeof(vldoor_t)	:
				th->function.acp1==(actionf_p1)T_MoveFloor    ? 4+sizeof(floormove_t):
				th->function.acp1==(actionf_p1)T_PlatRaise    ? 4+sizeof(plat_t)	:
				th->function.acp1==(actionf_p1)T_LightFlash   ? 4+sizeof(lightflash_t):
				th->function.acp1==(actionf_p1)T_StrobeFlash  ? 4+sizeof(strobe_t)	:
				th->function.acp1==(actionf_p1)T_Glow		  ? 4+sizeof(glow_t)	:
				th->function.acp1==(actionf_p1)T_Glow2		  ? 4+sizeof(glow2_t)	:
				th->function.acp1==(actionf_p1)T_FireFlicker  ? 4+sizeof(fireflicker_t):
				th->function.acp1==(actionf_p1)T_Flicker	  ? 4+sizeof(fireflicker_t):
				th->function.acp1==(actionf_p1)T_PhasedLight  ? 4+sizeof(phased_t)	:
				th->function.acp1==(actionf_p1)T_MoveElevator ? 4+sizeof(elevator_t):
				th->function.acp1==(actionf_p1)T_Scroll		  ? 4+sizeof(scroll_t)	:
				th->function.acp1==(actionf_p1)T_FloorWaggle  ? 4+sizeof(floorWaggle_t):
				th->function.acp1==(actionf_p1)T_Pillar		  ? 4+sizeof(pillar_t)	:
				th->function.acp1==(actionf_p1)T_Pusher		  ? 4+sizeof(pusher_t)	:
				0;

	CheckSaveGame(size);		// killough

	// [RH] save the active buttons
	button = buttonlist;
	if (button) {
		*save_p++ = 1;
		while (button) {
			button_t *but;
			PADSAVEP();
			memcpy (save_p, button, sizeof(*button));
			but = (button_t *)save_p;
			save_p += sizeof(*button);
			but->line = (line_t *)(but->line ? but->line - lines : -1);
			button = button->next;
		}
	} else {
		*save_p++ = 0;
	}

	// [RH] save the active quakes
	quake = ActiveQuakes;
	if (quake) {
		*save_p++ = 1;
		while (quake) {
			quake_t *q;
			PADSAVEP();
			memcpy (save_p, quake, sizeof(*quake));
			q = (quake_t *)save_p;
			q->quakespot = (mobj_t *)q->quakespot->thinker.prev;
			save_p += sizeof(*quake);
			quake = quake->next;
		}
	} else {
		*save_p++ = 0;
	}

	// save off the current thinkers
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function.acv == (actionf_v)NULL)
		{
			// [RH] Use the linked list for both plats and ceilings
			for (ceiling = activeceilings; ceiling; ceiling = ceiling->next)
				if (ceiling == (ceiling_t *)th)
					goto ceiling;

			for (plat = activeplats; plat; plat = plat->next)
				if (plat == (plat_t *)th)
					goto plat;

			continue;
		}

		if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
		{
		ceiling:
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
		plat:
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
		else if (th->function.acp1 == (actionf_p1)T_Flicker)
		{
			fireflicker_t *flick;

			*save_p++ = tc_flicker;
			PADSAVEP();
			flick = (fireflicker_t *)save_p;
			memcpy (flick, th, sizeof(*flick));
			save_p += sizeof(*flick);
			flick->sector = (sector_t *)(flick->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1)T_MoveElevator)
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
		else if (th->function.acp1 == (actionf_p1) T_Glow2)
		{		// [RH] Second type of glowing sectors
			glow2_t *glow;

			*save_p++ = tc_glow2;
			PADSAVEP();
			glow = (glow2_t *)save_p;
			memcpy (glow, th, sizeof(*glow));
			save_p += sizeof(*glow);
			glow->sector = (sector_t *)(glow->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_FloorWaggle)
		{		// [RH] Waggling floors
			floorWaggle_t *waggle;

			*save_p++ = tc_waggle;
			PADSAVEP();
			waggle = (floorWaggle_t *)save_p;
			memcpy (waggle, th, sizeof(*waggle));
			save_p += sizeof(*waggle);
			waggle->sector = (sector_t *)(waggle->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_PhasedLight)
		{		// [RH] Phased lighting
			phased_t *phased;

			*save_p++ = tc_phased;
			PADSAVEP();
			phased = (phased_t *)save_p;
			memcpy (phased, th, sizeof(*phased));
			save_p += sizeof(*phased);
			phased->sector = (sector_t *)(phased->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_Pillar)
		{		// [RH] Pillar builders
			pillar_t *pillar;

			*save_p++ = tc_pillar;
			PADSAVEP();
			pillar = (pillar_t *)save_p;
			memcpy (pillar, th, sizeof(*pillar));
			save_p += sizeof(*pillar);
			pillar->sector = (sector_t *)(pillar->sector - sectors);
		}
		else if (th->function.acp1 == (actionf_p1) T_Pusher)
		{		// phares 3/22/98: Push/Pull effect thinkers
			pusher_t *pusher;
			*save_p++ = tc_pusher;
			PADSAVEP();
			pusher = (pusher_t *)save_p;
			memcpy (save_p, th, sizeof(*pusher));
			save_p += sizeof(*pusher);
			pusher->source = (mobj_t *) pusher->source->thinker.prev;	// [RH] remember source
			continue;
		}
		else if (th->function.acp1 == (actionf_p1) T_RotatePoly)
		{
			*save_p++ = tc_rotate_poly;
			memcpy (save_p, th, sizeof(polyevent_t));
			save_p += sizeof(polyevent_t);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1) T_MovePoly)
		{
			*save_p++ = tc_move_poly;
			memcpy (save_p, th, sizeof(polyevent_t));
			save_p += sizeof(polyevent_t);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1) T_PolyDoor)
		{
			*save_p++ = tc_poly_door;
			memcpy (save_p, th, sizeof(polydoor_t));
			save_p += sizeof(polydoor_t);
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
		
	// [RH] read the active buttons
	if (*save_p++) {
		button_t *button, **buttonptr;
		buttonptr = &buttonlist;
		do {
			button = Z_Malloc (sizeof(*button), PU_LEVEL, 0);
			PADSAVEP();
			memcpy (button, save_p, sizeof(*button));
			button->line = (((int)button->line) == -1) ? NULL : &lines[(int)button->line];
			*buttonptr = button;
			buttonptr = &button->next;
			save_p += sizeof(*button);
		} while (*buttonptr);
	}

	// [RH] read the active quakes
	if (*save_p++) {
		quake_t *quake, **quakeptr;
		quakeptr = &ActiveQuakes;
		do {
			quake = Z_Malloc (sizeof(*quake), PU_LEVEL, 0);
			PADSAVEP();
			memcpy (quake, save_p, sizeof(*quake));
			*quakeptr = quake;
			quake->quakespot = (mobj_t *)mobj_p[(size_t)(quake->quakespot)];
			quakeptr = &quake->next;
			save_p += sizeof(*quake);
		} while (*quakeptr);
	}

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

		  case tc_flicker:
			PADSAVEP();
			flick = Z_Malloc (sizeof(*flick), PU_LEVEL, NULL);
			memcpy (flick, save_p, sizeof(*flick));
			save_p += sizeof(*flick);
			flick->sector = &sectors[(int)flick->sector];
			flick->thinker.function.acp1 = (actionf_p1)T_Flicker;
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

		  case tc_glow2:		// [RH] Another style of glowing light
			{
			  glow2_t *glow2 = Z_Malloc (sizeof(*glow2), PU_LEVEL, NULL);
			  PADSAVEP();
			  memcpy (glow2, save_p, sizeof(*glow2));
			  save_p += sizeof(*glow2);
			  glow2->sector = &sectors[(int)glow2->sector];
			  glow2->thinker.function.acp1 = (actionf_p1)T_Glow2;
			  P_AddThinker (&glow2->thinker);
			  break;
			}
			
		  case tc_waggle:		// [RH] Waggling floors
			{
			  floorWaggle_t *waggle = Z_Malloc (sizeof(*waggle), PU_LEVEL, NULL);
			  PADSAVEP();
			  memcpy (waggle, save_p, sizeof(*waggle));
			  save_p += sizeof(*waggle);
			  waggle->sector = &sectors[(int)waggle->sector];
			  waggle->sector->floordata = waggle;
			  waggle->thinker.function.acp1 = (actionf_p1)T_FloorWaggle;
			  P_AddThinker (&waggle->thinker);
			  break;
			}

		  case tc_phased:		// [RH] Phased lighting
			{
			  phased_t *phased = Z_Malloc (sizeof(*phased), PU_LEVEL, NULL);
			  PADSAVEP();
			  memcpy (phased, save_p, sizeof(*phased));
			  save_p += sizeof(*phased);
			  phased->sector = &sectors[(int)phased->sector];
			  phased->thinker.function.acp1 = (actionf_p1)T_PhasedLight;
			  P_AddThinker (&phased->thinker);
			  break;
			}

		  case tc_pillar:		// [RH] Pillar builders
			{
			  pillar_t *pillar = Z_Malloc (sizeof(*pillar), PU_LEVEL, NULL);
			  PADSAVEP();
			  memcpy (pillar, save_p, sizeof(*pillar));
			  save_p += sizeof(*pillar);
			  pillar->sector = &sectors[(int)pillar->sector];
			  pillar->thinker.function.acp1 = (actionf_p1)T_Pillar;
			  P_AddThinker (&pillar->thinker);
			  break;
			}

		  case tc_pusher:		// phares 3/22/98: new Push/Pull effect thinkers
			{
			  pusher_t *pusher = Z_Malloc (sizeof(pusher_t), PU_LEVEL, NULL);
			  PADSAVEP();
			  memcpy (pusher, save_p, sizeof(pusher_t));
			  save_p += sizeof(pusher_t);
			  pusher->thinker.function.acp1 = (actionf_p1) T_Pusher;
			  pusher->source = mobj_p[(size_t)pusher->source];	// [RH] remember source
			  P_AddThinker (&pusher->thinker);
			  break;
			}

		  case tc_rotate_poly:
			{
			  polyevent_t *event = Z_Malloc (sizeof(polyevent_t), PU_LEVEL, NULL);
			  memcpy (event, save_p, sizeof(polyevent_t));
			  save_p += sizeof(polyevent_t);
			  event->thinker.function.acp1 = (actionf_p1) T_RotatePoly;
			  P_AddThinker (&event->thinker);
			}
			break;

		  case tc_move_poly:
			{
			  polyevent_t *event = Z_Malloc (sizeof(polyevent_t), PU_LEVEL, NULL);
			  memcpy (event, save_p, sizeof(polyevent_t));
			  save_p += sizeof(polyevent_t);
			  event->thinker.function.acp1 = (actionf_p1) T_MovePoly;
			  P_AddThinker (&event->thinker);
			}
			break;

		  case tc_poly_door:
			{
			  polyevent_t *door = Z_Malloc (sizeof(polydoor_t), PU_LEVEL, NULL);
			  memcpy (door, save_p, sizeof(polydoor_t));
			  save_p += sizeof(polydoor_t);
			  door->thinker.function.acp1 = (actionf_p1) T_PolyDoor;
			  P_AddThinker (&door->thinker);
			}
			break;

		  default:
			I_Error ("P_UnarchiveSpecials: Unknown tclass %i in snapshot", tclass);
		}
	}
}

// [RH] Save the state of the random number generator
void P_ArchiveRNGState (void)
{
	CheckSaveGame (sizeof(rng));
	memcpy (save_p, &rng, sizeof(rng));
	save_p += sizeof(rng);
}

// [RH] Load the state of the random number generator
void P_UnArchiveRNGState (void)
{
	memcpy (&rng, save_p, sizeof(rng));
	save_p += sizeof(rng);
}


typedef enum
{
	tc_script = 98,
	tc_scrend
} scriptclass_t;

// [RH] Store state of all running scripts
void P_ArchiveScripts (void)
{
	script_t *scr = Scripts, *script;

	while (scr) {
		CheckSaveGame (sizeof(*scr));
		*save_p++ = tc_script;
		PADSAVEP();

		script = (script_t *)save_p;
		memcpy (script, scr, sizeof(*script));
		save_p += sizeof(*script);
		script->pc = (int *)(script->pc - (int *)level.behavior);
		script->activationline = (line_t *)
			(script->activationline ? script->activationline - lines : -1);
		if (script->activator)
			script->activator = script->activator->thinker.function.acp1 ==
				(actionf_p1) P_MobjThinker ?
				(mobj_t *) script->activator->thinker.prev : NULL;
		if (RunningScripts[script->script] != scr)
			script->script += 2000;	// ACS_ExecuteAlways script
		scr = script->next;
	}
	*save_p++ = tc_scrend;

	// killough 2/14/98: restore prev pointers
	{
		thinker_t *prev = &thinkercap, *th;
		for (th = thinkercap.next ; th != &thinkercap ; prev=th, th=th->next)
			th->prev = prev;
	}
}

// [RH] Restore state of all running scripts
void P_UnArchiveScripts (void)
{
	script_t **scripttail = &Scripts, *scriptprev = NULL;

	P_ClearScripts ();
	Z_FreeTags (PU_LEVACS, PU_LEVACS);

	while (*save_p++ == tc_script) {
		script_t *script = Z_Malloc (sizeof(*script), PU_LEVACS, NULL);
		PADSAVEP();
		memcpy (script, save_p, sizeof(*script));
		save_p += sizeof(*script);
		script->pc = ((int)script->pc) + (int *)level.behavior;
		script->activationline = (((int)script->activationline) == -1) ? NULL :
								 &lines[(int)script->activationline];
		script->activator = mobj_p[(size_t)script->activator];
		if (script->script < 2000)
			RunningScripts[script->script] = script;
		else
			script->script -= 2000;

		*scripttail = script;
		scripttail = &script->next;
		script->prev = scriptprev;
		scriptprev = script;
	}

	*scripttail = NULL;
	LastScript = scriptprev;

	Z_Free (mobj_p);	// free translation table

	if (*(save_p - 1) != tc_scrend)
		I_Error ("Unknown sclass %d in snapshot", *(save_p - 1));
}

//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

#define ASEG_SOUNDS 109

void P_ArchiveSounds (void)
{
	seqnode_t *node;
	sector_t *sec;
	int difference;
	int i;

	WriteLong (ASEG_SOUNDS, &save_p);

	// Save the sound sequences
	WriteLong (ActiveSequences, &save_p);
	for (node = SequenceListHead; node; node = node->next)
	{
		WriteString (Sequences[node->sequence]->name, &save_p);
		WriteLong (node->delayTics, &save_p);
		WriteLong (*((int *)&node->volume), &save_p);
		WriteLong (SN_GetSequenceOffset (node->sequence,
			node->sequencePtr), &save_p);
		WriteString (S_sfx[node->currentSoundID].name, &save_p);
		for (i = 0; i < po_NumPolyobjs; i++)
		{
			if (node->mobj == (mobj_t *)&polyobjs[i].startSpot)
			{
				break;
			}
		}
		if (i == po_NumPolyobjs)
		{ // Sound is attached to a sector, not a polyobj
			sec = R_PointInSubsector(node->mobj->x, node->mobj->y)->sector;
			difference = (int)((byte *)sec
				-(byte *)&sectors[0])/sizeof(sector_t);
			WriteLong (0, &save_p); // 0 -- sector sound origin
		}
		else
		{
			WriteLong (1, &save_p); // 1 -- polyobj sound origin
			difference = i;
		}
		WriteLong (difference, &save_p);
	}
}

//==========================================================================
//
// UnarchiveSounds
//
//==========================================================================

void P_UnArchiveSounds (void)
{
	int i;
	int numSequences;
	int delayTics;
	int volTemp;
	float volume;
	int seqOffset;
	int polySnd;
	int secNum;
	char *soundName;
	char *sequenceName;
	mobj_t *sndMobj;

	i = ReadLong (&save_p);
	if (i != ASEG_SOUNDS)
		I_Error ("Sound sequence marker missing");

	// Reload and restart all sound sequences
	numSequences = ReadLong (&save_p);
	i = 0;
	while (i < numSequences)
	{
		sequenceName = ReadString (&save_p);
		delayTics = ReadLong (&save_p);
		volTemp = ReadLong (&save_p);
		volume = *((float *)&volTemp);
		seqOffset = ReadLong (&save_p);

		soundName = ReadString (&save_p);
		polySnd = ReadLong (&save_p);
		secNum = ReadLong (&save_p);
		if (!polySnd)
		{
			sndMobj = (mobj_t *)&sectors[secNum].soundorg;
		}
		else
		{
			sndMobj = (mobj_t *)&polyobjs[secNum].startSpot;
		}
		SN_StartSequenceName (sndMobj, sequenceName);
		SN_ChangeNodeData (i, seqOffset, delayTics, volume, S_FindSound (soundName));
		free (soundName);
		free (sequenceName);
		i++;
	}
}

//==========================================================================
//
// ArchivePolyobjs
//
//==========================================================================
#define ASEG_POLYOBJS	104

void P_ArchivePolyobjs (void)
{
	int i;

	WriteLong (ASEG_POLYOBJS, &save_p);
	WriteLong (po_NumPolyobjs, &save_p);
	for(i = 0; i < po_NumPolyobjs; i++)
	{
		WriteLong (polyobjs[i].tag, &save_p);
		WriteLong (polyobjs[i].angle, &save_p);
		WriteLong (polyobjs[i].startSpot.x, &save_p);
		WriteLong (polyobjs[i].startSpot.y, &save_p);
  	}
}

//==========================================================================
//
// UnarchivePolyobjs
//
//==========================================================================

void P_UnArchivePolyobjs (void)
{
	int i, data;
	fixed_t deltaX;
	fixed_t deltaY;

	data = ReadLong (&save_p);
	if (data != ASEG_POLYOBJS)
		I_Error ("Polyobject marker missing");

	if (ReadLong (&save_p) != po_NumPolyobjs)
	{
		I_Error ("UnarchivePolyobjs: Bad polyobj count");
	}
	for (i = 0; i < po_NumPolyobjs; i++)
	{
		if (ReadLong (&save_p) != polyobjs[i].tag)
		{
			I_Error ("UnarchivePolyobjs: Invalid polyobj tag");
		}
		PO_RotatePolyobj (polyobjs[i].tag, (angle_t)ReadLong (&save_p));
		deltaX = ReadLong (&save_p) - polyobjs[i].startSpot.x;
		deltaY = ReadLong (&save_p) - polyobjs[i].startSpot.y;
		PO_MovePolyobj (polyobjs[i].tag, deltaX, deltaY);
	}
}
