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

// State.
#include "doomstat.h"
#include "r_state.h"

#include "z_zone.h"
#include "w_wad.h"

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
	int episode;

	for (i = 0, list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20, i++)
		;

	if (i == 0) {
		switchlist = Z_Malloc (sizeof(*switchlist), PU_STATIC, 0);
		*switchlist = -1;
		numswitches = 0;
	} else {
		switchlist = Z_Malloc (sizeof(*switchlist)*(i*2+1), PU_STATIC, 0);

		if (gamemode == registered || gamemode == retail)
			episode = 2;
		else if ( gamemode == commercial )
			episode = 3;
		else
			episode = 1;

		for (i = 0, list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20) {
			if (episode >= (list_p[18] | (list_p[19] << 8)))
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
void P_StartButton (line_t *line, bwhere_e w, int texture, int time, mobj_t *soundorg)
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
	button->soundorg = soundorg;
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
		} else if (switchlist[i] == texMid) {
			texture = &sides[line->sidenum[0]].midtexture;
			where = middle;
		} else if (switchlist[i] == texBot) {
			texture = &sides[line->sidenum[0]].bottomtexture;
			where = bottom;
		}

		if (texture) {
			// [RH] The original code played the sound at buttonlist->soundorg.
			//		I spawn a temporary mobj on the button's line to play the
			//		sound. It automatically removes itself after 60 seconds.
			mobj_t *soundorg = P_SpawnMobj (
									line->v1->x + (line->dx >> 1),
									line->v1->y + (line->dy >> 1),
									0, // z doesn't matter
									MT_SWITCHTEMP,
									0);
			S_StartSound (soundorg, sound, 78);
			*texture = (short)switchlist[i^1];
			if (useAgain)
				P_StartButton (line, where, switchlist[i], BUTTONTIME, soundorg);
			break;
		}
	}
}



//
// P_UseSpecialLine
// Called when a thing uses a special line.
// Only the front sides of lines are usable.
//
BOOL P_UseSpecialLine (mobj_t *thing, line_t *line, int side)
{
	BOOL result;

	// [RH] Line needs to be setup for use activation
	if ((line->flags & ML_ACTIVATIONMASK) != ML_ACTIVATEUSE)
		return false;

	// [RH] Don't let monsters activate the special unless the line says they can.
	//		(Some lines automatically imply ML_MONSTERSCANACTIVATE)
	if (!thing->player) {
		if (line->flags & ML_SECRET)
			return false;	// monsters never activate secrets
		if (!(line->flags & ML_MONSTERSCANACTIVATE) &&
			(line->special != Door_Raise || line->args[0] != 0) &&
			line->special != Teleport &&
			line->special != Teleport_NoFog)
			return false;
	}

	// [RH] Use LineSpecials[] dispatcher table.
	result = LineSpecials[line->special] (line, thing, line->args[0],
										  line->args[1], line->args[2],
										  line->args[3], line->args[4]);

	// [RH] It's possible for a script to render the line unusable even if it has the
	//		repeatable flag set by calling clearlinespecial().
	if (result)
		P_ChangeSwitchTexture (line, (line->flags & ML_REPEATABLE) && line->special);

	return result;
}

