
//**************************************************************************
//**
//** ct_chat.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: ct_chat.c,v $
//** $Revision: 1.12 $
//** $Date: 96/01/16 10:35:26 $
//** $Author: bgokey $
//**
//**************************************************************************

#include <string.h>
#include <ctype.h>
#include "doomdef.h"
#include "z_zone.h"
#include "m_swap.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "s_sound.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "dstrings.h"
#include "c_consol.h"
#include "c_dispch.h"
#include "c_cvars.h"
#include "v_text.h"
#include "v_video.h"

#define QUEUESIZE		128
#define MESSAGESIZE		128
#define MESSAGELEN		265
#define HU_INPUTX		0
#define HU_INPUTY		(0 + (SHORT(hu_font[0]->height) +1))

extern cvar_t *con_scaletext;

// Public data

void CT_Init (void);
void CT_Drawer (void);
BOOL CT_Responder (event_t *ev);
void Cmd_MessageMode (player_t *plyr, int argc, char **argv);
void Cmd_Say (player_t *plyr, int argc, char **argv);
void Cmd_MessageMode2 (player_t *plyr, int argc, char **argv);
void Cmd_Say_Team (player_t *plyr, int argc, char **argv);
void HU_DrawScores (player_t *plyr);

int chatmodeon;

patch_t *hu_font[HU_FONTSIZE];

// Private data

static void CT_ClearChatMessage (void);
static void CT_AddChar (char c);
static void CT_BackSpace (void);
static void ShoveChatStr (const char *str, int who);

static int len;
static byte ChatQueue[QUEUESIZE];

BOOL altdown;

cvar_t *chat_macros[10];

//===========================================================================
//
// CT_Init
//
// 	Initialize chat mode data
//===========================================================================

void CT_Init (void)
{
	int i, j, sub;
	char *tplate, buffer[12];

	len = 0; // initialize the queue index
	chatmodeon = 0;
	ChatQueue[0] = 0;

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

	for (i = 0; i < HU_FONTSIZE; i++)
	{
		sprintf (buffer, tplate, j++ - sub);
		hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
	}
}

//===========================================================================
//
// CT_Stop
//
//===========================================================================

void CT_Stop (void)
{
	chatmodeon = 0;
}

//===========================================================================
//
// CT_Responder
//
//===========================================================================

BOOL CT_Responder (event_t *ev)
{
	unsigned char c;

	if (ev->data1 == KEY_RALT || ev->data1 == KEY_LALT)
	{
		altdown = (ev->type == ev_keydown);
		return false;
	}
	else if (ev->data1 == KEY_LSHIFT || ev->data1 == KEY_RSHIFT)
	{
		return false;
	}
	else if (gamestate != GS_LEVEL || ev->type != ev_keydown)
	{
		return false;
	}

	if (chatmodeon)
	{
		c = ev->data3;	// [RH] Use localized keymap

		// send a macro
		if (altdown)
		{
			if (ev->data2 >= '0' && ev->data2 <= '9')
			{
				ShoveChatStr (chat_macros[ev->data2 - '0']->string, chatmodeon - 1);
				CT_Stop ();
				return true;
			}
		}
		if (ev->data1 == KEY_ENTER)
		{
			ShoveChatStr (ChatQueue, chatmodeon - 1);
			CT_Stop ();
			return true;
		}
		else if (ev->data1 == KEY_ESCAPE)
		{
			CT_Stop ();
			return true;
		}
		else if (ev->data1 == KEY_BACKSPACE)
		{
			CT_BackSpace ();
			return true;
		}
		else
		{
			CT_AddChar (c);
			return true;
		}
	}

	return false;
}

//===========================================================================
//
// CT_Drawer
//
//===========================================================================

void CT_Drawer (void)
{
	if (chatmodeon)
	{
		static const char *prompt = "Say: ";
		void (*drawfunc)(int,int,int,screen_t*,patch_t*);
		int i, x, c, scalex, y, promptwidth;

		if (con_scaletext->value)
		{
			scalex = CleanXfac;
			y = (!viewactive ? -30 : -10) * CleanYfac;
			drawfunc = V_DrawCNMWrapper;
		} else {
			scalex = 1;
			y = (!viewactive ? -30 : -10);
			drawfunc = V_DrawWrapper;
		}

		y += (screens[0].height == realviewheight && viewactive) ? screens[0].height : ST_Y;

		promptwidth = V_StringWidth (prompt) * scalex;
		x = SHORT(hu_font['_' - HU_FONTSTART]->width) * scalex * 2 + promptwidth;

		// figure out if the text is wider than the screen.
		// if so, only draw the right-most portion of it.
		for (i = len - 1; i >= 0 && x < screens[0].width; i--)
		{
			c = toupper(ChatQueue[i] & 0x7f) - HU_FONTSTART;
			if (c < 0 || c >= HU_FONTSIZE)
			{
				x += 4 * scalex;
			}
			else
			{
				x += SHORT(hu_font[c]->width) * scalex;
			}
		}

		if (i >= 0)
		{
			i++;
		}
		else
		{
			i = 0;
		}

		// draw the prompt, text, and cursor
		ChatQueue[len] = '_';
		ChatQueue[len+1] = '\0';
		if (con_scaletext->value)
		{
			V_DrawTextClean (CR_RED, 0, y, prompt);
			V_DrawTextClean (CR_GREY, promptwidth, y, ChatQueue + i);
		}
		else
		{
			V_DrawText (CR_RED, 0, y, prompt);
			V_DrawText (CR_GREY, promptwidth, y, ChatQueue + i);
		}
		ChatQueue[len] = '\0';

		BorderTopRefresh = true;
	}

	if (deathmatch->value &&
		((Actions & ACTION_SHOWSCORES) ||
		 players[consoleplayer].camera->health <= 0))
	{
		HU_DrawScores (&players[consoleplayer]);
	}
}

//===========================================================================
//
// CT_AddChar
//
//===========================================================================

static void CT_AddChar (char c)
{
	if (len < QUEUESIZE-2)
	{
		ChatQueue[len++] = c;
		ChatQueue[len] = 0;
	}
}

//===========================================================================
//
// CT_BackSpace
//
// 	Backs up a space, when the user hits (obviously) backspace
//===========================================================================

static void CT_BackSpace (void)
{
	if (len)
	{
		ChatQueue[--len] = 0;
	}
}

//===========================================================================
//
// CT_ClearChatMessage
//
// 	Clears out the data for the chat message.
//===========================================================================

static void CT_ClearChatMessage (void)
{
	ChatQueue[0] = 0;
	len = 0;
}

static void ShoveChatStr (const char *str, int who)
{
	Net_WriteByte (DEM_SAY);
	Net_WriteByte ((byte)who);
	Net_WriteString (str);
}

void Cmd_MessageMode (player_t *plyr, int argc, char **argv)
{
	chatmodeon = 1;
	C_HideConsole ();
	CT_ClearChatMessage ();
}

void Cmd_Say (player_t *plyr, int argc, char **argv)
{
	if (argc > 1)
	{
		ShoveChatStr (BuildString (argc - 1, argv + 1), 0);
	}
}

void Cmd_MessageMode2 (player_t *plyr, int argc, char **argv)
{
	chatmodeon = 2;
	C_HideConsole ();
	CT_ClearChatMessage ();
}

void Cmd_Say_Team (player_t *plyr, int argc, char **argv)
{
	if (argc > 1)
	{
		ShoveChatStr (BuildString (argc - 1, argv + 1), 1);
	}
}

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	return players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount;
}

extern cvar_t *timelimit;

void HU_DrawScores (player_t *plyr)
{
	char str[80];
	int sortedplayers[MAXPLAYERS];
	int i, j, x, y, maxwidth, margin;
	int player;

	if (plyr->camera->player)
		player = plyr->camera->player - players;
	else
		player = plyr - players;

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
			int width = V_StringWidth (players[i].userinfo.netname);
			if (teamplay->value)
				width += V_StringWidth (players[i].userinfo.team) + 24;
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	x = (screens[0].width >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);
	margin = x + 40 * CleanXfac;

	y = (ST_Y >> 1) - (MAXPLAYERS * 6);
	if (y < 48) y = 48;

	if (deathmatch->value && timelimit->value && gamestate == GS_LEVEL) {
		int timeleft = (int)(timelimit->value * TICRATE * 60) - level.time;
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
		
		V_DrawTextClean (CR_GREY, screens[0].width/2 - V_StringWidth (str)/2*CleanXfac, y - 12 * CleanYfac, str);
	}

	for (i = 0; i < MAXPLAYERS && y < ST_Y - 12 * CleanYfac; i++) {
		int color = players[sortedplayers[i]].userinfo.color;

		if (playeringame[sortedplayers[i]]) {
			if (screens[0].is8bit)
				color = BestColor (DefaultPalette->basecolors,
								   RPART(color), GPART(color), BPART(color),
								   DefaultPalette->numcolors);

			V_Clear (x, y, x + 24 * CleanXfac, y + SHORT(hu_font[0]->height) * CleanYfac, &screens[0], color);

			sprintf (str, "%d", players[sortedplayers[i]].fragcount);
			V_DrawTextClean (sortedplayers[i] == player ? CR_GREEN : CR_BRICK,
							 margin, y, str);

			if (teamplay->value && players[sortedplayers[i]].userinfo.team[0])
				sprintf (str, "%s (%s)", players[sortedplayers[i]].userinfo.netname,
						 players[sortedplayers[i]].userinfo.team);
			else
				strcpy (str, players[sortedplayers[i]].userinfo.netname);

			if (sortedplayers[i] != player) {
				if (demoplayback && sortedplayers[i] == consoleplayer)
					color = CR_GOLD;
				else
					color = CR_GREY;
			} else {
				color = CR_GREEN;
			}

			V_DrawTextClean (color, margin + 32 * CleanXfac, y, str);

			y += 10 * CleanYfac;
		}
	}
	BorderNeedRefresh = true;
}
