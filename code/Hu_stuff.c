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

static const char
rcsid[] = "$Id: hu_stuff.c,v 1.4 1997/02/03 16:47:52 b1 Exp $";

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

#include "c_cvars.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

//
// Locally used constants, shortcuts.
//
#define HU_INPUTTOGGLE	't'
#define HU_INPUTX		HU_MSGX
#define HU_INPUTY		(HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0]->height) +1))
#define HU_INPUTWIDTH	64
#define HU_INPUTHEIGHT	1



cvar_t *chat_macros[10];

char*	player_names[MAXPLAYERS] =
{
	HUSTR_PLRGREEN,
	HUSTR_PLRINDIGO,
	HUSTR_PLRBROWN,
	HUSTR_PLRRED
};


char					chat_char; // remove later.
static player_t*		plr;
patch_t*				hu_font[HU_FONTSIZE];
boolean 				chat_on;
static hu_itext_t		w_chat;
static boolean			always_off = false;
static char 			chat_dest[MAXPLAYERS];
static hu_itext_t w_inputbuffer[MAXPLAYERS];

static boolean			message_on;
boolean 				message_dontfuckwithme;
static boolean			message_nottobefuckedwith;

static hu_stext_t		w_message;
static int				message_counter;

extern cvar_t		   *showMessages;
extern boolean			automapactive;

static boolean			headsupactive = false;

const char* 	shiftxform;

const char french_shiftxform[] =
{
	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31,
	' ', '!', '"', '#', '$', '%', '&',
	'"', // shift-'
	'(', ')', '*', '+',
	'?', // shift-,
	'_', // shift--
	'>', // shift-.
	'?', // shift-/
	'0', // shift-0
	'1', // shift-1
	'2', // shift-2
	'3', // shift-3
	'4', // shift-4
	'5', // shift-5
	'6', // shift-6
	'7', // shift-7
	'8', // shift-8
	'9', // shift-9
	'/',
	'.', // shift-;
	'<',
	'+', // shift-=
	'>', '?', '@',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'[', // shift-[
	'!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
	']', // shift-]
	'"', '_',
	'\'', // shift-`
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'{', '|', '}', '~', 127

};

const char english_shiftxform[] =
{

	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31,
	' ', '!', '"', '#', '$', '%', '&',
	'"', // shift-'
	'(', ')', '*', '+',
	'<', // shift-,
	'_', // shift--
	'>', // shift-.
	'?', // shift-/
	')', // shift-0
	'!', // shift-1
	'@', // shift-2
	'#', // shift-3
	'$', // shift-4
	'%', // shift-5
	'^', // shift-6
	'&', // shift-7
	'*', // shift-8
	'(', // shift-9
	':',
	':', // shift-;
	'<',
	'+', // shift-=
	'>', '?', '@',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'[', // shift-[
	'!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
	']', // shift-]
	'"', '_',
	'\'', // shift-`
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'{', '|', '}', '~', 127
};

char frenchKeyMap[128]=
{
	0,
	1,2,3,4,5,6,7,8,9,10,
	11,12,13,14,15,16,17,18,19,20,
	21,22,23,24,25,26,27,28,29,30,
	31,
	' ','!','"','#','$','%','&','%','(',')','*','+',';','-',':','!',
	'0','1','2','3','4','5','6','7','8','9',':','M','<','=','>','?',
	'@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
	'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^','_',
	'@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
	'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^',127
};

char ForeignTranslation(unsigned char ch)
{
	return ch < 128 ? frenchKeyMap[ch] : ch;
}

void HU_Init(void)
{

	int 		i;
	int 		j;
	char		buffer[9];

	if (french)
		shiftxform = french_shiftxform;
	else
		shiftxform = english_shiftxform;

	// load the heads-up font
	j = HU_FONTSTART;
	for (i=0;i<HU_FONTSIZE;i++)
	{
		sprintf(buffer, "STCFN%.3d", j++);
		hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
	}

}

void HU_Stop(void)
{
	headsupactive = false;
}

void HU_Start(void)
{

	int 		i;

	if (headsupactive)
		HU_Stop();

	plr = &players[consoleplayer];
	message_on = false;
	message_dontfuckwithme = false;
	message_nottobefuckedwith = false;
	chat_on = false;

	// create the message widget
	HUlib_initSText(&w_message,
					HU_MSGX, HU_MSGY, HU_MSGHEIGHT,
					hu_font,
					HU_FONTSTART, &message_on);

	// create the chat widget
	HUlib_initIText(&w_chat,
					HU_INPUTX, HU_INPUTY,
					hu_font,
					HU_FONTSTART, &chat_on);

	// create the inputbuffer widgets
	for (i=0 ; i<MAXPLAYERS ; i++)
		HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);

	headsupactive = true;

}

void HU_Drawer(void)
{

	HUlib_drawSText(&w_message);
	HUlib_drawIText(&w_chat);

}

void HU_Erase(void)
{

	HUlib_eraseSText(&w_message);
	HUlib_eraseIText(&w_chat);

}

void HU_Ticker(void)
{

	int i, rc;
	char c;

	// tick down message counter if message is up
	if (message_counter && !--message_counter)
	{
		message_on = false;
		message_nottobefuckedwith = false;
	}

	// display message if necessary
	if (plr->message) {
		// [RH] We let the console code figure out
		// whether or not we should show the message.
		Printf ("%s\n", plr->message);
		plr->message = NULL;
		message_dontfuckwithme = 0;
	}

	// check for incoming chat characters
	if (netgame)
	{
		for (i=0 ; i<MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;
			if (i != consoleplayer
				&& (c = players[i].cmd.chatchar))
			{
				if (c <= HU_BROADCAST)
					chat_dest[i] = c;
				else
				{
					if (c >= 'a' && c <= 'z')
						c = (char) shiftxform[(unsigned char) c];
					rc = HUlib_keyInIText(&w_inputbuffer[i], c);
					if (rc && c == KEY_ENTER)
					{
						if (w_inputbuffer[i].l.len
							&& (chat_dest[i] == consoleplayer+1
								|| chat_dest[i] == HU_BROADCAST))
						{
							HUlib_addMessageToSText(&w_message,
													player_names[i],
													w_inputbuffer[i].l.l);
							
							message_nottobefuckedwith = true;
							message_on = true;
							message_counter = HU_MSGTIMEOUT;
							if ( gamemode == commercial )
							  S_StartSound(ORIGIN_AMBIENT, sfx_radio);
							else
							  S_StartSound(ORIGIN_AMBIENT, sfx_tink);
						}
						HUlib_resetIText(&w_inputbuffer[i]);
					}
				}
				players[i].cmd.chatchar = 0;
			}
		}
	}

}

#define QUEUESIZE				128

static char 	chatchars[QUEUESIZE];
static int		head = 0;
static int		tail = 0;


void HU_queueChatChar(char c)
{
	if (((head + 1) & (QUEUESIZE-1)) == tail)
	{
		plr->message = HUSTR_MSGU;
	}
	else
	{
		chatchars[head] = c;
		head = (head + 1) & (QUEUESIZE-1);
	}
}

char HU_dequeueChatChar(void)
{
	char c;

	if (head != tail)
	{
		c = chatchars[tail];
		tail = (tail + 1) & (QUEUESIZE-1);
	}
	else
	{
		c = 0;
	}

	return c;
}

boolean HU_Responder(event_t *ev)
{

	static char 		lastmessage[HU_MAXLINELENGTH+1];
	char*				macromessage;
	boolean 			eatkey = false;
	static boolean		shiftdown = false;
	static boolean		altdown = false;
	unsigned char		c;
	int 				i;
	int 				numplayers;
	
	static char 		destination_keys[MAXPLAYERS] =
	{
		HUSTR_KEYGREEN,
		HUSTR_KEYINDIGO,
		HUSTR_KEYBROWN,
		HUSTR_KEYRED
	};
	
	static int			num_nobrainers = 0;

	numplayers = 0;
	for (i=0 ; i<MAXPLAYERS ; i++)
		numplayers += playeringame[i];

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

	if (!chat_on)
	{
		if (ev->data1 == HU_MSGREFRESH)
		{
			message_on = true;
			message_counter = HU_MSGTIMEOUT;
			eatkey = true;
		}
		else if (netgame && ev->data1 == HU_INPUTTOGGLE)
		{
			eatkey = chat_on = true;
			HUlib_resetIText(&w_chat);
			HU_queueChatChar(HU_BROADCAST);
		}
		else if (netgame && numplayers > 2)
		{
			for (i=0; i<MAXPLAYERS ; i++)
			{
				if (ev->data2 == destination_keys[i])
				{
					if (playeringame[i] && i!=consoleplayer)
					{
						eatkey = chat_on = true;
						HUlib_resetIText(&w_chat);
						HU_queueChatChar((char)(i+1));
						break;
					}
					else if (i == consoleplayer)
					{
						num_nobrainers++;
						if (num_nobrainers < 3)
							plr->message = HUSTR_TALKTOSELF1;
						else if (num_nobrainers < 6)
							plr->message = HUSTR_TALKTOSELF2;
						else if (num_nobrainers < 9)
							plr->message = HUSTR_TALKTOSELF3;
						else if (num_nobrainers < 32)
							plr->message = HUSTR_TALKTOSELF4;
						else
							plr->message = HUSTR_TALKTOSELF5;
					}
				}
			}
		}
	}
	else
	{
		c = ev->data2;
		// send a macro
		if (altdown)
		{
			c = c - '0';
			if (c > 9)
				return false;
			// fprintf(stderr, "got here\n");
			macromessage = chat_macros[c]->string;
			
			// kill last message with a '\n'
			HU_queueChatChar(KEY_ENTER); // DEBUG!!!
			
			// send the macro message
			while (*macromessage)
				HU_queueChatChar(*macromessage++);
			HU_queueChatChar(KEY_ENTER);
			
			// leave chat mode and notify that it was sent
			chat_on = false;
			strcpy(lastmessage, chat_macros[c]->string);
			plr->message = lastmessage;
			eatkey = true;
		}
		else
		{
			if (french)
				c = ForeignTranslation(c);
			if (shiftdown || (c >= 'a' && c <= 'z'))
				c = shiftxform[c];
			eatkey = HUlib_keyInIText(&w_chat, c);
			if (eatkey)
			{
				// static unsigned char buf[20]; // DEBUG
				HU_queueChatChar(c);
				
				// sprintf(buf, "KEY: %d => %d", ev->data1, c);
				//		plr->message = buf;
			}
			if (c == KEY_ENTER)
			{
				chat_on = false;
				if (w_chat.l.len)
				{
					strcpy(lastmessage, w_chat.l.l);
					plr->message = lastmessage;
				}
			}
			else if (c == KEY_ESCAPE)
				chat_on = false;
		}
	}

	return eatkey;

}
