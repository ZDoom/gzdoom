
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
#include "c_console.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_text.h"
#include "v_video.h"
#include "gi.h"
#include "d_gui.h"
#include "i_input.h"

#define QUEUESIZE		128
#define MESSAGESIZE		128
#define MESSAGELEN		265
#define HU_INPUTX		0
#define HU_INPUTY		(0 + (screen->Font->GetHeight () + 1))

EXTERN_CVAR (Bool, con_scaletext)

// Public data

void CT_Init ();
void CT_Drawer ();
BOOL CT_Responder (event_t *ev);
void HU_DrawScores (player_t *plyr);

int chatmodeon;

// Private data

static void CT_ClearChatMessage ();
static void CT_AddChar (char c);
static void CT_BackSpace ();
static void ShoveChatStr (const char *str, int who);

static int len;
static byte ChatQueue[QUEUESIZE];

CVAR (String, chatmacro1, "I'm ready to kick butt!", CVAR_ARCHIVE)
CVAR (String, chatmacro2, "I'm OK.", CVAR_ARCHIVE)
CVAR (String, chatmacro3, "I'm not looking too good!", CVAR_ARCHIVE)
CVAR (String, chatmacro4, "Help!", CVAR_ARCHIVE)
CVAR (String, chatmacro5, "You suck!", CVAR_ARCHIVE)
CVAR (String, chatmacro6, "Next time, scumbag...", CVAR_ARCHIVE)
CVAR (String, chatmacro7, "Come here!", CVAR_ARCHIVE)
CVAR (String, chatmacro8, "I'll take care of it.", CVAR_ARCHIVE)
CVAR (String, chatmacro9, "Yes", CVAR_ARCHIVE)
CVAR (String, chatmacro0, "No", CVAR_ARCHIVE)

FStringCVar *chat_macros[10] =
{
	&chatmacro0,
	&chatmacro1,
	&chatmacro2,
	&chatmacro3,
	&chatmacro4,
	&chatmacro5,
	&chatmacro6,
	&chatmacro7,
	&chatmacro8,
	&chatmacro9
};

//===========================================================================
//
// CT_Init
//
// 	Initialize chat mode data
//===========================================================================

void CT_Init ()
{
	len = 0; // initialize the queue index
	chatmodeon = 0;
	ChatQueue[0] = 0;
}

//===========================================================================
//
// CT_Stop
//
//===========================================================================

void CT_Stop ()
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
	if (chatmodeon && ev->type == EV_GUI_Event)
	{
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
		{
			if (ev->data1 == '\r')
			{
				ShoveChatStr ((char *)ChatQueue, chatmodeon - 1);
				CT_Stop ();
				return true;
			}
			else if (ev->data1 == GK_ESCAPE)
			{
				CT_Stop ();
				return true;
			}
			else if (ev->data1 == '\b')
			{
				CT_BackSpace ();
				return true;
			}
			else if (ev->data1 == 'C' && (ev->data3 & GKM_CTRL))
			{
				I_PutInClipboard ((char *)ChatQueue);
				return true;
			}
			else if (ev->data1 == 'V' && (ev->data3 & GKM_CTRL))
			{
				char *clip = I_GetFromClipboard ();
				if (clip != NULL)
				{
					char *clip_p = clip;
					strtok (clip, "\n\r\b");
					while (*clip_p)
					{
						CT_AddChar (*clip_p++);
					}
					delete[] clip;
				}
			}
		}
		else if (ev->subtype == EV_GUI_Char)
		{
			// send a macro
			if (ev->data2 && (ev->data1 >= '0' && ev->data1 <= '9'))
			{
				ShoveChatStr (*chat_macros[ev->data1 - '0'], chatmodeon - 1);
				CT_Stop ();
			}
			else
			{
				CT_AddChar (ev->data1);
			}
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
		int i, x, scalex, y, promptwidth;

		if (con_scaletext)
		{
			scalex = CleanXfac;
			y = (!viewactive ? -30 : -10) * CleanYfac;
		}
		else
		{
			scalex = 1;
			y = (!viewactive ? -30 : -10);
		}

		y += (SCREENHEIGHT == realviewheight && viewactive) ? SCREENHEIGHT : ST_Y;

		promptwidth = screen->StringWidth (prompt) * scalex;
		x = screen->Font->GetCharWidth ('_') * scalex * 2 + promptwidth;

		// figure out if the text is wider than the screen->
		// if so, only draw the right-most portion of it.
		for (i = len - 1; i >= 0 && x < SCREENWIDTH; i--)
		{
			x += screen->Font->GetCharWidth (ChatQueue[i] & 0x7f) * scalex;
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
		ChatQueue[len] = gameinfo.gametype == GAME_Doom ? '_' : '[';
		ChatQueue[len+1] = '\0';
		if (con_scaletext)
		{
			screen->DrawTextClean (CR_GREEN, 0, y, prompt);
			screen->DrawTextClean (CR_GREY, promptwidth, y, ChatQueue + i);
		}
		else
		{
			screen->DrawText (CR_GREEN, 0, y, prompt);
			screen->DrawText (CR_GREY, promptwidth, y, ChatQueue + i);
		}
		ChatQueue[len] = '\0';

		BorderTopRefresh = screen->GetPageCount ();
	}

	if (deathmatch &&
		(Button_ShowScores.bDown ||
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

static void CT_BackSpace ()
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

static void CT_ClearChatMessage ()
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

CCMD (messagemode)
{
	if (!menuactive)
	{
		chatmodeon = 1;
		C_HideConsole ();
		CT_ClearChatMessage ();
	}
}

CCMD (say)
{
	ShoveChatStr (argv[1], 0);
}

CCMD (messagemode2)
{
	if (!menuactive)
	{
		chatmodeon = 2;
		C_HideConsole ();
		CT_ClearChatMessage ();
	}
}

CCMD (say_team)
{
	ShoveChatStr (argv[1], 1);
}

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	return (*(player_t **)arg2)->fragcount - (*(player_t **)arg1)->fragcount;
}

EXTERN_CVAR (Float, timelimit)

void HU_DrawScores (player_t *player)
{
	char str[80];
	player_t *sortedplayers[MAXPLAYERS];
	int i, j, x, y, maxwidth, margin;

	if (player->camera->player)
		player = player->camera->player;

	sortedplayers[MAXPLAYERS-1] = player;
	for (i = 0, j = 0; j < MAXPLAYERS - 1; i++, j++)
	{
		if (&players[i] == player)
			i++;
		sortedplayers[j] = &players[i];
	}

	qsort (sortedplayers, MAXPLAYERS, sizeof(player_t *), compare);

	maxwidth = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			int width = screen->StringWidth (players[i].userinfo.netname);
			if (teamplay)
				width += screen->StringWidth (
					players[i].userinfo.team == TEAM_None ? "None"
					: TeamNames[players[i].userinfo.team]) + 24;
			if (width > maxwidth)
				maxwidth = width;
		}
	}

	x = (SCREENWIDTH >> 1) - (((maxwidth + 32 + 32 + 16) * CleanXfac) >> 1);
	margin = x + 40 * CleanXfac;

	y = (ST_Y >> 1) - (MAXPLAYERS * 6);
	if (y < 48) y = 48;

	if (deathmatch && timelimit && gamestate == GS_LEVEL)
	{
		int timeleft = (int)(timelimit * TICRATE * 60) - level.time;
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
		
		screen->DrawTextClean (CR_GREY, SCREENWIDTH/2 - screen->StringWidth (str)/2*CleanXfac,
			y - 12 * CleanYfac, str);
	}

	int height = screen->Font->GetHeight() * CleanYfac;
	for (i = 0; i < MAXPLAYERS && y < ST_Y - 12 * CleanYfac; i++)
	{
		int color = sortedplayers[i]->userinfo.color;

		if (playeringame[sortedplayers[i] - players])
		{
			color = ColorMatcher.Pick (RPART(color), GPART(color), BPART(color));

			screen->Clear (x, y, x + 24 * CleanXfac, y + height, color);

			sprintf (str, "%d", sortedplayers[i]->fragcount);
			screen->DrawTextClean (sortedplayers[i] == player ? CR_GREEN : CR_BRICK,
							 margin, y, str);

			if (teamplay && sortedplayers[i]->userinfo.team != TEAM_None)
				sprintf (str, "%s (%s)", sortedplayers[i]->userinfo.netname,
						 TeamNames[sortedplayers[i]->userinfo.team]);
			else
				strcpy (str, sortedplayers[i]->userinfo.netname);

			if (sortedplayers[i] != player)
			{
				color = (demoplayback && sortedplayers[i] == &players[consoleplayer]) ? CR_GOLD : CR_GREY;
			}
			else
			{
				color = CR_GREEN;
			}

			screen->DrawTextClean (color, margin + 32 * CleanXfac, y, str);

			y += height + CleanYfac;
		}
	}
	BorderNeedRefresh = screen->GetPageCount ();
}
