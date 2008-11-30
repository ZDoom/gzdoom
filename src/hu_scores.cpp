/*
** hu_scores.cpp
** Routines for drawing the scoreboards.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2007-2008 Christopher Westley
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

#include "c_console.h"
#include "st_stuff.h"
#include "teaminfo.h"
#include "templates.h"
#include "v_video.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_netinf.h"
#include "v_font.h"
#include "v_palette.h"
#include "d_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void HU_DoDrawScores (player_t *, player_t *[MAXPLAYERS]);
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
CVAR (Int,	sb_teamdeathmatch_headingcolor,		CR_RED,		CVAR_ARCHIVE)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int STACK_ARGS comparepoints (const void *arg1, const void *arg2)
{
	if (deathmatch)
		return (*(player_t **)arg2)->fragcount - (*(player_t **)arg1)->fragcount;
	else
		return (*(player_t **)arg2)->killcount - (*(player_t **)arg1)->killcount;
}

static int STACK_ARGS compareteams (const void *arg1, const void *arg2)
{
	return (*(player_t **)arg1)->userinfo.team - (*(player_t **)arg2)->userinfo.team;
}

// CODE --------------------------------------------------------------------

//==========================================================================
//
// HU_DrawScores
//
//==========================================================================

void HU_DrawScores (player_t *player)
{
	if (deathmatch)
	{
		if (teamplay)
		{
			if (!sb_teamdeathmatch_enable)
				return;
		}
		else
		{
			if (!sb_deathmatch_enable)
				return;
		}
	}
	else
	{
		if (!sb_cooperative_enable || !multiplayer)
			return;
	}

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

	if (teamplay && deathmatch)
		qsort (sortedplayers, MAXPLAYERS, sizeof(player_t *), compareteams);
	else
		qsort (sortedplayers, MAXPLAYERS, sizeof(player_t *), comparepoints);

	HU_DoDrawScores (player, sortedplayers);

	BorderNeedRefresh = screen->GetPageCount ();
}

//==========================================================================
//
// HU_DoDrawScores
//
//==========================================================================

static void HU_DoDrawScores (player_t *player, player_t *sortedplayers[MAXPLAYERS])
{
	int color;
	int height = SmallFont->GetHeight() * CleanYfac;
	unsigned int i;
	int maxwidth = 0;
	int numTeams = 0;
	int x, y;
	
	if (deathmatch)
	{
		if (teamplay)
			color = sb_teamdeathmatch_headingcolor;
		else
			color = sb_deathmatch_headingcolor;
	}
	else
	{
		color = sb_cooperative_headingcolor;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			int width = SmallFont->StringWidth (players[i].userinfo.netname);
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	if (teamplay && deathmatch)
		gamestate == GS_INTERMISSION ? y = SCREENHEIGHT * 2 / 7 : y = SCREENHEIGHT / 16;
	else
		gamestate == GS_INTERMISSION ? y = SCREENHEIGHT / 4 : y = SCREENHEIGHT / 16;

	HU_DrawTimeRemaining (ST_Y - height);

	if (teamplay && deathmatch)
	{
		for (i = 0; i < teams.Size (); i++)
		{
			teams[i].players = 0;
			teams[i].score = 0;
		}

		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[sortedplayers[i]-players] && TEAMINFO_IsValidTeam (sortedplayers[i]->userinfo.team))
			{
				if (teams[sortedplayers[i]->userinfo.team].players++ == 0)
				{
					numTeams++;
				}

				teams[sortedplayers[i]->userinfo.team].score += sortedplayers[i]->fragcount;
			}
		}

		int scorexwidth = SCREENWIDTH / 32;
		for (i = 0; i < teams.Size (); i++)
		{
			if (teams[i].players)
			{
				char score[80];
				mysnprintf (score, countof(score), "%d", teams[i].score);

				screen->DrawText (BigFont, teams[i].GetTextColor(), scorexwidth, gamestate == GS_INTERMISSION ? y * 4 / 5 : y / 2, score,
					DTA_CleanNoMove, true, TAG_DONE);

				scorexwidth += SCREENWIDTH / 8;
			}
		}

		gamestate == GS_INTERMISSION ? y += 0 : y += SCREENWIDTH / 32;
	}

	screen->DrawText (SmallFont, color, SCREENWIDTH / 32, y, "Color",
		DTA_CleanNoMove, true, TAG_DONE);

	screen->DrawText (SmallFont, color, SCREENWIDTH / 4, y, deathmatch ? "Frags" : "Kills",
		DTA_CleanNoMove, true, TAG_DONE);

	screen->DrawText (SmallFont, color, SCREENWIDTH / 2, y, "Name",
		DTA_CleanNoMove, true, TAG_DONE);

	x = (SCREENWIDTH >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);
	gamestate == GS_INTERMISSION ? y = SCREENHEIGHT * 2 / 7 : y = SCREENHEIGHT / 10;

	if (teamplay && deathmatch)
		y += SCREENWIDTH / 32;

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
			mysnprintf (str, countof(str), "Level ends in %d:%02d:%02d", hours, minutes, seconds);
		else
			mysnprintf (str, countof(str), "Level ends in %d:%02d", minutes, seconds);
		
		screen->DrawText (SmallFont, CR_GREY, SCREENWIDTH/2 - SmallFont->StringWidth (str)/2*CleanXfac,
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

	D_GetPlayerColor (int(player - players), &h, &s, &v);
	HSVtoRGB (&r, &g, &b, h, s, v);

	screen->Clear (SCREENWIDTH / 24, y, SCREENWIDTH / 24 + 24*CleanXfac, y + height, -1,
		MAKEARGB(255,clamp(int(r*255.f),0,255),
					 clamp(int(g*255.f),0,255),
					 clamp(int(b*255.f),0,255)));

	if (teamplay && deathmatch)
	{
		if (TEAMINFO_IsValidTeam (player->userinfo.team))
			color = teams[player->userinfo.team].GetTextColor ();
		else
			color = CR_GREY;
	}
	else
	{
		if (!highlight)
			deathmatch ? color = sb_deathmatch_otherplayercolor : color = sb_cooperative_otherplayercolor;
		else
			deathmatch ? color = sb_deathmatch_yourplayercolor : color = sb_cooperative_yourplayercolor;
	}

	mysnprintf (str, countof(str), "%d", deathmatch ? player->fragcount : player->killcount);

	screen->DrawText (SmallFont, color, SCREENWIDTH / 4, y, player->playerstate == PST_DEAD && !deathmatch ? "DEAD" : str,
		DTA_CleanNoMove, true, TAG_DONE);

	screen->DrawText (SmallFont, color, SCREENWIDTH / 2, y, player->userinfo.netname,
		DTA_CleanNoMove, true, TAG_DONE);

	if (teamplay && teams[player->userinfo.team].logo.GetChars ())
	{
		screen->DrawTexture (TexMan[teams[player->userinfo.team].logo.GetChars ()], SCREENWIDTH / 5, y,
			DTA_CleanNoMove, true, TAG_DONE);
	}

	if (player->mo->ScoreIcon.isValid())
	{
		screen->DrawTexture (TexMan[player->mo->ScoreIcon], SCREENWIDTH * 4 / 9, y,
			DTA_CleanNoMove, true, TAG_DONE);
	}
}
