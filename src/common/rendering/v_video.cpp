/*
** Video basics and init code.
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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


#include <stdio.h>

#include "i_system.h"
#include "c_cvars.h"
#include "x86.h"
#include "i_video.h"

#include "c_console.h"

#include "m_argv.h"

#include "v_video.h"
#include "v_text.h"
#include "sc_man.h"

#include "filesystem.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "hardware.h"
#include "m_png.h"
#include "menu.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"
#include "version.h"
#include "texturemanager.h"
#include "i_interface.h"
#include "v_draw.h"


EXTERN_CVAR(Int, menu_resolution_custom_width)
EXTERN_CVAR(Int, menu_resolution_custom_height)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

CVAR(Bool, r_skipmats, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

// 0 means 'no pipelining' for non GLES2 and 4 elements for GLES2
CUSTOM_CVAR(Int, gl_pipeline_depth, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self >= HW_MAX_PIPELINE_BUFFERS) self = 0;
	Printf("Changing the pipeline depth requires a restart for " GAMENAME ".\n");
}

CUSTOM_CVAR(Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < GameTicRate && self != 0)
	{
		self = GameTicRate;
	}
	else if (self > 1000)
	{
		self = 1000;
	}
}


CUSTOM_CVAR(Int, uiscale, 0, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
		return;
	}
	if (sysCallbacks.OnScreenSizeChanged) 
		sysCallbacks.OnScreenSizeChanged();
	setsizeneeded = true;
}



EXTERN_CVAR(Bool, r_blendmethod)

int active_con_scale();

#define DBGBREAK assert(0)

class DDummyFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	DDummyFrameBuffer (int width, int height)
		: DFrameBuffer (0, 0)
	{
		SetVirtualSize(width, height);
	}
	// These methods should never be called.
	void Update() override { DBGBREAK; }
	bool IsFullscreen() override { DBGBREAK; return 0; }
	int GetClientWidth() override { DBGBREAK; return 0; }
	int GetClientHeight() override { DBGBREAK; return 0; }
	void InitializeState() override {}

	float Gamma;
};

int DisplayWidth, DisplayHeight;

// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DFrameBuffer *screen;

CVAR (Int, vid_defwidth, 640, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defheight, 480, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, ticker, false, 0)

CUSTOM_CVAR (Bool, vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetVSync (*self);
	}
}

// [RH] Set true when vid_setmode command has been executed
bool setmodeneeded = false;
bool setsizeneeded = false;

//==========================================================================
//
// DCanvas Constructor
//
//==========================================================================

DCanvas::DCanvas (int _width, int _height, bool _bgra)
{
	// Init member vars
	Width = _width;
	Height = _height;
	Bgra = _bgra;
	Resize(_width, _height);
}

//==========================================================================
//
// DCanvas Destructor
//
//==========================================================================

DCanvas::~DCanvas ()
{
}

//==========================================================================
//
//
//
//==========================================================================

void DCanvas::Resize(int width, int height, bool optimizepitch)
{
	Width = width;
	Height = height;

	// Making the pitch a power of 2 is very bad for performance
	// Try to maximize the number of cache lines that can be filled
	// for each column drawing operation by making the pitch slightly
	// longer than the width. The values used here are all based on
	// empirical evidence.

	if (width <= 640 || !optimizepitch)
	{
		// For low resolutions, just keep the pitch the same as the width.
		// Some speedup can be seen using the technique below, but the speedup
		// is so marginal that I don't consider it worthwhile.
		Pitch = width;
	}
	else
	{
		// If we couldn't figure out the CPU's L1 cache line size, assume
		// it's 32 bytes wide.
		if (CPU.DataL1LineSize == 0)
		{
			CPU.DataL1LineSize = 32;
		}
		// The Athlon and P3 have very different caches, apparently.
		// I am going to generalize the Athlon's performance to all AMD
		// processors and the P3's to all non-AMD processors. I don't know
		// how smart that is, but I don't have a vast plethora of
		// processors to test with.
		if (CPU.bIsAMD)
		{
			Pitch = width + CPU.DataL1LineSize;
		}
		else
		{
			Pitch = width + max(0, CPU.DataL1LineSize - 8);
		}
	}
	int bytes_per_pixel = Bgra ? 4 : 1;
	Pixels.Resize(Pitch * height * bytes_per_pixel);
	memset (Pixels.Data(), 0, Pixels.Size());
}


CCMD(clean)
{
	Printf ("CleanXfac: %d\nCleanYfac: %d\n", CleanXfac, CleanYfac);
}


void V_UpdateModeSize (int width, int height)
{	
	// This calculates the menu scale.
	// The optimal scale will always be to fit a virtual 640 pixel wide display onto the screen.
	// Exceptions are made for a few ranges where the available virtual width is > 480.

	// This reference size is being used so that on 800x450 (small 16:9) a scale of 2 gets used.

	CleanXfac = max(min(screen->GetWidth() / 400, screen->GetHeight() / 240), 1);
	if (CleanXfac >= 4) CleanXfac--;	// Otherwise we do not have enough space for the episode/skill menus in some languages.
	CleanYfac = CleanXfac;
	CleanWidth = screen->GetWidth() / CleanXfac;
	CleanHeight = screen->GetHeight() / CleanYfac;

	int w = screen->GetWidth();
	int h = screen->GetHeight();
	
	// clamp screen aspect ratio to 17:10, for anything wider the width will be reduced
	double aspect = (double)w / h;
	if (aspect > 1.7) w = int(w * 1.7 / aspect);
	
	int factor;
	if (w < 640) factor = 1;
	else if (w >= 1024 && w < 1280) factor = 2;
	else if (w >= 1600 && w < 1920) factor = 3; 
	else  factor = w / 640;

	if (w < 1360) factor = 1;
	else if (w < 1920) factor = 2;
	else factor = int(factor * 0.7);

	CleanYfac_1 = CleanXfac_1 = factor;// max(1, int(factor * 0.7));
	CleanWidth_1 = width / CleanXfac_1;
	CleanHeight_1 = height / CleanYfac_1;

	DisplayWidth = width;
	DisplayHeight = height;
}

void V_OutputResized (int width, int height)
{
	V_UpdateModeSize(width, height);
	// set new resolution in 2D drawer
	twod->Begin(screen->GetWidth(), screen->GetHeight());
	twod->End();
	setsizeneeded = true;
	C_NewModeAdjust();
	if (sysCallbacks.OnScreenSizeChanged) 
		sysCallbacks.OnScreenSizeChanged();
}

bool IVideo::SetResolution ()
{
	DFrameBuffer *buff = CreateFrameBuffer();

	if (buff == NULL)	// this cannot really happen
	{
		return false;
	}

	screen = buff;
	screen->InitializeState();

	V_UpdateModeSize(screen->GetWidth(), screen->GetHeight());

	return true;
}

//
// V_Init
//

void V_InitScreenSize ()
{ 
	const char *i;
	int width, height, bits;

	width = height = bits = 0;

	if ( (i = Args->CheckValue ("-width")) )
		width = atoi (i);

	if ( (i = Args->CheckValue ("-height")) )
		height = atoi (i);

	if (width == 0)
	{
		if (height == 0)
		{
			width = vid_defwidth;
			height = vid_defheight;
		}
		else
		{
			width = (height * 8) / 6;
		}
	}
	else if (height == 0)
	{
		height = (width * 6) / 8;
	}
	// Remember the passed arguments for the next time the game starts up windowed.
	vid_defwidth = width;
	vid_defheight = height;
}

void V_InitScreen()
{
	screen = new DDummyFrameBuffer (vid_defwidth, vid_defheight);
}

void V_Init2()
{
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}

	UCVarValue val;

	val.Bool = !!Args->CheckParm("-devparm");
	ticker->SetGenericRepDefault(val, CVAR_Bool);


	I_InitGraphics();

	Video->SetResolution();	// this only fails via exceptions.
	Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	// init these for the scaling menu
	menu_resolution_custom_width = SCREENWIDTH;
	menu_resolution_custom_height = SCREENHEIGHT;

	screen->SetVSync(vid_vsync);
	FBaseCVar::ResetColors ();
	C_NewModeAdjust();
	setsizeneeded = true;
}

CUSTOM_CVAR (Int, vid_aspect, 0, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	setsizeneeded = true;
	if (sysCallbacks.OnScreenSizeChanged) 
		sysCallbacks.OnScreenSizeChanged();
}

DEFINE_ACTION_FUNCTION(_Screen, GetAspectRatio)
{
	ACTION_RETURN_FLOAT(ActiveRatio(screen->GetWidth(), screen->GetHeight(), nullptr));
}

CCMD(vid_setsize)
{
	if (argv.argc() < 3)
	{
		Printf("Usage: vid_setsize width height\n");
	}
	else
	{
		screen->SetWindowSize((int)strtol(argv[1], nullptr, 0), (int)strtol(argv[2], nullptr, 0));
		V_OutputResized(screen->GetClientWidth(), screen->GetClientHeight());
	}
}


void IVideo::DumpAdapters ()
{
	Printf("Multi-monitor support unavailable.\n");
}

CUSTOM_CVAR(Bool, vid_fullscreen, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setmodeneeded = true;
}

CUSTOM_CVAR(Bool, vid_hdr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CCMD(vid_listadapters)
{
	if (Video != NULL)
		Video->DumpAdapters();
}

bool vid_hdr_active = false;

DEFINE_GLOBAL(SmallFont)
DEFINE_GLOBAL(SmallFont2)
DEFINE_GLOBAL(BigFont)
DEFINE_GLOBAL(ConFont)
DEFINE_GLOBAL(NewConsoleFont)
DEFINE_GLOBAL(NewSmallFont)
DEFINE_GLOBAL(AlternativeSmallFont)
DEFINE_GLOBAL(AlternativeBigFont)
DEFINE_GLOBAL(OriginalSmallFont)
DEFINE_GLOBAL(OriginalBigFont)
DEFINE_GLOBAL(IntermissionFont)
DEFINE_GLOBAL(CleanXfac)
DEFINE_GLOBAL(CleanYfac)
DEFINE_GLOBAL(CleanWidth)
DEFINE_GLOBAL(CleanHeight)
DEFINE_GLOBAL(CleanXfac_1)
DEFINE_GLOBAL(CleanYfac_1)
DEFINE_GLOBAL(CleanWidth_1)
DEFINE_GLOBAL(CleanHeight_1)

//==========================================================================
//
// CVAR transsouls
//
// How translucent things drawn with STYLE_SoulTrans are. Normally, only
// Lost Souls have this render style.
// Values less than 0.25 will automatically be set to
// 0.25 to ensure some degree of visibility. Likewise, values above 1.0 will
// be set to 1.0, because anything higher doesn't make sense.
//
//==========================================================================

CUSTOM_CVAR(Float, transsouls, 0.75f, CVAR_ARCHIVE)
{
	if (self < 0.25f)
	{
		self = 0.25f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}

