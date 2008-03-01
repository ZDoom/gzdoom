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

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32

#include "d_player.h"
#include "r_data.h"
#include "r_bsp.h"
#include "r_state.h"


//
// POV related.
//
extern DCanvas			*RenderTarget;
extern bool				bRenderingToCanvas;
extern fixed_t			viewcos;
extern fixed_t			viewsin;
extern fixed_t			viewtancos;
extern fixed_t			viewtansin;
extern fixed_t			FocalTangent;
extern fixed_t			FocalLengthX, FocalLengthY;
extern float			FocalLengthXfloat;
extern fixed_t			InvZtoScale;
extern int				WidescreenRatio;

extern angle_t			LocalViewAngle;			// [RH] Added to consoleplayer's angle
extern int				LocalViewPitch;			// [RH] Used directly instead of consoleplayer's pitch
extern bool				LocalKeyboardTurner;	// [RH] The local player used the keyboard to turn, so interpolate

extern float			WallTMapScale;
extern float			WallTMapScale2;

extern int				viewwidth;
extern int				viewheight;
extern int				viewwindowx;
extern int				viewwindowy;



extern "C" int			centerx;
extern "C" int			centery;

extern fixed_t			centerxfrac;
extern fixed_t			centeryfrac;
extern fixed_t			yaspectmul;
extern float			iyaspectmulfloat;

extern FDynamicColormap*basecolormap;	// [RH] Colormap for sector currently being drawn

extern int				validcount;

extern int				linecount;
extern int				loopcount;

extern int				r_Yaspect;

extern bool				r_dontmaplines;

//
// Lighting.
//
// [RH] This has changed significantly from Doom, which used lookup
// tables based on 1/z for walls and z for flats and only recognized
// 16 discrete light levels. The terminology I use is borrowed from Build.
//

#define INVERSECOLORMAP			32
#define GOLDCOLORMAP			33
#define REDCOLORMAP				34
#define GREENCOLORMAP			35

// The size of a single colormap, in bits
#define COLORMAPSHIFT			8

// Convert a light level into an unbounded colormap index (shade). Result is
// fixed point. Why the +12? I wish I knew, but experimentation indicates it
// is necessary in order to best reproduce Doom's original lighting.
#define LIGHT2SHADE(l)			((NUMCOLORMAPS*2*FRACUNIT)-(((l)+12)*FRACUNIT*NUMCOLORMAPS/128))

// Convert a shade and visibility to a clamped colormap index.
// Result is not fixed point.
#define GETPALOOKUP(vis,shade)	(clamp<int> (((shade)-(vis))>>FRACBITS, 0, NUMCOLORMAPS-1))

extern fixed_t			GlobVis;

void R_SetVisibility (float visibility);
float R_GetVisibility ();

extern fixed_t			r_BaseVisibility;
extern fixed_t			r_WallVisibility;
extern fixed_t			r_FloorVisibility;
extern float			r_TiltVisibility;
extern fixed_t			r_SpriteVisibility;
extern fixed_t			r_SkyVisibility;

extern fixed_t			r_TicFrac;
extern DWORD			r_FrameTime;
extern bool				r_NoInterpolate;

extern int				extralight, r_actualextralight;
extern bool				foggy;
extern int				fixedlightlev;
extern lighttable_t*	fixedcolormap;


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
extern void				(*transcolfunc) (void);
// No shadow effects on floors.
extern void 			(*spanfunc) (void);

// [RH] Function pointers for the horizontal column drawers.
extern void (*hcolfunc_pre) (void);
extern void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
extern void (STACK_ARGS *hcolfunc_post4) (int sx, int yl, int yh);


//
// Utility functions.

//==========================================================================
//
// R_PointOnSide
//
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front/on) or 1 (back).
//
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int R_PointOnSide (fixed_t x, fixed_t y, const node_t *node)
{
	return DMulScale32 (y-node->y, node->dx, node->x-x, node->dy) > 0;
}

extern fixed_t			viewx;
extern fixed_t			viewy;

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
inline angle_t R_PointToAngle (fixed_t x, fixed_t y) { return R_PointToAngle2 (viewx, viewy, x, y); }
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy);

void R_SetFOV (float fov);
float R_GetFOV ();
void R_InitTextureMapping ();

void R_SetViewAngle ();

//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderActorView (AActor *actor, bool dontmaplines = false);
void R_RefreshViewBorder ();
void R_SetupBuffer (bool inview);

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);

void R_ResetViewInterpolation ();

// Called by startup code.
int R_GuesstimateNumTextures ();
void R_Init (void);
void R_ExecuteSetViewSize (void);

// Called by M_Responder.
void R_SetViewSize (int blocks);

// [RH] Initialize multires stuff for renderer
void R_MultiresInit (void);

// BUILD stuff for interpolating between frames, but modified (rather a lot)
#define INTERPOLATION_BUCKETS 107

enum EInterpType
{
	INTERP_SectorFloor,		// Pass a sector_t *
	INTERP_SectorCeiling,	// Pass a sector_t *
	INTERP_Vertex,			// Pass a vertex_t *
	INTERP_FloorPanning,	// Pass a sector_t *
	INTERP_CeilingPanning,	// Pass a sector_t *
	INTERP_WallPanning,		// Pass a side_t *
};

struct FActiveInterpolation
{
	FActiveInterpolation *Next;
	void *Address;
	EInterpType Type;

	friend FArchive &operator << (FArchive &arc, FActiveInterpolation *&interp);

	static int CountInterpolations ();
	static int CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill);

private:
	fixed_t oldipos[2], bakipos[2];

	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);

	static size_t HashKey(EInterpType type, void *interptr);
	static FActiveInterpolation *FindInterpolation(EInterpType, void *interptr, FActiveInterpolation **&interp_p);

	friend void updateinterpolations();
	friend void setinterpolation(EInterpType, void *interptr);
	friend void stopinterpolation(EInterpType, void *interptr);
	friend void dointerpolations(fixed_t smoothratio);
	friend void restoreinterpolations();
	friend void clearinterpolations();
	friend void SerializeInterpolations(FArchive &arc);

	static FActiveInterpolation *curiposhash[INTERPOLATION_BUCKETS];
};

void updateinterpolations();
void setinterpolation(EInterpType, void *interptr);
void stopinterpolation(EInterpType, void *interptr);
void dointerpolations(fixed_t smoothratio);
void restoreinterpolations();
void clearinterpolations();
void SerializeInterpolations(FArchive &arc);

extern void R_FreePastViewers ();
extern void R_ClearPastViewer (AActor *actor);

extern int stacked_extralight;
extern float stacked_visibility;
extern fixed_t stacked_viewx, stacked_viewy, stacked_viewz;
extern angle_t stacked_angle;

extern void R_CopyStackedViewParameters();

#endif // __R_MAIN_H__
