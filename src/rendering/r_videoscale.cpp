/*---------------------------------------------------------------------------
**
** Copyright(C) 2017 Magnus Norddahl
** Copyright(C) 2017-2020 Rachael Alexanderson
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

#include <math.h>
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_video.h"
#include "templates.h"
#include "r_videoscale.h"

#include "console/c_console.h"
#include "menu/menu.h"

#define NUMSCALEMODES countof(vScaleTable)

extern bool setsizeneeded, multiplayer, generic_ui;

EXTERN_CVAR(Int, vid_aspect)

EXTERN_CVAR(Bool, log_vgafont)
EXTERN_CVAR(Bool, dlg_vgafont)

CUSTOM_CVAR(Int, vid_scale_customwidth, VID_MIN_WIDTH, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < VID_MIN_WIDTH)
		self = VID_MIN_WIDTH;
	setsizeneeded = true;
}
CUSTOM_CVAR(Int, vid_scale_customheight, VID_MIN_HEIGHT, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < VID_MIN_HEIGHT)
		self = VID_MIN_HEIGHT;
	setsizeneeded = true;
}
CVAR(Bool, vid_scale_linear, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Float, vid_scale_custompixelaspect, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
	if (self < 0.2 || self > 5.0)
		self = 1.0;
}

namespace
{
	uint32_t min_width = VID_MIN_WIDTH;
	uint32_t min_height = VID_MIN_HEIGHT;

	float v_MinimumToFill(uint32_t inwidth, uint32_t inheight)
	{
		// sx = screen x dimension, sy = same for y
		float sx = (float)inwidth, sy = (float)inheight;
		static float lastsx = 0., lastsy = 0., result = 0.;
		if (lastsx != sx || lastsy != sy)
		{
			if (sx <= 0. || sy <= 0.)
				return 1.; // prevent x/0 error
			// set absolute minimum scale to fill the entire screen but get as close to 640x400 as possible
			float ssx = (float)(VID_MIN_UI_WIDTH) / sx, ssy = (float)(VID_MIN_UI_HEIGHT) / sy;
			result = (ssx < ssy) ? ssy : ssx;
			lastsx = sx;
			lastsy = sy;
		}
		return result;
	}
	inline uint32_t v_mfillX(uint32_t inwidth, uint32_t inheight)
	{
		return (uint32_t)((float)inwidth * v_MinimumToFill(inwidth, inheight));
	}
	inline uint32_t v_mfillY(uint32_t inwidth, uint32_t inheight)
	{
		return (uint32_t)((float)inheight * v_MinimumToFill(inwidth, inheight));
	}
	inline void refresh_minimums()
	{
		// specialUI is tracking a state where high-res console fonts are actually required, and
		// aren't actually rendered correctly in 320x200. this forces the game to revert to the 640x400
		// minimum set in GZDoom 4.0.0, but only while those fonts are required.

		static bool lastspecialUI = false;
		bool isInActualMenu = false;

		bool specialUI = (generic_ui || !!log_vgafont || !!dlg_vgafont || ConsoleState != c_up || multiplayer ||
			(menuactive == MENU_On && CurrentMenu && !CurrentMenu->IsKindOf("ConversationMenu")));

		if (specialUI == lastspecialUI)
			return;

		lastspecialUI = specialUI;
		setsizeneeded = true;

		if (!specialUI)
		{
			min_width = VID_MIN_WIDTH;
			min_height = VID_MIN_HEIGHT;
		}
		else
		{
			min_width = VID_MIN_UI_WIDTH;
			min_height = VID_MIN_UI_HEIGHT;
		}
	}
	
	// the odd formatting of this struct definition is meant to resemble a table header. set your tab stops to 4 when editing this file.
	struct v_ScaleTable
		{ bool isValid;		uint32_t(*GetScaledWidth)(uint32_t Width, uint32_t Height);								uint32_t(*GetScaledHeight)(uint32_t Width, uint32_t Height);						float pixelAspect;		bool isCustom;	};
	v_ScaleTable vScaleTable[] =
	{
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return Width; },		        		[](uint32_t Width, uint32_t Height)->uint32_t { return Height; },	        		1.0f,	  				false   },	// 0  - Native
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return v_mfillX(Width, Height); },		[](uint32_t Width, uint32_t Height)->uint32_t { return v_mfillY(Width, Height); },	1.0f,					false   },	// 6  - Minimum Scale to Fill Entire Screen
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return 640; },		            		[](uint32_t Width, uint32_t Height)->uint32_t { return 400; },			        	1.2f,   				false   },	// 2  - 640x400 (formerly 320x200)
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return 960; },		            		[](uint32_t Width, uint32_t Height)->uint32_t { return 600; },				        1.2f,  				 	false   },	// 3  - 960x600 (formerly 640x400)
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return 1280; },		           		[](uint32_t Width, uint32_t Height)->uint32_t { return 800; },	        			1.2f,   				false   },	// 4  - 1280x800
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return vid_scale_customwidth; },		[](uint32_t Width, uint32_t Height)->uint32_t { return vid_scale_customheight; },	1.0f,   				true    },	// 5  - Custom
		{ true,				[](uint32_t Width, uint32_t Height)->uint32_t { return 320; },		            		[](uint32_t Width, uint32_t Height)->uint32_t { return 200; },			        	1.2f,   				false   },	// 7  - 320x200
	};
	bool isOutOfBounds(int x)
	{
		return (x < 0 || x >= int(NUMSCALEMODES) || vScaleTable[x].isValid == false);
	}
}

void R_ShowCurrentScaling();
CUSTOM_CVAR(Float, vid_scalefactor, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setsizeneeded = true;
	if (self < 0.05 || self > 2.0)
		self = 1.0;
	if (self != 1.0)
		R_ShowCurrentScaling();
}

CUSTOM_CVAR(Int, vid_scalemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
	if (isOutOfBounds(self))
		self = 0;
}

CUSTOM_CVAR(Bool, vid_cropaspect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
}

bool ViewportLinearScale()
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	// always use linear if supersampling
	int x = screen->GetClientWidth(), y = screen->GetClientHeight();
	float aspectmult = ViewportPixelAspect();
	if (aspectmult > 1.f)
		aspectmult = 1.f / aspectmult;
	if ((ViewportScaledWidth(x,y) > (x * aspectmult)) || (ViewportScaledHeight(x,y) > (y * aspectmult)))
		return true;
	
	return vid_scale_linear;
}

int ViewportScaledWidth(int width, int height)
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	refresh_minimums();
	if (vid_cropaspect && height > 0)
	{
		width = ((float)width/height > ActiveRatio(width, height)) ? (int)(height * ActiveRatio(width, height)) : width;
		height = ((float)width/height < ActiveRatio(width, height)) ? (int)(width / ActiveRatio(width, height)) : height;
	}
	return (int)MAX((int32_t)min_width, (int32_t)(vid_scalefactor * vScaleTable[vid_scalemode].GetScaledWidth(width, height)));
}

int ViewportScaledHeight(int width, int height)
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	if (vid_cropaspect && height > 0)
	{
		height = ((float)width/height < ActiveRatio(width, height)) ? (int)(width / ActiveRatio(width, height)) : height;
		width = ((float)width/height > ActiveRatio(width, height)) ? (int)(height * ActiveRatio(width, height)) : width;
	}
	return (int)MAX((int32_t)min_height, (int32_t)(vid_scalefactor * vScaleTable[vid_scalemode].GetScaledHeight(width, height)));
}

float ViewportPixelAspect()
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	// hack - use custom scaling if in "custom" mode
	if (vScaleTable[vid_scalemode].isCustom)
		return vid_scale_custompixelaspect;
	return vScaleTable[vid_scalemode].pixelAspect;
}

void R_ShowCurrentScaling()
{
	int x1 = screen->GetClientWidth(), y1 = screen->GetClientHeight(), x2 = ViewportScaledWidth(x1, y1), y2 = ViewportScaledHeight(x1, y1);
	Printf("Current vid_scalefactor: %f\n", (float)(vid_scalefactor));
	Printf("Real resolution: %i x %i\nEmulated resolution: %i x %i\n", x1, y1, x2, y2);
}

CCMD (vid_showcurrentscaling)
{
	R_ShowCurrentScaling();
}

CCMD (vid_scaletowidth)
{
	if (argv.argc() > 1)
	{
		// the following enables the use of ViewportScaledWidth to get the proper dimensions in custom scale modes
		vid_scalefactor = 1;
		vid_scalefactor = (float)((double)atof(argv[1]) / ViewportScaledWidth(screen->GetClientWidth(), screen->GetClientHeight()));
	}
}

CCMD (vid_scaletoheight)
{
	if (argv.argc() > 1)
	{
		vid_scalefactor = 1;
		vid_scalefactor = (float)((double)atof(argv[1]) / ViewportScaledHeight(screen->GetClientWidth(), screen->GetClientHeight()));
	}
}

inline bool atob(char* I)
{
    if (stricmp (I, "true") == 0 || stricmp (I, "1") == 0)
        return true;
    return false;
}

CCMD (vid_setscale)
{
    if (argv.argc() > 2)
    {
        vid_scale_customwidth = atoi(argv[1]);
        vid_scale_customheight = atoi(argv[2]);
        if (argv.argc() > 3)
        {
            vid_scale_linear = atob(argv[3]);
            if (argv.argc() > 4)
            {
                vid_scale_custompixelaspect = (float)atof(argv[4]);
            }
        }
        vid_scalemode = 5;
	vid_scalefactor = 1.0;
    }
    else
    {
        Printf("Usage: vid_setscale <x> <y> [bool linear] [float pixel-shape]\nThis command will create a custom viewport scaling mode.\n");
    }
}

CCMD (vid_scaletolowest)
{
	uint32_t method = 0;
	if (argv.argc() > 1)
		method = atoi(argv[1]);
	switch (method)
	{
	case 1:		// Method 1: set a custom video scaling
		vid_scalemode = 5;
		vid_scalefactor = 1.0;
		vid_scale_custompixelaspect = 1.0;
		vid_scale_customwidth = v_mfillX(screen->GetClientWidth(), screen->GetClientHeight());
		vid_scale_customheight = v_mfillY(screen->GetClientWidth(), screen->GetClientHeight());
		break;
	case 2:		// Method 2: use the actual downscaling mode directly
		vid_scalemode = 1;
		vid_scalefactor = 1.0;
		break;
	default:	// Default method: use vid_scalefactor to achieve the result on a default scaling mode
		vid_scalemode = 0;
		vid_scalefactor = v_MinimumToFill(screen->GetClientWidth(), screen->GetClientHeight());
		break;
	}
}
