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

class DActiveButton : public DThinker
{
	DECLARE_SERIAL (DActiveButton, DThinker);
public:
	enum EWhere
	{
		BUTTON_Top,
		BUTTON_Middle,
		BUTTON_Bottom,
		BUTTON_Nowhere
	};

	DActiveButton ();
	DActiveButton (line_t *, EWhere, SWORD tex, SDWORD time, fixed_t x, fixed_t y);

	void RunThink ();

	line_t	*m_Line;
	EWhere	m_Where;
	SWORD	m_Texture;
	SDWORD	m_Timer;
	fixed_t	m_X, m_Y;	// Location of timer sound

	friend FArchive &operator<< (FArchive &arc, EWhere where)
	{
		return arc << (BYTE)where;
	}
	friend FArchive &operator>> (FArchive &arc, EWhere &out)
	{
		BYTE in; arc >> in; out = (EWhere)in; return arc;
	}
};

static int *switchlist;
static int  numswitches;

//
// P_InitSwitchList
// Only called at game initialization.
//
// [RH] Rewritten to use a BOOM-style SWITCHES lump and remove the
//		MAXSWITCHES limit.
void P_InitSwitchList(void)
{
	byte *alphSwitchList = (byte *)W_CacheLumpName ("SWITCHES", PU_STATIC);
	byte *list_p;
	int i;

	for (i = 0, list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20, i++)
		;

	if (i == 0)
	{
		switchlist = (int *)Z_Malloc (sizeof(*switchlist), PU_STATIC, 0);
		*switchlist = -1;
		numswitches = 0;
	}
	else
	{
		switchlist = (int *)Z_Malloc (sizeof(*switchlist)*(i*2+1), PU_STATIC, 0);

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
static void P_StartButton (line_t *line, DActiveButton::EWhere w, int texture,
						   int time, fixed_t x, fixed_t y)
{
	DActiveButton *button;
	TThinkerIterator<DActiveButton> iterator;
	
	// See if button is already pressed
	while ( (button = iterator.Next ()) )
	{
		if (button->m_Line == line)
			return;
	}

	new DActiveButton (line, w, texture, time, x, y);
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
		short *texture = NULL;
		DActiveButton::EWhere where;

		if (switchlist[i] == texTop)
		{
			texture = &sides[line->sidenum[0]].toptexture;
			where = DActiveButton::BUTTON_Top;
		}
		else if (switchlist[i] == texBot)
		{
			texture = &sides[line->sidenum[0]].bottomtexture;
			where = DActiveButton::BUTTON_Bottom;
		}
		else if (switchlist[i] == texMid)
		{
			texture = &sides[line->sidenum[0]].midtexture;
			where = DActiveButton::BUTTON_Middle;
		}

		if (texture)
		{
			// [RH] The original code played the sound at buttonlist->soundorg,
			//		which wasn't necessarily anywhere near the switch if it was
			//		facing a big sector.
			fixed_t x = line->v1->x + (line->dx >> 1);
			fixed_t y = line->v1->y + (line->dy >> 1);
			S_Sound (x, y, CHAN_VOICE, sound, 1, ATTN_STATIC);
			*texture = (short)switchlist[i^1];
			if (useAgain)
				P_StartButton (line, where, switchlist[i], BUTTONTIME, x, y);
			break;
		}
	}
}

IMPLEMENT_SERIAL (DActiveButton, DThinker)

DActiveButton::DActiveButton ()
{
	m_Line = NULL;
	m_Where = BUTTON_Nowhere;
	m_Texture = 0;
	m_Timer = 0;
	m_X = 0;
	m_Y = 0;
}

DActiveButton::DActiveButton (line_t *line, EWhere where, SWORD texture,
							  SDWORD time, fixed_t x, fixed_t y)
{
	m_Line = line;
	m_Where = where;
	m_Texture = texture;
	m_Timer = time;
	m_X = x;
	m_Y = y;
}

void DActiveButton::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Line << m_Where << m_Texture << m_Timer << m_X << m_Y;
	}
	else
	{
		arc >> m_Line >> m_Where >> m_Texture >> m_Timer >> m_X >> m_Y;
	}
}

void DActiveButton::RunThink ()
{
	if (0 >= --m_Timer)
	{
		switch (m_Where)
		{
		case BUTTON_Top:
			sides[m_Line->sidenum[0]].toptexture = m_Texture;
			break;
			
		case BUTTON_Middle:
			sides[m_Line->sidenum[0]].midtexture = m_Texture;
			break;
			
		case BUTTON_Bottom:
			sides[m_Line->sidenum[0]].bottomtexture = m_Texture;
			break;
		}
		S_Sound (m_X, m_Y, CHAN_VOICE, "switches/normbutn", 1, ATTN_STATIC);
		delete this;
	}
}
