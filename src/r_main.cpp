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
#include "m_alloc.h"
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
#include "vectors.h"

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

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

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
static TArray<InterpolationViewer> PastViewers;
static int centerxwide;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, r_viewsize, "", CVAR_NOSET)

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
fixed_t			FocalTangent;
fixed_t			FocalLengthX;
fixed_t			FocalLengthY;
float			FocalLengthXfloat;
int 			viewangleoffset;
int 			validcount = 1; 	// increment every time a check is made
lighttable_t	*basecolormap;		// [RH] colormap currently drawing with
int				fixedlightlev;
lighttable_t	*fixedcolormap;
float			WallTMapScale;
float			WallTMapScale2;

extern "C" {
int 			centerx;
int				centery;
}

DCanvas			*RenderTarget;		// [RH] canvas to render to
bool			bRenderingToCanvas;	// [RH] True if rendering to a special canvas
fixed_t			globaluclip, globaldclip;
fixed_t 		centerxfrac;
fixed_t 		centeryfrac;
fixed_t			yaspectmul;
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
int				setblocks, setdetail = -1;

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
void (*hcolfunc_post4) (int sx, int yl, int yh);

cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastcenteryfrac;
static bool NoInterpolateView;

// CODE --------------------------------------------------------------------

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

	fixed_t ax = abs (x);
	fixed_t ay = abs (y);
	int div;
	angle_t angle;

	if (ax > ay)
	{
		swap (ax, ay);
	}
	div = SlopeDiv (ax, ay);
	angle = tantoangle[div];

	if (x >= 0)
	{
		if (y >= 0)
		{
			if (x > y)
			{ // octant 0
				return angle;
			}
			else
			{ // octant 1
				return ANG90 - 1 - angle;
			}
		}
		else // y < 0
		{
			if (x > -y)
			{ // octant 8
				return (angle_t)-(SDWORD)angle;
			}
			else
			{ // octant 7
				return ANG270 + angle;
			}
		}
	}
	else // x < 0
	{
		if (y >= 0)
		{
			if (-x > y)
			{ // octant 3
				return ANG180 - 1 - angle;
			}
			else
			{ // octant 2
				return ANG90 + angle;
			}
		}
		else // y < 0
		{
			if (x < y)
			{ // octant 4
				return ANG180 + angle;
			}
			else
			{ // octant 5
				return ANG270 - 1 - angle;
			}
		}
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
		swap (dx, dy);
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
	finetangent[0] = (angle_t)(FRACUNIT*tan ((0.5-FINEANGLES/4)*pimul)+0.5);
	for (i = 1; i < FINEANGLES/2; i++)
	{
		finetangent[i] = (angle_t)(FRACUNIT*tan ((i-FINEANGLES/4)*pimul)+0.5);
	}
	
	// finesine table
	for (i = 0; i < FINEANGLES/4; i++)
	{
		finesine[i] = (angle_t)(FRACUNIT * sin (i*pimul));
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
		xtoviewangle[i] = (angle_t)(-(signed)xtoviewangle[viewwidth-i-1]);
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

	r_BaseVisibility = toint (vis * 65536.f);

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = FixedMul (Scale (InvZtoScale, SCREENWIDTH*(BaseRatioSizes[WidescreenRatio][1]<<detailyshift),
		(viewwidth<<detailxshift)*SCREENHEIGHT*3), FixedMul (r_WallVisibility, FocalTangent));

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
// CVAR r_detail
//
// Selects a pixel doubling mode
//
//==========================================================================

CUSTOM_CVAR (Int, r_detail, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	static bool badrecovery = false;

	if (badrecovery)
	{
		badrecovery = false;
		return;
	}

	if (self < 0 || self > 3)
	{
		Printf ("Bad detail mode. (Use 0-3)\n");
		badrecovery = true;
		self = (detailyshift << 1) | detailxshift;
		return;
	}

	setdetail = self;
	setsizeneeded = true;
}

//==========================================================================
//
// R_SetDetail
//
//==========================================================================

void R_SetDetail (int detail)
{
	detailxshift = detail & 1;
	detailyshift = (detail >> 1) & 1;
}

//==========================================================================
//
// R_SetWindow
//
//==========================================================================

void R_SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight)
{
	int virtheight, virtwidth;

	if (windowSize >= 11)
	{
		realviewwidth = fullWidth;
		freelookviewheight = realviewheight = fullHeight;
	}
	else if (windowSize == 10)
	{
		realviewwidth = fullWidth;
		realviewheight = stHeight;
		freelookviewheight = fullHeight;
	}
	else
	{
		realviewwidth = ((setblocks*fullWidth)/10) & (~15);
		realviewheight = ((setblocks*stHeight)/10)&~7;
		freelookviewheight = ((setblocks*fullHeight)/10)&~7;
	}

	// If the screen is approximately 16:9 or 16:10, consider it widescreen.
	WidescreenRatio = CheckRatio (fullWidth, fullHeight);

	DrawFSHUD = (windowSize == 11);
	
	viewwidth = realviewwidth >> detailxshift;
	viewheight = realviewheight >> detailyshift;
	fuzzviewheight = viewheight - 2;	// Maximum row the fuzzer can draw to
	freelookviewheight >>= detailyshift;
	halfviewwidth = (viewwidth >> 1) - 1;

	if (!bRenderingToCanvas)
	{ // Set r_viewsize cvar to reflect the current view size
		UCVarValue value;
		char temp[16];

		sprintf (temp, "%d x %d", viewwidth, viewheight);
		value.String = temp;
		r_viewsize.ForceSet (value, CVAR_String);
	}

	lastcenteryfrac = 1<<30;
	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	virtwidth = fullWidth >> detailxshift;
	virtheight = fullHeight >> detailyshift;
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

	MaxVisForWall = FixedMul (Scale (InvZtoScale, SCREENWIDTH*(r_Yaspect<<detailyshift),
		(viewwidth<<detailxshift)*SCREENHEIGHT), FocalTangent);
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

	if (setdetail >= 0)
	{
		R_SetDetail (setdetail);
		setdetail = -1;
	}

	R_SetWindow (setblocks, SCREENWIDTH, SCREENHEIGHT, ST_Y);

	// Handle resize, e.g. smaller view windows with border and/or status bar.
	viewwindowx = (screen->GetWidth() - (viewwidth<<detailxshift))>>1;

	// Same with base row offset.
	viewwindowy = ((viewwidth<<detailxshift) == screen->GetWidth()) ?
		0 : (ST_Y-(viewheight<<detailyshift)) >> 1;
}

//==========================================================================
//
// CVAR screenblocks
//
// Selects the size of the visible window
//
//==========================================================================

CUSTOM_CVAR (Int, screenblocks, 10, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
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
		r_detail.Callback ();
	}
}

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_Init (void)
{
	R_InitData ();
	R_InitPointToAngle ();
	R_InitTables ();
	// viewwidth / viewheight are set by the defaults

	R_SetViewSize (screenblocks);
	R_InitPlanes ();
	R_InitTranslationTables ();

	R_InitParticles ();	// [RH] Setup particle engine

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
		viewpitch = clamp<int> (iview->nviewpitch - (LocalViewPitch & 0xFFFF0000), -ANGLE_1*32, +ANGLE_1*56);
	}
	else
	{
		viewpitch = iview->oviewpitch + FixedMul (frac, iview->nviewpitch - iview->oviewpitch);
		viewangle = iview->oviewangle + FixedMul (frac, iview->nviewangle - iview->oviewangle);
	}
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
	for (size_t i = 0; i < PastViewers.Size(); ++i)
	{
		if (PastViewers[i].ViewActor == actor)
		{
			return &PastViewers[i];
		}
	}

	// Not found, so make a new one
	InterpolationViewer iview;
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
// R_SetupFrame
//
//==========================================================================

void R_SetupFrame (AActor *actor)
{
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

	if (player != NULL &&
		((player->cheats & CF_CHASECAM)/* || (camera->health <= 0)*/) &&
		(camera->RenderStyle != STYLE_None) &&
		!(camera->renderflags & RF_INVISIBLE) &&
		camera->sprite != 0)	// Sprite 0 is always TNT1
	{
		// [RH] Use chasecam view
		P_AimCamera (camera);
		iview->nviewx = CameraX;
		iview->nviewy = CameraY;
		iview->nviewz = CameraZ;
		viewsector = CameraSector;
	}
	else
	{
		iview->nviewx = camera->x;
		iview->nviewy = camera->y;
		iview->nviewz = camera->player ? camera->player->viewz : camera->z;
		viewsector = camera->Sector;
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
	if (cl_capfps || r_NoInterpolate/* || netgame*/)
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

	R_SetViewAngle ();

	dointerpolations (r_TicFrac);

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

	if (player && player->xviewshift && !paused)
	{
		int intensity = player->xviewshift;
		viewx += ((pr_torchflicker() % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
		viewy += ((pr_torchflicker() % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
	}

	extralight = camera->player ? camera->player->extralight : 0;

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend

	if (viewsector->heightsec &&
		!(viewsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		const sector_t *s = viewsector->heightsec;
		newblend = viewz < s->floorplane.ZatPoint (viewx, viewy)
			? s->bottommap
			: viewz > s->ceilingplane.ZatPoint (viewx, viewy)
			? s->topmap
			: s->midmap;
		if (APART(newblend) == 0 && newblend >= numfakecmaps)
			newblend = 0;
	}
	else
	{
		newblend = 0;
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

	fixedcolormap = NULL;
	fixedlightlev = 0;

	if (player != NULL && camera == player->mo && player->fixedcolormap)
	{
		if (player->fixedcolormap < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedcolormap*256;
			fixedcolormap = NormalLight.Maps;
		}
		else
		{
			fixedcolormap = InvulnerabilityColormap;
		}
	}
	// [RH] Inverse light for shooting the Sigil
	else if (extralight == INT_MIN)
	{
		fixedcolormap = InvulnerabilityColormap;
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
		int i = lastcenteryfrac - centeryfrac;
		if (i != 0)
		{
			int e;

			if ((i & (FRACUNIT-1)) == 0)	// Unlikely, but possible
			{
				i >>= FRACBITS;
				if (abs (i) < viewheight)
				{
					fixed_t *from, *to;
					if (i > 0)
					{
//						memmove (yslope, yslope + i, (viewheight - i) * sizeof(fixed_t));
						int index = 0;
						from = yslope + i;
						to = yslope;
						i = e = viewheight - i;
						do
						{
							*(to + index) = *(from + index);
							index++;
						} while (--e);
						e = viewheight;
					}
					else
					{
//						memmove (yslope - i, yslope, (viewheight + i) * sizeof(fixed_t));
						from = yslope;
						to = yslope - i;
						e = viewheight + i - 1;
						do
						{
							*(to + e) = *(from + e);
						} while (--e >= 0);
						e = -i;
						i = 0;
					}
				}
				else
				{
					i = 0;
					e = viewheight;
				}
			}
			else
			{
				i = 0;
				e = viewheight;
			}

			{
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
		}
		lastcenteryfrac = centeryfrac;
	}

	P_UnPredictPlayer ();
	framecount++;
	validcount++;
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

	size_t mirrorsAtStart = WallMirrors.Size ();

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

	R_DrawPlanes ();
	R_DrawSkyBoxes ();

	// Allow up to 4 recursions through a mirror
	if (depth < 4)
	{
		size_t mirrorsAtEnd = WallMirrors.Size ();

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

void R_SetupBuffer (bool inview)
{
	static BYTE *lastbuff = NULL;

	int pitch = RenderTarget->GetPitch();
	BYTE *lineptr = RenderTarget->GetBuffer() + viewwindowy*pitch + viewwindowx;

	if (inview)
	{
		pitch <<= detailyshift;
	}
	if (detailxshift)
	{
		lineptr += viewwidth;
	}

	if (dc_pitch != pitch || lineptr != lastbuff)
	{
		if (dc_pitch != pitch)
		{
			dc_pitch = pitch;
			R_InitFuzzTable (pitch);
#ifdef USEASM
			ASM_PatchPitch ();
#endif
		}
		dc_destorg = lineptr;
		for (int i = 0; i < screen->GetHeight(); i++)
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

void R_RenderActorView (AActor *actor)
{
	WallCycles = PlaneCycles = MaskedCycles = WallScanCycles = 0;

	R_SetupBuffer (true);
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

	// [RH] Hack to make windows into underwater areas possible
	r_fakingunderwater = false;

	// [RH] Setup particles for this frame
	R_FindParticleSubsectors ();

	clock (WallCycles);
	// Never draw the player unless in chasecam mode
	if (camera->player && !(camera->player->cheats & CF_CHASECAM) && camera->health > 0)
	{
		WORD savedflags = camera->renderflags;
		camera->renderflags |= RF_INVISIBLE;
//memset (screen->GetBuffer(), 255, screen->GetWidth()*screen->GetHeight());
		R_RenderBSPNode (nodes + numnodes - 1);
		camera->renderflags = savedflags;
	}
	else
	{	// The head node is the last node output.
		// [[RH] Not that this tells me anything...]
		R_RenderBSPNode (nodes + numnodes - 1);
	}
	unclock (WallCycles);

	NetUpdate ();

	if (viewactive)
	{
		clock (PlaneCycles);
		R_DrawPlanes ();
		R_DrawSkyBoxes ();
		unclock (PlaneCycles);

		// [RH] Walk through mirrors
		size_t lastmirror = WallMirrors.Size ();
		for (size_t i = 0; i < lastmirror; i++)
		{
			R_EnterMirror (drawsegs + WallMirrors[i], 0);
		}
		WallMirrors.Clear ();

		NetUpdate ();
		
		clock (MaskedCycles);
		R_DrawMasked ();
		unclock (MaskedCycles);

		NetUpdate ();
	}

	restoreinterpolations ();

	// If there is vertical doubling, and the view window is not an even height,
	// draw a black line at the bottom of the view window.
	if (detailyshift && viewwindowy == 0 && (realviewheight & 1))
	{
		screen->Clear (0, realviewheight-1, realviewwidth, realviewheight, 0);
	}

	R_SetupBuffer (false);
}

//==========================================================================
//
// R_RenderViewToCanvas
//
// Pre: Canvas is already locked.
//
//==========================================================================

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas,
	int x, int y, int width, int height)
{
	const int saveddetail = detailxshift | (detailyshift << 1);

	detailxshift = detailyshift = 0;
	realviewwidth = viewwidth = width;

	RenderTarget = canvas;
	bRenderingToCanvas = true;

	R_SetDetail (0);
	R_SetWindow (12, width, height, height);
	viewwindowx = x;
	viewwindowy = y;

	R_RenderActorView (actor);

	RenderTarget = screen;
	bRenderingToCanvas = false;
	R_SetDetail (saveddetail);
	R_ExecuteSetViewSize ();
	screen->Lock (true);
	R_SetupBuffer (false);
	screen->Unlock ();
}

FCanvasTextureInfo *FCanvasTextureInfo::List;
//==========================================================================
//
// FCanvasTextureInfo :: Add
//
// Assigns a camera to a canvas texture.
//
//==========================================================================

void FCanvasTextureInfo::Add (AActor *viewpoint, int picnum, int fov)
{
	FCanvasTextureInfo *probe;
	FCanvasTexture *texture;

	texture = static_cast<FCanvasTexture *>(TexMan[picnum]);
	if (!texture->bHasCanvas)
	{
		Printf ("%s is not a valid target for a camera\n", texture->Name);
		return;
	}

	// First, is this texture already assigned to a camera?
	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Texture == texture)
		{
			// Yes, change its assignment to this new camera
			probe->Viewpoint = viewpoint;
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
				arc << probe->Viewpoint << probe->FOV;
				TexMan.WriteTexture (arc, probe->PicNum);
			}
		}
		AActor *nullactor = NULL;
		arc << nullactor;
	}
	else
	{
		AActor *viewpoint;
		int picnum, fov;
		
		EmptyList ();
		arc << viewpoint;
		while (viewpoint != NULL)
		{
			arc << fov;
			picnum = TexMan.ReadTexture (arc);
			Add (viewpoint, picnum, fov);
			arc << viewpoint;
		}
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

// Stuff from BUILD to interpolate floors and ceilings
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
// This code has been modified from its original form.

static bool didInterp;

FActiveInterpolation *FActiveInterpolation::curiposhash[INTERPOLATION_BUCKETS];

void FActiveInterpolation::CopyInterpToOld ()
{
	switch (Type)
	{
	case INTERP_SectorFloor:
		oldipos[0] = ((sector_t*)Address)->floorplane.d;
		oldipos[1] = ((sector_t*)Address)->floortexz;
		break;
	case INTERP_SectorCeiling:
		oldipos[0] = ((sector_t*)Address)->ceilingplane.d;
		oldipos[1] = ((sector_t*)Address)->ceilingtexz;
		break;
	case INTERP_Vertex:
		oldipos[0] = ((vertex_t*)Address)->x;
		oldipos[1] = ((vertex_t*)Address)->y;
		break;
	case INTERP_FloorPanning:
		oldipos[0] = ((sector_t*)Address)->floor_xoffs;
		oldipos[1] = ((sector_t*)Address)->floor_yoffs;
		break;
	case INTERP_CeilingPanning:
		oldipos[0] = ((sector_t*)Address)->ceiling_xoffs;
		oldipos[1] = ((sector_t*)Address)->ceiling_yoffs;
		break;
	case INTERP_WallPanning:
		oldipos[0] = ((side_t*)Address)->rowoffset;
		oldipos[1] = ((side_t*)Address)->textureoffset;
		break;
	}
}

void FActiveInterpolation::CopyBakToInterp ()
{
	switch (Type)
	{
	case INTERP_SectorFloor:
		((sector_t*)Address)->floorplane.d = bakipos[0];
		((sector_t*)Address)->floortexz = bakipos[1];
		break;
	case INTERP_SectorCeiling:
		((sector_t*)Address)->ceilingplane.d = bakipos[0];
		((sector_t*)Address)->ceilingtexz = bakipos[1];
		break;
	case INTERP_Vertex:
		((vertex_t*)Address)->x = bakipos[0];
		((vertex_t*)Address)->y = bakipos[1];
		break;
	case INTERP_FloorPanning:
		((sector_t*)Address)->floor_xoffs = bakipos[0];
		((sector_t*)Address)->floor_yoffs = bakipos[1];
		break;
	case INTERP_CeilingPanning:
		((sector_t*)Address)->ceiling_xoffs = bakipos[0];
		((sector_t*)Address)->ceiling_yoffs = bakipos[1];
		break;
	case INTERP_WallPanning:
		((side_t*)Address)->rowoffset = bakipos[0];
		((side_t*)Address)->textureoffset = bakipos[1];
		break;
	}
}

void FActiveInterpolation::DoAnInterpolation (fixed_t smoothratio)
{
	fixed_t *adr1, *adr2, pos;

	switch (Type)
	{
	case INTERP_SectorFloor:
		adr1 = &((sector_t*)Address)->floorplane.d;
		adr2 = &((sector_t*)Address)->floortexz;
		break;
	case INTERP_SectorCeiling:
		adr1 = &((sector_t*)Address)->ceilingplane.d;
		adr2 = &((sector_t*)Address)->ceilingtexz;
		break;
	case INTERP_Vertex:
		adr1 = &((vertex_t*)Address)->x;
		adr2 = &((vertex_t*)Address)->y;
		break;
	case INTERP_FloorPanning:
		adr1 = &((sector_t*)Address)->floor_xoffs;
		adr2 = &((sector_t*)Address)->floor_yoffs;
		break;
	case INTERP_CeilingPanning:
		adr1 = &((sector_t*)Address)->ceiling_xoffs;
		adr2 = &((sector_t*)Address)->ceiling_yoffs;
		break;
	case INTERP_WallPanning:
		adr1 = &((side_t*)Address)->rowoffset;
		adr2 = &((side_t*)Address)->textureoffset;
		break;
	default:
		return;
	}

	pos = bakipos[0] = *adr1;
	*adr1 = oldipos[0] + FixedMul (pos - oldipos[0], smoothratio);

	pos = bakipos[1] = *adr2;
	*adr2 = oldipos[1] + FixedMul (pos - oldipos[1], smoothratio);
}

int FActiveInterpolation::HashKey (EInterpType type, void *interptr)
{
	return (int)type * ((int)interptr>>5);
}

int FActiveInterpolation::CountInterpolations ()
{
	int d1, d2, d3;
	return CountInterpolations (&d1, &d2, &d3);
}

int FActiveInterpolation::CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill)
{
	int count = 0;
	int inuse = 0;
	int minuse = INT_MAX;
	int maxuse = INT_MIN;

	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		int use = 0;
		FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
		if (probe != NULL)
		{
			inuse++;
		}
		while (probe != NULL)
		{
			count++;
			use++;
			probe = probe->Next;
		}
		if (use > 0 && use < minuse)
		{
			minuse = use;
		}
		if (use > maxuse)
		{
			maxuse = use;
		}
	}
	*usedbuckets = inuse;
	*minbucketfill = minuse == INT_MAX ? 0 : minuse;
	*maxbucketfill = maxuse;
	return count;
}

FActiveInterpolation *FActiveInterpolation::FindInterpolation (EInterpType type, void *interptr, FActiveInterpolation **&interp_p)
{
	int hash = HashKey (type, interptr) % INTERPOLATION_BUCKETS;
	FActiveInterpolation *probe, **probe_p;

	for (probe_p = &curiposhash[hash], probe = *probe_p;
		probe != NULL;
		probe_p = &probe->Next, probe = *probe_p)
	{
		if (probe->Address > interptr)
		{ // We passed the place it would have been, so it must not be here.
			probe = NULL;
			break;
		}
		if (probe->Address == interptr && probe->Type == type)
		{ // Found it.
			break;
		}
	}
	interp_p = probe_p;
	return probe;
}

void clearinterpolations()
{
	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; )
		{
			FActiveInterpolation *next = probe->Next;
			delete probe;
			probe = next;
		}
		FActiveInterpolation::curiposhash[i] = NULL;
	}
}

void updateinterpolations()  //Stick at beginning of domovethings
{
	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; probe = probe->Next)
		{
			probe->CopyInterpToOld ();
		}
	}
}

void setinterpolation(EInterpType type, void *posptr)
{
	FActiveInterpolation **interp_p;
	FActiveInterpolation *interp = FActiveInterpolation::FindInterpolation (type, posptr, interp_p);
	if (interp != NULL) return; // It's already active
	interp = new FActiveInterpolation;
	interp->Type = type;
	interp->Address = posptr;
	interp->Next = *interp_p;
	*interp_p = interp;
	interp->CopyInterpToOld ();
}

void stopinterpolation(EInterpType type, void *posptr)
{
	FActiveInterpolation **interp_p;
	FActiveInterpolation *interp = FActiveInterpolation::FindInterpolation (type, posptr, interp_p);
	if (interp != NULL)
	{
		*interp_p = interp->Next;
		delete interp;
	}
}

void dointerpolations(fixed_t smoothratio)       //Stick at beginning of drawscreen
{
	if (smoothratio == FRACUNIT)
	{
		didInterp = false;
		return;
	}

	didInterp = true;

	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; probe = probe->Next)
		{
			probe->DoAnInterpolation (smoothratio);
		}
	}
}

void restoreinterpolations()  //Stick at end of drawscreen
{
	if (didInterp)
	{
		didInterp = false;
		for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
		{
			for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
				probe != NULL; probe = probe->Next)
			{
				probe->CopyBakToInterp ();
			}
		}
	}
}

void SerializeInterpolations (FArchive &arc)
{
	FActiveInterpolation *interp;
	int numinterpolations;
	int i;

	if (arc.IsStoring ())
	{
		numinterpolations = FActiveInterpolation::CountInterpolations();
		arc.WriteCount (numinterpolations);
		for (i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
		{
			for (interp = FActiveInterpolation::curiposhash[i];
				interp != NULL; interp = interp->Next)
			{
				arc << interp;
			}
		}
	}
	else
	{
		clearinterpolations ();
		numinterpolations = arc.ReadCount ();
		for (i = numinterpolations; i > 0; --i)
		{
			FActiveInterpolation **interp_p;
			arc << interp;
			if (FActiveInterpolation::FindInterpolation (interp->Type, interp->Address, interp_p) == NULL)
			{ // Should always return NULL, because there should never been any duplicates stored.
				interp->Next = *interp_p;
				*interp_p = interp;
			}
		}
	}
}

FArchive &operator << (FArchive &arc, FActiveInterpolation *&interp)
{
	BYTE type;
	union
	{
		vertex_t *vert;
		sector_t *sect;
		side_t *side;
		void *ptr;
	} ptr;

	if (arc.IsStoring ())
	{
		type = interp->Type;
		ptr.ptr = interp->Address;
		arc << type;
		switch (type)
		{
		case INTERP_Vertex:			arc << ptr.vert;	break;
		case INTERP_WallPanning:	arc << ptr.side;	break;
		default:					arc << ptr.sect;	break;
		}
	}
	else
	{
		interp = new FActiveInterpolation;
		arc << type;
		interp->Type = (EInterpType)type;
		switch (type)
		{
		case INTERP_Vertex:			arc << ptr.vert;	break;
		case INTERP_WallPanning:	arc << ptr.side;	break;
		default:					arc << ptr.sect;	break;
		}
		interp->Address = ptr.ptr;
	}
	return arc;
}

ADD_STAT (interpolations, out)
{
	int inuse, minuse, maxuse, total;
	total = FActiveInterpolation::CountInterpolations (&inuse, &minuse, &maxuse);
	sprintf (out, "%d interpolations  buckets:%3d  min:%3d  max:%3d  avg:%3d  %d%% full  %d%% buckfull",
		total, inuse, minuse, maxuse, inuse?total/inuse:0, total*100/INTERPOLATION_BUCKETS, inuse*100/INTERPOLATION_BUCKETS);
}
