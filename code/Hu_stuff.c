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
// DESCRIPTION:  Heads-up displays
//
//-----------------------------------------------------------------------------

#include <ctype.h>

#include "doomdef.h"

#include "z_zone.h"

#include "m_swap.h"

#include "hu_stuff.h"
#include "hu_lib.h"
#include "w_wad.h"

#include "s_sound.h"

#include "doomstat.h"

#include "st_stuff.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#include "c_consol.h"
#include "c_dispch.h"
#include "c_cvars.h"

#include "v_text.h"

// [RH] Some chat-related console commands
void Cmd_MessageMode (player_t *plyr, int argc, char **argv);
void Cmd_Say (player_t *plyr, int argc, char **argv);

// [RH] Show scores stuff
#include "v_video.h"

void HU_DrawScores (int player);

//
// Locally used constants, shortcuts.
//
#define HU_INPUTX		HU_MSGX
#define HU_INPUTY		(HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0]->height) +1))
#define HU_INPUTWIDTH	64
#define HU_INPUTHEIGHT	1



cvar_t *chat_macros[10];

static player_t*	plr;
patch_t*			hu_font[HU_FONTSIZE];
BOOL 				chat_on;
static hu_itext_t	w_chat;

static BOOL			message_on;
BOOL 				message_dontfuckwithme;
static BOOL			message_nottobefuckedwith;

extern cvar_t		*showMessages;
extern BOOL			automapactive;

static BOOL			headsupactive = false;

void HU_Init(void)
{
	int 	i;
	int 	j;
	char	buffer[9];
	char	*tplate;
	int		sub;

	// load the heads-up font
	j = HU_FONTSTART;

	// [RH] Quick hack to handle the FONTA of Heretic and Hexen
	if (W_CheckNumForName ("FONTA01") >= 0) {
		tplate = "FONTA%02u";
		sub = HU_FONTSTART - 1;
	} else {
		tplate = "STCFN%.3d";
		sub = 0;
	}

	for (i=0;i<HU_FONTSIZE;i++)
	{
		sprintf(buffer, tplate, j++ - sub);
		hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
	}
}

void HU_Stop(void)
{
	headsupactive = false;
}

void HU_Start(void)
{
	if (headsupactive)
		HU_Stop();

	plr = &players[displayplayer];		// [RH] Not consoleplayer
	message_on = false;
	message_dontfuckwithme = false;
	message_nottobefuckedwith = false;
	chat_on = false;

	// create the chat widget
	HUlib_initIText(&w_chat,
					HU_INPUTX, HU_INPUTY,
					hu_font,
					HU_FONTSTART, &chat_on);

	headsupactive = true;
}

void HU_Drawer(void)
{
	HUlib_drawIText(&w_chat);

	if (deathmatch->value && ((Actions & ACTION_SHOWSCORES) || players[displayplayer].health <= 0))
		HU_DrawScores (displayplayer);
}

void HU_Erase(void)
{
	HUlib_eraseIText(&w_chat);
}

static int compare (const void *arg1, const void *arg2)
{
	return players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount;
}

void HU_DrawScores (int player)
{
	int sortedplayers[MAXPLAYERS];
	int i, j, x, y, maxwidth, margin;

	sortedplayers[MAXPLAYERS-1] = player;
	for (i = 0, j = 0; j < MAXPLAYERS - 1; i++, j++) {
		if (i == player)
			i++;
		sortedplayers[j] = i;
	}

	qsort (sortedplayers, MAXPLAYERS, sizeof(int), compare);

	maxwidth = 0;
	for (i = 0; i < MAXPLAYERS; i++) {
		if (playeringame[i]) {
			int width = V_StringWidth (players[i].userinfo->netname);
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	x = (screens[0].width >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);
	margin = x + 40 * CleanXfac;

	y = (ST_Y >> 1) - (MAXPLAYERS * 6);
	if (y < 48) y = 48;

	for (i = 0; i < MAXPLAYERS && y < ST_Y - 12 * CleanYfac; i++) {
		int color = players[sortedplayers[i]].userinfo->color;
		char str[24];

		if (playeringame[sortedplayers[i]]) {
			if (screens[0].is8bit)
				color = BestColor (DefaultPalette->basecolors,
								   RPART(color), GPART(color), BPART(color),
								   DefaultPalette->numcolors);

			V_Clear (x, y, x + 24 * CleanXfac, y + SHORT(hu_font[0]->height) * CleanYfac, &screens[0], color);

			sprintf (str, "%d", players[sortedplayers[i]].fragcount);
			V_DrawTextClean (margin, y, str);

			strcpy (str, players[sortedplayers[i]].userinfo->netname);

			if (sortedplayers[i] != player)
				for (j = 0; j < 23 && str[j]; j++)
					str[j] ^= 0x80;

			V_DrawTextClean (margin + 32 * CleanXfac, y, str);

			y += 12;
		}
	}
}

void HU_Ticker(void)
{
	// display message if necessary
	if (plr->message) {
		// [RH] We let the console code figure out
		// whether or not we should show the message.
		Printf ("%s\n", plr->message);
		plr->message = NULL;
		message_dontfuckwithme = 0;
	}

	// [RH] The original code gathered chat messages character-by-character
	//		from ticcmds here. We now send them using special messages which
	//		are handled in d_proto.c instead.
	// check for incoming chat characters
}


static void ShoveChatStr (const char *str, int who)
{
	Net_WriteByte (DEM_SAY);
	Net_WriteByte ((byte)who);
	Net_WriteString (str);
}

BOOL HU_Responder (event_t *ev)
{
	BOOL 			eatkey = false;
	static BOOL		shiftdown = false;
	static BOOL		altdown = false;
	unsigned char	c;

	if (ev->data1 == KEY_RSHIFT)
	{
		shiftdown = ev->type == ev_keydown;
		return false;
	}
	else if (ev->data1 == KEY_RALT || ev->data1 == KEY_LALT)
	{
		altdown = ev->type == ev_keydown;
		return false;
	}

	if (ev->type != ev_keydown)
		return false;

	if (chat_on)
	{
		c = ev->data3;	// [RH] Use localized keymap

		// send a macro
		if (altdown)
		{
			c = c - '0';

			if (c > 9)
				return false;

			ShoveChatStr (chat_macros[c]->string, 0);
			
			// leave chat mode
			chat_on = false;
			eatkey = true;
		}
		else
		{
			c = toupper (c);

			eatkey = HUlib_keyInIText(&w_chat, c);

			if (ev->data1 == KEY_ENTER) {
				chat_on = false;
				ShoveChatStr (w_chat.l.l, 0);
			} else if (ev->data1 == KEY_ESCAPE)
				chat_on = false;
		}
	}

	return eatkey;
}

void Cmd_MessageMode (player_t *plyr, int argc, char **argv)
{
	if (netgame) {
		chat_on = true;
		C_HideConsole ();
		HUlib_resetIText (&w_chat);
	}
}

void Cmd_Say (player_t *plyr, int argc, char **argv)
{
	if (argc > 1) {
		ShoveChatStr (BuildString (argc - 1, argv + 1), 0);
	}
}