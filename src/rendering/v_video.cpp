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
#include "r_state.h"
#include "am_map.h"

#include "doomstat.h"

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"

#include "v_video.h"
#include "v_text.h"
#include "sc_man.h"

#include "filesystem.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "sbar.h"
#include "hardware.h"
#include "m_png.h"
#include "r_utility.h"
#include "swrenderer/r_renderer.h"
#include "menu/menu.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"
#include "version.h"
#include "g_levellocals.h"
#include "am_map.h"
#include "texturemanager.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "v_palette.h"

CVAR(Float, underwater_fade_scalar, 1.0f, CVAR_ARCHIVE) // [Nash] user-settable underwater blend intensity

EXTERN_CVAR(Int, menu_resolution_custom_width)
EXTERN_CVAR(Int, menu_resolution_custom_height)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

CUSTOM_CVAR(Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
}

CUSTOM_CVAR(Int, vid_rendermode, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 4)
	{
		self = 4;
	}
	else if (self == 2 || self == 3)
	{
		self = self - 2; // softpoly to software
	}

	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
	screen->SetTextureFilterMode();

	// No further checks needed. All this changes now is which scene drawer the render backend calls.
}

CUSTOM_CVAR(Int, vid_preferbackend, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// [SP] This may seem pointless - but I don't want to implement live switching just
	// yet - I'm pretty sure it's going to require a lot of reinits and destructions to
	// do it right without memory leaks

	switch(self)
	{
	case 2:
		Printf("Selecting SoftPoly backend...\n");
		break;
#ifdef HAVE_VULKAN
	case 1:
		Printf("Selecting Vulkan backend...\n");
		break;
#endif
	default:
		Printf("Selecting OpenGL backend...\n");
	}

	Printf("Changing the video backend requires a restart for " GAMENAME ".\n");
}

CVAR(Int, vid_renderer, 1, 0)	// for some stupid mods which threw caution out of the window...

CUSTOM_CVAR(Int, uiscale, 0, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
		return;
	}
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	setsizeneeded = true;
}



EXTERN_CVAR(Bool, r_blendmethod)

int active_con_scale();

FRenderer *SWRenderer;

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
	void Update() { DBGBREAK; }
	bool IsFullscreen() { DBGBREAK; return 0; }
	int GetClientWidth() { DBGBREAK; return 0; }
	int GetClientHeight() { DBGBREAK; return 0; }
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
bool	setmodeneeded = false;

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
			Pitch = width + MAX(0, CPU.DataL1LineSize - 8);
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

	CleanXfac = std::max(std::min(screen->GetWidth() / 400, screen->GetHeight() / 240), 1);
	if (CleanXfac >= 4) CleanXfac--;	// Otherwise we do not have enough space for the episode/skill menus in some languages.
	CleanYfac = CleanXfac;
	CleanWidth = screen->GetWidth() / CleanXfac;
	CleanHeight = screen->GetHeight() / CleanYfac;

	int w = screen->GetWidth();
	int factor;
	if (w < 640) factor = 1;
	else if (w >= 1024 && w < 1280) factor = 2;
	else if (w >= 1600 && w < 1920) factor = 3; 
	else  factor = w / 640;

	if (w < 1360) factor = 1;
	else if (w < 1920) factor = 2;
	else factor = int(factor * 0.7);

	CleanYfac_1 = CleanXfac_1 = factor;// MAX(1, int(factor * 0.7));
	CleanWidth_1 = width / CleanXfac_1;
	CleanHeight_1 = height / CleanYfac_1;

	DisplayWidth = width;
	DisplayHeight = height;

	R_OldBlend = ~0;
}

void V_OutputResized (int width, int height)
{
	V_UpdateModeSize(width, height);
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	C_NewModeAdjust();
	// Reload crosshair if transitioned to a different size
	ST_LoadCrosshair(true);
	if (primaryLevel && primaryLevel->automap)
		primaryLevel->automap->NewResolution();
}

void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *_cx1, int *_cx2)
{
	if (designheight < 240 && realheight >= 480) designheight = 240;
	*cleanx = *cleany = std::min(realwidth / designwidth, realheight / designheight);
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
	screen->SetGamma();

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
	float gamma = static_cast<DDummyFrameBuffer *>(screen)->Gamma;

	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}

	UCVarValue val;

	val.Bool = !!Args->CheckParm("-devparm");
	ticker.SetGenericRepDefault(val, CVAR_Bool);


	I_InitGraphics();

	Video->SetResolution();	// this only fails via exceptions.
	Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	// init these for the scaling menu
	menu_resolution_custom_width = SCREENWIDTH;
	menu_resolution_custom_height = SCREENHEIGHT;

	screen->SetVSync(vid_vsync);
	screen->SetGamma ();
	FBaseCVar::ResetColors ();
	C_NewModeAdjust();
	setsizeneeded = true;
}

CUSTOM_CVAR (Int, vid_aspect, 0, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
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

CUSTOM_CVAR_NAMED(Bool, vid_fullscreen, fullscreen, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
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
DEFINE_GLOBAL(generic_ui)

IHardwareTexture* CreateHardwareTexture()
{
	return screen->CreateHardwareTexture();
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

FVector4 DFrameBuffer::CalcBlend(sector_t* viewsector, PalEntry* modulateColor)
{
	float blend[4] = { 0,0,0,0 };
	PalEntry blendv = 0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t* player = nullptr;
	bool fullbright = false;

	if (modulateColor) *modulateColor = 0xffffffff;

	if (players[consoleplayer].camera != nullptr)
	{
		player = players[consoleplayer].camera->player;
		if (player)
			fullbright = (player->fixedcolormap != NOFIXEDCOLORMAP || player->extralight == INT_MIN || player->fixedlightlevel != -1);
	}

	// don't draw sector based blends when any fullbright screen effect is active.
	if (!fullbright)
	{
		const auto& vpp = r_viewpoint.Pos;
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->GetHeightSec())
			{
				auto s = viewsector->heightsec;
				blendv = s->floorplane.PointOnSide(vpp) < 0 ? s->bottommap : s->ceilingplane.PointOnSide(vpp) < 0 ? s->topmap : s->midmap;
			}
		}
		else
		{
			TArray<lightlist_t>& lightlist = viewsector->e->XFloor.lightlist;

			for (unsigned int i = 0; i < lightlist.Size(); i++)
			{
				double lightbottom;
				if (i < lightlist.Size() - 1)
					lightbottom = lightlist[i + 1].plane.ZatPoint(vpp);
				else
					lightbottom = viewsector->floorplane.ZatPoint(vpp);

				if (lightbottom < vpp.Z && (!lightlist[i].caster || !(lightlist[i].caster->flags & FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv = lightlist[i].blend;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->Colormap.FadeColor) blendv = 0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a == 0 && blendv != 0) blendv.a = 128;
					break;
				}
			}
		}

		if (blendv.a == 0 && V_IsTrueColor())	// The paletted software renderer uses the original colormap as this frame's palette, but in true color that isn't doable.
		{
			blendv = R_BlendForColormap(blendv);
		}

		if (blendv.a == 255)
		{

			extra_red = blendv.r / 255.0f;
			extra_green = blendv.g / 255.0f;
			extra_blue = blendv.b / 255.0f;

			// If this is a multiplicative blend do it separately and add the additive ones on top of it.

			// black multiplicative blends are ignored
			if (extra_red || extra_green || extra_blue)
			{
				if (modulateColor) *modulateColor = blendv;
			}
			blendv = 0;
		}
		else if (blendv.a)
		{
			// [Nash] allow user to set blend intensity
			int cnt = blendv.a;
			cnt = (int)(cnt * underwater_fade_scalar);

			V_AddBlend(blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, cnt / 255.0f, blend);
		}
	}
	else if (player && player->fixedlightlevel != -1 && player->fixedcolormap == NOFIXEDCOLORMAP)
	{
		// Draw fixedlightlevel effects as a 2D overlay. The hardware renderer just processes such a scene fullbright without any lighting.
		auto torchtype = PClass::FindActor(NAME_PowerTorch);
		auto litetype = PClass::FindActor(NAME_PowerLightAmp);
		PalEntry color = 0xffffffff;
		for (AActor* in = player->mo->Inventory; in; in = in->Inventory)
		{
			// Need special handling for light amplifiers 
			if (in->IsKindOf(torchtype))
			{
				// The software renderer already bakes the torch flickering into its output, so this must be omitted here.
				float r = vid_rendermode < 4 ? 1.f : (0.8f + (7 - player->fixedlightlevel) / 70.0f);
				if (r > 1.0f) r = 1.0f;
				int rr = (int)(r * 255);
				int b = rr;
				if (gl_enhanced_nightvision) b = b * 3 / 4;
				color = PalEntry(255, rr, rr, b);
			}
			else if (in->IsKindOf(litetype))
			{
				if (gl_enhanced_nightvision)
				{
					color = PalEntry(255, 104, 255, 104);
				}
			}
		}
		if (modulateColor)
		{
			*modulateColor = color;
		}
	}

	if (player)
	{
		V_AddPlayerBlend(player, blend, 0.5, 175);
	}

	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t* player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		V_AddBlend(player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	const float br = clamp(blend[0] * 255.f, 0.f, 255.f);
	const float bg = clamp(blend[1] * 255.f, 0.f, 255.f);
	const float bb = clamp(blend[2] * 255.f, 0.f, 255.f);
	return { br, bg, bb, blend[3] };
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

void DFrameBuffer::DrawBlend(sector_t* viewsector)
{
	PalEntry modulateColor;
	auto blend = CalcBlend(viewsector, &modulateColor);
	if (modulateColor != 0xffffffff)
	{
		Dim(twod, modulateColor, 1, 0, 0, GetWidth(), GetHeight(), &LegacyRenderStyles[STYLE_Multiply]);
	}

	const PalEntry bcolor(255, uint8_t(blend.X), uint8_t(blend.Y), uint8_t(blend.Z));
	Dim(twod, bcolor, blend.W, 0, 0, GetWidth(), GetHeight());
}


//==========================================================================
//
// V_DrawFrame
//
// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
//
//==========================================================================

void DrawFrame(F2DDrawer* drawer, int left, int top, int width, int height)
{
	FTexture* p;
	const gameborder_t* border = &gameinfo.Border;
	// Sanity check for incomplete gameinfo
	if (border == NULL)
		return;
	int offset = border->offset;
	int right = left + width;
	int bottom = top + height;

	// Draw top and bottom sides.
	p = TexMan.GetTextureByName(border->t);
	drawer->AddFlatFill(left, top - p->GetDisplayHeight(), right, top, p, true);
	p = TexMan.GetTextureByName(border->b);
	drawer->AddFlatFill(left, bottom, right, bottom + p->GetDisplayHeight(), p, true);

	// Draw left and right sides.
	p = TexMan.GetTextureByName(border->l);
	drawer->AddFlatFill(left - p->GetDisplayWidth(), top, left, bottom, p, true);
	p = TexMan.GetTextureByName(border->r);
	drawer->AddFlatFill(right, top, right + p->GetDisplayWidth(), bottom, p, true);

	// Draw beveled corners.
	DrawTexture(drawer, TexMan.GetTextureByName(border->tl), left - offset, top - offset, TAG_DONE);
	DrawTexture(drawer, TexMan.GetTextureByName(border->tr), left + width, top - offset, TAG_DONE);
	DrawTexture(drawer, TexMan.GetTextureByName(border->bl), left - offset, top + height, TAG_DONE);
	DrawTexture(drawer, TexMan.GetTextureByName(border->br), left + width, top + height, TAG_DONE);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawFrame)
{
	PARAM_PROLOGUE;
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	DrawFrame(twod, x, y, w, h);
	return 0;
}
