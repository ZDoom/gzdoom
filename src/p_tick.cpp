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
#include "s_sound.h"
#include "doomstat.h"
#include "sbar.h"
#include "r_interpolate.h"

extern gamestate_t wipegamestate;

//==========================================================================
//
// P_CheckTickerPaused
//
// Returns true if the ticker should be paused. In that cause, it also
// pauses sound effects and possibly music. If the ticker should not be
// paused, then it returns false but does not unpause anything.
//
//==========================================================================

bool P_CheckTickerPaused ()
{
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
		S_PauseSound (!(level.flags & LEVEL_PAUSE_MUSIC_IN_MENUS));
		return true;
	}
	return false;
}

//
// P_Ticker
//
void P_Ticker (void)
{
	int i;

	interpolator.UpdateInterpolations ();
	r_NoInterpolate = true;

	// run the tic
	if (paused || (playerswiping && !demoplayback) || P_CheckTickerPaused())
		return;

	// [RH] Frozen mode is only changed every 4 tics, to make it work with A_Tracer().
	if ((level.time & 3) == 0)
	{
		if (bglobal.changefreeze)
		{
			bglobal.freeze ^= 1;
			bglobal.changefreeze = 0;
		}
	}

	// [BC] Do a quick check to see if anyone has the freeze time power. If they do,
	// then don't resume the sound, since one of the effects of that power is to shut
	// off the music.
	for (i = 0; i < MAXPLAYERS; i++ )
	{
		if (playeringame[i] && players[i].cheats & CF_TIMEFREEZE)
			break;
	}

	if ( i == MAXPLAYERS )
		S_ResumeSound ();

	P_ResetSightCounters (false);

	// Since things will be moving, it's okay to interpolate them in the renderer.
	r_NoInterpolate = false;

	if (!bglobal.freeze && !(level.flags & LEVEL_FROZEN))
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
	if (!bglobal.freeze && !(level.flags & LEVEL_FROZEN))
	{
		P_UpdateSpecials ();
		P_RunEffects ();	// [RH] Run particle effects
	}

	// for par times
	level.time++;
	level.maptime++;
	level.totaltime++;
}
