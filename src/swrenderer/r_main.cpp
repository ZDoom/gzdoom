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
//		Rendering main loop and setup functions,
//		 utility functions (BSP, geometry, trigonometry).
//		See tables.c, too.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "templates.h"
#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_bbox.h"
#include "r_main.h"
#include "drawers/r_draw.h"
#include "plane/r_flatplane.h"
#include "scene/r_bsp.h"
#include "segments/r_drawsegment.h"
#include "segments/r_portalsegment.h"
#include "segments/r_clipsegment.h"
#include "scene/r_3dfloors.h"
#include "scene/r_portal.h"
#include "r_sky.h"
#include "drawers/r_draw_rgba.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "stats.h"
#include "i_video.h"
#include "i_system.h"
#include "a_sharedglobal.h"
#include "r_data/r_translate.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "v_palette.h"
#include "po_man.h"
#include "p_effect.h"
#include "st_start.h"
#include "v_font.h"
#include "r_data/colormaps.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "version.h"
#include "c_console.h"
#include "r_memory.h"
#include "swrenderer/things/r_playersprite.h"

CVAR (String, r_viewsize, "", CVAR_NOSET)
CVAR (Bool, r_shadercolormaps, true, CVAR_ARCHIVE)

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{

// MACROS ------------------------------------------------------------------

#if 0
#define TEST_X 32343794 
#define TEST_Y 111387517 
#define TEST_Z 2164524 
#define TEST_ANGLE 2468347904 
#endif


// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_SpanInitData ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_ShutdownRenderer();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int fuzzviewheight;


// PRIVATE DATA DECLARATIONS -----------------------------------------------

static double CurrentVisibility = 8.f;
static double MaxVisForWall;
static double MaxVisForFloor;
bool r_dontmaplines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

bool			r_swtruecolor;

double			r_BaseVisibility;
double			r_WallVisibility;
double			r_FloorVisibility;
float			r_TiltVisibility;
double			r_SpriteVisibility;
double			r_ParticleVisibility;

double			GlobVis;
fixed_t			viewingrangerecip;
double			FocalLengthX;
double			FocalLengthY;
FDynamicColormap*basecolormap;		// [RH] colormap currently drawing with
int				fixedlightlev;
FSWColormap		*fixedcolormap;
FSpecialColormap *realfixedcolormap;
double			WallTMapScale2;


bool			bRenderingToCanvas;	// [RH] True if rendering to a special canvas
double			globaluclip, globaldclip;
double			CenterX, CenterY;
double			YaspectMul;
double			BaseYaspectMul;		// yaspectmul without a forced aspect ratio
double			IYaspectMul;
double			InvZtoScale;

// just for profiling purposes
int 			linecount;
int 			loopcount;


//
// precalculated math tables
//

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 		xtoviewangle[MAXWIDTH+1];

bool			foggy;			// [RH] ignore extralight and fullbright?
int				r_actualextralight;

cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastcenteryfrac;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// R_InitTextureMapping
//
//==========================================================================

void R_InitTextureMapping ()
{
	int i;

	// Calc focallength so FieldOfView angles cover viewwidth.
	FocalLengthX = CenterX / FocalTangent;
	FocalLengthY = FocalLengthX * YaspectMul;

	// This is 1/FocalTangent before the widescreen extension of FOV.
	viewingrangerecip = FLOAT2FIXED(1. / tan(FieldOfView.Radians() / 2));


	// Now generate xtoviewangle for sky texture mapping.
	// [RH] Do not generate viewangletox, because texture mapping is no
	// longer done with trig, so it's not needed.
	const double slopestep = FocalTangent / centerx;
	double slope;

	for (i = centerx, slope = 0; i <= viewwidth; i++, slope += slopestep)
	{
		xtoviewangle[i] = angle_t((2 * M_PI - atan(slope)) * (ANGLE_180 / M_PI));
	}
	for (i = 0; i < centerx; i++)
	{
		xtoviewangle[i] = 0 - xtoviewangle[viewwidth - i - 1];
	}
}

//==========================================================================
//
// R_SetVisibility
//
// Changes how rapidly things get dark with distance
//
//==========================================================================

void R_SetVisibility(double vis)
{
	// Allow negative visibilities, just for novelty's sake
	vis = clamp(vis, -204.7, 204.7);	// (205 and larger do not work in 5:4 aspect ratio)

	CurrentVisibility = vis;

	if (FocalTangent == 0 || FocalLengthY == 0)
	{ // If r_visibility is called before the renderer is all set up, don't
	  // divide by zero. This will be called again later, and the proper
	  // values can be initialized then.
		return;
	}

	r_BaseVisibility = vis;

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = (InvZtoScale * SCREENWIDTH*AspectBaseHeight(WidescreenRatio) /
		(viewwidth*SCREENHEIGHT*3)) * (r_WallVisibility * FocalTangent);

	// Prevent overflow on floors/ceilings. Note that the calculation of
	// MaxVisForFloor means that planes less than two units from the player's
	// view could still overflow, but there is no way to totally eliminate
	// that while still using fixed point math.
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForFloor)
		r_FloorVisibility = -MaxVisForFloor;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForFloor)
		r_FloorVisibility = MaxVisForFloor;
	else
		r_FloorVisibility = r_BaseVisibility;

	r_FloorVisibility = 160.0 * r_FloorVisibility / FocalLengthY;

	r_TiltVisibility = float(vis * FocalTangent * (16.f * 320.f) / viewwidth);
	r_SpriteVisibility = r_WallVisibility;
}

//==========================================================================
//
// R_GetVisibility
//
//==========================================================================

double R_GetVisibility()
{
	return CurrentVisibility;
}

//==========================================================================
//
// CCMD r_visibility
//
// Controls how quickly light ramps across a 1/z range. Set this, and it
// sets all the r_*Visibility variables (except r_SkyVisibilily, which is
// currently unused).
//
//==========================================================================

CCMD (r_visibility)
{
	if (argv.argc() < 2)
	{
		Printf ("Visibility is %g\n", R_GetVisibility());
	}
	else if (!netgame)
	{
		R_SetVisibility(atof(argv[1]));
	}
	else
	{
		Printf ("Visibility cannot be changed in net games.\n");
	}
}

//==========================================================================
//
// R_SetWindow
//
//==========================================================================

void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio)
{
	int virtheight, virtwidth, virtwidth2, virtheight2;

	if (!bRenderingToCanvas)
	{ // Set r_viewsize cvar to reflect the current view size
		UCVarValue value;
		char temp[16];

		mysnprintf (temp, countof(temp), "%d x %d", viewwidth, viewheight);
		value.String = temp;
		r_viewsize.ForceSet (value, CVAR_String);
	}

	fuzzviewheight = viewheight - 2;	// Maximum row the fuzzer can draw to

	lastcenteryfrac = 1<<30;
	CenterX = centerx;
	CenterY = centery;

	virtwidth = virtwidth2 = fullWidth;
	virtheight = virtheight2 = fullHeight;

	if (AspectTallerThanWide(trueratio))
	{
		virtheight2 = virtheight2 * AspectMultiplier(trueratio) / 48;
	}
	else
	{
		virtwidth2 = virtwidth2 * AspectMultiplier(trueratio) / 48;
	}

	if (AspectTallerThanWide(WidescreenRatio))
	{
		virtheight = virtheight * AspectMultiplier(WidescreenRatio) / 48;
	}
	else
	{
		virtwidth = virtwidth * AspectMultiplier(WidescreenRatio) / 48;
	}

	BaseYaspectMul = 320.0 * virtheight2 / (r_Yaspect * virtwidth2);
	YaspectMul = 320.0 * virtheight / (r_Yaspect * virtwidth);
	IYaspectMul = (double)virtwidth * r_Yaspect / 320.0 / virtheight;
	InvZtoScale = YaspectMul * CenterX;

	WallTMapScale2 = IYaspectMul / CenterX;

	// psprite scales
	RenderPlayerSprite::SetupSpriteScale();

	// thing clipping
	fillshort (screenheightarray, viewwidth, (short)viewheight);

	R_InitTextureMapping ();

	MaxVisForWall = (InvZtoScale * (SCREENWIDTH*r_Yaspect) /
		(viewwidth*SCREENHEIGHT * FocalTangent));
	MaxVisForWall = 32767.0 / MaxVisForWall;
	MaxVisForFloor = 32767.0 / (viewheight >> 2) * FocalLengthY / 160;

	// Reset r_*Visibility vars
	R_SetVisibility(R_GetVisibility());
}

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_InitRenderer()
{
	atterm(R_ShutdownRenderer);
	// viewwidth / viewheight are set by the defaults
	fillshort (zeroarray, MAXWIDTH, 0);

	R_InitShadeMaps();
	R_InitColumnDrawers ();
}

//==========================================================================
//
// R_ShutdownRenderer
//
//==========================================================================

static void R_ShutdownRenderer()
{
	RenderTranslucent::Deinit();
	R_DeinitPlanes();
	Clip3DFloors::Instance()->Cleanup();
	R_DeinitOpenings();
	R_FreeDrawSegs();
}

//==========================================================================
//
// R_SetupColormap
//
// Sets up special fixed colormaps
//
//==========================================================================

void R_SetupColormap(player_t *player)
{
	realfixedcolormap = NULL;
	fixedcolormap = NULL;
	fixedlightlev = -1;

	if (player != NULL && camera == player->mo)
	{
		if (player->fixedcolormap >= 0 && player->fixedcolormap < (int)SpecialColormaps.Size())
		{
			realfixedcolormap = &SpecialColormaps[player->fixedcolormap];
			if (RenderTarget == screen && (r_swtruecolor || ((DFrameBuffer *)screen->Accel2D && r_shadercolormaps)))
			{
				// Render everything fullbright. The copy to video memory will
				// apply the special colormap, so it won't be restricted to the
				// palette.
				fixedcolormap = &realcolormaps;
			}
			else
			{
				fixedcolormap = &SpecialColormaps[player->fixedcolormap];
			}
		}
		else if (player->fixedlightlevel >= 0 && player->fixedlightlevel < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedlightlevel * 256;
			// [SP] Emulate GZDoom's light-amp goggles.
			if (r_fullbrightignoresectorcolor && fixedlightlev >= 0)
			{
				fixedcolormap = &FullNormalLight;
			}
		}
	}
	// [RH] Inverse light for shooting the Sigil
	if (fixedcolormap == NULL && extralight == INT_MIN)
	{
		fixedcolormap = &SpecialColormaps[INVERSECOLORMAP];
		extralight = 0;
	}
}

//==========================================================================
//
// R_SetupFreelook
//
// [RH] freelook stuff
//
//==========================================================================

void R_SetupFreelook()
{
	double dy;
		
	if (camera != NULL)
	{
		dy = FocalLengthY * (-ViewPitch).Tan();
	}
	else
	{
		dy = 0;
	}

	CenterY = (viewheight / 2.0) + dy;
	centery = xs_ToInt(CenterY);
	globaluclip = -CenterY / InvZtoScale;
	globaldclip = (viewheight - CenterY) / InvZtoScale;

	R_SetupPlaneSlope();
}

//==========================================================================
//
// R_SetupBuffer
//
// Precalculate all row offsets and fuzz table.
//
//==========================================================================

void R_SetupBuffer ()
{
	using namespace drawerargs;

	static BYTE *lastbuff = NULL;

	int pitch = RenderTarget->GetPitch();
	int pixelsize = r_swtruecolor ? 4 : 1;
	BYTE *lineptr = RenderTarget->GetBuffer() + (viewwindowy*pitch + viewwindowx) * pixelsize;

	if (dc_pitch != pitch || lineptr != lastbuff)
	{
		if (dc_pitch != pitch)
		{
			dc_pitch = pitch;
			R_InitFuzzTable (pitch);
		}
		dc_destorg = lineptr;
		dc_destheight = RenderTarget->GetHeight() - viewwindowy;
		for (int i = 0; i < RenderTarget->GetHeight(); i++)
		{
			ylookup[i] = i * pitch;
		}
	}
}

//==========================================================================
//
// R_RenderActorView
//
//==========================================================================

void R_RenderActorView (AActor *actor, bool dontmaplines)
{
	WallCycles.Reset();
	PlaneCycles.Reset();
	MaskedCycles.Reset();
	WallScanCycles.Reset();

	Clip3DFloors *clip3d = Clip3DFloors::Instance();
	clip3d->fakeActive = false; // kg3D - reset fake floor indicator
	clip3d->ResetClip(); // reset clips (floor/ceiling)

	R_SetupBuffer ();
	R_SetupFrame (actor);

	// Clear buffers.
	R_ClearClipSegs (0, viewwidth);
	R_ClearDrawSegs ();
	R_ClearPlanes (true);
	RenderTranslucent::Clear();

	// opening / clipping determination
	RenderBSP::Instance()->ClearClip();
	R_FreeOpenings();

	NetUpdate ();

	colfunc = basecolfunc;
	spanfunc = &SWPixelFormatDrawers::DrawSpan;

	RenderPortal::Instance()->SetMainPortal();

	r_dontmaplines = dontmaplines;
	
	// [RH] Hack to make windows into underwater areas possible
	RenderBSP::Instance()->ResetFakingUnderwater();

	// [RH] Setup particles for this frame
	P_FindParticleSubsectors ();

	WallCycles.Clock();
	ActorRenderFlags savedflags = camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!r_showviewer)
	{
		camera->renderflags |= RF_INVISIBLE;
	}
	// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
	PO_LinkToSubsectors();
	RenderBSP::Instance()->RenderScene();
	Clip3DFloors::Instance()->ResetClip(); // reset clips (floor/ceiling)
	camera->renderflags = savedflags;
	WallCycles.Unclock();

	NetUpdate ();

	if (viewactive)
	{
		PlaneCycles.Clock();
		R_DrawPlanes();
		RenderPortal::Instance()->RenderPlanePortals();
		PlaneCycles.Unclock();

		RenderPortal::Instance()->RenderLinePortals();

		NetUpdate ();
		
		MaskedCycles.Clock();
		RenderTranslucent::Render();
		MaskedCycles.Unclock();

		NetUpdate ();
	}
	WallPortals.Clear ();
	interpolator.RestoreInterpolations ();
	R_SetupBuffer ();

	// If we don't want shadered colormaps, NULL it now so that the
	// copy to the screen does not use a special colormap shader.
	if (!r_shadercolormaps && !r_swtruecolor)
	{
		realfixedcolormap = NULL;
	}
}

//==========================================================================
//
// R_RenderViewToCanvas
//
// Pre: Canvas is already locked.
//
//==========================================================================

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas,
	int x, int y, int width, int height, bool dontmaplines)
{
	const bool savedviewactive = viewactive;
	const bool savedoutputformat = r_swtruecolor;

	if (r_swtruecolor != canvas->IsBgra())
	{
		r_swtruecolor = canvas->IsBgra();
		R_InitColumnDrawers();
	}
	
	R_BeginDrawerCommands();

	viewwidth = width;
	RenderTarget = canvas;
	bRenderingToCanvas = true;

	R_SetWindow (12, width, height, height, true);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;

	R_RenderActorView (actor, dontmaplines);

	R_EndDrawerCommands();

	RenderTarget = screen;
	bRenderingToCanvas = false;
	R_ExecuteSetViewSize ();
	screen->Lock (true);
	R_SetupBuffer ();
	screen->Unlock ();

	viewactive = savedviewactive;
	r_swtruecolor = savedoutputformat;

	if (r_swtruecolor != canvas->IsBgra())
	{
		R_InitColumnDrawers();
	}
}

//==========================================================================
//
// R_MultiresInit
//
// Called from V_SetResolution()
//
//==========================================================================

void R_MultiresInit ()
{
	R_PlaneInitData ();
}


//==========================================================================
//
// STAT fps
//
// Displays statistics about rendering times
//
//==========================================================================

ADD_STAT (fps)
{
	FString out;
	out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
		FrameCycles.TimeMS(), WallCycles.TimeMS(), PlaneCycles.TimeMS(), MaskedCycles.TimeMS());
	return out;
}


static double f_acc, w_acc,p_acc,m_acc;
static int acc_c;

ADD_STAT (fps_accumulated)
{
	f_acc += FrameCycles.TimeMS();
	w_acc += WallCycles.TimeMS();
	p_acc += PlaneCycles.TimeMS();
	m_acc += MaskedCycles.TimeMS();
	acc_c++;
	FString out;
	out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  %d counts",
		f_acc/acc_c, w_acc/acc_c, p_acc/acc_c, m_acc/acc_c, acc_c);
	Printf(PRINT_LOG, "%s\n", out.GetChars());
	return out;
}

//==========================================================================
//
// STAT wallcycles
//
// Displays the minimum number of cycles spent drawing walls
//
//==========================================================================

static double bestwallcycles = HUGE_VAL;

ADD_STAT (wallcycles)
{
	FString out;
	double cycles = WallCycles.Time();
	if (cycles && cycles < bestwallcycles)
		bestwallcycles = cycles;
	out.Format ("%g", bestwallcycles);
	return out;
}

//==========================================================================
//
// CCMD clearwallcycles
//
// Resets the count of minimum wall drawing cycles
//
//==========================================================================

CCMD (clearwallcycles)
{
	bestwallcycles = HUGE_VAL;
}

#if 0
// The replacement code for Build's wallscan doesn't have any timing calls so this does not work anymore.
static double bestscancycles = HUGE_VAL;

ADD_STAT (scancycles)
{
	FString out;
	double scancycles = WallScanCycles.Time();
	if (scancycles && scancycles < bestscancycles)
		bestscancycles = scancycles;
	out.Format ("%g", bestscancycles);
	return out;
}

CCMD (clearscancycles)
{
	bestscancycles = HUGE_VAL;
}
#endif

}
