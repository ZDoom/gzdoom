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
#include "z_zone.h"
#include "i_video.h"
#include "vectors.h"

// MACROS ------------------------------------------------------------------

#define DISTMAP			(2)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_SpanInitData ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool DrawFSHUD;		// [RH] Defined in d_main.cpp
extern short *openings;
extern bool r_fakingunderwater;
EXTERN_CVAR (Bool, r_particles)

// PRIVATE DATA DECLARATIONS -----------------------------------------------

static float LastFOV;
static float CurrentVisibility = 8.f;
static fixed_t MaxVisForWall;
static fixed_t MaxVisForFloor;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, r_viewsize, "", CVAR_NOSET)

fixed_t			r_BaseVisibility;
fixed_t			r_WallVisibility;
fixed_t			r_FloorVisibility;
float			r_TiltVisibility;
fixed_t			r_SpriteVisibility;
fixed_t			r_ParticleVisibility;
fixed_t			r_SkyVisibility;

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

angle_t 		viewangle;
sector_t		*viewsector;

fixed_t 		viewcos, viewtancos;
fixed_t 		viewsin, viewtansin;

AActor			*camera;	// [RH] camera to draw from. doesn't have to be a player

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
  return (y -= y1, (x -= x1) || y) ?
    x >= 0 ?
      y >= 0 ? 
        (x > y) ? tantoangle[SlopeDiv(y,x)] :						// octant 0 
                ANG90-1-tantoangle[SlopeDiv(x,y)] :					// octant 1
        x > (y = -y) ? (angle_t)-(SDWORD)tantoangle[SlopeDiv(y,x)] :	// octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :			// octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] :	// octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :		// octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[SlopeDiv(y,x)] :	// octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] :	// octant 5
    0;
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
// R_PointToDist
//
// Returns the distance from the viewpoint to some other point. In a
// floating point environment, we'd probably be better off using the
// Pythagorean Theorem to determine the result.
//
// killough 5/2/98: simplified
// [RH] Simplified further [sin (t + 90 deg) == cos (t)]
//
//==========================================================================

fixed_t R_PointToDist (fixed_t x, fixed_t y)
{
	fixed_t dx = abs(x - viewx);
	fixed_t dy = abs(y - viewy);

	if (dy > dx)
	{
		swap (dx, dy);
	}

	return FixedDiv (dx, finecosine[tantoangle[FixedDiv (dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}

//==========================================================================
//
// R_PointToDist2
//
//==========================================================================

fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy)
{
	dx = abs (dx);
	dy = abs (dy);

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

	const int hitan = finetangent[FINEANGLES/4+FieldOfView/2];

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
	else if (fov > 175.f)
		fov = 175.f;
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
	if (FocalTangent == 0)
	{
		return;
	}

	// Allow negative visibilities, just for novelty's sake
	//vis = clamp (vis, -204.7f, 204.7f);

	CurrentVisibility = vis;

	r_BaseVisibility = toint (vis * 65536.f);

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = FixedMul (Scale (InvZtoScale, SCREENWIDTH*(200<<detailyshift),
		(viewwidth<<detailxshift)*SCREENHEIGHT), FixedMul (r_WallVisibility, FocalTangent));

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

	r_TiltVisibility = vis * 16.f * 320.f
		* (float)FocalTangent / (float)viewwidth;
	r_SpriteVisibility = r_WallVisibility;
	
	// particles are slightly more visible than regular sprites
	r_ParticleVisibility = r_SpriteVisibility * 2;
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

	DrawFSHUD = (windowSize == 11);
	
	viewwidth = realviewwidth >> detailxshift;
	viewheight = realviewheight >> detailyshift;
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

	yaspectmul = Scale ((320<<FRACBITS), virtheight, 200 * virtwidth);
	iyaspectmulfloat = (float)virtwidth * 0.625f / (float)virtheight;	// 0.625 = 200/320
	InvZtoScale = yaspectmul * centerx;

	WallTMapScale = (float)centerx * 32.f;
	WallTMapScale2 = iyaspectmulfloat * 2.f / (float)centerx;

	// psprite scales
	pspritexscale = centerxfrac / 160;
	pspriteyscale = FixedMul (pspritexscale, yaspectmul);
	pspritexiscale = FixedDiv (FRACUNIT, pspritexscale);

	// thing clipping
	clearbufshort (screenheightarray, viewwidth, (WORD)viewheight);

	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	R_InitTextureMapping ();

	MaxVisForWall = FixedMul (Scale (InvZtoScale, SCREENWIDTH*(200<<detailyshift),
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
	int nodenum;

	// single subsector is a special case
	if (!numnodes)								
		return subsectors;
				
	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}
		
	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//==========================================================================
//
// R_SetViewAngle
//
//==========================================================================

void R_SetViewAngle (angle_t ang)
{
	viewangle = ang;

	viewsin = finesine[ang>>ANGLETOFINESHIFT];
	viewcos = finecosine[ang>>ANGLETOFINESHIFT];

	viewtansin = FixedMul (FocalTangent, viewsin);
	viewtancos = FixedMul (FocalTangent, viewcos);
}

//==========================================================================
//
// R_SetupFrame
//
//==========================================================================

void R_SetupFrame (player_t *player)
{				
	unsigned int newblend;
	
	camera = player->camera;	// [RH] Use camera instead of viewplayer

	if ((player->cheats & CF_CHASECAM) &&
		(camera->RenderStyle != STYLE_None) &&
		!(camera->renderflags & RF_INVISIBLE) &&
		camera->sprite != 0)	// Sprite 0 is always TNT1
	{
		// [RH] Use chasecam view
		P_AimCamera (camera);
		viewx = CameraX;
		viewy = CameraY;
		viewz = CameraZ;
		viewsector = CameraSector;
	}
	else
	{
		viewx = camera->x;
		viewy = camera->y;
		viewz = camera->player ? camera->player->viewz : camera->z;
		viewsector = camera->Sector;
	}

	// Keep the view within the sector's floor and ceiling
	fixed_t theZ = viewsector->ceilingplane.ZatPoint
		(camera->x, camera->y) - 4*FRACUNIT;
	if (viewz > theZ)
	{
		viewz = theZ;
	}

	theZ = camera->Sector->floorplane.ZatPoint
		(camera->x, camera->y) + 4*FRACUNIT;
	if (viewz < theZ)
	{
		viewz = theZ;
	}

	if (camera->player && camera->player->xviewshift && !paused)
	{
		int intensity = camera->player->xviewshift;
		viewx += ((P_Random(pr_torch) % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
		viewy += ((P_Random(pr_torch) % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
	}

	extralight = camera->player ? camera->player->extralight : 0;

	R_SetViewAngle (camera->angle + viewangleoffset);

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
			NormalLight.Maps = realcolormaps;;
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

	if (camera == player->mo && player->fixedcolormap)
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

	// [RH] freelook stuff
	{
		fixed_t dy = FixedMul (FocalLengthY, finetangent[(ANGLE_90-camera->pitch)>>ANGLETOFINESHIFT]);

		centeryfrac = (viewheight << (FRACBITS-1)) + dy;
		centery = centeryfrac >> FRACBITS;
		globaluclip = FixedDiv (-centeryfrac, InvZtoScale);
		globaldclip = FixedDiv ((viewheight<<FRACBITS)-centeryfrac, InvZtoScale);

		//centeryfrac &= 0xffff0000;
		int i = lastcenteryfrac - centeryfrac;
		if (i != 0)
		{
			int e;

			if (i & (FRACUNIT-1) == 0)	// Unlikely, but possible
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
		viewx = 2*v1->x - startx;
	}
	else if (ds->curline->linedef->dy == 0)
	{ // horizontal mirror
		viewy = 2*v1->y - starty;
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
	viewangle = 2*ds->curline->angle - startang;

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

	R_RenderBSPNode (numnodes - 1, -1);

	if (r_particles)
	{
		// [RH] add all the particles
		int i = ActiveParticles;
		while (i != -1)
		{
			R_ProjectParticle (Particles + i);
			i = Particles[i].next;
		}
	}

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

static void R_SetupBuffer ()
{
	static BYTE *lastbuff = NULL;

	int pitch = RenderTarget->GetPitch();
	BYTE *lineptr = RenderTarget->GetBuffer() + viewwindowy*pitch + viewwindowx;

	pitch <<= detailyshift;
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
		for (size_t i = 0; i < (size_t)viewheight; i++, lineptr += pitch)
		{
			ylookup[i] = lineptr;
		}
	}
}

//==========================================================================
//
// R_RenderPlayerView
//
//==========================================================================

CVAR (Int, restrict, -1, 0)
#ifdef _DEBUG
#include "c_dispatch.h"
CCMD (nodefun)
{
	if (argv.argc() != 2)
		return;
	int i = atoi (argv[1]);
	unsigned short *kids = nodes[i].children;
	Printf ("Left: %d (%c)   Right: %d (%c)\n",
		kids[0] & ~NF_SUBSECTOR, kids[0] & NF_SUBSECTOR ? 's' : 'n',
		kids[1] & ~NF_SUBSECTOR, kids[1] & NF_SUBSECTOR ? 's' : 'n');
}
#endif

void R_RenderPlayerView (player_t *player, void (*lengthyCallback)())
{
	WallCycles = PlaneCycles = MaskedCycles = WallScanCycles = 0;

	R_SetupBuffer ();
	R_SetupFrame (player);

	// Clear buffers.
	R_ClearClipSegs (0, viewwidth);
	R_ClearDrawSegs ();
	R_ClearPlanes (true);
	R_ClearSprites ();

	if (lengthyCallback != NULL)	lengthyCallback ();

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

	clock (WallCycles);
	// Never draw the player unless in chasecam mode
	if (camera->player && !(player->cheats & CF_CHASECAM))
	{
		WORD savedflags = camera->renderflags;
		camera->renderflags |= RF_INVISIBLE;
//memset (screen->GetBuffer(), 255, screen->GetWidth()*screen->GetHeight());
		R_RenderBSPNode (numnodes - 1, restrict);
		camera->renderflags = savedflags;
	}
	else
	{	// The head node is the last node output.
		// [[RH] Not that this tells me anything...]
		R_RenderBSPNode (numnodes - 1, -1);
	}
	unclock (WallCycles);

	if (r_particles)
	{
		// [RH] add all the particles
		int i = ActiveParticles;
		while (i != -1)
		{
			R_ProjectParticle (Particles + i);
			i = Particles[i].next;
		}
	}

	if (lengthyCallback != NULL)	lengthyCallback ();

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

	if (lengthyCallback != NULL)	lengthyCallback ();
	
	clock (MaskedCycles);
	R_DrawMasked ();
	unclock (MaskedCycles);

	if (lengthyCallback != NULL)	lengthyCallback ();
}

//==========================================================================
//
// R_RenderViewToCanvas
//
// Pre: Canvas is already locked.
//
//==========================================================================

void R_RenderViewToCanvas (player_t *player, DCanvas *canvas,
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

	R_RenderPlayerView (player, NULL);

	RenderTarget = screen;
	bRenderingToCanvas = false;
	R_SetDetail (saveddetail);
	R_ExecuteSetViewSize ();
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
