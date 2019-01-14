//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Ticker.
//
//-----------------------------------------------------------------------------

#include "p_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "b_bot.h"
#include "doomstat.h"
#include "sbar.h"
#include "r_data/r_interpolate.h"
#include "d_player.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "events.h"
#include "actorinlines.h"

extern gamestate_t wipegamestate;
extern bool globalfreeze;

//==========================================================================
//
// P_CheckTickerPaused
//
// Returns true if the ticker should be paused. In that case, it also
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
		auto nopause = gamestate != GS_LEVEL || !(currentSession->Levelinfo[0]->flags2 & LEVEL2_PAUSE_MUSIC_IN_MENUS);
		S_PauseSound (nopause, false);
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

	// This must be done before the pause check
	ForAllLevels([](FLevelLocals *Level)
	{
		Level->interpolator.UpdateInterpolations();
	});

	r_NoInterpolate = true;

	// run the tic
	if (paused || P_CheckTickerPaused())
		return;

	DPSprite::NewTick();

	// [BC] Do a quick check to see if anyone has the freeze time power. If they do,
	// then don't resume the sound, since one of the effects of that power is to shut
	// off the music.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].timefreezer != 0)
			break;
	}

	if (i == MAXPLAYERS)
		S_ResumeSound(false);

	P_ResetSightCounters(false);
	R_ClearInterpolationPath();

	ForAllLevels([](FLevelLocals *Level)
	{
		// [RH] Frozen mode is only changed every 4 tics, to make it work with A_Tracer().
		if ((Level->maptime & 3) == 0)
		{
			if (currentSession->changefreeze)
			{
				currentSession->frozenstate ^= currentSession->changefreeze;
				currentSession->changefreeze = 0;
				globalfreeze = !!(currentSession->isFrozen() & 2);	// for ZScript backwards compatibiity.
			}
		}

		// Reset all actor interpolations for all actors before the current thinking turn so that indirect actor movement gets properly interpolated.
		TThinkerIterator<AActor> it(Level);
		AActor *ac;

		while ((ac = it.Next()))
		{
			ac->ClearInterpolation();
		}

		// Since things will be moving, it's okay to interpolate them in the renderer.
		r_NoInterpolate = false;

		P_ThinkParticles(Level);	// [RH] make the particles think
	});

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i]) P_PlayerThink(&players[i]);
	}

	// [ZZ] call the WorldTick hook
	E_WorldTick();
	StatusBar->SetLevel(currentSession->Levelinfo[0]);
	StatusBar->CallTick ();		// [RH] moved this here

	ForAllLevels([](FLevelLocals *Level)
	{
		// Reset carry sectors
		if (Level->Scrolls.Size() > 0)
		{
			memset(&Level->Scrolls[0], 0, sizeof(Level->Scrolls[0]) * Level->Scrolls.Size());
		}

		Thinkers.RunThinkers(Level);

		//if added by MC: Freeze mode.
		if (!currentSession->isFrozen())
		{
			P_UpdateSpecials(Level);
			P_RunEffects(Level);	// [RH] Run particle effects
		}

		// for par times
		Level->maptime++;
	});
	currentSession->time++;
	currentSession->totaltime++;
}
