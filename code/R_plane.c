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

//
// opening
//

// Here comes the obnoxious "visplane".
int						MaxVisPlanes;
visplane_t				*visplanes;
visplane_t 				*lastvisplane;
visplane_t 				*floorplane;
visplane_t 				*ceilingplane;

// ?
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
lighttable_t**			planezlight;
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


static void PrepVisPlanes (int min)
{
	int i;
	unsigned short *stuff;

	for (i = min; i < MaxVisPlanes; i++) {
		stuff = (unsigned short *)Calloc (screens[0].width * 2 + 4, sizeof(unsigned short));
		visplanes[i].top = stuff + 1;
		visplanes[i].bottom = stuff + screens[0].width + 3;
	}
}

//
// R_InitPlanes
// Only at game startup.
// [RH] This function is actually used now.
//
void R_InitPlanes (void)
{
	MaxVisPlanes = 128;			// Default. Increased as needed.
	visplanes = (visplane_t *)Calloc (MaxVisPlanes, sizeof(visplane_t));
	PrepVisPlanes (0);
}

static void GetMoreVisPlanes (visplane_t **toupdate)
{
	int oldMax;
	visplane_t *old = visplanes;

	oldMax = MaxVisPlanes;
	MaxVisPlanes += 32;
	visplanes = (visplane_t *)Realloc (visplanes, (MaxVisPlanes) * sizeof(visplane_t));

	DPrintf ("MaxVisPlanes increased to %d\n", MaxVisPlanes);
	
	PrepVisPlanes (oldMax);

	if (visplanes == old)
		return;

	*toupdate = &visplanes[*toupdate - old];
	lastvisplane = &visplanes[lastvisplane - old];
	if (floorplane)
		floorplane = &visplanes[floorplane - old];
	if (ceilingplane)
		ceilingplane = &visplanes[ceilingplane - old];
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
		
		if (index >= MAXLIGHTZ )
			index = MAXLIGHTZ-1;

		ds_colormap = planezlight[index];
	}
		
	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	// high or low detail
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
	for (i=0 ; i<viewwidth ; i++)
	{
		floorclip[i] = (short)viewheight;
		ceilingclip[i] = -1;
	}

	lastvisplane = visplanes;
	lastopening = openings;
	
	// texture calculation
	memset (cachedheight, 0, sizeof(fixed_t) * screens[0].height);

	// left to right mapping
	angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;
		
	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (finecosine[angle],centerxfrac);
	baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}




//
// R_FindPlane
//
// killough 2/28/98: Add offsets

visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
						 fixed_t xoffs, fixed_t yoffs)
{
	visplane_t *check;

	if (picnum == skyflatnum)
		height = lightlevel = 0;		// all skys map together
		
	for (check=visplanes; check<lastvisplane; check++)
	{
		if (height == check->height
			&& picnum == check->picnum
			&& lightlevel == check->lightlevel
			&& xoffs == check->xoffs
			&& yoffs == check->yoffs
			)
		{
			break;
		}
	}

	if (check < lastvisplane)
		return check;
				
	if (lastvisplane - visplanes == MaxVisPlanes)
		GetMoreVisPlanes (&check);
				
	lastvisplane++;

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xoffs = xoffs;		// killough 2/28/98: Save offsets
	check->yoffs = yoffs;
	check->minx = screens[0].width;
	check->maxx = -1;
	
	memset (check->top,0xff,sizeof(short) * screens[0].width);
				
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

	for (x=intrl ; x<= intrh ; x++)
		if (pl->top[x] != 0xffff)
			break;

	if (x > intrh)
	{
		pl->minx = unionl;
		pl->maxx = unionh;

		// use the same one
		return pl;				
	}
		
	// [RH] Need to make sure we actually have this lastvisplane
	if (lastvisplane - visplanes == MaxVisPlanes)
		GetMoreVisPlanes (&pl);

	// make a new visplane
	lastvisplane->height = pl->height;
	lastvisplane->picnum = pl->picnum;
	lastvisplane->lightlevel = pl->lightlevel;
    lastvisplane->xoffs = pl->xoffs;			// killough 2/28/98
    lastvisplane->yoffs = pl->yoffs;
	
	pl = lastvisplane++;

	pl->minx = start;
	pl->maxx = stop;

	memset (pl->top,0xff,sizeof(short) * screens[0].width);
				
	return pl;
}


//
// R_MakeSpans
//
void R_MakeSpans (int x, int t1, int b1, int t2, int b2)
{
	//if (t1 != 0xffff)
		while (t1 < t2 && t1<=b1)
		{
			R_MapPlane (t1,spanstart[t1],x-1);
			t1++;
		}
	//if (b1 != 0xffff)
		while (b1 > b2 && b1>=t1)
		{
			R_MapPlane (b1,spanstart[b1],x-1);
			b1--;
		}
		
	//if (t2 != 0xffff)
		while (t2 < t1 && t2<=b2)
		{
			spanstart[t2] = x;
			t2++;
		}
	//if (b2 != 0xffff)
		while (b2 > b1 && b2>=t2)
		{
			spanstart[b2] = x;
			b2--;
		}
}

// [RH] This was separated from R_DrawPlanes() on 11.5.1998.
//		Also added support for columns with holes since double skies
//		opens up that possibility (modified from R_DrawMaskedColumn).
//		One implication of this is that the sky will always wrap
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
	visplane_t* 		pl;
	int 				light;
	int 				x;
	int 				stop;
	
	for (pl = visplanes ; pl < lastvisplane ; pl++)
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

			continue;
		}
		
		// regular flat
		ds_source = W_CacheLumpNum(firstflat +
								   flattranslation[pl->picnum],
								   PU_STATIC);
		
		xoffs = pl->xoffs;	// killough 2/28/98: Add offsets
		yoffs = pl->yoffs;
		planeheight = abs(pl->height-viewz);
		light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

		if (light >= LIGHTLEVELS)
			light = LIGHTLEVELS-1;

		if (light < 0)
			light = 0;

		planezlight = zlight[light];

		pl->top[pl->maxx+1] = 0xffff;
		pl->top[pl->minx-1] = 0xffff;
				
		stop = pl->maxx + 1;

		for (x=pl->minx ; x<= stop ; x++)
		{
			R_MakeSpans(x,pl->top[x-1],
						pl->bottom[x-1],
						pl->top[x],
						pl->bottom[x]);
		}
		
		Z_ChangeTag (ds_source, PU_CACHE);
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

	floorclip = Calloc (screens[0].width, sizeof(short));
	ceilingclip = Calloc (screens[0].width, sizeof(short));

	spanstart = Calloc (screens[0].height, sizeof(int));
	spanstop = Calloc (screens[0].height, sizeof(int));

	yslopetab = Calloc ((screens[0].height<<1)+(screens[0].height>>1), sizeof(fixed_t));
	distscale = Calloc (screens[0].width, sizeof(fixed_t));
	cachedheight = Calloc (screens[0].height, sizeof(fixed_t));
	cacheddistance = Calloc (screens[0].height, sizeof(fixed_t));
	cachedxstep = Calloc (screens[0].height, sizeof(fixed_t));
	cachedystep = Calloc (screens[0].height, sizeof(fixed_t));

	if (visplanes) {
		int i;

		for (i = 0; i < MaxVisPlanes; i++)
			free (visplanes[i].top - 1);

		PrepVisPlanes (0);
	}

	return true;
}