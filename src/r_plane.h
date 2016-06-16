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
//		Refresh, visplane stuff (floor, ceilings).
//
//-----------------------------------------------------------------------------


#ifndef __R_PLANE_H__
#define __R_PLANE_H__

#include <stddef.h>

class ASkyViewpoint;

//
// The infamous visplane
// 
struct visplane_s
{
	struct visplane_s *next;		// Next visplane in hash chain -- killough

	FDynamicColormap *colormap;		// [RH] Support multiple colormaps
	FSectorPortal *portal;			// [RH] Support sky boxes

	FTransform	xform;
	secplane_t	height;
	FTextureID	picnum;
	int			lightlevel;
	int			left, right;
	int			sky;

	// [RH] This set of variables copies information from the time when the
	// visplane is created. They are only used by stacks so that you can
	// have stacked sectors inside a skybox. If the visplane is not for a
	// stack, then they are unused.
	int			extralight;
	double		visibility;
	DVector3	viewpos;
	DAngle		viewangle;
	fixed_t		Alpha;
	bool		Additive;

	// kg3D - keep track of mirror and skybox owner
	int CurrentSkybox;
	int CurrentPortalUniq; // mirror counter, counts all of them
	int MirrorFlags; // this is not related to CurrentMirror

	unsigned short *bottom;			// [RH] bottom and top arrays are dynamically
	unsigned short pad;				//		allocated immediately after the
	unsigned short top[];			//		visplane.
};
typedef struct visplane_s visplane_t;



// Visplane related.
extern ptrdiff_t		lastopening;	// type short


typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t	floorfunc;
extern planefunction_t	ceilingfunc_t;

extern short			floorclip[MAXWIDTH];
extern short			ceilingclip[MAXWIDTH];

extern float			yslope[MAXHEIGHT];

void R_InitPlanes ();
void R_DeinitPlanes ();
void R_ClearPlanes (bool fullclear);

int R_DrawPlanes ();
void R_DrawPortals ();
void R_DrawSkyPlane (visplane_t *pl);
void R_DrawNormalPlane (visplane_t *pl, double xscale, double yscale, fixed_t alpha, bool additive, bool masked);
void R_DrawTiltedPlane (visplane_t *pl, double xscale, double yscale, fixed_t alpha, bool additive, bool masked);
void R_MapVisPlane (visplane_t *pl, void (*mapfunc)(int y, int x1));

visplane_t *R_FindPlane
( const secplane_t &height,
  FTextureID	picnum,
  int			lightlevel,
  double		alpha,
  bool			additive,
  const FTransform &xform,
  int			sky,
  FSectorPortal *portal);

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop);


// [RH] Added for multires support
bool R_PlaneInitData (void);


extern visplane_t*		floorplane;
extern visplane_t*		ceilingplane;

#endif // __R_PLANE_H__
