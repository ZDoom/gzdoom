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


#include "z_zone.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_acs.h"
#include "c_console.h"
#include "b_bot.h"

#include "doomstat.h"

extern constate_e ConsoleState;
extern gamestate_t wipegamestate;

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

	P_ThinkParticles ();	// [RH] make the particles think

	for (i = 0; i<MAXPLAYERS; i++)
		if (playeringame[i] &&
			/*Added by MC: Freeze mode.*/!(bglobal.freeze && players[i].isbot))
			P_PlayerThink (&players[i]);

	DThinker::RunThinkers ();

	//if added by MC: Freeze mode.
	if (!bglobal.freeze)
	{
		P_UpdateSpecials ();
		P_RespawnSpecials ();
	}

	// [RH] Apply falling damage
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
		{
			P_FallingDamage (players[i].mo);

			players[i].oldvelocity[0] = players[i].mo->momx;
			players[i].oldvelocity[1] = players[i].mo->momy;
			players[i].oldvelocity[2] = players[i].mo->momz;
		}

	P_RunEffects ();	// [RH] Run particle effects

	// for par times
	level.time++;
}
