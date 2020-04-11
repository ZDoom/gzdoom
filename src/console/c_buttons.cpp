/*
** c_dispatch.cpp
** Functions for executing console commands and aliases
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include "c_buttons.h"
#include "templates.h"
#include "c_dispatch.h"
#include "printf.h"
#include "cmdlib.h"
#include "c_console.h"


FButtonStatus Button_Mlook, Button_Klook, Button_Use, Button_AltAttack,
	Button_Attack, Button_Speed, Button_MoveRight, Button_MoveLeft,
	Button_Strafe, Button_LookDown, Button_LookUp, Button_Back,
	Button_Forward, Button_Right, Button_Left, Button_MoveDown,
	Button_MoveUp, Button_Jump, Button_ShowScores, Button_Crouch,
	Button_Zoom, Button_Reload,
	Button_User1, Button_User2, Button_User3, Button_User4,
	Button_AM_PanLeft, Button_AM_PanRight, Button_AM_PanDown, Button_AM_PanUp,
	Button_AM_ZoomIn, Button_AM_ZoomOut;


// To add new actions, go to the console and type "key <action name>".
// This will give you the key value to use in the first column. Then
// insert your new action into this list so that the keys remain sorted
// in ascending order. No two keys can be identical. If yours matches
// an existing key, change the name of your action.

FActionMap ActionMaps[] =
{
	{ &Button_AM_PanLeft,	0x0d52d67b, "am_panleft"},
	{ &Button_User2,		0x125f5226, "user2" },
	{ &Button_Jump,			0x1eefa611, "jump" },
	{ &Button_Right,		0x201f1c55, "right" },
	{ &Button_Zoom,			0x20ccc4d5, "zoom" },
	{ &Button_Back,			0x23a99cd7, "back" },
	{ &Button_AM_ZoomIn,	0x41df90c2, "am_zoomin"},
	{ &Button_Reload,		0x426b69e7, "reload" },
	{ &Button_LookDown,		0x4463f43a, "lookdown" },
	{ &Button_AM_ZoomOut,	0x51f7a334, "am_zoomout"},
	{ &Button_User4,		0x534c30ee, "user4" },
	{ &Button_Attack,		0x5622bf42, "attack" },
	{ &Button_User1,		0x577712d0, "user1" },
	{ &Button_Klook,		0x57c25cb2, "klook" },
	{ &Button_Forward,		0x59f3e907, "forward" },
	{ &Button_MoveDown,		0x6167ce99, "movedown" },
	{ &Button_AltAttack,	0x676885b8, "altattack" },
	{ &Button_MoveLeft,		0x6fa41b84, "moveleft" },
	{ &Button_MoveRight,	0x818f08e6, "moveright" },
	{ &Button_AM_PanRight,	0x8197097b, "am_panright"},
	{ &Button_AM_PanUp,		0x8d89955e, "am_panup"} ,
	{ &Button_Mlook,		0xa2b62d8b, "mlook" },
	{ &Button_Crouch,		0xab2c3e71, "crouch" },
	{ &Button_Left,			0xb000b483, "left" },
	{ &Button_LookUp,		0xb62b1e49, "lookup" },
	{ &Button_User3,		0xb6f8fe92, "user3" },
	{ &Button_Strafe,		0xb7e6a54b, "strafe" },
	{ &Button_AM_PanDown,	0xce301c81, "am_pandown"},
	{ &Button_ShowScores,	0xd5897c73, "showscores" },
	{ &Button_Speed,		0xe0ccb317, "speed" },
	{ &Button_Use,			0xe0cfc260, "use" },
	{ &Button_MoveUp,		0xfdd701c7, "moveup" },
};
#define NUM_ACTIONS countof(ActionMaps)


// FindButton scans through the actionbits[] array
// for a matching key and returns an index or -1 if
// the key could not be found. This uses binary search,
// so actionbits[] must be sorted in ascending order.

FButtonStatus *FindButton (unsigned int key)
{
	const FActionMap *bit;

	bit = BinarySearch<FActionMap, unsigned int>
			(ActionMaps, NUM_ACTIONS, &FActionMap::Key, key);
	return bit ? bit->Button : NULL;
}

bool FButtonStatus::PressKey (int keynum)
{
	int i, open;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force on
		Keys[0] = 0xffff;
		for (i = MAX_KEYS-1; i > 0; --i)
		{
			Keys[i] = 0;
		}
	}
	else
	{
		for (i = MAX_KEYS-1, open = -1; i >= 0; --i)
		{
			if (Keys[i] == 0)
			{
				open = i;
			}
			else if (Keys[i] == keynum)
			{ // Key is already down; do nothing
				return false;
			}
		}
		if (open < 0)
		{ // No free key slots, so do nothing
			Printf ("More than %u keys pressed for a single action!\n", MAX_KEYS);
			return false;
		}
		Keys[open] = keynum;
	}
	uint8_t wasdown = bDown;
	bDown = bWentDown = true;
	// Returns true if this key caused the button to go down.
	return !wasdown;
}

bool FButtonStatus::ReleaseKey (int keynum)
{
	int i, numdown, match;
	uint8_t wasdown = bDown;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force off
		for (i = MAX_KEYS-1; i >= 0; --i)
		{
			Keys[i] = 0;
		}
		bWentUp = true;
		bDown = false;
	}
	else
	{
		for (i = MAX_KEYS-1, numdown = 0, match = -1; i >= 0; --i)
		{
			if (Keys[i] != 0)
			{
				++numdown;
				if (Keys[i] == keynum)
				{
					match = i;
				}
			}
		}
		if (match < 0)
		{ // Key was not down; do nothing
			return false;
		}
		Keys[match] = 0;
		bWentUp = true;
		if (--numdown == 0)
		{
			bDown = false;
		}
	}
	// Returns true if releasing this key caused the button to go up.
	return wasdown && !bDown;
}

void ResetButtonTriggers ()
{
	for (int i = NUM_ACTIONS-1; i >= 0; --i)
	{
		ActionMaps[i].Button->ResetTriggers ();
	}
}

void ResetButtonStates ()
{
	for (int i = NUM_ACTIONS-1; i >= 0; --i)
	{
		FButtonStatus *button = ActionMaps[i].Button;

		if (button != &Button_Mlook && button != &Button_Klook)
		{
			button->ReleaseKey (0);
		}
		button->ResetTriggers ();
	}
}



int ListActionCommands (const char *pattern)
{
	char matcher[16];
	unsigned int i;
	int count = 0;

	for (i = 0; i < NUM_ACTIONS; ++i)
	{
		if (pattern == NULL || CheckWildcards (pattern,
			(mysnprintf (matcher, countof(matcher), "+%s", ActionMaps[i].Name), matcher)))
		{
			Printf ("+%s\n", ActionMaps[i].Name);
			count++;
		}
		if (pattern == NULL || CheckWildcards (pattern,
			(mysnprintf (matcher, countof(matcher), "-%s", ActionMaps[i].Name), matcher)))
		{
			Printf ("-%s\n", ActionMaps[i].Name);
			count++;
		}
	}
	return count;
}

void AddButtonTabCommands()
{
	// Add all the action commands for tab completion
	for (int i = 0; i < NUM_ACTIONS; i++)
	{
		char tname[16];
		strcpy (&tname[1], ActionMaps[i].Name);
		tname[0] = '+';
		C_AddTabCommand (tname);
		tname[0] = '-';
		C_AddTabCommand (tname);
	}
}