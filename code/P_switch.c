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
//
// DESCRIPTION:
//		Switches, buttons. Two-state animation. Exits.
//
//-----------------------------------------------------------------------------



#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "g_game.h"
#include "s_sound.h"
#include "doomstat.h"
#include "r_state.h"
#include "z_zone.h"
#include "w_wad.h"

#include "gi.h"

//
// CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//

static int *switchlist;
static int  numswitches;

button_t   *buttonlist;		// [RH] remove limit on number of buttons

//
// P_InitSwitchList
// Only called at game initialization.
//
// [RH] Rewritten to use a BOOM-style SWITCHES lump and remove the
//		MAXSWITCHES limit.
void P_InitSwitchList(void)
{
	byte *alphSwitchList = W_CacheLumpName ("SWITCHES", PU_STATIC);
	byte *list_p;
	int i;

	for (i = 0, list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20, i++)
		;

	if (i == 0)
	{
		switchlist = Z_Malloc (sizeof(*switchlist), PU_STATIC, 0);
		*switchlist = -1;
		numswitches = 0;
	}
	else
	{
		switchlist = Z_Malloc (sizeof(*switchlist)*(i*2+1), PU_STATIC, 0);

		for (i = 0, list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20)
		{
			if (((gameinfo.maxSwitch & 15) >= (list_p[18] & 15)) &&
				((gameinfo.maxSwitch & ~15) == (list_p[18] & ~15)))
			{
				// [RH] Skip this switch if it can't be found.
				if (R_CheckTextureNumForName (list_p /* .name1 */) < 0)
					continue;

				switchlist[i++] = R_TextureNumForName(list_p /* .name1 */);
				switchlist[i++] = R_TextureNumForName(list_p + 9 /* .name2 */);
			}
		}
		numswitches = i/2;
		switchlist[i] = -1;
	}

	Z_Free (alphSwitchList);
}

//
// Start a button counting down till it turns off.
// [RH] Rewritten to remove MAXBUTTONS limit and use temporary soundorgs.
//
void P_StartButton (line_t *line, bwhere_e w, int texture, int time, fixed_t x, fixed_t y)
{
	button_t *button;
	
	// See if button is already pressed
	button = buttonlist;
	while (button) {
		if (button->line == line)
			return;
		button = button->next;
	}

	button = Z_Malloc (sizeof(*button), PU_LEVEL, 0);

	button->line = line;
	button->where = w;
	button->btexture = texture;
	button->btimer = time;
	button->x = x;
	button->y = y;
	button->next = buttonlist;
	buttonlist = button;
}

//
// Function that changes wall texture.
// Tell it if switch is ok to use again (1=yes, it's a button).
//
void P_ChangeSwitchTexture (line_t *line, int useAgain)
{
	int texTop;
	int texMid;
	int texBot;
	int i;
	char *sound;

	if (!useAgain)
		line->special = 0;

	texTop = sides[line->sidenum[0]].toptexture;
	texMid = sides[line->sidenum[0]].midtexture;
	texBot = sides[line->sidenum[0]].bottomtexture;

	// EXIT SWITCH?
	if (line->special == Exit_Normal ||
		line->special == Exit_Secret ||
		line->special == Teleport_NewMap ||
		line->special == Teleport_EndGame)
		sound = "switches/exitbutn";
	else
		sound = "switches/normbutn";

	for (i = 0; i < numswitches*2; i++)
	{
		// [RH] Rewritten slightly to be more compact
		short *texture = NULL;
		bwhere_e where;

		if (switchlist[i] == texTop) {
			texture = &sides[line->sidenum[0]].toptexture;
			where = top;
		} else if (switchlist[i] == texBot) {
			texture = &sides[line->sidenum[0]].bottomtexture;
			where = bottom;
		} else if (switchlist[i] == texMid) {
			texture = &sides[line->sidenum[0]].midtexture;
			where = middle;
		}

		if (texture) {
			// [RH] The original code played the sound at buttonlist->soundorg,
			//		which wasn't necessarily anywhere near the switch if it was
			//		facing a big sector.
			fixed_t x = line->v1->x + (line->dx >> 1);
			fixed_t y = line->v1->y + (line->dy >> 1);
			S_PositionedSound (x, y, CHAN_VOICE, sound, 1, ATTN_STATIC);
			*texture = (short)switchlist[i^1];
			if (useAgain)
				P_StartButton (line, where, switchlist[i], BUTTONTIME, x, y);
			break;
		}
	}
}
