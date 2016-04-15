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
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_MAIN_H__
#define __R_MAIN_H__

#include "r_utility.h"
#include "d_player.h"
#include "v_palette.h"
#include "r_data/colormaps.h"


typedef BYTE lighttable_t;	// This could be wider for >8 bit display.

//
// POV related.
//
extern bool				bRenderingToCanvas;
extern double			ViewCos;
extern double			ViewSin;
extern fixed_t			viewingrangerecip;
extern double			FocalLengthX, FocalLengthY;
extern double			InvZtoScale;

extern double			WallTMapScale2;

extern int				viewwindowx;
extern int				viewwindowy;

extern double			CenterX;
extern double			CenterY;
extern double			YaspectMul;
extern double			IYaspectMul;

extern FDynamicColormap*basecolormap;	// [RH] Colormap for sector currently being drawn

extern int				linecount;
extern int				loopcount;

extern bool				r_dontmaplines;

//
// Lighting.
//
// [RH] This has changed significantly from Doom, which used lookup
// tables based on 1/z for walls and z for flats and only recognized
// 16 discrete light levels. The terminology I use is borrowed from Build.
//

// The size of a single colormap, in bits
#define COLORMAPSHIFT			8

// Convert a light level into an unbounded colormap index (shade). Result is
// fixed point. Why the +12? I wish I knew, but experimentation indicates it
// is necessary in order to best reproduce Doom's original lighting.
#define LIGHT2SHADE(l)			((NUMCOLORMAPS*2*FRACUNIT)-(((l)+12)*(FRACUNIT*NUMCOLORMAPS/128)))

// MAXLIGHTSCALE from original DOOM, divided by 2.
#define MAXLIGHTVIS				(24.0)

// Convert a shade and visibility to a clamped colormap index.
// Result is not fixed point.
// Change R_CalcTiltedLighting() when this changes.
#define GETPALOOKUP(vis,shade)	(clamp<int> (((shade)-FLOAT2FIXED(MIN(MAXLIGHTVIS,double(vis))))>>FRACBITS, 0, NUMCOLORMAPS-1))

extern double			GlobVis;

void R_SetVisibility(double visibility);
double R_GetVisibility();

extern double			r_BaseVisibility;
extern double			r_WallVisibility;
extern double			r_FloorVisibility;
extern float			r_TiltVisibility;
extern double			r_SpriteVisibility;

extern int				r_actualextralight;
extern bool				foggy;
extern int				fixedlightlev;
extern lighttable_t*	fixedcolormap;
extern FSpecialColormap*realfixedcolormap;


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void 			(*colfunc) (void);
extern void 			(*basecolfunc) (void);
extern void 			(*fuzzcolfunc) (void);
extern void				(*transcolfunc) (void);
// No shadow effects on floors.
extern void 			(*spanfunc) (void);

// [RH] Function pointers for the horizontal column drawers.
extern void (*hcolfunc_pre) (void);
extern void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post4) (int sx, int yl, int yh);


void R_InitTextureMapping ();


//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderActorView (AActor *actor, bool dontmaplines = false);
void R_SetupBuffer ();

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);

// [RH] Initialize multires stuff for renderer
void R_MultiresInit (void);


extern int stacked_extralight;
extern double stacked_visibility;
extern DVector3 stacked_viewpos;
extern DAngle stacked_angle;

extern void R_CopyStackedViewParameters();


#endif // __R_MAIN_H__
