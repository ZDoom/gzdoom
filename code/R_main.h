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

#include "d_player.h"
#include "r_data.h"



//
// POV related.
//
extern fixed_t			viewcos;
extern fixed_t			viewsin;

extern int				viewwidth;
extern int				viewheight;
extern int				viewwindowx;
extern int				viewwindowy;



extern int				centerx;
extern "C" int			centery;

extern fixed_t			centerxfrac;
extern fixed_t			centeryfrac;
extern fixed_t			yaspectmul;

extern byte*			basecolormap;	// [RH] Colormap for sector currently being drawn

extern int				validcount;

extern int				linecount;
extern int				loopcount;

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//	and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS 			16
#define LIGHTSEGSHIFT			 4

#define MAXLIGHTSCALE			48
#define LIGHTSCALEMULBITS		 8	// [RH] for hires lighting fix
#define LIGHTSCALESHIFT 		(12+LIGHTSCALEMULBITS)
#define MAXLIGHTZ			   128
#define LIGHTZSHIFT 			20

// [RH] Changed from lighttable_t* to int.
extern int				scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern int				scalelightfixed[MAXLIGHTSCALE];
extern int				zlight[LIGHTLEVELS][MAXLIGHTZ];

extern int				extralight;
extern BOOL				foggy;
extern int				fixedlightlev;
extern lighttable_t*	fixedcolormap;

extern int				lightscalexmul;	// [RH] for hires lighting fix
extern int				lightscaleymul;


// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32


// [RH] New detail modes
extern "C" int			detailxshift;
extern "C" int			detailyshift;


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void 			(*colfunc) (void);
extern void 			(*basecolfunc) (void);
extern void 			(*fuzzcolfunc) (void);
extern void				(*lucentcolfunc) (void);
extern void				(*transcolfunc) (void);
extern void				(*tlatedlucentcolfunc) (void);
// No shadow effects on floors.
extern void 			(*spanfunc) (void);

// [RH] Function pointers for the horizontal column drawers.
extern void (*hcolfunc_pre) (void);
extern void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post4) (int sx, int yl, int yh);


//
// Utility functions.
int R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
int R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line);
angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
inline angle_t R_PointToAngle (fixed_t x, fixed_t y) { return R_PointToAngle2 (viewx, viewy, x, y); }
fixed_t R_ScaleFromGlobalAngle (angle_t visangle);
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
fixed_t R_PointToDist (fixed_t x, fixed_t y);
fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy);
void R_SetFOV (float fov);



//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderPlayerView (player_t *player);

// Called by startup code.
void R_Init (void);

// Called by M_Responder.
void R_SetViewSize (int blocks);

// [RH] Initialize multires stuff for renderer
void R_MultiresInit (void);

#endif // __R_MAIN_H__
