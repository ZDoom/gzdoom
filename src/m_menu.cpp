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
// DESCRIPTION:
//		DOOM selection menu, options, episode etc.
//		Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "doomdef.h"
#include "gstrings.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_palette.h"
#include "w_wad.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_random.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_menu.h"
#include "v_text.h"
#include "st_stuff.h"
#include "d_gui.h"
#include "version.h"
#include "m_png.h"
#include "templates.h"
#include "lists.h"
#include "gi.h"
#include "p_tick.h"
#include "st_start.h"
#include "teaminfo.h"
#include "r_translate.h"
#include "g_level.h"
#include "d_event.h"
#include "colormatcher.h"
#include "d_netinf.h"

EMenuState		menuactive;
bool			M_DemoNoPlay;

DMenu *DMenu::CurrentMenu;


//============================================================================
//
// DMenu base class
//
//============================================================================

IMPLEMENT_POINTY_CLASS (DMenu)
	DECLARE_POINTER(mParentMenu)
END_POINTERS

DMenu::DMenu(DMenu *parent) 
{
	mParentMenu = parent;
}
	
DMenu::~DMenu() 
{
}

bool DMenu::Responder (event_t *ev) 
{ 
	return false; 
}

void DMenu::Ticker () 
{
}

void DMenu::Drawer () 
{
}

