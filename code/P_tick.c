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
//		Thinker, Ticker.
//
//-----------------------------------------------------------------------------


#include "z_zone.h"
#include "p_local.h"
#include "p_acs.h"
#include "c_consol.h"

#include "doomstat.h"

extern constate_e ConsoleState;
extern gamestate_t wipegamestate;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//



// Both the head and tail of the thinker list.
thinker_t thinkercap;


//
// P_InitThinkers
//
void P_InitThinkers (void)
{
	thinkercap.prev = thinkercap.next = &thinkercap;
}




//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker (thinker_t* thinker)
{
	thinkercap.prev->next = thinker;
	thinker->next = &thinkercap;
	thinker->prev = thinkercap.prev;
	thinkercap.prev = thinker;
}



//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void P_RemoveThinker (thinker_t* thinker)
{
  // FIXME: NOP.
  thinker->function.acv = (actionf_v)(-1);
}



//
// P_AllocateThinker
// Allocates memory and adds a new thinker at the end of the list.
//
void P_AllocateThinker (thinker_t *thinker)
{
}



//
// P_RunThinkers
//
void P_RunThinkers (void)
{
	thinker_t *currentthinker;

	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		if ( currentthinker->function.acv == (actionf_v)(-1) )
		{
			// time to remove it
			thinker_t *nextthinker;

			nextthinker = currentthinker->next;
			currentthinker->next->prev = currentthinker->prev;
			currentthinker->prev->next = currentthinker->next;
			Z_Free (currentthinker);
			currentthinker = nextthinker;
		}
		else
		{
			if (currentthinker->function.acp1)
				currentthinker->function.acp1 (currentthinker);
			currentthinker = currentthinker->next;
		}
	}
}



//
// P_Ticker
//

void P_Ticker (void)
{
	int i;

	// run the tic
	if (paused)
		return;

	// pause if in menu or console and at least one tic has been run
	if ( !netgame
		 && (menuactive || ConsoleState == c_down || ConsoleState == c_falling)
		 && !demoplayback
		 && !demorecording
		 && players[consoleplayer].viewz != 1
		 && wipegamestate == gamestate)
	{
		return;
	}


	for (i = 0; i<MAXPLAYERS; i++)
		if (playeringame[i])
			P_PlayerThink (&players[i]);

	P_RunThinkers ();
	P_RunScripts ();	// [RH] Execute any active scripts
	P_RunQuakes ();		// [RH] Shake the earth a little
	P_UpdateSpecials ();
	P_RespawnSpecials ();

	// [RH] Apply falling damage
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i]) {
			P_FallingDamage (players[i].mo);

			players[i].oldvelocity[0] = players[i].mo->momx;
			players[i].oldvelocity[1] = players[i].mo->momy;
			players[i].oldvelocity[2] = players[i].mo->momz;
		}

	// for par times
	level.time++;
}
