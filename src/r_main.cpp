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
#include "p_local.h"
#include "r_local.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "v_video.h"
#include "stats.h"
#include "i_video.h"
#include "i_system.h"
#include "a_sharedglobal.h"
#include "r_translate.h"
#include "p_3dmidtex.h"
#include "r_interpolate.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "po_man.h"

// MACROS ------------------------------------------------------------------

#if 0
#define TEST_X 32343794 
#define TEST_Y 111387517 
#define TEST_Z 2164524 
#define TEST_ANGLE 2468347904 
#endif

// TYPES -------------------------------------------------------------------

struct InterpolationViewer
{
	AActor *ViewActor;
	int otic;
	fixed_t oviewx, oviewy, oviewz;
	fixed_t nviewx, nviewy, nviewz;
	int oviewpitch, nviewpitch;
	angle_t oviewangle, nviewangle;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_SpanInitData ();
void RP_RenderBSPNode (void *node);
bool RP_SetupFrame (bool backside);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_Shutdown();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool DrawFSHUD;		// [RH] Defined in d_main.cpp
extern short *openings;
extern bool r_fakingunderwater;
extern "C" int fuzzviewheight;
EXTERN_CVAR (Bool, r_particles)
EXTERN_CVAR (Bool, cl_capfps)

// PRIVATE DATA DECLARATIONS -----------------------------------------------

static float CurrentVisibility = 8.f;
static fixed_t MaxVisForWall;
static fixed_t MaxVisForFloor;
static FRandom pr_torchflicker ("TorchFlicker");
static FRandom pr_hom;
static TArray<InterpolationViewer> PastViewers;
static bool polyclipped;
static bool r_showviewer;
bool r_dontmaplines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, r_viewsize, "", CVAR_NOSET)
CVAR (Int, r_polymost, 0, 0)
CVAR (Bool, r_deathcamera, false, CVAR_ARCHIVE)
CVAR (Int, r_clearbuffer, 0, 0)

fixed_t			r_BaseVisibility;
fixed_t			r_WallVisibility;
fixed_t			r_FloorVisibility;
float			r_TiltVisibility;
fixed_t			r_SpriteVisibility;
fixed_t			r_ParticleVisibility;
fixed_t			r_SkyVisibility;

fixed_t			r_TicFrac;			// [RH] Fractional tic to render
DWORD			r_FrameTime;		// [RH] Time this frame started drawing (in ms)
bool			r_NoInterpolate;

angle_t			LocalViewAngle;
int				LocalViewPitch;
bool			LocalKeyboardTurner;

float			LastFOV;
int				WidescreenRatio;

fixed_t			GlobVis;
fixed_t			viewingrangerecip;
fixed_t			FocalTangent;
fixed_t			FocalLengthX;
fixed_t			FocalLengthY;
float			FocalLengthXfloat;
int 			viewangleoffset;
int 			validcount = 1; 	// increment every time a check is made
FDynamicColormap*basecolormap;		// [RH] colormap currently drawing with
int				fixedlightlev;
lighttable_t	*fixedcolormap;
FSpecialColormap *realfixedcolormap;
float			WallTMapScale;
float			WallTMapScale2;

extern "C" {
int 			centerx;
int				centery;
int				centerxwide;

}

DCanvas			*RenderTarget;		// [RH] canvas to render to
bool			bRenderingToCanvas;	// [RH] True if rendering to a special canvas
fixed_t			globaluclip, globaldclip;
fixed_t 		centerxfrac;
fixed_t 		centeryfrac;
fixed_t			yaspectmul;
fixed_t			baseyaspectmul;		// yaspectmul without a forced aspect ratio
float			iyaspectmulfloat;
fixed_t			InvZtoScale;

// just for profiling purposes
int 			framecount; 	
int 			linecount;
int 			loopcount;

fixed_t 		viewx;
fixed_t 		viewy;
fixed_t 		viewz;
int				viewpitch;
int				otic;

angle_t 		viewangle;
sector_t		*viewsector;

fixed_t 		viewcos, viewtancos;
fixed_t 		viewsin, viewtansin;

AActor			*camera;	// [RH] camera to draw from. doesn't have to be a player

int				r_Yaspect = 200;	// Why did I make this a variable? It's never set anywhere.

//
// precalculated math tables
//
int FieldOfView = 2048;		// Fineangles in the SCREENWIDTH wide window

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 		xtoviewangle[MAXWIDTH+1];

int 			extralight;		// bumped light from gun blasts
bool			foggy;			// [RH] ignore extralight and fullbright?
int				r_actualextralight;

bool			setsizeneeded;
int				setblocks;

fixed_t			freelookviewheight;

unsigned int	R_OldBlend = ~0;

void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);

void (*hcolfunc_pre) (void);
void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
void (STACK_ARGS *hcolfunc_post4) (int sx, int yl, int yh);

cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

FCanvasTextureInfo *FCanvasTextureInfo::List;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastcenteryfrac;
static bool NoInterpolateView;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SlopeDiv
//
// Utility function, called by R_PointToAngle.
//
//==========================================================================

angle_t SlopeDiv (unsigned int num, unsigned den)
{
	unsigned int ans;

	if (den < 512)
		return (ANG45 - 1); //tantoangle[SLOPERANGE]

	ans = (num << 3) / (den >> 8);

	return ans <= SLOPERANGE ? tantoangle[ans] : (ANG45 - 1);
}


//==========================================================================
//
// R_PointToAngle
//
// To get a global angle from cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<=x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle[] table.
//
//==========================================================================

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x, fixed_t y)
{
	x -= x1;
	y -= y1;

	if ((x | y) == 0)
	{
		return 0;
	}

	// We need to be aware of overflows here. If the values get larger than INT_MAX/4
	// this code won't work anymore.

	if (x < INT_MAX/4 && x > -INT_MAX/4 && y < INT_MAX/4 && y > -INT_MAX/4)
	{
		if (x >= 0)
		{
			if (y >= 0)
			{
				if (x > y)
				{ // octant 0
					return SlopeDiv(y, x);
				}
				else
				{ // octant 1
					return ANG90 - 1 - SlopeDiv(x, y);
				}
			}
			else // y < 0
			{
				y = -y;
				if (x > y)
				{ // octant 8
					return 0 - SlopeDiv(y, x);
				}
				else
				{ // octant 7
					return ANG270 + SlopeDiv(x, y);
				}
			}
		}
		else // x < 0
		{
			x = -x;
			if (y >= 0)
			{
				if (x > y)
				{ // octant 3
					return ANG180 - 1 - SlopeDiv(y, x);
				}
				else
				{ // octant 2
					return ANG90 + SlopeDiv(x, y);
				}
			}
			else // y < 0
			{
				y = -y;
				if (x > y)
				{ // octant 4
					return ANG180 + SlopeDiv(y, x);
				}
				else
				{ // octant 5
					return ANG270 - 1 - SlopeDiv(x, y);
				}
			}
		}
	}
	else
	{
		// we have to use the slower but more precise floating point atan2 function here.
		return xs_RoundToUInt(atan2(double(y), double(x)) * (ANGLE_180/M_PI));
	}
}

//==========================================================================
//
// R_InitPointToAngle
//
//==========================================================================

void R_InitPointToAngle (void)
{
	double f;
	int i;
//
// slope (tangent) to angle lookup
//
	for (i = 0; i <= SLOPERANGE; i++)
	{
		f = atan2 ((double)i, (double)SLOPERANGE) / (6.28318530718 /* 2*pi */);
		tantoangle[i] = (angle_t)(0xffffffff*f);
	}
}

//==========================================================================
//
// R_PointToDist2
//
// Returns the distance from (0,0) to some other point. In a
// floating point environment, we'd probably be better off using the
// Pythagorean Theorem to determine the result.
//
// killough 5/2/98: simplified
// [RH] Simplified further [sin (t + 90 deg) == cos (t)]
// Not used. Should it go away?
//
//==========================================================================

fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy)
{
	dx = abs (dx);
	dy = abs (dy);

	if ((dx | dy) == 0)
	{
		return 0;
	}

	if (dy > dx)
	{
		swapvalues (dx, dy);
	}

	return FixedDiv (dx, finecosine[tantoangle[FixedDiv (dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}

//==========================================================================
//
// R_InitTables
//
//==========================================================================

void R_InitTables (void)
{
	int i;
	const double pimul = PI*2/FINEANGLES;

	// viewangle tangent table
	finetangent[0] = (fixed_t)(FRACUNIT*tan ((0.5-FINEANGLES/4)*pimul)+0.5);
	for (i = 1; i < FINEANGLES/2; i++)
	{
		finetangent[i] = (fixed_t)(FRACUNIT*tan ((i-FINEANGLES/4)*pimul)+0.5);
	}
	
	// finesine table
	for (i = 0; i < FINEANGLES/4; i++)
	{
		finesine[i] = (fixed_t)(FRACUNIT * sin (i*pimul));
	}
	for (i = 0; i < FINEANGLES/4; i++)
	{
		finesine[i+FINEANGLES/4] = finesine[FINEANGLES/4-1-i];
	}
	for (i = 0; i < FINEANGLES/2; i++)
	{
		finesine[i+FINEANGLES/2] = -finesine[i];
	}
	finesine[FINEANGLES/4] = FRACUNIT;
	finesine[FINEANGLES*3/4] = -FRACUNIT;
	memcpy (&finesine[FINEANGLES], &finesine[0], sizeof(angle_t)*FINEANGLES/4);
}


//==========================================================================
//
// R_InitTextureMapping
//
//==========================================================================

void R_InitTextureMapping ()
{
	int i;
	fixed_t slope;
	int fov = FieldOfView;

	// For widescreen displays, increase the FOV so that the middle part of the
	// screen that would be visible on a 4:3 display has the requested FOV.
	if (centerxwide != centerx)
	{ // centerxwide is what centerx would be if the display was not widescreen
		fov = int(atan(double(centerx)*tan(double(fov)*M_PI/(FINEANGLES))/double(centerxwide))*(FINEANGLES)/M_PI);
		if (fov > 170*FINEANGLES/360)
			fov = 170*FINEANGLES/360;
	}
	/*
	default: break;
	case 1: fov = MIN (fov * 512/433, 170 * FINEANGLES / 360);	break;	// 16:9
	case 2: fov = MIN (fov * 512/459, 170 * FINEANGLES / 360);	break;	// 16:10
	}
	*/

	const int hitan = finetangent[FINEANGLES/4+fov/2];

	// Calc focallength so FieldOfView fineangles covers viewwidth.
	FocalTangent = hitan;
	FocalLengthX = FixedDiv (centerxfrac, hitan);
	FocalLengthY = Scale (centerxfrac, yaspectmul, hitan);
	FocalLengthXfloat = (float)FocalLengthX / 65536.f;

	// This is 1/FocalTangent before the widescreen extension of FOV.
	viewingrangerecip = DivScale32(1, finetangent[FINEANGLES/4+(FieldOfView/2)]);

	// Now generate xtoviewangle for sky texture mapping.
	// [RH] Do not generate viewangletox, because texture mapping is no
	// longer done with trig, so it's not needed.
	const int t = MIN<int> ((FocalLengthX >> FRACBITS) + centerx, viewwidth);
	const fixed_t slopestep = hitan / centerx;
	const fixed_t dfocus = FocalLengthX >> DBITS;

	for (i = centerx, slope = 0; i <= t; i++, slope += slopestep)
	{
		xtoviewangle[i] = (angle_t)-(signed)tantoangle[slope >> DBITS];
	}
	for (; i <= viewwidth; i++)
	{
		xtoviewangle[i] = ANG270+tantoangle[dfocus / (i - centerx)];
	}
	for (i = 0; i < centerx; i++)
	{
		xtoviewangle[i] = (angle_t)(-(signed)xtoviewangle[viewwidth-i]);
	}
}

//==========================================================================
//
// R_SetFOV
//
// Changes the field of view in degrees
//
//==========================================================================

void R_SetFOV (float fov)
{
	if (fov < 5.f)
		fov = 5.f;
	else if (fov > 170.f)
		fov = 170.f;
	if (fov != LastFOV)
	{
		LastFOV = fov;
		FieldOfView = (int)(fov * (float)FINEANGLES / 360.f);
		setsizeneeded = true;
	}
}

//==========================================================================
//
// R_GetFOV
//
// Returns the current field of view in degrees
//
//==========================================================================

float R_GetFOV ()
{
	return LastFOV;
}

//==========================================================================
//
// R_SetVisibility
//
// Changes how rapidly things get dark with distance
//
//==========================================================================

void R_SetVisibility (float vis)
{
	// Allow negative visibilities, just for novelty's sake
	//vis = clamp (vis, -204.7f, 204.7f);

	CurrentVisibility = vis;

	if (FocalTangent == 0)
	{ // If r_visibility is called before the renderer is all set up, don't
	  // divide by zero. This will be called again later, and the proper
	  // values can be initialized then.
		return;
	}

	r_BaseVisibility = xs_RoundToInt(vis * 65536.f);

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = FixedMul (Scale (InvZtoScale, SCREENWIDTH*BaseRatioSizes[WidescreenRatio][1],
		viewwidth*SCREENHEIGHT*3), FixedMul (r_WallVisibility, FocalTangent));

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

	r_FloorVisibility = Scale (160*FRACUNIT, r_FloorVisibility, FocalLengthY);

	r_TiltVisibility = vis * (float)FocalTangent * (16.f * 320.f) / (float)viewwidth;
	r_SpriteVisibility = r_WallVisibility;
}

//==========================================================================
//
// R_GetVisibility
//
//==========================================================================

float R_GetVisibility ()
{
	return CurrentVisibility;
}

//==========================================================================
//
// R_SetViewSize
//
// Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
//
//==========================================================================

void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}

//==========================================================================
//
// R_SetWindow
//
//==========================================================================

void R_SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight)
{
	int virtheight, virtwidth, trueratio, virtwidth2, virtheight2;

	if (windowSize >= 11)
	{
		viewwidth = fullWidth;
		freelookviewheight = viewheight = fullHeight;
	}
	else if (windowSize == 10)
	{
		viewwidth = fullWidth;
		viewheight = stHeight;
		freelookviewheight = fullHeight;
	}
	else
	{
		viewwidth = ((setblocks*fullWidth)/10) & (~15);
		viewheight = ((setblocks*stHeight)/10)&~7;
		freelookviewheight = ((setblocks*fullHeight)/10)&~7;
	}

	// If the screen is approximately 16:9 or 16:10, consider it widescreen.
	WidescreenRatio = CheckRatio (fullWidth, fullHeight, &trueratio);

	DrawFSHUD = (windowSize == 11);
	
	fuzzviewheight = viewheight - 2;	// Maximum row the fuzzer can draw to
	halfviewwidth = (viewwidth >> 1) - 1;

	if (!bRenderingToCanvas)
	{ // Set r_viewsize cvar to reflect the current view size
		UCVarValue value;
		char temp[16];

		mysnprintf (temp, countof(temp), "%d x %d", viewwidth, viewheight);
		value.String = temp;
		r_viewsize.ForceSet (value, CVAR_String);
	}

	lastcenteryfrac = 1<<30;
	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	virtwidth = virtwidth2 = fullWidth;
	virtheight = virtheight2 = fullHeight;

	if (trueratio & 4)
	{
		virtheight2 = virtheight2 * BaseRatioSizes[trueratio][3] / 48;
	}
	else
	{
		virtwidth2 = virtwidth2 * BaseRatioSizes[trueratio][3] / 48;
	}

	if (WidescreenRatio & 4)
	{
		virtheight = virtheight * BaseRatioSizes[WidescreenRatio][3] / 48;
		centerxwide = centerx;
	}
	else
	{
		virtwidth = virtwidth * BaseRatioSizes[WidescreenRatio][3] / 48;
		centerxwide = centerx * BaseRatioSizes[WidescreenRatio][3] / 48;
	}

	baseyaspectmul = Scale(320 << FRACBITS, virtheight2, r_Yaspect * virtwidth2);
	yaspectmul = Scale ((320<<FRACBITS), virtheight, r_Yaspect * virtwidth);
	iyaspectmulfloat = (float)virtwidth * r_Yaspect / 320.f / (float)virtheight;
	InvZtoScale = yaspectmul * centerx;

	WallTMapScale = (float)centerx * 32.f;
	WallTMapScale2 = iyaspectmulfloat * 2.f / (float)centerx;

	// psprite scales
	pspritexscale = (centerxwide << FRACBITS) / 160;
	pspriteyscale = FixedMul (pspritexscale, yaspectmul);
	pspritexiscale = FixedDiv (FRACUNIT, pspritexscale);

	// thing clipping
	clearbufshort (screenheightarray, viewwidth, (short)viewheight);

	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	R_InitTextureMapping ();

	MaxVisForWall = FixedMul (Scale (InvZtoScale, SCREENWIDTH*r_Yaspect,
		viewwidth*SCREENHEIGHT), FocalTangent);
	MaxVisForWall = FixedDiv (0x7fff0000, MaxVisForWall);
	MaxVisForFloor = Scale (FixedDiv (0x7fff0000, viewheight<<(FRACBITS-2)), FocalLengthY, 160*FRACUNIT);

	// Reset r_*Visibility vars
	R_SetVisibility (R_GetVisibility ());
}

//==========================================================================
//
// R_ExecuteSetViewSize
//
//==========================================================================

void R_ExecuteSetViewSize ()
{
	setsizeneeded = false;
	BorderNeedRefresh = screen->GetPageCount ();

	R_SetWindow (setblocks, SCREENWIDTH, SCREENHEIGHT, ST_Y);

	// Handle resize, e.g. smaller view windows with border and/or status bar.
	viewwindowx = (screen->GetWidth() - viewwidth) >> 1;

	// Same with base row offset.
	viewwindowy = (viewwidth == screen->GetWidth()) ? 0 : (ST_Y - viewheight) >> 1;
}

//==========================================================================
//
// CVAR screenblocks
//
// Selects the size of the visible window
//
//==========================================================================

CUSTOM_CVAR (Int, screenblocks, 10, CVAR_ARCHIVE)
{
	if (self > 12)
		self = 12;
	else if (self < 3)
		self = 3;
	else
		R_SetViewSize (self);
}

//==========================================================================
//
// CVAR r_columnmethod
//
// Selects which version of the seg renderers to use.
//
//==========================================================================

CUSTOM_CVAR (Int, r_columnmethod, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self != 0 && self != 1)
	{
		self = 1;
	}
	else
	{ // Trigger the change
		setsizeneeded = true;
	}
}

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_Init ()
{
	atterm (R_Shutdown);

	R_InitData ();
	R_InitPointToAngle ();
	R_InitTables ();
	// viewwidth / viewheight are set by the defaults

	R_SetViewSize (screenblocks);
	R_InitPlanes ();
	R_InitTranslationTables ();
	R_InitParticles ();	// [RH] Setup particle engine
	R_InitColumnDrawers ();

	colfunc = basecolfunc = R_DrawColumn;
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	spanfunc = R_DrawSpan;

	// [RH] Horizontal column drawers
	hcolfunc_pre = R_DrawColumnHoriz;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post4 = rt_map4cols;

	framecount = 0;
}

//==========================================================================
//
// R_Shutdown
//
//==========================================================================

static void R_Shutdown ()
{
	R_DeinitParticles();
	R_DeinitTranslationTables();
	R_DeinitPlanes();
	R_DeinitData();
}

//==========================================================================
//
// R_PointInSubsector
//
//==========================================================================

subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t *node;
	int side;

	// single subsector is a special case
	if (numnodes == 0)
		return subsectors;
				
	node = nodes + numnodes - 1;

	do
	{
		side = R_PointOnSide (x, y, node);
		node = (node_t *)node->children[side];
	}
	while (!((size_t)node & 1));
		
	return (subsector_t *)((BYTE *)node - 1);
}

//==========================================================================
//
// R_InterpolateView
//
//==========================================================================

//CVAR (Int, tf, 0, 0)
EXTERN_CVAR (Bool, cl_noprediction)

void R_InterpolateView (player_t *player, fixed_t frac, InterpolationViewer *iview)
{
//	frac = tf;
	if (NoInterpolateView)
	{
		NoInterpolateView = false;
		iview->oviewx = iview->nviewx;
		iview->oviewy = iview->nviewy;
		iview->oviewz = iview->nviewz;
		iview->oviewpitch = iview->nviewpitch;
		iview->oviewangle = iview->nviewangle;
	}
	viewx = iview->oviewx + FixedMul (frac, iview->nviewx - iview->oviewx);
	viewy = iview->oviewy + FixedMul (frac, iview->nviewy - iview->oviewy);
	viewz = iview->oviewz + FixedMul (frac, iview->nviewz - iview->oviewz);
	if (player != NULL &&
		player - players == consoleplayer &&
		camera == player->mo &&
		!demoplayback &&
		iview->nviewx == camera->x &&
		iview->nviewy == camera->y && 
		!(player->cheats & (CF_TOTALLYFROZEN|CF_FROZEN)) &&
		player->playerstate == PST_LIVE &&
		player->mo->reactiontime == 0 &&
		!NoInterpolateView &&
		!paused &&
		(!netgame || !cl_noprediction) &&
		!LocalKeyboardTurner)
	{
		viewangle = iview->nviewangle + (LocalViewAngle & 0xFFFF0000);

		fixed_t delta = -(signed)(LocalViewPitch & 0xFFFF0000);

		viewpitch = iview->nviewpitch;
		if (delta > 0)
		{
			// Avoid overflowing viewpitch (can happen when a netgame is stalled)
			if (viewpitch + delta <= viewpitch)
			{
				viewpitch = screen->GetMaxViewPitch(true);
			}
			else
			{
				viewpitch = MIN(viewpitch + delta, screen->GetMaxViewPitch(true));
			}
		}
		else if (delta < 0)
		{
			// Avoid overflowing viewpitch (can happen when a netgame is stalled)
			if (viewpitch + delta >= viewpitch)
			{
				viewpitch = screen->GetMaxViewPitch(false);
			}
			else
			{
				viewpitch = MAX(viewpitch + delta, screen->GetMaxViewPitch(false));
			}
		}
	}
	else
	{
		viewpitch = iview->oviewpitch + FixedMul (frac, iview->nviewpitch - iview->oviewpitch);
		viewangle = iview->oviewangle + FixedMul (frac, iview->nviewangle - iview->oviewangle);
	}
	
	// Due to interpolation this is not necessarily the same as the sector the camera is in.
	viewsector = R_PointInSubsector(viewx, viewy)->sector;
}

//==========================================================================
//
// R_ResetViewInterpolation
//
//==========================================================================

void R_ResetViewInterpolation ()
{
	NoInterpolateView = true;
}

//==========================================================================
//
// R_SetViewAngle
//
//==========================================================================

void R_SetViewAngle ()
{
	angle_t ang = viewangle >> ANGLETOFINESHIFT;

	viewsin = finesine[ang];
	viewcos = finecosine[ang];

	viewtansin = FixedMul (FocalTangent, viewsin);
	viewtancos = FixedMul (FocalTangent, viewcos);
}

//==========================================================================
//
// FindPastViewer
//
//==========================================================================

static InterpolationViewer *FindPastViewer (AActor *actor)
{
	for (unsigned int i = 0; i < PastViewers.Size(); ++i)
	{
		if (PastViewers[i].ViewActor == actor)
		{
			return &PastViewers[i];
		}
	}

	// Not found, so make a new one
	InterpolationViewer iview = { 0 };
	iview.ViewActor = actor;
	iview.otic = -1;
	return &PastViewers[PastViewers.Push (iview)];
}

//==========================================================================
//
// R_FreePastViewers
//
//==========================================================================

void R_FreePastViewers ()
{
	PastViewers.Clear ();
}

//==========================================================================
//
// R_ClearPastViewer
//
// If the actor changed in a non-interpolatable way, remove it.
//
//==========================================================================

void R_ClearPastViewer (AActor *actor)
{
	for (unsigned int i = 0; i < PastViewers.Size(); ++i)
	{
		if (PastViewers[i].ViewActor == actor)
		{
			// Found it, so remove it.
			if (i == PastViewers.Size())
			{
				PastViewers.Delete (i);
			}
			else
			{
				PastViewers.Pop (PastViewers[i]);
			}
		}
	}
}

//==========================================================================
//
// R_CopyStackedViewParameters
//
//==========================================================================

void R_CopyStackedViewParameters()
{
	stacked_viewx = viewx;
	stacked_viewy = viewy;
	stacked_viewz = viewz;
	stacked_angle = viewangle;
	stacked_extralight = extralight;
	stacked_visibility = R_GetVisibility();
}

//==========================================================================
//
// R_SetupFrame
//
//==========================================================================

void R_SetupFrame (AActor *actor)
{
	if (actor == NULL)
	{
		I_Error ("Tried to render from a NULL actor.");
	}

	player_t *player = actor->player;
	unsigned int newblend;
	InterpolationViewer *iview;

	if (player != NULL && player->mo == actor)
	{	// [RH] Use camera instead of viewplayer
		camera = player->camera;
		if (camera == NULL)
		{
			camera = player->camera = player->mo;
		}
		if (camera == actor)
		{
			P_PredictPlayer (player);
		}
	}
	else
	{
		camera = actor;
	}

	if (camera == NULL)
	{
		I_Error ("You lost your body. Bad dehacked work is likely to blame.");
	}

	iview = FindPastViewer (camera);

	int nowtic = I_GetTime (false);
	if (iview->otic != -1 && nowtic > iview->otic)
	{
		iview->otic = nowtic;
		iview->oviewx = iview->nviewx;
		iview->oviewy = iview->nviewy;
		iview->oviewz = iview->nviewz;
		iview->oviewpitch = iview->nviewpitch;
		iview->oviewangle = iview->nviewangle;
	}

	if (player != NULL && gamestate != GS_TITLELEVEL &&
		((player->cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)) &&
		(camera->RenderStyle.BlendOp != STYLEOP_None) &&
		!(camera->renderflags & RF_INVISIBLE) &&
		camera->sprite != SPR_TNT1)
	{
		// [RH] Use chasecam view
		P_AimCamera (camera, iview->nviewx, iview->nviewy, iview->nviewz, viewsector);
		r_showviewer = true;
	}
	else
	{
		iview->nviewx = camera->x;
		iview->nviewy = camera->y;
		iview->nviewz = camera->player ? camera->player->viewz : camera->z + camera->GetClass()->Meta.GetMetaFixed(AMETA_CameraHeight);
		viewsector = camera->Sector;
		r_showviewer = false;
	}
	iview->nviewpitch = camera->pitch;
	if (camera->player != 0)
	{
		player = camera->player;
	}

	iview->nviewangle = camera->angle + viewangleoffset;
	if (iview->otic == -1 || r_NoInterpolate)
	{
		R_ResetViewInterpolation ();
		iview->otic = nowtic;
	}

	r_TicFrac = I_GetTimeFrac (&r_FrameTime);
	if (cl_capfps || r_NoInterpolate)
	{
		r_TicFrac = FRACUNIT;
	}

	R_InterpolateView (player, r_TicFrac, iview);

#ifdef TEST_X
	viewx = TEST_X;
	viewy = TEST_Y;
	viewz = TEST_Z;
	viewangle = TEST_ANGLE;
#endif

	R_CopyStackedViewParameters();
	R_SetViewAngle ();

	interpolator.DoInterpolations (r_TicFrac);

	// Keep the view within the sector's floor and ceiling
	fixed_t theZ = viewsector->ceilingplane.ZatPoint (viewx, viewy) - 4*FRACUNIT;
	if (viewz > theZ)
	{
		viewz = theZ;
	}

	theZ = viewsector->floorplane.ZatPoint (viewx, viewy) + 4*FRACUNIT;
	if (viewz < theZ)
	{
		viewz = theZ;
	}

	if (!paused)
	{
		int intensity = DEarthquake::StaticGetQuakeIntensity (camera);
		if (intensity != 0)
		{
			viewx += ((pr_torchflicker() % (intensity<<2))
						-(intensity<<1))<<FRACBITS;
			viewy += ((pr_torchflicker() % (intensity<<2))
						-(intensity<<1))<<FRACBITS;
		}
	}

	extralight = camera->player ? camera->player->extralight : 0;
	newblend = 0;

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend

	TArray<lightlist_t> &lightlist = viewsector->e->XFloor.lightlist;
	if (lightlist.Size() > 0)
	{
		for(unsigned int i=0;i<lightlist.Size();i++)
		{
			fixed_t lightbottom;
			if (i<lightlist.Size()-1) 
				lightbottom = lightlist[i+1].plane.ZatPoint(viewx, viewy);
			else 
				lightbottom = viewsector->floorplane.ZatPoint(viewx, viewy);

			if (lightbottom < viewz)
			{
				// 3d floor 'fog' is rendered as a blending value
				PalEntry blendv = lightlist[i].blend;

				// If no alpha is set, use 50%
				if (blendv.a==0 && blendv!=0) blendv.a=128;
				newblend = blendv.d;
				break;
			}
		}
	}
	else
	{
		const sector_t *s = viewsector->GetHeightSec();
		if (s != NULL)
		{
			newblend = viewz < s->floorplane.ZatPoint (viewx, viewy)
				? s->bottommap
				: viewz > s->ceilingplane.ZatPoint (viewx, viewy)
				? s->topmap
				: s->midmap;
			if (APART(newblend) == 0 && newblend >= numfakecmaps)
				newblend = 0;
		}
	}

	// [RH] Don't override testblend unless entering a sector with a
	//		blend different from the previous sector's. Same goes with
	//		NormalLight's maps pointer.
	if (R_OldBlend != newblend)
	{
		R_OldBlend = newblend;
		if (APART(newblend))
		{
			BaseBlendR = RPART(newblend);
			BaseBlendG = GPART(newblend);
			BaseBlendB = BPART(newblend);
			BaseBlendA = APART(newblend) / 255.f;
			NormalLight.Maps = realcolormaps;
		}
		else
		{
			NormalLight.Maps = realcolormaps + NUMCOLORMAPS*256*newblend;
			BaseBlendR = BaseBlendG = BaseBlendB = 0;
			BaseBlendA = 0.f;
		}
	}

	realfixedcolormap = NULL;
	fixedcolormap = NULL;
	fixedlightlev = -1;

	if (player != NULL && camera == player->mo)
	{
		if (player->fixedcolormap >= 0 && player->fixedcolormap < (int)SpecialColormaps.Size())
		{
			realfixedcolormap = &SpecialColormaps[player->fixedcolormap];
			if (RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
			{
				// Render everything fullbright. The copy to video memory will
				// apply the special colormap, so it won't be restricted to the
				// palette.
				fixedcolormap = realcolormaps;
			}
			else
			{
				fixedcolormap = SpecialColormaps[player->fixedcolormap].Colormap;
			}
		}
		else if (player->fixedlightlevel >= 0 && player->fixedlightlevel < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedlightlevel * 256;
		}
	}
	// [RH] Inverse light for shooting the Sigil
	if (fixedcolormap == NULL && extralight == INT_MIN)
	{
		fixedcolormap = SpecialColormaps[INVERSECOLORMAP].Colormap;
		extralight = 0;
	}

	// [RH] freelook stuff
	{
		fixed_t dy;
		
		if (camera != NULL)
		{
			dy = FixedMul (FocalLengthY, finetangent[(ANGLE_90-viewpitch)>>ANGLETOFINESHIFT]);
		}
		else
		{
			dy = 0;
		}

		centeryfrac = (viewheight << (FRACBITS-1)) + dy;
		centery = centeryfrac >> FRACBITS;
		globaluclip = FixedDiv (-centeryfrac, InvZtoScale);
		globaldclip = FixedDiv ((viewheight<<FRACBITS)-centeryfrac, InvZtoScale);

		//centeryfrac &= 0xffff0000;
		int e, i;

		i = 0;
		e = viewheight;
		fixed_t focus = FocalLengthY;
		fixed_t den;
		if (i < centery)
		{
			den = centeryfrac - (i << FRACBITS) - FRACUNIT/2;
			if (e <= centery)
			{
				do {
					yslope[i] = FixedDiv (focus, den);
					den -= FRACUNIT;
				} while (++i < e);
			}
			else
			{
				do {
					yslope[i] = FixedDiv (focus, den);
					den -= FRACUNIT;
				} while (++i < centery);
				den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
				do {
					yslope[i] = FixedDiv (focus, den);
					den += FRACUNIT;
				} while (++i < e);
			}
		}
		else
		{
			den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
			do {
				yslope[i] = FixedDiv (focus, den);
				den += FRACUNIT;
			} while (++i < e);
		}
	}

	P_UnPredictPlayer ();
	framecount++;
	validcount++;

	if (r_polymost)
	{
		polyclipped = RP_SetupFrame (false);
	}

	if (RenderTarget == screen && r_clearbuffer != 0)
	{
		int color;
		int hom = r_clearbuffer;

		if (hom == 3)
		{
			hom = ((I_FPSTime() / 128) & 1) + 1;
		}
		if (hom == 1)
		{
			color = GPalette.BlackIndex;
		}
		else if (hom == 2)
		{
			color = GPalette.WhiteIndex;
		}
		else if (hom == 4)
		{
			color = (I_FPSTime() / 32) & 255;
		}
		else
		{
			color = pr_hom();
		}
		memset(RenderTarget->GetBuffer(), color, RenderTarget->GetPitch() * RenderTarget->GetHeight());
	}
}

//==========================================================================
//
// R_RefreshViewBorder
//
// Draws the border around the player view, if needed.
//
//==========================================================================

void R_RefreshViewBorder ()
{
	if (setblocks < 10)
	{
		if (BorderNeedRefresh)
		{
			BorderNeedRefresh--;
			if (BorderTopRefresh)
			{
				BorderTopRefresh--;
			}
			R_DrawViewBorder();
		}
		else if (BorderTopRefresh)
		{
			BorderTopRefresh--;
			R_DrawTopBorder();
		}
	}
}

//==========================================================================
//
// R_EnterMirror
//
// [RH] Draw the reflection inside a mirror
//
//==========================================================================

void R_EnterMirror (drawseg_t *ds, int depth)
{
	angle_t startang = viewangle;
	fixed_t startx = viewx;
	fixed_t starty = viewy;

	CurrentMirror++;

	unsigned int mirrorsAtStart = WallMirrors.Size ();

	vertex_t *v1 = ds->curline->v1;

	// Reflect the current view behind the mirror.
	if (ds->curline->linedef->dx == 0)
	{ // vertical mirror
		viewx = v1->x - startx + v1->x;
	}
	else if (ds->curline->linedef->dy == 0)
	{ // horizontal mirror
		viewy = v1->y - starty + v1->y;
	}
	else
	{ // any mirror--use floats to avoid integer overflow
		vertex_t *v2 = ds->curline->v2;

		float dx = FIXED2FLOAT(v2->x - v1->x);
		float dy = FIXED2FLOAT(v2->y - v1->y);
		float x1 = FIXED2FLOAT(v1->x);
		float y1 = FIXED2FLOAT(v1->y);
		float x = FIXED2FLOAT(startx);
		float y = FIXED2FLOAT(starty);

		// the above two cases catch len == 0
		float r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = FLOAT2FIXED((x1 + r * dx)*2 - x);
		viewy = FLOAT2FIXED((y1 + r * dy)*2 - y);
	}
	viewangle = 2*R_PointToAngle2 (ds->curline->v1->x, ds->curline->v1->y,
								   ds->curline->v2->x, ds->curline->v2->y) - startang;

	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

	viewtansin = FixedMul (FocalTangent, viewsin);
	viewtancos = FixedMul (FocalTangent, viewcos);

	R_CopyStackedViewParameters();

	validcount++;
	ActiveWallMirror = ds->curline;

	R_ClearPlanes (false);
	R_ClearClipSegs (ds->x1, ds->x2 + 1);

	memcpy (ceilingclip + ds->x1, openings + ds->sprtopclip, (ds->x2 - ds->x1 + 1)*sizeof(*ceilingclip));
	memcpy (floorclip + ds->x1, openings + ds->sprbottomclip, (ds->x2 - ds->x1 + 1)*sizeof(*floorclip));

	WindowLeft = ds->x1;
	WindowRight = ds->x2;
	MirrorFlags = (depth + 1) & 1;

	R_RenderBSPNode (nodes + numnodes - 1);
	R_3D_ResetClip(); // reset clips (floor/ceiling)

	R_DrawPlanes ();
	R_DrawSkyBoxes ();

	// Allow up to 4 recursions through a mirror
	if (depth < 4)
	{
		unsigned int mirrorsAtEnd = WallMirrors.Size ();

		for (; mirrorsAtStart < mirrorsAtEnd; mirrorsAtStart++)
		{
			R_EnterMirror (drawsegs + WallMirrors[mirrorsAtStart], depth + 1);
		}
	}
	else
	{
		depth = depth;
	}

	viewangle = startang;
	viewx = startx;
	viewy = starty;
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
	static BYTE *lastbuff = NULL;

	int pitch = RenderTarget->GetPitch();
	BYTE *lineptr = RenderTarget->GetBuffer() + viewwindowy*pitch + viewwindowx;

	if (dc_pitch != pitch || lineptr != lastbuff)
	{
		if (dc_pitch != pitch)
		{
			dc_pitch = pitch;
			R_InitFuzzTable (pitch);
#if defined(X86_ASM) || defined(X64_ASM)
			ASM_PatchPitch ();
#endif
		}
		dc_destorg = lineptr;
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

	fakeActive = 0; // kg3D - reset fake floor idicator
	R_3D_ResetClip(); // reset clips (floor/ceiling)

	R_SetupBuffer ();
	R_SetupFrame (actor);

	// Clear buffers.
	R_ClearClipSegs (0, viewwidth);
	R_ClearDrawSegs ();
	R_ClearPlanes (true);
	R_ClearSprites ();

	NetUpdate ();

	// [RH] Show off segs if r_drawflat is 1
	if (r_drawflat)
	{
		hcolfunc_pre = R_FillColumnHorizP;
		hcolfunc_post1 = rt_copy1col;
		hcolfunc_post4 = rt_copy4cols;
		colfunc = R_FillColumnP;
		spanfunc = R_FillSpan;
	}
	else
	{
		hcolfunc_pre = R_DrawColumnHoriz;
		hcolfunc_post1 = rt_map1col;
		hcolfunc_post4 = rt_map4cols;
		colfunc = basecolfunc;
		spanfunc = R_DrawSpan;
	}

	WindowLeft = 0;
	WindowRight = viewwidth - 1;
	MirrorFlags = 0;
	ActiveWallMirror = NULL;

	r_dontmaplines = dontmaplines;
	
	// [RH] Hack to make windows into underwater areas possible
	r_fakingunderwater = false;

	// [RH] Setup particles for this frame
	R_FindParticleSubsectors ();

	WallCycles.Clock();
	DWORD savedflags = camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!r_showviewer)
	{
		camera->renderflags |= RF_INVISIBLE;
	}
	// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
	PO_LinkToSubsectors();
	if (r_polymost < 2)
	{
		R_RenderBSPNode (nodes + numnodes - 1);	// The head node is the last node output.
		R_3D_ResetClip(); // reset clips (floor/ceiling)
	}
	camera->renderflags = savedflags;
	WallCycles.Unclock();

	NetUpdate ();

	if (viewactive)
	{
		PlaneCycles.Clock();
		R_DrawPlanes ();
		R_DrawSkyBoxes ();
		PlaneCycles.Unclock();

		// [RH] Walk through mirrors
		size_t lastmirror = WallMirrors.Size ();
		for (unsigned int i = 0; i < lastmirror; i++)
		{
			R_EnterMirror (drawsegs + WallMirrors[i], 0);
		}

		NetUpdate ();
		
		MaskedCycles.Clock();
		R_DrawMasked ();
		MaskedCycles.Unclock();

		NetUpdate ();

		if (r_polymost)
		{
			RP_RenderBSPNode (nodes + numnodes - 1);
			if (polyclipped)
			{
				RP_SetupFrame (true);
				RP_RenderBSPNode (nodes + numnodes - 1);
			}
		}
	}
	WallMirrors.Clear ();
	interpolator.RestoreInterpolations ();
	R_SetupBuffer ();
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

	viewwidth = width;
	RenderTarget = canvas;
	bRenderingToCanvas = true;

	R_SetWindow (12, width, height, height);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;

	R_RenderActorView (actor, dontmaplines);

	RenderTarget = screen;
	bRenderingToCanvas = false;
	R_ExecuteSetViewSize ();
	screen->Lock (true);
	R_SetupBuffer ();
	screen->Unlock ();
	viewactive = savedviewactive;
}

//==========================================================================
//
// FCanvasTextureInfo :: Add
//
// Assigns a camera to a canvas texture.
//
//==========================================================================

void FCanvasTextureInfo::Add (AActor *viewpoint, FTextureID picnum, int fov)
{
	FCanvasTextureInfo *probe;
	FCanvasTexture *texture;

	if (!picnum.isValid())
	{
		return;
	}
	texture = static_cast<FCanvasTexture *>(TexMan[picnum]);
	if (!texture->bHasCanvas)
	{
		Printf ("%s is not a valid target for a camera\n", texture->Name);
		return;
	}

	// Is this texture already assigned to a camera?
	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Texture == texture)
		{
			// Yes, change its assignment to this new camera
			if (probe->Viewpoint != viewpoint || probe->FOV != fov)
			{
				texture->bFirstUpdate = true;
			}
			probe->Viewpoint = viewpoint;
			probe->FOV = fov;
			return;
		}
	}
	// No, create a new assignment
	probe = new FCanvasTextureInfo;
	probe->Viewpoint = viewpoint;
	probe->Texture = texture;
	probe->PicNum = picnum;
	probe->FOV = fov;
	probe->Next = List;
	texture->bFirstUpdate = true;
	List = probe;
}

//==========================================================================
//
// FCanvasTextureInfo :: UpdateAll
//
// Updates all canvas textures that were visible in the last frame.
//
//==========================================================================

void FCanvasTextureInfo::UpdateAll ()
{
	FCanvasTextureInfo *probe;

	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Viewpoint != NULL && probe->Texture->bNeedsUpdate)
		{
			probe->Texture->RenderView (probe->Viewpoint, probe->FOV);
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: EmptyList
//
// Removes all camera->texture assignments.
//
//==========================================================================

void FCanvasTextureInfo::EmptyList ()
{
	FCanvasTextureInfo *probe, *next;

	for (probe = List; probe != NULL; probe = next)
	{
		next = probe->Next;
		delete probe;
	}
	List = NULL;
}

//==========================================================================
//
// FCanvasTextureInfo :: Serialize
//
// Reads or writes the current set of mappings in an archive.
//
//==========================================================================

void FCanvasTextureInfo::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		FCanvasTextureInfo *probe;

		for (probe = List; probe != NULL; probe = probe->Next)
		{
			if (probe->Texture != NULL && probe->Viewpoint != NULL)
			{
				arc << probe->Viewpoint << probe->FOV << probe->PicNum;
			}
		}
		AActor *nullactor = NULL;
		arc << nullactor;
	}
	else
	{
		AActor *viewpoint;
		int fov;
		FTextureID picnum;
		
		EmptyList ();
		while (arc << viewpoint, viewpoint != NULL)
		{
			arc << fov << picnum;
			Add (viewpoint, picnum, fov);
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: Mark
//
// Marks all viewpoints in the list for the collector.
//
//==========================================================================

void FCanvasTextureInfo::Mark()
{
	for (FCanvasTextureInfo *probe = List; probe != NULL; probe = probe->Next)
	{
		GC::Mark(probe->Viewpoint);
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
	R_OldBlend = ~0;
}

