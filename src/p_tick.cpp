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

#include <float.h>
#include "p_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "b_bot.h"
#include "s_sound.h"
#include "doomstat.h"
#include "sbar.h"
#include "r_data/r_interpolate.h"
#include "i_sound.h"
#include "d_player.h"
#include "g_level.h"
#include "r_utility.h"
#include "p_spec.h"

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
		 && players[consoleplayer].viewz != NO_VALUE
		 && wipegamestate == gamestate)
	{
		S_PauseSound (!(level.flags2 & LEVEL2_PAUSE_MUSIC_IN_MENUS), false);
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

	if (!demoplayback)
	{
		// This is a separate slot from the wipe in D_Display(), because this
		// is delayed slightly due to latency. (Even on a singleplayer game!)
//		GSnd->SetSfxPaused(!!playerswiping, 2);
	}

	// run the tic
	if (paused || P_CheckTickerPaused())
		return;

	DPSprite::NewTick();

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
		if (playeringame[i] && players[i].timefreezer != 0)
			break;
	}

	if ( i == MAXPLAYERS )
		S_ResumeSound (false);

	P_ResetSightCounters (false);
	R_ClearInterpolationPath();

	// Since things will be moving, it's okay to interpolate them in the renderer.
	r_NoInterpolate = false;

	P_ThinkParticles();	// [RH] make the particles think

	for (i = 0; i<MAXPLAYERS; i++)
		if (playeringame[i] &&
			/*Added by MC: Freeze mode.*/!(bglobal.freeze && players[i].Bot != NULL))
			P_PlayerThink (&players[i]);

	StatusBar->Tick ();		// [RH] moved this here
	level.Tick ();			// [RH] let the level tick
	DThinker::RunThinkers ();

	//if added by MC: Freeze mode.
	if (!bglobal.freeze && !(level.flags2 & LEVEL2_FROZEN))
	{
		P_UpdateSpecials ();
		P_RunEffects ();	// [RH] Run particle effects
	}

	// for par times
	level.time++;
	level.maptime++;
	level.totaltime++;
}
