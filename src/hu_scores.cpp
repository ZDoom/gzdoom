/*
** hu_scores.cpp
** Routines for drawing the deathmatch scoreboard.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2007 Chris Westley
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

// HEADER FILES ------------------------------------------------------------

#include "st_stuff.h"
#include "c_console.h"
#include "v_video.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void HU_DrawNonTeamScores (player_t *, player_t *[MAXPLAYERS]);
static void HU_DrawTeamScores (player_t *, player_t *[MAXPLAYERS]);

static void HU_DrawTimeRemaining (int y);

static void HU_DrawPlayer (player_t *, bool, int, int, int, bool);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, timelimit)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool,	sb_cooperative_enable,				true,		CVAR_ARCHIVE)
CVAR (Int,	sb_cooperative_headingcolor,		CR_RED,		CVAR_ARCHIVE)
CVAR (Int,	sb_cooperative_yourplayercolor,		CR_GREEN,	CVAR_ARCHIVE)
CVAR (Int,	sb_cooperative_otherplayercolor,	CR_GREY,	CVAR_ARCHIVE)

CVAR (Bool,	sb_deathmatch_enable,				true,		CVAR_ARCHIVE)
CVAR (Int,	sb_deathmatch_headingcolor,			CR_RED,		CVAR_ARCHIVE)
CVAR (Int,	sb_deathmatch_yourplayercolor,		CR_GREEN,	CVAR_ARCHIVE)
CVAR (Int,	sb_deathmatch_otherplayercolor,		CR_GREY,	CVAR_ARCHIVE)

CVAR (Bool,	sb_teamdeathmatch_enable,			true,		CVAR_ARCHIVE)
CVAR (Int,	sb_teamdeathmatch_yourplayercolor,	CR_GREEN,	CVAR_ARCHIVE)
CVAR (Int,	sb_teamdeathmatch_otherplayercolor,	CR_GREY,	CVAR_ARCHIVE)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	if (deathmatch)
		return (*(player_t **)arg2)->fragcount - (*(player_t **)arg1)->fragcount;
	else
		return (*(player_t **)arg2)->killcount - (*(player_t **)arg1)->killcount;
}

// CODE --------------------------------------------------------------------

//==========================================================================
//
// HU_DrawScores
//
//==========================================================================

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

	if (deathmatch && teamplay)
		HU_DrawTeamScores (player, sortedplayers);
	else
		HU_DrawNonTeamScores (player, sortedplayers);

	BorderNeedRefresh = screen->GetPageCount ();
}

//==========================================================================
//
// HU_DrawNonTeamScores
//
//==========================================================================

static void HU_DrawNonTeamScores (player_t *player, player_t *sortedplayers[MAXPLAYERS])
{
	int color;
	int height = screen->Font->GetHeight() * CleanYfac;
	int i;
	int maxwidth = 0;
	int x ,y;
	
	deathmatch ? color = sb_deathmatch_headingcolor : color = sb_cooperative_headingcolor;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			int width = SmallFont->StringWidth (players[i].userinfo.netname);
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	gamestate == GS_INTERMISSION ? y = SCREENHEIGHT / 4 : y = SCREENHEIGHT / 16;

	HU_DrawTimeRemaining (ST_Y - height);

	screen->DrawText (color, SCREENWIDTH / 32, y, "Color",
		DTA_CleanNoMove, true, TAG_DONE);

	screen->DrawText (color, SCREENWIDTH / 4, y, deathmatch ? "Frags" : "Kills",
		DTA_CleanNoMove, true, TAG_DONE);

	screen->DrawText (color, SCREENWIDTH / 2, y, "Name",
		DTA_CleanNoMove, true, TAG_DONE);

	x = (SCREENWIDTH >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);
	gamestate == GS_INTERMISSION ? y = SCREENHEIGHT / 3.5 : y = SCREENHEIGHT / 10;

	for (i = 0; i < MAXPLAYERS && y < ST_Y - 12 * CleanYfac; i++)
	{
		if (playeringame[sortedplayers[i] - players])
		{
			HU_DrawPlayer (sortedplayers[i], player==sortedplayers[i], x, y, height, false);
			y += height + CleanYfac;
		}
	}
}

//==========================================================================
//
// HU_DrawTeamScores
//
//==========================================================================

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
		HU_DrawNonTeamScores (player, sorted);
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

			sprintf (str, "%s: %d", TeamNames[i], teamScore[i]);
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

//==========================================================================
//
// HU_DrawTimeRemaining
//
//==========================================================================

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

//==========================================================================
//
// HU_DrawPlayer
//
//==========================================================================

static void HU_DrawPlayer (player_t *player, bool highlight, int x, int y, int height, bool pack)
{
	float h, s, v, r, g, b;
	int color;
	char str[80];

	D_GetPlayerColor (player - players, &h, &s, &v);
	HSVtoRGB (&r, &g, &b, h, s, v);

	color = ColorMatcher.Pick (clamp (int(r*255.f),0,255),
		clamp (int(g*255.f),0,255), clamp (int(b*255.f),0,255));

	if (deathmatch && teamplay)
	{
		screen->Clear (x, y, x + 24*CleanXfac, y + height, color);

		if (player->mo->ScoreIcon > 0)
		{
			screen->DrawTexture (TexMan[player->mo->ScoreIcon], x+(pack?20:32)*CleanXfac, y,
				DTA_CleanNoMove, true, TAG_DONE);
		}

		sprintf (str, "%d", player->fragcount);

		if (!highlight)
			color = sb_teamdeathmatch_otherplayercolor;
		else
			color = sb_teamdeathmatch_yourplayercolor;

		screen->DrawText (color, x+(pack?28:40)*CleanXfac, y, str,
			DTA_CleanNoMove, true, TAG_DONE);

		screen->DrawText (color, x + (pack?54:72)*CleanXfac, y, player->userinfo.netname,
			DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		screen->Clear (SCREENWIDTH / 24, y, SCREENWIDTH / 24 + 24*CleanXfac, y + height, color);

		if (!highlight)
			deathmatch ? color = sb_deathmatch_otherplayercolor : color = sb_cooperative_otherplayercolor;
		else
			deathmatch ? color = sb_deathmatch_yourplayercolor : color = sb_cooperative_yourplayercolor;

		sprintf (str, "%d", deathmatch ? player->fragcount : player->killcount);

		screen->DrawText (color, SCREENWIDTH / 4, y, str,
			DTA_CleanNoMove, true, TAG_DONE);

		screen->DrawText (color, SCREENWIDTH / 2, y, player->userinfo.netname,
			DTA_CleanNoMove, true, TAG_DONE);

		if (player->mo->ScoreIcon > 0)
		{
			screen->DrawTexture (TexMan[player->mo->ScoreIcon], SCREENWIDTH / 2.25, y,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}
}
