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
// For more information on visplanes, see:
//
// http://classicgaming.com/doom/editing/
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
int 					*spanstop;

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


//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
}

//
// R_MapPlane
//
// Uses global vars:
//	planeheight
//	ds_source
//	basexscale
//	baseyscale
//	viewx
//	viewy
//	xoffs
//	yoffs
//	basecolormap	// [RH]
//
// BASIC PRIMITIVE
//
void R_MapPlane (int y, int x1, int x2)
{
	angle_t 	angle;
	fixed_t 	distance;
	fixed_t 	length;
	unsigned	index;

#ifdef RANGECHECK
	if (x2 < x1
		|| x1<0
		|| x2>=viewwidth
		|| (unsigned)y>(unsigned)viewheight)
	{
		I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
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

	if (fixedcolormap)
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


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes (void)
{
	int 		i;
	angle_t 	angle;
	
	// opening / clipping determination
	for (i = 0; i < viewwidth ; i++)
	{
		floorclip[i] = (short)viewheight;
		ceilingclip[i] = -1;
	}

	for (i = 0; i < MAXVISPLANES; i++)	// new code -- killough
		for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			freehead = &(*freehead)->next;

	lastopening = openings;
	
	// texture calculation
	memset (cachedheight, 0, sizeof(*cachedheight) * screens[0].height);

	// left to right mapping
	angle = (viewangle - ANG90)>>ANGLETOFINESHIFT;
		
	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (finecosine[angle],centerxfrac);
	baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}

// New function, by Lee Killough
// [RH] top and bottom buffers get allocated immediately
//		after the visplane.

static visplane_t *new_visplane(unsigned hash)
{
	visplane_t *check = freetail;

	if (!check) {
		check = Calloc (1, sizeof(*check) + sizeof(*check->top)*(screens[0].width*2));
		check->bottom = &check->top[screens[0].width+2];
	} else
		if (!(freetail = freetail->next))
			freehead = &freetail;
	check->next = visplanes[hash];
	visplanes[hash] = check;
	return check;
}


//
// R_FindPlane
//
// killough 2/28/98: Add offsets

visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
						 fixed_t xoffs, fixed_t yoffs)
{
	visplane_t *check;
	unsigned hash;						// killough

	if (picnum == skyflatnum)
		height = lightlevel = 0;		// all skys map together
		
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
	check->minx = screens[0].width;
	check->maxx = -1;
	
	memset (check->top, 0xff, sizeof(*check->top) * screens[0].width);
				
	return check;
}


//
// R_CheckPlane
//
visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop)
{
	int 		intrl;
	int 		intrh;
	int 		unionl;
	int 		unionh;
	int 		x;
		
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
		pl->minx = unionl;
		pl->maxx = unionh;
	}
	else
	{
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
		memset (pl->top, 0xff, sizeof(*pl->top) * screens[0].width);
	}
	return pl;
}


//
// R_MakeSpans
//
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

// [RH] This was separated from R_DrawPlanes() on 11.5.1998.
//		Also added support for columns with holes since double skies
//		opens up that possibility (modified from R_DrawMaskedColumn).
//		One implication of this is that the sky should always wrap
//		properly, provided that it is tall enough.

static void R_DrawMaskedSky (int skytexture, int skypos, fixed_t scale, fixed_t height, visplane_t *pl)
{
	int x, angle;
	column_t *col, *column;
	fixed_t basetexturemid = dc_texturemid;
	fixed_t yl, yh, topscreen;
	int min_yl, max_yh;
	unsigned int top, bottom;

	height = FixedMul (height, scale);

	for (x=pl->minx ; x <= pl->maxx ; x++)
	{
		dc_x = x;
		angle = ((((viewangle + xtoviewangle[x])>>(ANGLETOSKYSHIFT-16)) + skypos)>>16);
		column = (column_t *) ((byte *)R_GetColumn(skytexture, angle) - 3);

		if (column->topdelta == 0xff)
			// empty colum
			continue;

		top = pl->top[x];
		bottom = pl->bottom[x];

		if (top <= bottom) {
			min_yl = MAXINT;
			max_yh = MININT;

			topscreen = FixedMul (skytopfrac, scale);
			while (topscreen > (signed)(top << FRACBITS))
				topscreen -= height;

			while (max_yh < (signed)bottom) {
				col = column;
				
				while (col->topdelta != 0xff) {
					yl = scale * col->topdelta;
					yh = yl + scale * col->length;

					dc_yl = (yl + FRACUNIT - 1 + topscreen) >> FRACBITS;
					dc_yh = (yh - 1 + topscreen) >> FRACBITS;

					if (dc_yl < (signed)top)
						dc_yl = top;
					if ((unsigned)dc_yh > bottom)
						dc_yh = bottom;

					if (dc_yl <= dc_yh)
					{
						if (dc_yh > max_yh)
							max_yh = dc_yh;

						dc_source = (byte *)col + 3;
						dc_texturemid = basetexturemid - (col->topdelta<<FRACBITS);
						colfunc ();
					}

					col = (column_t *) ((byte *)col + col->length + 4);
				}
				topscreen += height;
			}
		}
	}

	dc_texturemid = basetexturemid;
}

static void R_DrawSky (int skytexture, int skypos, visplane_t *pl)
{
	int x, angle;

	for (x=pl->minx ; x <= pl->maxx ; x++) {
		dc_yl = pl->top[x];
		dc_yh = pl->bottom[x];

		if (dc_yl <= dc_yh)	{
			angle = ((((viewangle + xtoviewangle[x])>>(ANGLETOSKYSHIFT-16)) + skypos)>>16);
			dc_x = x;
			dc_source = R_GetColumn(skytexture, angle);
			colfunc ();
		}
	}
}

//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes (void)
{
	visplane_t *pl;
	int i;
	
	for (i = 0; i < MAXVISPLANES; i++)
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->minx > pl->maxx)
				continue;
			
			// sky flat
			if (pl->picnum == skyflatnum)
			{
				// Sky is allways drawn full bright,
				//	i.e. colormaps[0] is used.
				// Because of this hack, sky is not affected
				//	by INVUL inverse mapping.
				dc_colormap = DefaultPalette->maps.colormaps;

				if (level.flags & LEVEL_DOUBLESKY) {
					dc_iscale = sky2iscale >> sky2stretch;
					dc_texturemid = sky2texturemid;
					if (textureheight[sky2texture] == (128<<FRACBITS))
						R_DrawSky (sky2texture, sky2pos, pl);
					else
						R_DrawMaskedSky (sky2texture, sky2pos, sky2scale, sky2height, pl);
				}

				dc_iscale = sky1iscale >> sky1stretch;
				dc_texturemid = sky1texturemid;
				if ((level.flags & LEVEL_DOUBLESKY) || (textureheight[sky1texture] != (128<<FRACBITS)))
					R_DrawMaskedSky (sky1texture, sky1pos, sky1scale, sky1height, pl);
				else
					R_DrawSky (sky1texture, sky1pos, pl);
			}
			else
			{
				// regular flat
				int light, stop, x;

				ds_source = W_CacheLumpNum(firstflat +
										   flattranslation[pl->picnum],
										   PU_STATIC);
				
				xoffs = pl->xoffs;	// killough 2/28/98: Add offsets
				yoffs = pl->yoffs;
				basecolormap = pl->colormap;	// [RH] set basecolormap
				planeheight = abs(pl->height-viewz);
				light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

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
					R_MakeSpans(x,pl->top[x-1],
								pl->bottom[x-1],
								pl->top[x],
								pl->bottom[x]);
				}
				
				Z_ChangeTag (ds_source, PU_CACHE);
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

	floorclip = Calloc (screens[0].width, sizeof(*floorclip));
	ceilingclip = Calloc (screens[0].width, sizeof(*ceilingclip));

	spanstart = Calloc (screens[0].height, sizeof(*spanstart));
	spanstop = Calloc (screens[0].height, sizeof(*spanstop));

	yslopetab = Calloc ((screens[0].height<<1)+(screens[0].height>>1), sizeof(*yslopetab));
	distscale = Calloc (screens[0].width, sizeof(*distscale));
	cachedheight = Calloc (screens[0].height, sizeof(*cachedheight));
	cacheddistance = Calloc (screens[0].height, sizeof(*cacheddistance));
	cachedxstep = Calloc (screens[0].height, sizeof(*cachedxstep));
	cachedystep = Calloc (screens[0].height, sizeof(*cachedystep));

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