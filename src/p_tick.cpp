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
//		Ticker.
//
//-----------------------------------------------------------------------------


#include "p_local.h"
#include "p_effect.h"
#include "p_acs.h"
#include "c_console.h"
#include "b_bot.h"

#include "doomstat.h"
#include "sbar.h"

extern gamestate_t wipegamestate;

//
// P_Ticker
//
void P_Ticker (void)
{
	int i;

	updateinterpolations ();
	r_NoInterpolate = true;

	// run the tic
	if (paused)
		return;

	// pause if in menu or console and at least one tic has been run
	if ( !netgame
		 && gamestate != GS_TITLELEVEL
		 && ((menuactive != MENU_Off && menuactive != MENU_OnNoPause) ||
			 ConsoleState == c_down || ConsoleState == c_falling)
		 && !demoplayback
		 && !demorecording
		 && players[consoleplayer].viewz != 1
		 && wipegamestate == gamestate)
	{
		return;
	}

	P_ResetSightCounters (false);

	// Since things will be moving, it's okay to interpolate them in the renderer.
	r_NoInterpolate = false;

	if (!bglobal.freeze)
	{
		P_ThinkParticles ();	// [RH] make the particles think
	}
	StatusBar->Tick ();		// [RH] moved this here

	for (i = 0; i<MAXPLAYERS; i++)
		if (playeringame[i] &&
			/*Added by MC: Freeze mode.*/!(bglobal.freeze && players[i].isbot))
			P_PlayerThink (&players[i]);

	level.Tick ();			// [RH] let the level tick
	DThinker::RunThinkers ();

	//if added by MC: Freeze mode.
	if (!bglobal.freeze)
	{
		P_UpdateSpecials ();
		P_RunEffects ();	// [RH] Run particle effects
	}

	// for par times
	level.time++;
}
