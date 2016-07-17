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
//		Here is a core component: drawing the floors and ceilings,
//		 while maintaining a per column clipping list only.
//		Moreover, the sky areas have to be determined.
//
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// Lee Killough
//
// [RH] Further modified to significantly increase accuracy and add slopes.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#include "stats.h"

#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

//EXTERN_CVAR (Int, tx)
//EXTERN_CVAR (Int, ty)

extern subsector_t *InSubsector;

static void R_DrawSkyStriped (visplane_t *pl);

planefunction_t 		floorfunc;
planefunction_t 		ceilingfunc;

// Here comes the obnoxious "visplane".
#define MAXVISPLANES 128    /* must be a power of 2 */

// Avoid infinite recursion with stacked sectors by limiting them.
#define MAX_SKYBOX_PLANES 1000

// [RH] Allocate one extra for sky box planes.
static visplane_t		*visplanes[MAXVISPLANES+1];	// killough
static visplane_t		*freetail;					// killough
static visplane_t		**freehead = &freetail;		// killough

visplane_t 				*floorplane;
visplane_t 				*ceilingplane;

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  ((unsigned)((picnum)*3+(lightlevel)+(FLOAT2FIXED((height).fD()))*7) & (MAXVISPLANES-1))

// These are copies of the main parameters used when drawing stacked sectors.
// When you change the main parameters, you should copy them here too *unless*
// you are changing them to draw a stacked sector. Otherwise, stacked sectors
// won't draw in skyboxes properly.
int stacked_extralight;
double stacked_visibility;
DVector3 stacked_viewpos;
DAngle stacked_angle;


//
// opening
//

size_t					maxopenings;
short					*openings;
ptrdiff_t				lastopening;

//
// Clip values are the solid pixel bounding the range.
//	floorclip starts out SCREENHEIGHT and is just outside the range
//	ceilingclip starts out 0 and is just inside the range
//
short					floorclip[MAXWIDTH];
short					ceilingclip[MAXWIDTH];

//
// texture mapping
//

static double			planeheight;

extern "C" {
//
// spanend holds the end of a plane span in each screen row
//
short					spanend[MAXHEIGHT];
BYTE					*tiltlighting[MAXWIDTH];

int						planeshade;
FVector3				plane_sz, plane_su, plane_sv;
float					planelightfloat;
bool					plane_shade;
fixed_t					pviewx, pviewy;

void R_DrawTiltedPlane_ASM (int y, int x1);
}

float 					yslope[MAXHEIGHT];
static fixed_t			xscale, yscale;
static double			xstepscale, ystepscale;
static double			basexfrac, baseyfrac;

#ifdef X86_ASM
extern "C" void R_SetSpanSource_ASM (const BYTE *flat);
extern "C" void R_SetSpanSize_ASM (int xbits, int ybits);
extern "C" void R_SetSpanColormap_ASM (BYTE *colormap);
extern "C" void R_SetTiltedSpanSource_ASM (const BYTE *flat);
extern "C" BYTE *ds_curcolormap, *ds_cursource, *ds_curtiltedsource;
#endif
void					R_DrawSinglePlane (visplane_t *, fixed_t alpha, bool additive, bool masked);

//==========================================================================
//
// R_InitPlanes
//
// Called at game startup.
//
//==========================================================================

void R_InitPlanes ()
{
}

//==========================================================================
//
// R_DeinitPlanes
//
//==========================================================================

void R_DeinitPlanes ()
{
	fakeActive = 0;

	// do not use R_ClearPlanes because at this point the screen pointer is no longer valid.
	for (int i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
	{
		for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
		{
			freehead = &(*freehead)->next;
		}
	}
	for (visplane_t *pl = freetail; pl != NULL; )
	{
		visplane_t *next = pl->next;
		free (pl);
		pl = next;
	}
}

//==========================================================================
//
// R_MapPlane
//
// Globals used: planeheight, ds_source, basexscale, baseyscale,
// pviewx, pviewy, xoffs, yoffs, basecolormap, xscale, yscale.
//
//==========================================================================

void R_MapPlane (int y, int x1)
{
	int x2 = spanend[y];
	double distance;

#ifdef RANGECHECK
	if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>=(unsigned)viewheight)
	{
		I_FatalError ("R_MapPlane: %i, %i at %i", x1, x2, y);
	}
#endif

	// [RH] Notice that I dumped the caching scheme used by Doom.
	// It did not offer any appreciable speedup.

	distance = planeheight * yslope[y];

	ds_xstep = xs_ToFixed(32-ds_xbits, distance * xstepscale);
	ds_ystep = xs_ToFixed(32-ds_ybits, distance * ystepscale);
	ds_xfrac = xs_ToFixed(32-ds_xbits, distance * basexfrac) + pviewx;
	ds_yfrac = xs_ToFixed(32-ds_ybits, distance * baseyfrac) + pviewy;

	if (plane_shade)
	{
		// Determine lighting based on the span's distance from the viewer.
		ds_colormap = basecolormap->Maps + (GETPALOOKUP (
			GlobVis * fabs(CenterY - y), planeshade) << COLORMAPSHIFT);
	}

#ifdef X86_ASM
	if (ds_colormap != ds_curcolormap)
		R_SetSpanColormap_ASM (ds_colormap);
#endif

	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	spanfunc ();
}

//==========================================================================
//
// R_CalcTiltedLighting
//
// Calculates the lighting for one row of a tilted plane. If the definition
// of GETPALOOKUP changes, this needs to change, too.
//
//==========================================================================

extern "C" {
void R_CalcTiltedLighting (double lval, double lend, int width)
{
	double lstep;
	BYTE *lightfiller;
	BYTE *basecolormapdata = basecolormap->Maps;
	int i = 0;

	if (width == 0 || lval == lend)
	{ // Constant lighting
		lightfiller = basecolormapdata + (GETPALOOKUP(lval, planeshade) << COLORMAPSHIFT);
	}
	else
	{
		lstep = (lend - lval) / width;
		if (lval >= MAXLIGHTVIS)
		{ // lval starts "too bright".
			lightfiller = basecolormapdata + (GETPALOOKUP(lval, planeshade) << COLORMAPSHIFT);
			for (; i <= width && lval >= MAXLIGHTVIS; ++i)
			{
				tiltlighting[i] = lightfiller;
				lval += lstep;
			}
		}
		if (lend >= MAXLIGHTVIS)
		{ // lend ends "too bright".
			lightfiller = basecolormapdata + (GETPALOOKUP(lend, planeshade) << COLORMAPSHIFT);
			for (; width > i && lend >= MAXLIGHTVIS; --width)
			{
				tiltlighting[width] = lightfiller;
				lend -= lstep;
			}
		}
		if (width > 0)
		{
			lval = FIXED2DBL(planeshade) - lval;
			lend = FIXED2DBL(planeshade) - lend;
			lstep = (lend - lval) / width;
			if (lstep < 0)
			{ // Going from dark to light
				if (lval < 1.)
				{ // All bright
					lightfiller = basecolormapdata;
				}
				else
				{
					if (lval >= NUMCOLORMAPS)
					{ // Starts beyond the dark end
						BYTE *clight = basecolormapdata + ((NUMCOLORMAPS-1) << COLORMAPSHIFT);
						while (lval >= NUMCOLORMAPS && i <= width)
						{
							tiltlighting[i++] = clight;
							lval += lstep;
						}
						if (i > width)
							return;
					}
					while (i <= width && lval >= 0)
					{
						tiltlighting[i++] = basecolormapdata + (xs_ToInt(lval) << COLORMAPSHIFT);
						lval += lstep;
					}
					lightfiller = basecolormapdata;
				}
			}
			else
			{ // Going from light to dark
				if (lval >= (NUMCOLORMAPS-1))
				{ // All dark
					lightfiller = basecolormapdata + ((NUMCOLORMAPS-1) << COLORMAPSHIFT);
				}
				else
				{
					while (lval < 0 && i <= width)
					{
						tiltlighting[i++] = basecolormapdata;
						lval += lstep;
					}
					if (i > width)
						return;
					while (i <= width && lval < (NUMCOLORMAPS-1))
					{
						tiltlighting[i++] = basecolormapdata + (xs_ToInt(lval) << COLORMAPSHIFT);
						lval += lstep;
					}
					lightfiller = basecolormapdata + ((NUMCOLORMAPS-1) << COLORMAPSHIFT);
				}
			}
		}
	}
	for (; i <= width; i++)
	{
		tiltlighting[i] = lightfiller;
	}
}
}	// extern "C"

//==========================================================================
//
// R_MapTiltedPlane
//
//==========================================================================

void R_MapTiltedPlane (int y, int x1)
{
	int x2 = spanend[y];
	int width = x2 - x1;
	double iz, uz, vz;
	BYTE *fb;
	DWORD u, v;
	int i;

	iz = plane_sz[2] + plane_sz[1]*(centery-y) + plane_sz[0]*(x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	if (plane_shade)
	{
		uz = (iz + plane_sz[0]*width) * planelightfloat;
		vz = iz * planelightfloat;
		R_CalcTiltedLighting (vz, uz, width);
	}

	uz = plane_su[2] + plane_su[1]*(centery-y) + plane_su[0]*(x1-centerx);
	vz = plane_sv[2] + plane_sv[1]*(centery-y) + plane_sv[0]*(x1-centerx);

	fb = ylookup[y] + x1 + dc_destorg;

	BYTE vshift = 32 - ds_ybits;
	BYTE ushift = vshift - ds_xbits;
	int umask = ((1 << ds_xbits) - 1) << ds_ybits;

#if 0		// The "perfect" reference version of this routine. Pretty slow.
			// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;

		u = SQWORD(uz*z) + pviewx;
		v = SQWORD(vz*z) + pviewy;
		ds_colormap = tiltlighting[i];
		fb[i++] = ds_colormap[ds_source[(v >> vshift) | ((u >> ushift) & umask)]];
		iz += plane_sz[0];
		uz += plane_su[0];
		vz += plane_sv[0];
	} while (--width >= 0);
#else
//#define SPANSIZE 32
//#define INVSPAN 0.03125f
//#define SPANSIZE 8
//#define INVSPAN 0.125f
#define SPANSIZE 16
#define INVSPAN	0.0625f

	double startz = 1.f/iz;
	double startu = uz*startz;
	double startv = vz*startz;
	double izstep, uzstep, vzstep;

	izstep = plane_sz[0] * SPANSIZE;
	uzstep = plane_su[0] * SPANSIZE;
	vzstep = plane_sv[0] * SPANSIZE;
	x1 = 0;
	width++;

	while (width >= SPANSIZE)
	{
		iz += izstep;
		uz += uzstep;
		vz += vzstep;

		double endz = 1.f/iz;
		double endu = uz*endz;
		double endv = vz*endz;
		DWORD stepu = SQWORD((endu - startu) * INVSPAN);
		DWORD stepv = SQWORD((endv - startv) * INVSPAN);
		u = SQWORD(startu) + pviewx;
		v = SQWORD(startv) + pviewy;

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			fb[x1] = *(tiltlighting[x1] + ds_source[(v >> vshift) | ((u >> ushift) & umask)]);
			x1++;
			u += stepu;
			v += stepv;
		}
		startu = endu;
		startv = endv;
		width -= SPANSIZE;
	}
	if (width > 0)
	{
		if (width == 1)
		{
			u = SQWORD(startu);
			v = SQWORD(startv);
			fb[x1] = *(tiltlighting[x1] + ds_source[(v >> vshift) | ((u >> ushift) & umask)]);
		}
		else
		{
			double left = width;
			iz += plane_sz[0] * left;
			uz += plane_su[0] * left;
			vz += plane_sv[0] * left;

			double endz = 1.f/iz;
			double endu = uz*endz;
			double endv = vz*endz;
			left = 1.f/left;
			DWORD stepu = SQWORD((endu - startu) * left);
			DWORD stepv = SQWORD((endv - startv) * left);
			u = SQWORD(startu) + pviewx;
			v = SQWORD(startv) + pviewy;

			for (; width != 0; width--)
			{
				fb[x1] = *(tiltlighting[x1] + ds_source[(v >> vshift) | ((u >> ushift) & umask)]);
				x1++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

//==========================================================================
//
// R_MapColoredPlane
//
//==========================================================================

void R_MapColoredPlane (int y, int x1)
{
	memset (ylookup[y] + x1 + dc_destorg, ds_color, spanend[y] - x1 + 1);
}

//==========================================================================
//
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes (bool fullclear)
{
	int i;

	// Don't clear fake planes if not doing a full clear.
	if (!fullclear)
	{
		for (i = 0; i <= MAXVISPLANES-1; i++)	// new code -- killough
		{
			for (visplane_t **probe = &visplanes[i]; *probe != NULL; )
			{
				if ((*probe)->sky < 0)
				{ // fake: move past it
					probe = &(*probe)->next;
				}
				else
				{ // not fake: move to freelist
					visplane_t *vis = *probe;
					*freehead = vis;
					*probe = vis->next;
					vis->next = NULL;
					freehead = &vis->next;
				}
			}
		}
	}
	else
	{
		for (i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
		{
			for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			{
				freehead = &(*freehead)->next;
			}
		}

		// opening / clipping determination
		clearbufshort (floorclip, viewwidth, viewheight);
		// [RH] clip ceiling to console bottom
		clearbufshort (ceilingclip, viewwidth,
			!screen->Accel2D && ConBottom > viewwindowy && !bRenderingToCanvas
			? (ConBottom - viewwindowy) : 0);

		lastopening = 0;
	}
}

//==========================================================================
//
// new_visplane
//
// New function, by Lee Killough
// [RH] top and bottom buffers get allocated immediately after the visplane.
//
//==========================================================================

static visplane_t *new_visplane (unsigned hash)
{
	visplane_t *check = freetail;

	if (check == NULL)
	{
		check = (visplane_t *)M_Malloc (sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH*2));
		memset(check, 0, sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH*2));
		check->bottom = check->top + MAXWIDTH+2;
	}
	else if (NULL == (freetail = freetail->next))
	{
		freehead = &freetail;
	}

	check->next = visplanes[hash];
	visplanes[hash] = check;
	return check;
}


//==========================================================================
//
// R_FindPlane
//
// killough 2/28/98: Add offsets
//==========================================================================

visplane_t *R_FindPlane (const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive,
						const FTransform &xxform,
						 int sky, FSectorPortal *portal)
{
	secplane_t plane;
	visplane_t *check;
	unsigned hash;						// killough
	bool isskybox;
	const FTransform *xform = &xxform;
	fixed_t alpha = FLOAT2FIXED(Alpha);
	//angle_t angle = (xform.Angle + xform.baseAngle).BAMs();

	if (picnum == skyflatnum)	// killough 10/98
	{ // most skies map together
		FTransform nulltransform;
		lightlevel = 0;
		xform = &nulltransform;
		nulltransform.xOffs = nulltransform.yOffs = nulltransform.baseyOffs = 0;
		nulltransform.xScale = nulltransform.yScale = 1;
		nulltransform.Angle = nulltransform.baseAngle = 0.0;
		additive = false;
		// [RH] Map floor skies and ceiling skies to separate visplanes. This isn't
		// always necessary, but it is needed if a floor and ceiling sky are in the
		// same column but separated by a wall. If they both try to reside in the
		// same visplane, then only the floor sky will be drawn.
		plane.set(0., 0., height.fC(), 0.);
		isskybox = portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX);
	}
	else if (portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX))
	{
		plane = height;
		isskybox = true;
	}
	else
	{
		plane = height;
		isskybox = false;
		// kg3D - hack, store alpha in sky
		// i know there is ->alpha, but this also allows to identify fake plane
		// and ->alpha is for stacked sectors
		if (fake3D & (FAKE3D_FAKEFLOOR|FAKE3D_FAKECEILING)) sky = 0x80000000 | fakeAlpha;
		else sky = 0;	// not skyflatnum so it can't be a sky
		portal = NULL;
		alpha = OPAQUE;
	}

	// New visplane algorithm uses hash table -- killough
	hash = isskybox ? MAXVISPLANES : visplane_hash (picnum.GetIndex(), lightlevel, height);

	for (check = visplanes[hash]; check; check = check->next)	// killough
	{
		if (isskybox)
		{
			if (portal == check->portal && plane == check->height)
			{
				if (portal->mType != PORTS_SKYVIEWPOINT)
				{ // This skybox is really a stacked sector, so we need to
				  // check even more.
					if (check->extralight == stacked_extralight &&
						check->visibility == stacked_visibility &&
						check->viewpos == stacked_viewpos &&
						(
							// headache inducing logic... :(
							(portal->mType != PORTS_STACKEDSECTORTHING) ||
							(
								check->Alpha == alpha &&
								check->Additive == additive &&
								(alpha == 0 ||	// if alpha is > 0 everything needs to be checked
									(plane == check->height &&
									 picnum == check->picnum &&
									 lightlevel == check->lightlevel &&
									 basecolormap == check->colormap &&	// [RH] Add more checks
									 *xform == check->xform
									)
								) &&
								check->viewangle == stacked_angle
							)
						)
					   )
					{
						return check;
					}
				}
				else
				{
					return check;
				}
			}
		}
		else
		if (plane == check->height &&
			picnum == check->picnum &&
			lightlevel == check->lightlevel &&
			basecolormap == check->colormap &&	// [RH] Add more checks
			*xform == check->xform &&
			sky == check->sky &&
			CurrentPortalUniq == check->CurrentPortalUniq &&
			MirrorFlags == check->MirrorFlags &&
			CurrentSkybox == check->CurrentSkybox &&
			ViewPos == check->viewpos
			)
		{
		  return check;
		}
	}

	check = new_visplane (hash);		// killough

	check->height = plane;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xform = *xform;
	check->colormap = basecolormap;		// [RH] Save colormap
	check->sky = sky;
	check->portal = portal;
	check->left = viewwidth;			// Was SCREENWIDTH -- killough 11/98
	check->right = 0;
	check->extralight = stacked_extralight;
	check->visibility = stacked_visibility;
	check->viewpos = stacked_viewpos;
	check->viewangle = stacked_angle;
	check->Alpha = alpha;
	check->Additive = additive;
	check->CurrentPortalUniq = CurrentPortalUniq;
	check->MirrorFlags = MirrorFlags;
	check->CurrentSkybox = CurrentSkybox;

	clearbufshort (check->top, viewwidth, 0x7fff);

	return check;
}

//==========================================================================
//
// R_CheckPlane
//
//==========================================================================

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop)
{
	int intrl, intrh;
	int unionl, unionh;
	int x;

	assert (start >= 0 && start < viewwidth);
	assert (stop > start && stop <= viewwidth);

	if (start < pl->left)
	{
		intrl = pl->left;
		unionl = start;
	}
	else
	{
		unionl = pl->left;
		intrl = start;
	}
		
	if (stop > pl->right)
	{
		intrh = pl->right;
		unionh = stop;
	}
	else
	{
		unionh = pl->right;
		intrh = stop;
	}

	for (x = intrl; x < intrh && pl->top[x] == 0x7fff; x++)
		;

	if (x >= intrh)
	{
		// use the same visplane
		pl->left = unionl;
		pl->right = unionh;
	}
	else
	{
		// make a new visplane
		unsigned hash;

		if (pl->portal != NULL && !(pl->portal->mFlags & PORTSF_INSKYBOX) && viewactive)
		{
			hash = MAXVISPLANES;
		}
		else
		{
			hash = visplane_hash (pl->picnum.GetIndex(), pl->lightlevel, pl->height);
		}
		visplane_t *new_pl = new_visplane (hash);

		new_pl->height = pl->height;
		new_pl->picnum = pl->picnum;
		new_pl->lightlevel = pl->lightlevel;
		new_pl->xform = pl->xform;
		new_pl->colormap = pl->colormap;
		new_pl->portal = pl->portal;
		new_pl->extralight = pl->extralight;
		new_pl->visibility = pl->visibility;
		new_pl->viewpos = pl->viewpos;
		new_pl->viewangle = pl->viewangle;
		new_pl->sky = pl->sky;
		new_pl->Alpha = pl->Alpha;
		new_pl->Additive = pl->Additive;
		new_pl->CurrentPortalUniq = pl->CurrentPortalUniq;
		new_pl->MirrorFlags = pl->MirrorFlags;
		new_pl->CurrentSkybox = pl->CurrentSkybox;
		pl = new_pl;
		pl->left = start;
		pl->right = stop;
		clearbufshort (pl->top, viewwidth, 0x7fff);
	}
	return pl;
}


//==========================================================================
//
// R_MakeSpans
//
//
//==========================================================================

inline void R_MakeSpans (int x, int t1, int b1, int t2, int b2, void (*mapfunc)(int y, int x1))
{
}

//==========================================================================
//
// R_DrawSky
//
// Can handle overlapped skies. Note that the front sky is *not* masked in
// in the normal convention for patches, but uses color 0 as a transparent
// color instead.
//
// Note that since ZDoom now uses color 0 as transparent for other purposes,
// you can use normal texture transparency, so the distinction isn't so
// important anymore, but you should still be aware of it.
//
//==========================================================================

static FTexture *frontskytex, *backskytex;
static angle_t skyflip;
static int frontpos, backpos;
static double frontyScale;
static fixed_t frontcyl, backcyl;
static double skymid;
static angle_t skyangle;
static double frontiScale;

extern float swall[MAXWIDTH];
extern fixed_t lwall[MAXWIDTH];
extern fixed_t rw_offset;
extern FTexture *rw_pic;

// Allow for layer skies up to 512 pixels tall. This is overkill,
// since the most anyone can ever see of the sky is 500 pixels.
// We need 4 skybufs because wallscan can draw up to 4 columns at a time.
static BYTE skybuf[4][512];
static DWORD lastskycol[4];
static int skycolplace;

// Get a column of sky when there is only one sky texture.
static const BYTE *R_GetOneSkyColumn (FTexture *fronttex, int x)
{
	angle_t column = (skyangle + xtoviewangle[x]) ^ skyflip;
	return fronttex->GetColumn((UMulScale16(column, frontcyl) + frontpos) >> FRACBITS, NULL);
}

// Get a column of sky when there are two overlapping sky textures
static const BYTE *R_GetTwoSkyColumns (FTexture *fronttex, int x)
{
	DWORD ang = (skyangle + xtoviewangle[x]) ^ skyflip;
	DWORD angle1 = (DWORD)((UMulScale16(ang, frontcyl) + frontpos) >> FRACBITS);
	DWORD angle2 = (DWORD)((UMulScale16(ang, backcyl) + backpos) >> FRACBITS);

	// Check if this column has already been built. If so, there's
	// no reason to waste time building it again.
	DWORD skycol = (angle1 << 16) | angle2;
	int i;

	for (i = 0; i < 4; ++i)
	{
		if (lastskycol[i] == skycol)
		{
			return skybuf[i];
		}
	}

	lastskycol[skycolplace] = skycol;
	BYTE *composite = skybuf[skycolplace];
	skycolplace = (skycolplace + 1) & 3;

	// The ordering of the following code has been tuned to allow VC++ to optimize
	// it well. In particular, this arrangement lets it keep count in a register
	// instead of on the stack.
	const BYTE *front = fronttex->GetColumn (angle1, NULL);
	const BYTE *back = backskytex->GetColumn (angle2, NULL);

	int count = MIN<int> (512, MIN (backskytex->GetHeight(), fronttex->GetHeight()));
	i = 0;
	do
	{
		if (front[i])
		{
			composite[i] = front[i];
		}
		else
		{
			composite[i] = back[i];
		}
	} while (++i, --count);
	return composite;
}

static void R_DrawSky (visplane_t *pl)
{
	int x;
	float swal;

 	if (pl->left >= pl->right)
		return;

	swal = skyiscale;
	for (x = pl->left; x < pl->right; ++x)
	{
		swall[x] = swal;
	}

	if (MirrorFlags & RF_XFLIP)
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			lwall[x] = (viewwidth - x) << FRACBITS;
		}
	}
	else
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			lwall[x] = x << FRACBITS;
		}
	}

	for (x = 0; x < 4; ++x)
	{
		lastskycol[x] = 0xffffffff;
	}

	rw_pic = frontskytex;
	rw_offset = 0;

	frontyScale = rw_pic->Scale.Y;
	dc_texturemid = skymid * frontyScale;

	if (1 << frontskytex->HeightBits == frontskytex->GetHeight())
	{ // The texture tiles nicely
		for (x = 0; x < 4; ++x)
		{
			lastskycol[x] = 0xffffffff;
		}
		wallscan (pl->left, pl->right, (short *)pl->top, (short *)pl->bottom, swall, lwall,
			frontyScale, backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
	}
	else
	{ // The texture does not tile nicely
		frontyScale *= skyscale;
		frontiScale = 1 / frontyScale;
		R_DrawSkyStriped (pl);
	}
}

static void R_DrawSkyStriped (visplane_t *pl)
{
	short drawheight = short(frontskytex->GetHeight() * frontyScale);
	double topfrac;
	double iscale = frontiScale;
	short top[MAXWIDTH], bot[MAXWIDTH];
	short yl, yh;
	int x;

	topfrac = fmod(skymid + iscale * (1 - CenterY), frontskytex->GetHeight());
	if (topfrac < 0) topfrac += frontskytex->GetHeight();
	yl = 0;
	yh = short((frontskytex->GetHeight() - topfrac) * frontyScale);
	dc_texturemid = topfrac - iscale * (1 - CenterY);

	while (yl < viewheight)
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			top[x] = MAX (yl, (short)pl->top[x]);
			bot[x] = MIN (yh, (short)pl->bottom[x]);
		}
		for (x = 0; x < 4; ++x)
		{
			lastskycol[x] = 0xffffffff;
		}
		wallscan (pl->left, pl->right, top, bot, swall, lwall, rw_pic->Scale.Y,
			backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
		yl = yh;
		yh += drawheight;
		dc_texturemid = iscale * (centery-yl-1);
	}
}

//==========================================================================
//
// R_DrawPlanes
//
// At the end of each frame.
//
//==========================================================================

CVAR (Bool, tilt, false, 0);
//CVAR (Int, pa, 0, 0)

int R_DrawPlanes ()
{
	visplane_t *pl;
	int i;
	int vpcount = 0;

	ds_color = 3;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentPortalUniq != CurrentPortalUniq || pl->CurrentSkybox != CurrentSkybox)
				continue;
			// kg3D - draw only real planes now
			if(pl->sky >= 0) {
				vpcount++;
				R_DrawSinglePlane (pl, OPAQUE, false, false);
			}
		}
	}
	return vpcount;
}

// kg3D - draw all visplanes with "height"
void R_DrawHeightPlanes(double height)
{
	visplane_t *pl;
	int i;

	ds_color = 3;

	DVector3 oViewPos = ViewPos;
	DAngle oViewAngle = ViewAngle;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentSkybox != CurrentSkybox || pl->CurrentPortalUniq != CurrentPortalUniq)
				continue;
			if(pl->sky < 0 && pl->height.Zat0() == height) {
				ViewPos = pl->viewpos;
				ViewAngle = pl->viewangle;
				MirrorFlags = pl->MirrorFlags;
				R_DrawSinglePlane (pl, pl->sky & 0x7FFFFFFF, pl->Additive, true);
			}
		}
	}
	ViewPos = oViewPos;
	ViewAngle = oViewAngle;
}


//==========================================================================
//
// R_DrawSinglePlane
//
// Draws a single visplane.
//
//==========================================================================

void R_DrawSinglePlane (visplane_t *pl, fixed_t alpha, bool additive, bool masked)
{
	if (pl->left >= pl->right)
		return;

	if (r_drawflat)
	{ // [RH] no texture mapping
		ds_color += 4;
		R_MapVisPlane (pl, R_MapColoredPlane);
	}
	else if (pl->picnum == skyflatnum)
	{ // sky flat
		R_DrawSkyPlane (pl);
	}
	else
	{ // regular flat
		FTexture *tex = TexMan(pl->picnum, true);

		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}

		if (!masked && !additive)
		{ // If we're not supposed to see through this plane, draw it opaque.
			alpha = OPAQUE;
		}
		else if (!tex->bMasked)
		{ // Don't waste time on a masked texture if it isn't really masked.
			masked = false;
		}
		R_SetupSpanBits(tex);
		double xscale = pl->xform.xScale * tex->Scale.X;
		double yscale = pl->xform.yScale * tex->Scale.Y;
		ds_source = tex->GetPixels ();

		basecolormap = pl->colormap;
		planeshade = LIGHT2SHADE(pl->lightlevel);

		if (r_drawflat || (!pl->height.isSlope() && !tilt))
		{
			R_DrawNormalPlane(pl, xscale, yscale, alpha, additive, masked);
		}
		else
		{
			R_DrawTiltedPlane(pl, xscale, yscale, alpha, additive, masked);
		}
	}
	NetUpdate ();
}

//==========================================================================
//
// R_DrawPortals
//
// Draws any recorded sky boxes and then frees them.
//
// The process:
//   1. Move the camera to coincide with the SkyViewpoint.
//   2. Clear out the old planes. (They have already been drawn.)
//   3. Clear a window out of the ClipSegs just large enough for the plane.
//   4. Pretend the existing vissprites and drawsegs aren't there.
//   5. Create a drawseg at 0 distance to clip sprites to the visplane. It
//      doesn't need to be associated with a line in the map, since there
//      will never be any sprites in front of it.
//   6. Render the BSP, then planes, then masked stuff.
//   7. Restore the previous vissprites and drawsegs.
//   8. Repeat for any other sky boxes.
//   9. Put the camera back where it was to begin with.
//
//==========================================================================
CVAR (Bool, r_skyboxes, true, 0)
static int numskyboxes;

void R_DrawPortals ()
{
	static TArray<size_t> interestingStack;
	static TArray<ptrdiff_t> drawsegStack;
	static TArray<ptrdiff_t> visspriteStack;
	static TArray<DVector3> viewposStack;
	static TArray<visplane_t *> visplaneStack;

	numskyboxes = 0;

	if (visplanes[MAXVISPLANES] == NULL)
		return;

	R_3D_EnterSkybox();
	CurrentPortalInSkybox = true;

	int savedextralight = extralight;
	DVector3 savedpos = ViewPos;
	DAngle savedangle = ViewAngle;
	ptrdiff_t savedvissprite_p = vissprite_p - vissprites;
	ptrdiff_t savedds_p = ds_p - drawsegs;
	ptrdiff_t savedlastopening = lastopening;
	size_t savedinteresting = FirstInterestingDrawseg;
	double savedvisibility = R_GetVisibility();
	AActor *savedcamera = camera;
	sector_t *savedsector = viewsector;

	int i;
	visplane_t *pl;

	for (pl = visplanes[MAXVISPLANES]; pl != NULL; pl = visplanes[MAXVISPLANES])
	{
		// Pop the visplane off the list now so that if this skybox adds more
		// skyboxes to the list, they will be drawn instead of skipped (because
		// new skyboxes go to the beginning of the list instead of the end).
		visplanes[MAXVISPLANES] = pl->next;
		pl->next = NULL;

		if (pl->right < pl->left || !r_skyboxes || numskyboxes == MAX_SKYBOX_PLANES || pl->portal == NULL)
		{
			R_DrawSinglePlane (pl, OPAQUE, false, false);
			*freehead = pl;
			freehead = &pl->next;
			continue;
		}

		numskyboxes++;

		FSectorPortal *port = pl->portal;
		switch (port->mType)
		{
		case PORTS_SKYVIEWPOINT:
		{
			// Don't let gun flashes brighten the sky box
			ASkyViewpoint *sky = barrier_cast<ASkyViewpoint*>(port->mSkybox);
			extralight = 0;
			R_SetVisibility(sky->args[0] * 0.25f);

			ViewPos = sky->InterpolatedPosition(r_TicFracF);
			ViewAngle = savedangle + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * r_TicFracF);

			R_CopyStackedViewParameters();
			break;
		}

		case PORTS_STACKEDSECTORTHING:
		case PORTS_PORTAL:
		case PORTS_LINKEDPORTAL:
			extralight = pl->extralight;
			R_SetVisibility (pl->visibility);
			ViewPos.X = pl->viewpos.X + port->mDisplacement.X;
			ViewPos.Y = pl->viewpos.Y + port->mDisplacement.Y;
			ViewPos.Z = pl->viewpos.Z;
			ViewAngle = pl->viewangle;
			break;

		case PORTS_HORIZON:
		case PORTS_PLANE:
			// not implemented yet

		default:
			R_DrawSinglePlane(pl, OPAQUE, false, false);
			*freehead = pl;
			freehead = &pl->next;
			numskyboxes--;
			continue;
		}

		port->mFlags |= PORTSF_INSKYBOX;
		if (port->mPartner > 0) sectorPortals[port->mPartner].mFlags |= PORTSF_INSKYBOX;
		camera = NULL;
		viewsector = port->mDestination;
		assert(viewsector != NULL);
		R_SetViewAngle ();
		validcount++;	// Make sure we see all sprites

		R_ClearPlanes (false);
		R_ClearClipSegs (pl->left, pl->right);
		WindowLeft = pl->left;
		WindowRight = pl->right;

		for (i = pl->left; i < pl->right; i++)
		{
			if (pl->top[i] == 0x7fff)
			{
				ceilingclip[i] = viewheight;
				floorclip[i] = -1;
			}
			else
			{
				ceilingclip[i] = pl->top[i];
				floorclip[i] = pl->bottom[i];
			}
		}

		// Create a drawseg to clip sprites to the sky plane
		R_CheckDrawSegs ();
		ds_p->CurrentPortalUniq = CurrentPortalUniq;
		ds_p->siz1 = INT_MAX;
		ds_p->siz2 = INT_MAX;
		ds_p->sz1 = 0;
		ds_p->sz2 = 0;
		ds_p->x1 = pl->left;
		ds_p->x2 = pl->right;
		ds_p->silhouette = SIL_BOTH;
		ds_p->sprbottomclip = R_NewOpening (pl->right - pl->left);
		ds_p->sprtopclip = R_NewOpening (pl->right - pl->left);
		ds_p->maskedtexturecol = ds_p->swall = -1;
		ds_p->bFogBoundary = false;
		ds_p->curline = NULL;
		ds_p->fake = 0;
		memcpy (openings + ds_p->sprbottomclip, floorclip + pl->left, (pl->right - pl->left)*sizeof(short));
		memcpy (openings + ds_p->sprtopclip, ceilingclip + pl->left, (pl->right - pl->left)*sizeof(short));

		firstvissprite = vissprite_p;
		firstdrawseg = ds_p++;
		FirstInterestingDrawseg = InterestingDrawsegs.Size();

		interestingStack.Push (FirstInterestingDrawseg);
		ptrdiff_t diffnum = firstdrawseg - drawsegs;
		drawsegStack.Push (diffnum);
		diffnum = firstvissprite - vissprites;
		visspriteStack.Push (diffnum);
		viewposStack.Push(ViewPos);
		visplaneStack.Push (pl);

		InSubsector = NULL;
		R_RenderBSPNode (nodes + numnodes - 1);
		R_3D_ResetClip(); // reset clips (floor/ceiling)
		R_DrawPlanes ();

		port->mFlags &= ~PORTSF_INSKYBOX;
		if (port->mPartner > 0) sectorPortals[port->mPartner].mFlags &= ~PORTSF_INSKYBOX;
	}

	// Draw all the masked textures in a second pass, in the reverse order they
	// were added. This must be done separately from the previous step for the
	// sake of nested skyboxes.
	while (interestingStack.Pop (FirstInterestingDrawseg))
	{
		ptrdiff_t pd = 0;

		drawsegStack.Pop (pd);
		firstdrawseg = drawsegs + pd;
		visspriteStack.Pop (pd);
		firstvissprite = vissprites + pd;

		// Masked textures and planes need the view coordinates restored for proper positioning.
		viewposStack.Pop(ViewPos);

		R_DrawMasked ();

		ds_p = firstdrawseg;
		vissprite_p = firstvissprite;

		visplaneStack.Pop (pl);
		if (pl->Alpha > 0)
		{
			R_DrawSinglePlane (pl, pl->Alpha, pl->Additive, true);
		}
		*freehead = pl;
		freehead = &pl->next;
	}
	firstvissprite = vissprites;
	vissprite_p = vissprites + savedvissprite_p;
	firstdrawseg = drawsegs;
	ds_p = drawsegs + savedds_p;
	InterestingDrawsegs.Resize ((unsigned int)FirstInterestingDrawseg);
	FirstInterestingDrawseg = savedinteresting;

	lastopening = savedlastopening;

	camera = savedcamera;
	viewsector = savedsector;
	ViewPos = savedpos;
	R_SetVisibility(savedvisibility);
	extralight = savedextralight;
	ViewAngle = savedangle;
	R_SetViewAngle ();

	CurrentPortalInSkybox = false;
	R_3D_LeaveSkybox();

	if(fakeActive) return;

	for (*freehead = visplanes[MAXVISPLANES], visplanes[MAXVISPLANES] = NULL; *freehead; )
		freehead = &(*freehead)->next;
}

ADD_STAT(skyboxes)
{
	FString out;
	out.Format ("%d skybox planes", numskyboxes);
	return out;
}

//==========================================================================
//
// R_DrawSkyPlane
//
//==========================================================================

void R_DrawSkyPlane (visplane_t *pl)
{
	FTextureID sky1tex, sky2tex;
	double frontdpos = 0, backdpos = 0;

	if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
	{
		sky1tex = sky2texture;
	}
	else
	{
		sky1tex = sky1texture;
	}
	sky2tex = sky2texture;
	skymid = skytexturemid;
	skyangle = ViewAngle.BAMs();

	if (pl->picnum == skyflatnum)
	{
		if (!(pl->sky & PL_SKYFLAT))
		{	// use sky1
		sky1:
			frontskytex = TexMan(sky1tex, true);
			if (level.flags & LEVEL_DOUBLESKY)
				backskytex = TexMan(sky2tex, true);
			else
				backskytex = NULL;
			skyflip = 0;
			frontdpos = sky1pos;
			backdpos = sky2pos;
			frontcyl = sky1cyl;
			backcyl = sky2cyl;
		}
		else if (pl->sky == PL_SKYFLAT)
		{	// use sky2
			frontskytex = TexMan(sky2tex, true);
			backskytex = NULL;
			frontcyl = sky2cyl;
			skyflip = 0;
			frontdpos = sky2pos;
		}
		else
		{	// MBF's linedef-controlled skies
			// Sky Linedef
			const line_t *l = &lines[(pl->sky & ~PL_SKYFLAT)-1];

			// Sky transferred from first sidedef
			const side_t *s = l->sidedef[0];
			int pos;

			// Texture comes from upper texture of reference sidedef
			// [RH] If swapping skies, then use the lower sidedef
			if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
			{
				pos = side_t::bottom;
			}
			else
			{
				pos = side_t::top;
			}

			frontskytex = TexMan(s->GetTexture(pos), true);
			if (frontskytex == NULL || frontskytex->UseType == FTexture::TEX_Null)
			{ // [RH] The blank texture: Use normal sky instead.
				goto sky1;
			}
			backskytex = NULL;

			// Horizontal offset is turned into an angle offset,
			// to allow sky rotation as well as careful positioning.
			// However, the offset is scaled very small, so that it
			// allows a long-period of sky rotation.
			skyangle += FLOAT2FIXED(s->GetTextureXOffset(pos));

			// Vertical offset allows careful sky positioning.
			skymid = s->GetTextureYOffset(pos) - 28;

			// We sometimes flip the picture horizontally.
			//
			// Doom always flipped the picture, so we make it optional,
			// to make it easier to use the new feature, while to still
			// allow old sky textures to be used.
			skyflip = l->args[2] ? 0u : ~0u;

			int frontxscale = int(frontskytex->Scale.X * 1024);
			frontcyl = MAX(frontskytex->GetWidth(), frontxscale);
			if (skystretch)
			{
				skymid = skymid * frontskytex->GetScaledHeightDouble() / SKYSTRETCH_HEIGHT;
			}
		}
	}
	frontpos = int(fmod(frontdpos, sky1cyl * 65536.0));
	if (backskytex != NULL)
	{
		backpos = int(fmod(backdpos, sky2cyl * 65536.0));
	}

	bool fakefixed = false;
	if (fixedcolormap)
	{
		dc_colormap = fixedcolormap;
	}
	else
	{
		fakefixed = true;
		fixedcolormap = dc_colormap = NormalLight.Maps;
	}

	R_DrawSky (pl);

	if (fakefixed)
		fixedcolormap = NULL;
}

//==========================================================================
//
// R_DrawNormalPlane
//
//==========================================================================

void R_DrawNormalPlane (visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked)
{
#ifdef X86_ASM
	if (ds_source != ds_cursource)
	{
		R_SetSpanSource_ASM (ds_source);
	}
#endif

	if (alpha <= 0)
	{
		return;
	}

	double planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();
	double xstep, ystep, leftxfrac, leftyfrac, rightxfrac, rightyfrac;
	double x;

	xscale = xs_ToFixed(32 - ds_xbits, _xscale);
	yscale = xs_ToFixed(32 - ds_ybits, _yscale);
	if (planeang != 0)
	{
		double cosine = cos(planeang), sine = sin(planeang);
		pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine);
		pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine);
	}
	else
	{
		pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X);
		pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.Y);
	}

	pviewx = FixedMul (xscale, pviewx);
	pviewy = FixedMul (yscale, pviewy);
	
	// left to right mapping
	planeang += (ViewAngle - 90).Radians();

	// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
	xstep = cos(planeang) / FocalLengthX;
	ystep = -sin(planeang) / FocalLengthX;

	// [RH] flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		xstep = -xstep;
		ystep = -ystep;
	}

	planeang += M_PI/2;
	double cosine = cos(planeang), sine = -sin(planeang);
	x = pl->right - centerx - 0.5;
	rightxfrac = _xscale * (cosine + x * xstep);
	rightyfrac = _yscale * (sine + x * ystep);
	x = pl->left - centerx - 0.5;
	leftxfrac = _xscale * (cosine + x * xstep);
	leftyfrac = _yscale * (sine + x * ystep);

	basexfrac = rightxfrac;
	baseyfrac = rightyfrac;
	xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left);
	ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left);

	planeheight = fabs(pl->height.Zat0() - ViewPos.Z);

	GlobVis = r_FloorVisibility / planeheight;
	if (fixedlightlev >= 0)
		ds_colormap = basecolormap->Maps + fixedlightlev, plane_shade = false;
	else if (fixedcolormap)
		ds_colormap = fixedcolormap, plane_shade = false;
	else
		plane_shade = true;

	if (spanfunc != R_FillSpan)
	{
		if (masked)
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = R_DrawSpanMaskedTranslucent;
					dc_srcblend = Col2RGB8[alpha>>10];
					dc_destblend = Col2RGB8[(OPAQUE-alpha)>>10];
				}
				else
				{
					spanfunc = R_DrawSpanMaskedAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha>>10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT>>10];
				}
			}
			else
			{
				spanfunc = R_DrawSpanMasked;
			}
		}
		else
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = R_DrawSpanTranslucent;
					dc_srcblend = Col2RGB8[alpha>>10];
					dc_destblend = Col2RGB8[(OPAQUE-alpha)>>10];
				}
				else
				{
					spanfunc = R_DrawSpanAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha>>10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT>>10];
				}
			}
			else
			{
				spanfunc = R_DrawSpan;
			}
		}
	}
	R_MapVisPlane (pl, R_MapPlane);
}

//==========================================================================
//
// R_DrawTiltedPlane
//
//==========================================================================

void R_DrawTiltedPlane (visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked)
{
	static const float ifloatpow2[16] =
	{
		// ifloatpow2[i] = 1 / (1 << i)
		64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f,
		0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f,
		0.00390625f, 0.001953125f
		/*, 0.0009765625f, 0.00048828125f, 0.000244140625f,
		1.220703125e-4f, 6.103515625e-5, 3.0517578125e-5*/
	};
	double lxscale, lyscale;
	double xscale, yscale;
	FVector3 p, m, n;
	double ang, planeang, cosine, sine;
	double zeroheight;

	if (alpha <= 0)
	{
		return;
	}

	lxscale = _xscale * ifloatpow2[ds_xbits];
	lyscale = _yscale * ifloatpow2[ds_ybits];
	xscale = 64.f / lxscale;
	yscale = 64.f / lyscale;
	zeroheight = pl->height.ZatPoint(ViewPos);

	pviewx = xs_ToFixed(32 - ds_xbits, pl->xform.xOffs * pl->xform.xScale);
	pviewy = xs_ToFixed(32 - ds_ybits, pl->xform.yOffs * pl->xform.yScale);
	planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();

	// p is the texture origin in view space
	// Don't add in the offsets at this stage, because doing so can result in
	// errors if the flat is rotated.
	ang = M_PI*3/2 - ViewAngle.Radians();
	cosine = cos(ang), sine = sin(ang);
	p[0] = ViewPos.X * cosine - ViewPos.Y * sine;
	p[2] = ViewPos.X * sine + ViewPos.Y * cosine;
	p[1] = pl->height.ZatPoint(0.0, 0.0) - ViewPos.Z;

	// m is the v direction vector in view space
	ang = ang - M_PI / 2 - planeang;
	cosine = cos(ang), sine = sin(ang);
	m[0] = yscale * cosine;
	m[2] = yscale * sine;
//	m[1] = pl->height.ZatPointF (0, iyscale) - pl->height.ZatPointF (0,0));
//	VectorScale2 (m, 64.f/VectorLength(m));

	// n is the u direction vector in view space
#if 0
	//let's use the sin/cosine we already know instead of computing new ones
	ang += M_PI/2
	n[0] = -xscale * cos(ang);
	n[2] = -xscale * sin(ang);
#else
	n[0] = xscale * sine;
	n[2] = -xscale * cosine;
#endif
//	n[1] = pl->height.ZatPointF (ixscale, 0) - pl->height.ZatPointF (0,0));
//	VectorScale2 (n, 64.f/VectorLength(n));

	// This code keeps the texture coordinates constant across the x,y plane no matter
	// how much you slope the surface. Use the commented-out code above instead to keep
	// the textures a constant size across the surface's plane instead.
	cosine = cos(planeang), sine = sin(planeang);
	m[1] = pl->height.ZatPoint(ViewPos.X + yscale * sine, ViewPos.Y + yscale * cosine) - zeroheight;
	n[1] = -(pl->height.ZatPoint(ViewPos.X - xscale * cosine, ViewPos.Y + xscale * sine) - zeroheight);

	plane_su = p ^ m;
	plane_sv = p ^ n;
	plane_sz = m ^ n;

	plane_su.Z *= FocalLengthX;
	plane_sv.Z *= FocalLengthX;
	plane_sz.Z *= FocalLengthX;

	plane_su.Y *= IYaspectMul;
	plane_sv.Y *= IYaspectMul;
	plane_sz.Y *= IYaspectMul;

	// Premultiply the texture vectors with the scale factors
	plane_su *= 4294967296.f;
	plane_sv *= 4294967296.f;

	if (MirrorFlags & RF_XFLIP)
	{
		plane_su[0] = -plane_su[0];
		plane_sv[0] = -plane_sv[0];
		plane_sz[0] = -plane_sz[0];
	}

	planelightfloat = (r_TiltVisibility * lxscale * lyscale) / (fabs(pl->height.ZatPoint(ViewPos) - ViewPos.Z)) / 65536.f;

	if (pl->height.fC() > 0)
		planelightfloat = -planelightfloat;

	if (fixedlightlev >= 0)
		ds_colormap = basecolormap->Maps + fixedlightlev, plane_shade = false;
	else if (fixedcolormap)
		ds_colormap = fixedcolormap, plane_shade = false;
	else
		ds_colormap = basecolormap->Maps, plane_shade = true;

	if (!plane_shade)
	{
		for (int i = 0; i < viewwidth; ++i)
		{
			tiltlighting[i] = ds_colormap;
		}
	}

#if defined(X86_ASM)
	if (ds_source != ds_curtiltedsource)
		R_SetTiltedSpanSource_ASM (ds_source);
	R_MapVisPlane (pl, R_DrawTiltedPlane_ASM);
#else
	R_MapVisPlane (pl, R_MapTiltedPlane);
#endif
}

//==========================================================================
//
// R_MapVisPlane
//
// t1/b1 are at x
// t2/b2 are at x+1
// spanend[y] is at the right edge
//
//==========================================================================

void R_MapVisPlane (visplane_t *pl, void (*mapfunc)(int y, int x1))
{
	int x = pl->right - 1;
	int t2 = pl->top[x];
	int b2 = pl->bottom[x];

	if (b2 > t2)
	{
		clearbufshort (spanend+t2, b2-t2, x);
	}

	for (--x; x >= pl->left; --x)
	{
		int t1 = pl->top[x];
		int b1 = pl->bottom[x];
		const int xr = x+1;
		int stop;

		// Draw any spans that have just closed
		stop = MIN (t1, b2);
		while (t2 < stop)
		{
			mapfunc (t2++, xr);
		}
		stop = MAX (b1, t2);
		while (b2 > stop)
		{
			mapfunc (--b2, xr);
		}

		// Mark any spans that have just opened
		stop = MIN (t2, b1);
		while (t1 < stop)
		{
			spanend[t1++] = x;
		}
		stop = MAX (b2, t2);
		while (b1 > stop)
		{
			spanend[--b1] = x;
		}

		t2 = pl->top[x];
		b2 = pl->bottom[x];
		basexfrac -= xstepscale;
		baseyfrac -= ystepscale;
	}
	// Draw any spans that are still open
	while (t2 < b2)
	{
		mapfunc (--b2, pl->left);
	}
}

//==========================================================================
//
// R_PlaneInitData
//
//==========================================================================

bool R_PlaneInitData ()
{
	int i;
	visplane_t *pl;

	// Free all visplanes and let them be re-allocated as needed.
	pl = freetail;

	while (pl)
	{
		visplane_t *next = pl->next;
		M_Free (pl);
		pl = next;
	}
	freetail = NULL;
	freehead = &freetail;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		pl = visplanes[i];
		visplanes[i] = NULL;
		while (pl)
		{
			visplane_t *next = pl->next;
			M_Free (pl);
			pl = next;
		}
	}

	return true;
}
