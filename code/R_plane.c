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
//-----------------------------------------------------------------------------



#include <stdlib.h>

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"

#include "m_alloc.h"
#include "v_video.h"

planefunction_t 		floorfunc;
planefunction_t 		ceilingfunc;

// Here comes the obnoxious "visplane".
#define MAXVISPLANES 128    /* must be a power of 2 */

static visplane_t		*visplanes[MAXVISPLANES];	// killough
static visplane_t		*freetail;					// killough
static visplane_t		**freehead = &freetail;		// killough

visplane_t 				*floorplane;
visplane_t 				*ceilingplane;

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  ((unsigned)((picnum)*3+(lightlevel)+(height)*7) & (MAXVISPLANES-1))

//
// opening
//

size_t					maxopenings;
short					*openings;
short					*lastopening;


//
// Clip values are the solid pixel bounding the range.
//	floorclip starts out SCREENHEIGHT
//	ceilingclip starts out -1
//
short					*floorclip;
short					*ceilingclip;

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int 					*spanstart;

//
// texture mapping
//
int*					planezlight;	// [RH] Changed from lighttable_t** to int*
fixed_t 				planeheight;

fixed_t 				*yslopetab;		// [RH] Added for freelook. ylook points into it
fixed_t 				*yslope;
fixed_t 				*distscale;
fixed_t 				basexscale;
fixed_t 				baseyscale;
static fixed_t			xoffs, yoffs;	// killough 2/28/98: flat offsets

fixed_t 				*cachedheight;
fixed_t 				*cacheddistance;
fixed_t 				*cachedxstep;
fixed_t 				*cachedystep;


//==========================================================================
//
// R_InitPlanes
//
// Called at game startup.
//
//==========================================================================

void R_InitPlanes (void)
{
}

//==========================================================================
//
// R_MapPlane
//
// Globals used: planeheight, ds_source, basexscale, baseyscale,
// viewx, viewy, xoffs, yoffs, basecolormap.
//
//==========================================================================

void R_MapPlane (int y, int x1, int x2)
{
	angle_t angle;
	fixed_t distance, length;
	unsigned index;

#ifdef RANGECHECK
	if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>=(unsigned)viewheight)
	{
		I_FatalError ("R_MapPlane: %i, %i at %i", x1, x2, y);
	}
#endif
	if (planeheight != cachedheight[y])
	{
		cachedheight[y] = planeheight;
		distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
		ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
		ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
	}
	else
	{
		distance = cacheddistance[y];
		ds_xstep = cachedxstep[y];
		ds_ystep = cachedystep[y];
	}

	length = FixedMul (distance,distscale[x1]);
	angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;

	// killough 2/28/98: Add offsets
	ds_xfrac = viewx + FixedMul(finecosine[angle], length) + xoffs;
	ds_yfrac = -viewy - FixedMul(finesine[angle], length) + yoffs;

	if (fixedlightlev)
		ds_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		ds_colormap = fixedcolormap;
	else
	{
		index = distance >> LIGHTZSHIFT;
		
		if (index >= MAXLIGHTZ)
			index = MAXLIGHTZ-1;

		ds_colormap = planezlight[index] + basecolormap;	// [RH] add basecolormap
	}
		
	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	spanfunc ();		
}

//==========================================================================
//
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes (void)
{
	int i;
	angle_t angle;
	
	// opening / clipping determination
	for (i = 0; i < viewwidth ; i++)
	{
		floorclip[i] = (short)viewheight;
	}
	memset (ceilingclip, 0xff, sizeof(*ceilingclip) * viewwidth);

	for (i = 0; i < MAXVISPLANES; i++)	// new code -- killough
		for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			freehead = &(*freehead)->next;

	lastopening = openings;
	
	// Texture calculation
	memset (cachedheight, 0, sizeof(*cachedheight) * screen.height);
	angle = (viewangle - ANG90)>>ANGLETOFINESHIFT;	// left to right mapping
	// Scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (finecosine[angle], centerxfrac);
	baseyscale = -FixedDiv (finesine[angle], centerxfrac);
}

//==========================================================================
//
// New function, by Lee Killough
// [RH] top and bottom buffers get allocated immediately
//		after the visplane.
//
//==========================================================================

static visplane_t *new_visplane(unsigned hash)
{
	visplane_t *check = freetail;

	if (!check) {
		check = Calloc (1, sizeof(*check) + sizeof(*check->top)*(screen.width*2));
		check->bottom = &check->top[screen.width+2];
	} else
		if (!(freetail = freetail->next))
			freehead = &freetail;
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

visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
						 fixed_t xoffs, fixed_t yoffs)
{
	visplane_t *check;
	unsigned hash;						// killough

	if (picnum == skyflatnum || picnum & PL_SKYFLAT)	// killough 10/98
		height = lightlevel = 0;		// most skies map together
		
	// New visplane algorithm uses hash table -- killough
	hash = visplane_hash (picnum, lightlevel, height);

	for (check = visplanes[hash]; check; check = check->next)	// killough
		if (height == check->height &&
			picnum == check->picnum &&
			lightlevel == check->lightlevel &&
			xoffs == check->xoffs &&	// killough 2/28/98: Add offset checks
			yoffs == check->yoffs &&
			basecolormap == check->colormap)	// [RH] Add colormap check
		  return check;

	check = new_visplane (hash);		// killough

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xoffs = xoffs;				// killough 2/28/98: Save offsets
	check->yoffs = yoffs;
	check->colormap = basecolormap;		// [RH] Save colormap
	check->minx = viewwidth;			// Was SCREENWIDTH -- killough 11/98
	check->maxx = -1;
	
	memset (check->top, 0xff, sizeof(*check->top) * screen.width);
				
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
		
	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}
		
	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for (x=intrl ; x <= intrh && pl->top[x] == 0xffff; x++)
		;

	if (x > intrh)
	{
		// use the same visplane
		pl->minx = unionl;
		pl->maxx = unionh;
	}
	else
	{
		// make a new visplane
		unsigned hash = visplane_hash (pl->picnum, pl->lightlevel, pl->height);
		visplane_t *new_pl = new_visplane (hash);

		new_pl->height = pl->height;
		new_pl->picnum = pl->picnum;
		new_pl->lightlevel = pl->lightlevel;
		new_pl->xoffs = pl->xoffs;			// killough 2/28/98
		new_pl->yoffs = pl->yoffs;
		new_pl->colormap = pl->colormap;	// [RH] Copy colormap
		pl = new_pl;
		pl->minx = start;
		pl->maxx = stop;
		memset (pl->top, 0xff, sizeof(*pl->top) * screen.width);
	}
	return pl;
}


//==========================================================================
//
// R_MakeSpans
//
//==========================================================================

void R_MakeSpans (int x, int t1, int b1, int t2, int b2)
{
	for (; t1 < t2 && t1 <= b1; t1++)
		R_MapPlane(t1, spanstart[t1], x-1);
	for (; b1 > b2 && b1 >= t1; b1--)
		R_MapPlane(b1, spanstart[b1] ,x-1);
	while (t2 < t1 && t2 <= b2)
		spanstart[t2++] = x;
	while (b2 > b1 && b2 >= t2)
		spanstart[b2--] = x;
}

//==========================================================================
//
// [RH] R_DrawSky
//
// Can handle parallax skies. Note that the front sky is *not* masked in
// in the normal convention for patches, but uses color 0 as a transparent
// color.
//
//==========================================================================

static visplane_t *_skypl;
static int frontskytex, backskytex;
static angle_t skyflip;
static int frontpos, backpos;

static void _skycolumn (void (*drawfunc)(void), int x)
{
	dc_yl = _skypl->top[x];
	dc_yh = _skypl->bottom[x];

	if (dc_yl <= dc_yh)	{
		int angle = ((((viewangle + xtoviewangle[x])^skyflip)>>(ANGLETOSKYSHIFT-16)) + frontpos)>>16;

		if (backskytex == -1)
		{
			dc_source = R_GetColumn (frontskytex, angle);
			drawfunc ();
		}
		else
		{
			byte composite[256];	// Skies shouldn't be taller than this
			byte *source;
			byte *source2;
			byte *dest;
			int count;
			int top;
			int bottom;

			top = dc_texturemid + (dc_yl - centery) * dc_iscale;
			bottom = top + (dc_yh - dc_yl) * dc_iscale;
			top >>= FRACBITS;
			bottom >>= FRACBITS;
			count = bottom - top + 1;

			source = R_GetColumn (frontskytex, angle) + top;
			angle = ((((viewangle + xtoviewangle[x])^skyflip)>>(ANGLETOSKYSHIFT-16)) + backpos)>>16;
			source2 = R_GetColumn (backskytex, angle) + top;
			dest = composite + top;

			do
			{
				if (*source) {
					*dest++ = *source++;
					source2++;
				} else {
					*dest++ = *source2++;
					source++;
				}
			} while (--count);
			dc_source = composite;
			drawfunc ();
		}
	}
}

static void R_DrawSky (visplane_t *pl)
{
	int x;

	if (pl->minx > pl->maxx)
		return;

	dc_mask = 255;
	dc_iscale = skyiscale >> skystretch;
	dc_texturemid = skytexturemid;
	_skypl = pl;

	if (!r_columnmethod->value) {
		for (x = pl->minx; x <= pl->maxx; x++) {
			dc_x = x;
			_skycolumn (colfunc, x);
		}
	} else {
		int stop = (pl->maxx+1) & ~3;

		x = pl->minx;

		if (x & 1) {
			dc_x = x;
			_skycolumn (colfunc, x);
			x++;
		}

		if (x & 2) {
			if (x < pl->maxx) {
				rt_initcols();
				dc_x = 0;
				_skycolumn (hcolfunc_pre, x);
				x++;
				dc_x = 1;
				_skycolumn (hcolfunc_pre, x);
				rt_draw2cols (0, x - 1);
				x++;
			} else if (x == pl->maxx) {
				dc_x = x;
				_skycolumn (colfunc, x);
				x++;
			}
		}

		while (x < stop) {
			rt_initcols();
			dc_x = 0;
			_skycolumn (hcolfunc_pre, x);
			x++;
			dc_x = 1;
			_skycolumn (hcolfunc_pre, x);
			x++;
			dc_x = 2;
			_skycolumn (hcolfunc_pre, x);
			x++;
			dc_x = 3;
			_skycolumn (hcolfunc_pre, x);
			rt_draw4cols (x - 3);
			x++;
		}

		if (pl->maxx == x) {
			dc_x = x;
			_skycolumn (colfunc, x);
			x++;
		} else if (pl->maxx > x) {
			rt_initcols();
			dc_x = 0;
			_skycolumn (hcolfunc_pre, x);
			x++;
			dc_x = 1;
			_skycolumn (hcolfunc_pre, x);
			rt_draw2cols (0, x - 1);
			if (++x <= pl->maxx) {
				dc_x = x;
				_skycolumn (colfunc, x);
				x++;
			}
		}
	}
}

//==========================================================================
//
// R_DrawPlanes
//
// At the end of each frame.
//
//==========================================================================

void R_DrawPlanes (void)
{
	visplane_t *pl;
	int i;
	
	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->minx > pl->maxx)
				continue;
			
			// sky flat
			if (pl->picnum == skyflatnum || pl->picnum & PL_SKYFLAT)
			{
				if (pl->picnum == skyflatnum)
				{	// use sky1
					frontskytex = sky1texture;
					if (level.flags & LEVEL_DOUBLESKY)
						backskytex = sky2texture;
					else
						backskytex = -1;
					skyflip = 0;
					frontpos = sky1pos;
					backpos = sky2pos;
				}
				else if (pl->picnum == PL_SKYFLAT)
				{	// use sky2
					frontskytex = sky2texture;
					backskytex = -1;
					skyflip = 0;
					frontpos = sky2pos;
				}
				else
				{	// MBF's linedef-controlled skies
					// Sky Linedef
					const line_t *l = &lines[(pl->picnum & ~PL_SKYFLAT)-1];

					// Sky transferred from first sidedef
					const side_t *s = *l->sidenum + sides;

					// Texture comes from upper texture of reference sidedef
					frontskytex = texturetranslation[s->toptexture];
					backskytex = -1;

					// Horizontal offset is turned into an angle offset,
					// to allow sky rotation as well as careful positioning.
					// However, the offset is scaled very small, so that it
					// allows a long-period of sky rotation.
					frontpos = (-s->textureoffset) >> (ANGLETOSKYSHIFT-16);

					// Vertical offset allows careful sky positioning.
					dc_texturemid = s->rowoffset - 28*FRACUNIT;

					// We sometimes flip the picture horizontally.
					//
					// Doom always flipped the picture, so we make it optional,
					// to make it easier to use the new feature, while to still
					// allow old sky textures to be used.
					skyflip = l->args[2] ? 0u : ~0u;
				}

				if (fixedlightlev) {
					dc_colormap = DefaultPalette->maps.colormaps + fixedlightlev;
				} else if (fixedcolormap) {
					dc_colormap = fixedcolormap;
				} else if (!fixedcolormap) {
					dc_colormap = DefaultPalette->maps.colormaps;
					colfunc = R_StretchColumn;
					hcolfunc_post1 = rt_copy1col;
					hcolfunc_post2 = rt_copy2cols;
					hcolfunc_post4 = rt_copy4cols;
				}

				R_DrawSky (pl);

				colfunc = basecolfunc;
				hcolfunc_post1 = rt_map1col;
				hcolfunc_post2 = rt_map2cols;
				hcolfunc_post4 = rt_map4cols;
			}
			else
			{
				// regular flat
				int light, stop, x;

//				ds_color = pl->color;	// [RH] color if r_drawflat is 1

				ds_source = W_CacheLumpNum(firstflat +
										   flattranslation[pl->picnum],
										   PU_STATIC);
				
				xoffs = pl->xoffs;	// killough 2/28/98: Add offsets
				yoffs = pl->yoffs;
				basecolormap = pl->colormap;	// [RH] set basecolormap
				planeheight = abs(pl->height-viewz);
				light = (pl->lightlevel >> LIGHTSEGSHIFT) + (foggy ? 0 : extralight);

				if (light >= LIGHTLEVELS)
					light = LIGHTLEVELS-1;
				else if (light < 0)
					light = 0;

				planezlight = zlight[light];

				pl->top[pl->maxx+1] = 0xffff;
				pl->top[pl->minx-1] = 0xffff;
						
				stop = pl->maxx + 1;

				for (x = pl->minx; x <= stop; x++)
				{
					R_MakeSpans (x, pl->top[x-1], pl->bottom[x-1],
									pl->top[x],	pl->bottom[x]);
				}
				
				Z_ChangeTag (ds_source, PU_CACHE);
			}
		}
	}
}

BOOL R_PlaneInitData (void)
{
	if (floorclip)		free (floorclip);
	if (ceilingclip)	free (ceilingclip);
	if (spanstart)		free (spanstart);
	if (yslopetab)		free (yslopetab);
	if (distscale)		free (distscale);
	if (cachedheight)	free (cachedheight);
	if (cacheddistance)	free (cacheddistance);
	if (cachedxstep)	free (cachedxstep);
	if (cachedystep)	free (cachedystep);

	floorclip = Malloc (screen.width * sizeof(*floorclip));
	ceilingclip = Malloc (screen.width * sizeof(*ceilingclip));

	spanstart = Calloc (screen.height, sizeof(*spanstart));

	yslopetab = Calloc ((screen.height<<1)+(screen.height>>1), sizeof(*yslopetab));
	distscale = Calloc (screen.width, sizeof(*distscale));
	cachedheight = Calloc (screen.height, sizeof(*cachedheight));
	cacheddistance = Calloc (screen.height, sizeof(*cacheddistance));
	cachedxstep = Calloc (screen.height, sizeof(*cachedxstep));
	cachedystep = Calloc (screen.height, sizeof(*cachedystep));

	// Free all visplanes and let them be re-allocated as needed.
	{
		int i;
		visplane_t *pl = freetail;

		while (pl) {
			visplane_t *next = pl->next;
			free (pl);
			pl = next;
		}
		freetail = NULL;
		freehead = &freetail;

		for (i = 0; i < MAXVISPLANES; i++) {
			pl = visplanes[i];
			visplanes[i] = NULL;
			while (pl) {
				visplane_t *next = pl->next;
				free (pl);
				pl = next;
			}
		}
	}

	return true;
}