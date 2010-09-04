/*
** m_options.cpp
** New options menu code
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
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
** Sorry this got so convoluted. It was originally much cleaner until
** I started adding all sorts of gadgets to the menus. I might someday
** make a project of rewriting the entire menu system using Amiga-style
** taglists to describe each menu item. We'll see... (Probably not.)
*/

#include "templates.h"

#include "doomdef.h"
#include "gstrings.h"

#include "c_console.h"
#include "c_dispatch.h"
#include "c_bind.h"

#include "d_main.h"
#include "d_gui.h"

#include "i_system.h"
#include "i_video.h"

#include "i_music.h"
#include "i_input.h"
#include "m_joy.h"

#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "gi.h"

#include "r_local.h"
#include "v_palette.h"
#include "gameconfigfile.h"

#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

#include "m_misc.h"
#include "hardware.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "d_event.h"

#include "sbar.h"

// Data.
#include "m_menu.h"
// just some basics so that the rest of the source still compiles.
int testingmode;		// Holds time to revert to old mode
CUSTOM_CVAR (Bool, vid_tft, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	/*
	if (self)
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 5.f;
		ModesItems[VM_ASPECTITEM].e.values = RatiosTFT;
	}
	else
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 4.f;
		ModesItems[VM_ASPECTITEM].e.values = Ratios;
		if (menu_screenratios == 4)
		{
			menu_screenratios = 0;
		}
	}
	*/
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->ScreenSizeChanged();
	}	
}
