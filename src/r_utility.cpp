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
#include "r_sky.h"
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
#include "r_renderer.h"
#include "r_data/colormaps.h"
#include "farchive.h"


// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool DrawFSHUD;		// [RH] Defined in d_main.cpp
EXTERN_CVAR (Bool, cl_capfps)

extern lighttable_t*	fixedcolormap;
extern FSpecialColormap*realfixedcolormap;

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

// PRIVATE DATA DECLARATIONS -----------------------------------------------
static TArray<InterpolationViewer> PastViewers;
static FRandom pr_torchflicker ("TorchFlicker");
static FRandom pr_hom;
static bool NoInterpolateView;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, r_deathcamera, false, CVAR_ARCHIVE)
CVAR (Int, r_clearbuffer, 0, 0)
CVAR (Bool, r_drawvoxels, true, 0)
CVAR (Bool, r_drawplayersprites, true, 0)	// [RH] Draw player sprites?
CUSTOM_CVAR(Float, r_quakeintensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

DCanvas			*RenderTarget;		// [RH] canvas to render to

int 			viewwindowx;
int 			viewwindowy;

fixed_t 		viewx;
fixed_t 		viewy;
fixed_t 		viewz;
int				viewpitch;

extern "C" 
{
		int 	viewwidth;
		int 	viewheight;
		int		centerx;
		int		centery;
		int		centerxwide;
}

int				otic;

angle_t 		viewangle;
sector_t		*viewsector;

fixed_t 		viewcos, viewtancos;
fixed_t 		viewsin, viewtansin;

AActor			*camera;	// [RH] camera to draw from. doesn't have to be a player

fixed_t			r_TicFrac;			// [RH] Fractional tic to render
DWORD			r_FrameTime;		// [RH] Time this frame started drawing (in ms)
bool			r_NoInterpolate;
bool			r_showviewer;

angle_t			LocalViewAngle;
int				LocalViewPitch;
bool			LocalKeyboardTurner;

float			LastFOV;
int				WidescreenRatio;
int				setblocks;
int				extralight;
bool			setsizeneeded;
fixed_t			FocalTangent;

unsigned int	R_OldBlend = ~0;
int 			validcount = 1; 	// increment every time a check is made
int				FieldOfView = 2048;		// Fineangles in the SCREENWIDTH wide window

FCanvasTextureInfo *FCanvasTextureInfo::List;


// CODE --------------------------------------------------------------------
static void R_Shutdown ();

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
	int trueratio;

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
	
	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	centery = viewheight/2;
	centerx = viewwidth/2;
	if (WidescreenRatio & 4)
	{
		centerxwide = centerx;
	}
	else
	{
		centerxwide = centerx * BaseRatioSizes[WidescreenRatio][3] / 48;
	}


	int fov = FieldOfView;

	// For widescreen displays, increase the FOV so that the middle part of the
	// screen that would be visible on a 4:3 display has the requested FOV.
	if (centerxwide != centerx)
	{ // centerxwide is what centerx would be if the display was not widescreen
		fov = int(atan(double(centerx)*tan(double(fov)*M_PI/(FINEANGLES))/double(centerxwide))*(FINEANGLES)/M_PI);
		if (fov > 170*FINEANGLES/360)
			fov = 170*FINEANGLES/360;
	}

	FocalTangent = finetangent[FINEANGLES/4+fov/2];
	Renderer->SetWindow(windowSize, fullWidth, fullHeight, stHeight, trueratio);
}

//==========================================================================
//
// R_ExecuteSetViewSize
//
//==========================================================================

void R_ExecuteSetViewSize ()
{
	setsizeneeded = false;
	V_SetBorderNeedRefresh();

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
// R_Init
//
//==========================================================================

void R_Init ()
{
	atterm (R_Shutdown);

	StartScreen->Progress();
	V_InitFonts();
	StartScreen->Progress();
	// Colormap init moved back to InitPalette()
	//R_InitColormaps ();
	//StartScreen->Progress();

	R_InitPointToAngle ();
	R_InitTables ();
	R_InitTranslationTables ();
	R_SetViewSize (screenblocks);
	Renderer->Init();
}

//==========================================================================
//
// R_Shutdown
//
//==========================================================================

static void R_Shutdown ()
{
	R_DeinitTranslationTables();
	R_DeinitColormaps ();
	FCanvasTextureInfo::EmptyList();
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
		!(player->cheats & CF_INTERPVIEW) &&
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

		fixed_t delta = player->centering ? 0 : -(signed)(LocalViewPitch & 0xFFFF0000);

		viewpitch = iview->nviewpitch;
		if (delta > 0)
		{
			// Avoid overflowing viewpitch (can happen when a netgame is stalled)
			if (viewpitch + delta <= viewpitch)
			{
				viewpitch = player->MaxPitch;
			}
			else
			{
				viewpitch = MIN(viewpitch + delta, player->MaxPitch);
			}
		}
		else if (delta < 0)
		{
			// Avoid overflowing viewpitch (can happen when a netgame is stalled)
			if (viewpitch + delta >= viewpitch)
			{
				viewpitch = player->MinPitch;
			}
			else
			{
				viewpitch = MAX(viewpitch + delta, player->MinPitch);
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
	InterpolationViewer iview = { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
		((player->cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)))
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

	iview->nviewangle = camera->angle;
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
			fixed_t quakefactor = FLOAT2FIXED(r_quakeintensity);

			viewx += quakefactor * ((pr_torchflicker() % (intensity<<2)) - (intensity<<1));
			viewy += quakefactor * ((pr_torchflicker() % (intensity<<2)) - (intensity<<1));
		}
	}

	extralight = camera->player ? camera->player->extralight : 0;

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend
	newblend = 0;

	TArray<lightlist_t> &lightlist = viewsector->e->XFloor.lightlist;
	if (lightlist.Size() > 0)
	{
		for(unsigned int i = 0; i < lightlist.Size(); i++)
		{
			secplane_t *plane;
			int viewside;
			plane = (i < lightlist.Size()-1) ? &lightlist[i+1].plane : &viewsector->floorplane;
			viewside = plane->PointOnSide(viewx, viewy, viewz);
			// Reverse the direction of the test if the plane was downward facing.
			// We want to know if the view is above it, whatever its orientation may be.
			if (plane->c < 0)
				viewside = -viewside;
			if (viewside > 0)
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
			newblend = s->floorplane.PointOnSide(viewx, viewy, viewz) < 0
				? s->bottommap
				: s->ceilingplane.PointOnSide(viewx, viewy, viewz) < 0
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

	Renderer->CopyStackedViewParameters();
	Renderer->SetupFrame(player);

	validcount++;

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
		Renderer->ClearBuffer(color);
	}
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

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	unsigned char *savecolormap = fixedcolormap;
	FSpecialColormap *savecm = realfixedcolormap;

	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Viewpoint != NULL && probe->Texture->bNeedsUpdate)
		{
			Renderer->RenderTextureView(probe->Texture, probe->Viewpoint, probe->FOV);
		}
	}

	fixedcolormap = savecolormap;
	realfixedcolormap = savecm;
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
		probe->Texture->Unload();
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

