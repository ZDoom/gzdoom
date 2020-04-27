/*
** resolutionmenu.cpp
** Basic Custom Resolution Selector for the Menu
**
**---------------------------------------------------------------------------
** Copyright 2018 Rachael Alexanderson
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

#include "doomdef.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_video.h"
#include "menu/menu.h"

CVAR(Int, menu_resolution_custom_width, 640, 0)
CVAR(Int, menu_resolution_custom_height, 480, 0)

EXTERN_CVAR(Bool, vid_fullscreen)
EXTERN_CVAR(Bool, win_maximized)
EXTERN_CVAR(Float, vid_scale_custompixelaspect)
EXTERN_CVAR(Int, vid_scale_customwidth)
EXTERN_CVAR(Int, vid_scale_customheight)
EXTERN_CVAR(Int, vid_scalemode)
EXTERN_CVAR(Float, vid_scalefactor)

CCMD (menu_resolution_set_custom)
{
	if (argv.argc() > 2)
	{
		menu_resolution_custom_width = atoi(argv[1]);
		menu_resolution_custom_height = atoi(argv[2]);
	}
	else
	{
		Printf("This command is not meant to be used outside the menu! But if you want to use it, please specify <x> and <y>.\n");
	}
	M_PreviousMenu();
}

CCMD (menu_resolution_commit_changes)
{
	int do_fullscreen = vid_fullscreen;
	if (argv.argc() > 1)
	{
		do_fullscreen = atoi(argv[1]);
	}

	if (do_fullscreen == false)
	{
		vid_scalemode = 0;
		vid_scalefactor = 1.;
		screen->SetWindowSize(menu_resolution_custom_width, menu_resolution_custom_height);
		V_OutputResized(screen->GetClientWidth(), screen->GetClientHeight());
	}
	else
	{
		vid_fullscreen = true;
		vid_scalemode = 5;
		vid_scalefactor = 1.;
		vid_scale_customwidth = menu_resolution_custom_width;
		vid_scale_customheight = menu_resolution_custom_height;
		vid_scale_custompixelaspect = 1.0;
	}
}


