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
#include "g_game.h"
#include "am_map.h"
#include "i_interface.h"

extern gamestate_t wipegamestate;
extern uint8_t globalfreeze, globalchangefreeze;

void C_Ticker();
void M_Ticker();
void D_RunCutscene();

//==========================================================================
//
// P_RunClientsideLogic
//
// Handles all logic that should be ran every tick including while
// predicting. Only put non-playsim behaviors in here to avoid desyncs
// when playing online.
//
//==========================================================================

void P_RunClientsideLogic()
{
	C_Ticker();
	M_Ticker();

	// [ZZ] also tick the UI part of the events
	primaryLevel->localEventManager->UiTick();

	if (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].inventorytics > 0)
				--players[i].inventorytics;
		}

		for (auto level : AllLevels())
		{
			auto it = level->GetClientsideThinkerIterator<AActor>();
			AActor* ac = nullptr;
			while ((ac = it.Next()) != nullptr)
			{
				ac->ClearInterpolation();
				ac->ClearFOVInterpolation();
			}

			level->ClientsideThinkers.RunClientsideThinkers(level);
		}

		StatusBar->CallTick();

		// TODO: Should this be called on all maps...?
		if (gamestate == GS_LEVEL)
			primaryLevel->automap->Ticker();
	}
	else if (gamestate == GS_CUTSCENE || gamestate == GS_INTRO)
	{
		D_RunCutscene();
	}

	// [MK] Additional ticker for UI events right after all others
	primaryLevel->localEventManager->PostUiTick();
}

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
		// Only the current UI level's settings are relevant for sound.
		S_PauseSound (!(primaryLevel->flags2 & LEVEL2_PAUSE_MUSIC_IN_MENUS), false);
		return true;
	}
	return false;
}

void P_ClearLevelInterpolation()
{
	for (auto Level : AllLevels())
	{
		Level->interpolator.UpdateInterpolations();

		auto it = Level->GetThinkerIterator<AActor>();
		AActor* ac;

		while ((ac = it.Next()))
		{
			ac->ClearInterpolation();
			ac->ClearFOVInterpolation();
		}
	}

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			DPSprite* pspr = players[i].psprites;
			while (pspr)
			{
				pspr->ResetInterpolation();

				pspr = pspr->Next;
			}
		}
	}

	R_ClearInterpolationPath();
	if (StatusBar != nullptr)
		StatusBar->ClearInterpolation();
}

//
// P_Ticker
//
void P_Ticker (void)
{
	int i;

	for (auto Level : AllLevels())
	{
		Level->interpolator.UpdateInterpolations();
	}
	r_NoInterpolate = true;

	if (!demoplayback)
	{
		// This is a separate slot from the wipe in D_Display(), because this
		// is delayed slightly due to latency. (Even on a singleplayer game!)
//		GSnd->SetSfxPaused(!!playerswiping, 2);
	}

	// run the tic
	if (paused || P_CheckTickerPaused())
	{
		// This must run even when the game is paused to catch changes from netevents before the frame is rendered.
		for (auto Level : AllLevels())
		{
			auto it = Level->GetThinkerIterator<AActor>();
			AActor* ac;

			while ((ac = it.Next()))
			{
				if (ac->flags8 & MF8_RECREATELIGHTS)
				{
					ac->flags8 &= ~MF8_RECREATELIGHTS;
					ac->SetDynamicLights();
				}
			}
		}
		return;
	}

	DPSprite::NewTick();

	// [RH] Frozen mode is only changed every 4 tics, to make it work with A_Tracer().
	// This may not be perfect but it is not really relevant for sublevels that tracer homing behavior is preserved.
	if ((primaryLevel->maptime & 3) == 0)
	{
		if (globalchangefreeze)
		{
			globalfreeze ^= 1;
			globalchangefreeze = 0;
			for (auto Level : AllLevels())
			{
				Level->frozenstate = (Level->frozenstate & ~2) | (2 * globalfreeze);
			}
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

	// Reset all actor interpolations on all levels before the current thinking turn so that indirect actor movement gets properly interpolated.
	for (auto Level : AllLevels())
	{
		// todo: set up a sandbox for secondary levels here.
		auto it = Level->GetThinkerIterator<AActor>();
		AActor *ac;

		while ((ac = it.Next()))
		{
			ac->ClearInterpolation();
			ac->ClearFOVInterpolation();
		}

		P_ThinkParticles(Level);	// [RH] make the particles think

		for (i = 0; i < MAXPLAYERS; i++)
			if (Level->PlayerInGame(i))
				P_PlayerThink(Level->Players[i]);

		// [ZZ] call the WorldTick hook
		Level->localEventManager->WorldTick();
		Level->Tick();			// [RH] let the level tick
		Level->Thinkers.RunThinkers(Level);

		//if added by MC: Freeze mode.
		if (!Level->isFrozen())
		{
			P_UpdateSpecials(Level);
		}

		// for par times
		Level->time++;
		Level->maptime++;
		Level->totaltime++;
	}
	if (players[consoleplayer].mo != NULL) {
		if (players[consoleplayer].mo->Vel.Length() > primaryLevel->max_velocity) { primaryLevel->max_velocity = players[consoleplayer].mo->Vel.Length(); }
		primaryLevel->avg_velocity += (players[consoleplayer].mo->Vel.Length() - primaryLevel->avg_velocity) / primaryLevel->maptime;
	}
}
