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
extern fixed_t FocalLengthX, FocalLengthY;

int*					planezlight;
fixed_t 				planeheight;

fixed_t 				*yslope;
fixed_t 				*distscale;
fixed_t 				xstepscale;
fixed_t 				ystepscale;
static fixed_t			xscale, yscale;
static fixed_t			pviewx, pviewy;
static angle_t			baseangle;

#ifdef USEASM
extern "C" void R_SetSpanSource_ASM (byte *flat);
extern "C" void R_SetSpanColormap_ASM (byte *colormap);
extern "C" byte *ds_curcolormap, *ds_cursource;
#endif

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
// pviewx, pviewy, xoffs, yoffs, basecolormap, xscale, yscale, baseangle.
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

	// Find the z-coordinate of the left edge of the span in camera space
	// This is some simple triangle scaling:
	//		(vertical distance of plane from camera) * (focal length)
	//		---------------------------------------------------------
	//		     (vertical distance of span from screen center)
	distance = FixedMul (planeheight, yslope[y]);

	// Use this to determine stepping values. Because the plane is always
	// viewed with constant z, knowing the distance from the span is enough
	// to do a rough approximation of the stepping values. In reality, you
	// should find the (u,v) coordinates at the left and right edges of the
	// span and step between them, but that involves more math (including
	// some divides).
	ds_xstep = FixedMul (xstepscale, distance) << 10;
	ds_ystep = FixedMul (ystepscale, distance) << 10;

	// Find the length of a 2D vector from the camera to the left edge of
	// the span in camera space. This is accomplished using some trig:
	//		    (distance)
	//		------------------
	//		 sin (view angle)
	length = FixedMul (distance, distscale[x1]);

	// Find the angle from the center of the screen to the start of the span.
	// This is also precalculated in the distscale array used above (minus the
	// baseangle rotation). Baseangle compensates for the player's view angle
	// and also for the texture's rotation relative to the world.
	angle = (baseangle + xtoviewangle[x1]) >> ANGLETOFINESHIFT;

	// Find the (u,v) coordinate of the left edge of the span by extending a
	// ray from the camera position out into texture space. (For all intents and
	// purposes, texture space is equivalent to world space here.) The (u,v) values
	// are multiplied by scaling factors for the plane to scale the texture.
	ds_xfrac = FixedMul (xscale, pviewx + FixedMul (finecosine[angle], length)) << 10;
	ds_yfrac = FixedMul (yscale, pviewy - FixedMul (finesine[angle], length)) << 10;

	if (fixedlightlev)
		ds_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		ds_colormap = fixedcolormap;
	else
	{
		// Determine lighting based on the span's distance from the viewer.
		index = distance >> LIGHTZSHIFT;
		
		if (index >= MAXLIGHTZ)
			index = MAXLIGHTZ-1;

		ds_colormap = planezlight[index] + basecolormap;
	}

#ifdef USEASM
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
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes (void)
{
	int i;
	
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

	if (!check)
	{
		check = (visplane_t *)Calloc (1, sizeof(*check) + sizeof(*check->top)*(screen->width*2));
		check->bottom = &check->top[screen->width+2];
	}
	else
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
						 fixed_t xoffs, fixed_t yoffs,
						 fixed_t xscale, fixed_t yscale, angle_t angle)
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
			basecolormap == check->colormap &&	// [RH] Add colormap check
			xscale == check->xscale &&
			yscale == check->yscale &&
			angle == check->angle
			)
		  return check;

	check = new_visplane (hash);		// killough

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xoffs = xoffs;				// killough 2/28/98: Save offsets
	check->yoffs = yoffs;
	check->xscale = xscale;
	check->yscale = yscale;
	check->angle = angle;
	check->colormap = basecolormap;		// [RH] Save colormap
	check->minx = viewwidth;			// Was SCREENWIDTH -- killough 11/98
	check->maxx = -1;
	
	memset (check->top, 0xff, sizeof(*check->top) * screen->width);
				
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
		new_pl->xscale = pl->xscale;
		new_pl->yscale = pl->yscale;
		new_pl->angle = pl->angle;
		new_pl->colormap = pl->colormap;	// [RH] Copy colormap
		pl = new_pl;
		pl->minx = start;
		pl->maxx = stop;
		memset (pl->top, 0xff, sizeof(*pl->top) * screen->width);
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
		R_MapPlane (t1, spanstart[t1], x-1);
	for (; b1 > b2 && b1 >= t1; b1--)
		R_MapPlane (b1, spanstart[b1] ,x-1);
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

	if (dc_yl <= dc_yh)
	{
		int angle = ((((viewangle + xtoviewangle[x])^skyflip)>>sky1shift) + frontpos)>>16;

		dc_texturefrac = dc_texturemid + (dc_yl - centery) * dc_iscale;
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

			top = dc_texturefrac >> FRACBITS;
			bottom = (dc_texturemid + (dc_yh - centery) * dc_iscale) >> FRACBITS;
			count = bottom - top + 1;

			source = R_GetColumn (frontskytex, angle) + top;
			angle = ((((viewangle + xtoviewangle[x])^skyflip)>>sky2shift) + backpos)>>16;
			source2 = R_GetColumn (backskytex, angle) + top;
			dest = composite + top;

			do
			{
				if (*source)
				{
					*dest++ = *source++;
					source2++;
				}
				else
				{
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

	if (!r_columnmethod.value)
	{
		for (x = pl->minx; x <= pl->maxx; x++)
		{
			dc_x = x;
			_skycolumn (colfunc, x);
		}
	}
	else
	{
		int stop = (pl->maxx+1) & ~3;

		x = pl->minx;

		if (x & 1)
		{
			dc_x = x;
			_skycolumn (colfunc, x);
			x++;
		}

		if (x & 2)
		{
			if (x < pl->maxx)
			{
				rt_initcols();
				dc_x = 0;
				_skycolumn (hcolfunc_pre, x);
				x++;
				dc_x = 1;
				_skycolumn (hcolfunc_pre, x);
				rt_draw2cols (0, x - 1);
				x++;
			}
			else if (x == pl->maxx)
			{
				dc_x = x;
				_skycolumn (colfunc, x);
				x++;
			}
		}

		while (x < stop)
		{
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

		if (pl->maxx == x)
		{
			dc_x = x;
			_skycolumn (colfunc, x);
			x++;
		}
		else if (pl->maxx > x)
		{
			rt_initcols();
			dc_x = 0;
			_skycolumn (hcolfunc_pre, x);
			x++;
			dc_x = 1;
			_skycolumn (hcolfunc_pre, x);
			rt_draw2cols (0, x - 1);
			if (++x <= pl->maxx)
			{
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

	ds_color = 3;

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
					frontpos = (-s->textureoffset) >> 6;

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
				int useflatnum = flattranslation[pl->picnum];

				ds_color += 4;	// [RH] color if r_drawflat is 1

				ds_source = (byte *)W_CacheLumpNum (firstflat + useflatnum,
										   PU_STATIC);

				if (flatwarp[useflatnum])
				{
					if ((!warpedflats[useflatnum]
						 && Z_Malloc (64*64, PU_STATIC, &warpedflats[useflatnum]))
						|| flatwarpedwhen[useflatnum] != level.time)
					{
						static byte buffer[64];
						int timebase = level.time*23;

						flatwarpedwhen[useflatnum] = level.time;
						byte *warped = warpedflats[useflatnum];

						int x;
						for (x = 63; x >= 0; x--)
						{
							int yt, yf = (finesine[(timebase+(x+17)*128)&FINEMASK]>>13) & 63;
							byte *source = ds_source + x;
							byte *dest = warped + x;
							for (yt = 64; yt; yt--, yf = (yf+1)&63, dest += 64)
								*dest = *(source+yf*64);
						}
						timebase = level.time*32;
						int y;
						for (y = 63; y >= 0; y--)
						{
							int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & 63;
							byte *source = warped + y*64;
							byte *dest = buffer;
							for (xt = 64; xt; xt--, xf = (xf+1)&63)
								*dest++ = *(source+xf);
							memcpy (warped+y*64, buffer, 64);
						}
						Z_ChangeTag (ds_source, PU_CACHE);
						ds_source = warped;
					}
					else
					{
						Z_ChangeTag (ds_source, PU_CACHE);
						ds_source = warpedflats[useflatnum];
						Z_ChangeTag (ds_source, PU_STATIC);
					}
				}

#ifdef USEASM
				if (ds_source != ds_cursource)
					R_SetSpanSource_ASM (ds_source);
#endif

				xscale = pl->xscale;
				yscale = pl->yscale;

				fixed_t cosine = finecosine[pl->angle >> ANGLETOFINESHIFT];
				fixed_t sine = finesine[pl->angle >> ANGLETOFINESHIFT];
				pviewx = FixedMul (viewx, cosine) - FixedMul (viewy, sine) + pl->xoffs;
				pviewy = -(FixedMul (viewx, sine) + FixedMul (viewy, cosine)) + pl->yoffs;

				angle_t angle = (viewangle - ANG90 + pl->angle) >> ANGLETOFINESHIFT;	// left to right mapping
				// Scale will be unit scale at SCREENWIDTH/2 distance
				xstepscale = FixedMul (xscale, FixedDiv (finecosine[angle], FocalLengthX));
				ystepscale = FixedMul (yscale, -FixedDiv (finesine[angle], FocalLengthX));

				baseangle = viewangle + pl->angle;

				basecolormap = pl->colormap;	// [RH] set basecolormap
				planeheight = abs (pl->height - viewz);
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

//==========================================================================
//
// R_PlaneInitData
//
//==========================================================================

BOOL R_PlaneInitData (void)
{
	int i;
	visplane_t *pl;

	if (floorclip)		delete[] floorclip;
	if (ceilingclip)	delete[] ceilingclip;
	if (spanstart)		delete[] spanstart;
	if (yslope)			delete[] yslope;
	if (distscale)		delete[] distscale;

	floorclip = new short[screen->width];
	ceilingclip = new short[screen->width];

	spanstart = new int[screen->height];

	yslope = new fixed_t[screen->height];
	distscale = new fixed_t[screen->width];

	// Free all visplanes and let them be re-allocated as needed.
	pl = freetail;

	while (pl)
	{
		visplane_t *next = pl->next;
		free (pl);
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
			free (pl);
			pl = next;
		}
	}

	return true;
}

//==========================================================================
//
// R_AlignFlat
//
//==========================================================================

BOOL R_AlignFlat (int linenum, int side, int fc)
{
	line_t *line = lines + linenum;
	sector_t *sec = side ? line->backsector : line->frontsector;

	if (!sec)
		return false;

	fixed_t x = line->v1->x;
	fixed_t y = line->v1->y;

	angle_t angle = R_PointToAngle2 (x, y, line->v2->x, line->v2->y);
	angle_t norm = (angle-ANGLE_90) >> ANGLETOFINESHIFT;

	fixed_t dist = -FixedMul (finecosine[norm], x) - FixedMul (finesine[norm], y);

	if (side)
	{
		angle = angle + ANGLE_180;
		dist = -dist;
	}

	if (fc)
	{
		sec->base_ceiling_angle = 0-angle;
		sec->base_ceiling_yoffs = dist & ((1<<(FRACBITS+8))-1);
	}
	else
	{
		sec->base_floor_angle = 0-angle;
		sec->base_floor_yoffs = dist & ((1<<(FRACBITS+8))-1);
	}

	return true;
}