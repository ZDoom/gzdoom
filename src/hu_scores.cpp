/*
** hu_scores.cpp
** Routines for drawing the deathmatch scoreboard.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <string.h>
#include <ctype.h>
#include "doomdef.h"
#include "m_swap.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "s_sound.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_text.h"
#include "v_video.h"
#include "gi.h"
#include "d_gui.h"
#include "i_input.h"
#include "templates.h"

static void HU_DrawTeamScores (player_t *, player_t *[MAXPLAYERS]);
static void HU_DrawSingleScores (player_t *, player_t *[MAXPLAYERS]);
static void HU_DrawTimeRemaining (int y);
static void HU_DrawPlayer (player_t *, bool, int, int, int, bool);

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	return (*(player_t **)arg2)->fragcount - (*(player_t **)arg1)->fragcount;
}

EXTERN_CVAR (Float, timelimit)

void HU_DrawScores (player_t *player)
{
	int i, j;
	player_t *sortedplayers[MAXPLAYERS];

	if (player->camera && player->camera->player)
		player = player->camera->player;

	sortedplayers[MAXPLAYERS-1] = player;
	for (i = 0, j = 0; j < MAXPLAYERS - 1; i++, j++)
	{
		if (&players[i] == player)
			i++;
		sortedplayers[j] = &players[i];
	}

	qsort (sortedplayers, MAXPLAYERS, sizeof(player_t *), compare);

	if (teamplay)
	{
		HU_DrawTeamScores (player, sortedplayers);
	}
	else
	{
		HU_DrawSingleScores (player, sortedplayers);
	}

	BorderNeedRefresh = screen->GetPageCount ();
}

static void HU_DrawSingleScores (player_t *player, player_t *sortedplayers[MAXPLAYERS])
{
	int i, x, y, maxwidth;
	int height = screen->Font->GetHeight() * CleanYfac;

	maxwidth = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			int width = SmallFont->StringWidth (players[i].userinfo.netname);
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	x = (SCREENWIDTH >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);

	y = (ST_Y >> 1) - (MAXPLAYERS * 6);
	if (y < 48) y = 48;

	HU_DrawTimeRemaining (ST_Y - height);

	for (i = 0; i < MAXPLAYERS && y < ST_Y - 12 * CleanYfac; i++)
	{
		if (playeringame[sortedplayers[i] - players])
		{
			HU_DrawPlayer (sortedplayers[i], player==sortedplayers[i], x, y, height, false);
			y += height + CleanYfac;
		}
	}
}

static void HU_DrawTeamScores (player_t *player, player_t *sorted[MAXPLAYERS])
{
	static const int teamColors[NUM_TEAMS] = { CR_RED, CR_BLUE, CR_GREEN, CR_GOLD };

	char str[80];
	int height = screen->Font->GetHeight() * CleanYfac;
	int teamPlayers[NUM_TEAMS] = { 0 }, teamSlot[NUM_TEAMS];
	int teamX[NUM_TEAMS], teamY[NUM_TEAMS], teamScore[NUM_TEAMS] = { 0 };
	int numTeams = 0;
	int i, j, tallest;

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[sorted[i]-players] && sorted[i]->userinfo.team < NUM_TEAMS)
		{
			if (teamPlayers[sorted[i]->userinfo.team]++ == 0)
			{
				numTeams++;
			}
			teamScore[sorted[i]->userinfo.team] += sorted[i]->fragcount;
		}
	}

	if (numTeams == 0)
	{
		HU_DrawSingleScores (player, sorted);
		return;
	}

	HU_DrawTimeRemaining (ST_Y - height);

	screen->SetFont (BigFont);

	for (i = j = tallest = 0; i < NUM_TEAMS; ++i)
	{
		if (teamPlayers[i])
		{
			teamPlayers[j] = teamPlayers[i];
			teamSlot[i] = j;

			if (j < 2 && teamPlayers[i] > tallest)
			{
				tallest = teamPlayers[i];
			}
			else if (j == 2)
			{
				tallest = tallest * (height+CleanYfac) + 36*CleanYfac + teamY[0];
			}

			teamX[j] = (j&1) ? (SCREENWIDTH+teamX[0]) >> 1 : 10*CleanXfac;
			teamY[j] = (j&2) ? tallest : (gamestate==GS_LEVEL?32*CleanYfac:
								(56-100)*CleanYfac+(SCREENHEIGHT/2));

			sprintf (str, "%s %d", TeamNames[i], teamScore[i]);
			screen->DrawText (teamColors[i], teamX[j],
				teamY[j] - 20*CleanYfac, str, DTA_CleanNoMove, true, TAG_DONE);

			j++;
		}
	}

	screen->SetFont (SmallFont);

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[sorted[i]-players] && sorted[i]->userinfo.team < NUM_TEAMS)
		{
			int slot = teamSlot[sorted[i]->userinfo.team];
			HU_DrawPlayer (sorted[i], player==sorted[i], teamX[slot], teamY[slot], height, true);
			teamY[slot] += height + CleanYfac;
		}
	}
}

static void HU_DrawTimeRemaining (int y)
{
	if (deathmatch && timelimit && gamestate == GS_LEVEL)
	{
		char str[80];
		int timeleft = (int)(timelimit * TICRATE * 60) - level.maptime;
		int hours, minutes, seconds;

		if (timeleft < 0)
			timeleft = 0;

		hours = timeleft / (TICRATE * 3600);
		timeleft -= hours * TICRATE * 3600;
		minutes = timeleft / (TICRATE * 60);
		timeleft -= minutes * TICRATE * 60;
		seconds = timeleft / TICRATE;

		if (hours)
			sprintf (str, "Level ends in %02d:%02d:%02d", hours, minutes, seconds);
		else
			sprintf (str, "Level ends in %02d:%02d", minutes, seconds);
		
		screen->DrawText (CR_GREY, SCREENWIDTH/2 - SmallFont->StringWidth (str)/2*CleanXfac,
			y, str, DTA_CleanNoMove, true, TAG_DONE);
	}

}

static void HU_DrawPlayer (player_t *player, bool highlight, int x, int y, int height, bool pack)
{
	char str[80];
	float h, s, v, r, g, b;
	int color;

	D_GetPlayerColor (player - players, &h, &s, &v);
	HSVtoRGB (&r, &g, &b, h, s, v);

	color = ColorMatcher.Pick (clamp (int(r*255.f),0,255),
		clamp (int(g*255.f),0,255), clamp (int(b*255.f),0,255));

	screen->Clear (x, y, x + 24*CleanXfac, y + height, color);

	if (player->mo->ScoreIcon > 0)
	{
		screen->DrawTexture (TexMan[player->mo->ScoreIcon], x+(pack?20:32)*CleanXfac, y,
			DTA_CleanNoMove, true, TAG_DONE);
	}

	sprintf (str, "%d", player->fragcount);
	screen->DrawText (highlight ? CR_GREEN : CR_BRICK, x+(pack?28:40)*CleanXfac, y, str,
		DTA_CleanNoMove, true, TAG_DONE);

	if (!highlight)
	{
		color = (demoplayback && player == &players[consoleplayer]) ? CR_GOLD : CR_GREY;
	}
	else
	{
		color = CR_GREEN;
	}

	screen->DrawText (color, x + (pack?54:72)*CleanXfac, y, player->userinfo.netname,
		DTA_CleanNoMove, true, TAG_DONE);
}
