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
//		All the clipping: columns, horizontal spans, sky columns.
//
//-----------------------------------------------------------------------------






#include "m_alloc.h"
#include <stdlib.h>

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

static BOOL		segtextured;	// True if any of the segs textures might be visible.
static BOOL		markfloor;		// False if the back side is the same plane.
static BOOL		markceiling;
static BOOL		maskedtexture;
static int		toptexture;
static int		bottomtexture;
static int		midtexture;


angle_t 		rw_normalangle;	// angle to line origin
int 			rw_angle1;
fixed_t 		rw_distance;
lighttable_t**	walllights;

//
// regular wall
//
static int		rw_x;
static int		rw_stopx;
static angle_t	rw_centerangle;
static fixed_t	rw_offset;
static fixed_t	rw_scale;
static fixed_t	rw_scalestep;
static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;

static int		worldtop;
static int		worldbottom;
static int		worldhigh;
static int		worldlow;

static fixed_t	pixhigh;
static fixed_t	pixlow;
static fixed_t	pixhighstep;
static fixed_t	pixlowstep;

static fixed_t	topfrac;
static fixed_t	topstep;

static fixed_t	bottomfrac;
static fixed_t	bottomstep;

static short	*maskedtexturecol;


//
// R_RenderMaskedSegRange
//
void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2)
{
	column_t*	col;
	int 		lightnum;
	int 		texnum;
	sector_t	tempsec;		// killough 4/13/98
	
	// Calculate light table.
	// Use different light tables
	//	 for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	curline = ds->curline;

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	texnum = texturetranslation[curline->sidedef->midtexture];
		
	// killough 4/13/98: get correct lightlevel for 2s normal textures
	lightnum = (R_FakeFlat(frontsector, &tempsec, NULL, NULL, false)
				->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (curline->v1->y == curline->v2->y)
		lightnum--;
	else if (curline->v1->x == curline->v2->x)
		lightnum++;

	walllights = lightnum >= LIGHTLEVELS ? scalelight[LIGHTLEVELS-1] :
		lightnum <  0           ? scalelight[0] : scalelight[lightnum];

	maskedtexturecol = ds->maskedtexturecol;

	rw_scalestep = ds->scalestep;				
	spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
	mfloorclip = ds->sprbottomclip;
	mceilingclip = ds->sprtopclip;
	
	// find positioning
	if (curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		dc_texturemid = frontsector->floorheight > backsector->floorheight
			? frontsector->floorheight : backsector->floorheight;
		dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
	}
	else
	{
		dc_texturemid =frontsector->ceilingheight<backsector->ceilingheight
			? frontsector->ceilingheight : backsector->ceilingheight;
		dc_texturemid = dc_texturemid - viewz;
	}
	dc_texturemid += curline->sidedef->rowoffset;
						
	if (fixedcolormap)
		dc_colormap = fixedcolormap;
	
	// draw the columns
	for (dc_x = x1 ; dc_x <= x2 ; dc_x++)
	{
		if (maskedtexturecol[dc_x] != MAXSHORT)
		{
			// calculate lighting
			if (!fixedcolormap)
			{
				unsigned index = spryscale>>LIGHTSCALESHIFT;

				if (index >= MAXLIGHTSCALE)
					index = MAXLIGHTSCALE-1;

				dc_colormap = walllights[index];
			}
						
			// killough 3/2/98:
			//
			// This calculation used to overflow and cause crashes in Doom:
			//
			// sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
			//
			// This code fixes it, by using double-precision intermediate
			// arithmetic and by skipping the drawing of 2s normals whose
			// mapping to screen coordinates is totally out of range:

			{
				__int64 t = ((__int64) centeryfrac << FRACBITS) -
					(__int64) dc_texturemid * spryscale;
// [RH] This doesn't work properly as-is with freelook
//				if (t + (__int64) textureheight[texnum] * spryscale < 0 ||
//					 t > (__int64) screens[0].height << FRACBITS*2)
//					continue;		// skip if the texture is out of screen's range
				sprtopscreen = (long)(t >> FRACBITS);
			}
			dc_iscale = 0xffffffffu / (unsigned)spryscale;
			
			// killough 1/25/98: here's where Medusa came in, because
			// it implicitly assumed that the column was all one patch.
			// Originally, Doom did not construct complete columns for
			// multipatched textures, so there were no header or trailer
			// bytes in the column referred to below, which explains
			// the Medusa effect. The fix is to construct true columns
			// when forming multipatched textures (see r_data.c).

			// draw the texture
			col = (column_t *)( 
				(byte *)R_GetColumn(texnum,maskedtexturecol[dc_x]) -3);
						
			R_DrawMaskedColumn (col);
			maskedtexturecol[dc_x] = MAXSHORT;
		}
		spryscale += rw_scalestep;
	}
		
}




//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//	texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//	textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS				12
#define HEIGHTUNIT				(1<<HEIGHTBITS)

void R_RenderSegLoop (void)
{
	angle_t 			angle;
	unsigned			index;
	int 				yl;
	int 				yh;
	int 				mid;
	fixed_t 			texturecolumn;
	int 				top;
	int 				bottom;

	//texturecolumn = 0;								// shut up compiler warning
		
	for ( ; rw_x < rw_stopx ; rw_x++)
	{
		// mark floor / ceiling areas
		yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

		// no space above wall?
		if (yl < ceilingclip[rw_x]+1)
			yl = ceilingclip[rw_x]+1;
		
		if (markceiling)
		{
			top = ceilingclip[rw_x]+1;
			bottom = yl-1;

			if (bottom >= floorclip[rw_x])
				bottom = floorclip[rw_x]-1;

			if (top <= bottom)
			{
				ceilingplane->top[rw_x] = top;
				ceilingplane->bottom[rw_x] = bottom;
			}
		}
				
		yh = bottomfrac>>HEIGHTBITS;

		if (yh >= floorclip[rw_x])
			yh = floorclip[rw_x]-1;

		if (markfloor)
		{
			top = yh+1;
			bottom = floorclip[rw_x]-1;
			if (top <= ceilingclip[rw_x])
				top = ceilingclip[rw_x]+1;
			if (top <= bottom)
			{
				floorplane->top[rw_x] = top;
				floorplane->bottom[rw_x] = bottom;
			}
		}
		
		// texturecolumn and lighting are independent of wall tiers
		if (segtextured)
		{
			// calculate texture offset
			angle = (rw_centerangle + xtoviewangle[rw_x])>>ANGLETOFINESHIFT;
			texturecolumn = rw_offset-FixedMul(finetangent[angle],rw_distance);
			texturecolumn >>= FRACBITS;
			// calculate lighting
			index = rw_scale>>LIGHTSCALESHIFT;

			if (index >=  MAXLIGHTSCALE )
				index = MAXLIGHTSCALE-1;

			dc_colormap = walllights[index];
			dc_x = rw_x;
			dc_iscale = 0xffffffffu / (unsigned)rw_scale;
		}
		
		// draw the wall tiers
		if (midtexture)
		{
			// single sided line
			dc_yl = yl;
			dc_yh = yh;
			dc_texturemid = rw_midtexturemid;
			dc_source = R_GetColumn(midtexture,texturecolumn);
			colfunc ();
			ceilingclip[rw_x] = viewheight;
			floorclip[rw_x] = -1;
		}
		else
		{
			// two sided line
			if (toptexture)
			{
				// top wall
				mid = pixhigh>>HEIGHTBITS;
				pixhigh += pixhighstep;

				if (mid >= floorclip[rw_x])
					mid = floorclip[rw_x]-1;

				if (mid >= yl)
				{
					dc_yl = yl;
					dc_yh = mid;
					dc_texturemid = rw_toptexturemid;
					dc_source = R_GetColumn(toptexture,texturecolumn);
					colfunc ();
					ceilingclip[rw_x] = mid;
				}
				else
					ceilingclip[rw_x] = yl-1;
			}
			else
			{
				// no top wall
				if (markceiling)
					ceilingclip[rw_x] = yl-1;
			}
						
			if (bottomtexture)
			{
				// bottom wall
				mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
				pixlow += pixlowstep;

				// no space above wall?
				if (mid <= ceilingclip[rw_x])
					mid = ceilingclip[rw_x]+1;
				
				if (mid <= yh)
				{
					dc_yl = mid;
					dc_yh = yh;
					dc_texturemid = rw_bottomtexturemid;
					dc_source = R_GetColumn(bottomtexture,
											texturecolumn);
					colfunc ();
					floorclip[rw_x] = mid;
				}
				else
					floorclip[rw_x] = yh+1;
			}
			else
			{
				// no bottom wall
				if (markfloor)
					floorclip[rw_x] = yh+1;
			}
						
			if (maskedtexture)
			{
				// save texturecol
				//	for backdrawing of masked mid texture
				maskedtexturecol[rw_x] = texturecolumn;
			}
		}
				
		rw_scale += rw_scalestep;
		topfrac += topstep;
		bottomfrac += bottomstep;
	}
}

// killough 5/2/98: move from r_main.c, made static, simplified

static fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
	fixed_t dx = abs(x - viewx);
	fixed_t dy = abs(y - viewy);

	if (dy > dx)
	{
		fixed_t t = dx;
		dx = dy;
		dy = t;
	}

	return FixedDiv(dx, finesine[(tantoangle[FixedDiv(dy,dx) >> DBITS]
								  + ANG90) >> ANGLETOFINESHIFT]);
}


//
// R_StoreWallRange
// A wall segment will be drawn
//	between start and stop pixels (inclusive).
//
extern short *openings;
extern size_t maxopenings;

void R_StoreWallRange (int start, int stop)
{
	fixed_t 			hyp;
	fixed_t 			sineval;
	angle_t 			distangle, offsetangle;
	fixed_t 			vtop;
	int 				lightnum;
				
#ifdef RANGECHECK
	if (start >=viewwidth || start > stop)
		I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

	// don't overflow and crash
	if (ds_p == &drawsegs[MaxDrawSegs]) {
		unsigned pos = ds_p - drawsegs;
		// [RH] Grab some more drawsegs
		MaxDrawSegs = MaxDrawSegs ? MaxDrawSegs*2 : 32;
		drawsegs = Realloc (drawsegs, MaxDrawSegs * sizeof(drawseg_t));
		ds_p = drawsegs + pos;
		DPrintf ("MaxDrawSegs increased to %d\n", MaxDrawSegs);
	}
	
	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;
	
	// calculate rw_distance for scale calculation
	rw_normalangle = curline->angle + ANG90;
	offsetangle = abs(rw_normalangle-rw_angle1);
	
	if (offsetangle > ANG90)
		offsetangle = ANG90;

	distangle = ANG90 - offsetangle;
	hyp = R_PointToDist (curline->v1->x, curline->v1->y);
	sineval = finesine[distangle>>ANGLETOFINESHIFT];
	rw_distance = FixedMul (hyp, sineval);
				
		
	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop;
	ds_p->curline = curline;
	rw_stopx = stop+1;
	
	{	// killough 1/6/98, 2/1/98: remove limit on openings
		size_t pos = lastopening - openings;
		size_t need = (rw_stopx - start)*4 + pos;

		if (need > maxopenings)
		{
			drawseg_t *ds;
			short *oldopenings = openings;
			short *oldlast = lastopening;

			do
				maxopenings = maxopenings ? maxopenings*2 : 16384;
			while (need > maxopenings);
			openings = Realloc(openings, maxopenings * sizeof(*openings));
			lastopening = openings + pos;
			DPrintf ("MaxOpenings increased to %u\n", maxopenings);

			// [RH] We also need to adjust the openings pointers that
			//		were already stored in drawsegs.
			for (ds = drawsegs; ds < ds_p; ds++) {
#define ADJUST(p) if (ds->p + ds->x1 >= oldopenings && ds->p + ds->x1 <= oldlast)\
					  ds->p = ds->p - oldopenings + openings;
				ADJUST (maskedtexturecol);
				ADJUST (sprtopclip);
				ADJUST (sprbottomclip);
			}
#undef ADJUST
		}
	}  // killough: end of code to remove limits on openings

	// calculate scale at both ends and step
	ds_p->scale1 = rw_scale = 
		R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start]);
	
	if (stop > start )
	{
		ds_p->scale2 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop]);
		ds_p->scalestep = rw_scalestep = 
			(ds_p->scale2 - rw_scale) / (stop-start);
	}
	else
	{
		ds_p->scale2 = ds_p->scale1;
	}
	
	// calculate texture boundaries
	//	and decide if floor / ceiling marks are needed
	worldtop = frontsector->ceilingheight - viewz;
	worldbottom = frontsector->floorheight - viewz;
		
	midtexture = toptexture = bottomtexture = maskedtexture = 0;
	ds_p->maskedtexturecol = NULL;
		
	if (!backsector)
	{
		// single sided line
		midtexture = texturetranslation[sidedef->midtexture];

		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;

		if (linedef->flags & ML_DONTPEGBOTTOM)
		{
			vtop = frontsector->floorheight +
				textureheight[sidedef->midtexture];
			// bottom of texture at bottom
			rw_midtexturemid = vtop - viewz;	
		}
		else
		{
			// top of texture at top
			rw_midtexturemid = worldtop;
		}

		rw_midtexturemid += sidedef->rowoffset;

		{	// killough 3/27/98: reduce offset
			/* [RH] Not necessary unless I do BOOM's fix for wrapping
			 * textures of any height
			fixed_t h = textureheight[sidedef->midtexture];
			if (h & (h-FRACUNIT))
				rw_midtexturemid %= h;
			 */
		}

		ds_p->silhouette = SIL_BOTH;
		ds_p->sprtopclip = screenheightarray;
		ds_p->sprbottomclip = negonearray;
		ds_p->bsilheight = MAXINT;
		ds_p->tsilheight = MININT;
	}
	else
	{
		// two sided line
		ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
		ds_p->silhouette = 0;
		
		if (frontsector->floorheight > backsector->floorheight)
		{
			ds_p->silhouette = SIL_BOTTOM;
			ds_p->bsilheight = frontsector->floorheight;
		}
		else if (backsector->floorheight > viewz)
		{
			ds_p->silhouette = SIL_BOTTOM;
			ds_p->bsilheight = MAXINT;
		}
		
		if (frontsector->ceilingheight < backsector->ceilingheight)
		{
			ds_p->silhouette |= SIL_TOP;
			ds_p->tsilheight = frontsector->ceilingheight;
		}
		else if (backsector->ceilingheight < viewz)
		{
			ds_p->silhouette |= SIL_TOP;
			ds_p->tsilheight = MININT;
		}
				
		if (backsector->ceilingheight <= frontsector->floorheight)
		{
			ds_p->sprbottomclip = negonearray;
			ds_p->bsilheight = MAXINT;
			ds_p->silhouette |= SIL_BOTTOM;
		}
		
		if (backsector->floorheight >= frontsector->ceilingheight)
		{
			ds_p->sprtopclip = screenheightarray;
			ds_p->tsilheight = MININT;
			ds_p->silhouette |= SIL_TOP;
		}
		
		// killough 1/17/98: this test is required if the fix
		// for the automap bug (r_bsp.c) is used, or else some
		// sprites will be displayed behind closed doors. That
		// fix prevents lines behind closed doors with dropoffs
		// from being displayed on the automap.
		//
		// killough 4/7/98: make doorclosed external variable

		{
			extern int doorclosed;	// killough 1/17/98, 2/8/98, 4/7/98
			if (doorclosed || backsector->ceilingheight<=frontsector->floorheight)
			{
				ds_p->sprbottomclip = negonearray;
				ds_p->bsilheight = MAXINT;
				ds_p->silhouette |= SIL_BOTTOM;
			}
			if (doorclosed || backsector->floorheight>=frontsector->ceilingheight)
			{						// killough 1/17/98, 2/8/98
				ds_p->sprtopclip = screenheightarray;
				ds_p->tsilheight = MININT;
				ds_p->silhouette |= SIL_TOP;
			}
		}

		worldhigh = backsector->ceilingheight - viewz;
		worldlow = backsector->floorheight - viewz;
				
		// hack to allow height changes in outdoor areas
		if (frontsector->ceilingpic == skyflatnum 
			&& backsector->ceilingpic == skyflatnum)
		{
			worldtop = worldhigh;
		}
		
						
		markfloor = worldlow != worldbottom
			|| backsector->floorpic != frontsector->floorpic
			|| backsector->lightlevel != frontsector->lightlevel

			// killough 3/7/98: Add checks for (x,y) offsets
			|| backsector->floor_xoffs != frontsector->floor_xoffs
			|| backsector->floor_yoffs != frontsector->floor_yoffs

			// killough 4/15/98: prevent 2s normals
			// from bleeding through deep water
			|| frontsector->heightsec != -1

			// killough 4/17/98: draw floors if different light levels
			|| backsector->floorlightsec != frontsector->floorlightsec
			;

		markceiling = worldhigh != worldtop
			|| backsector->ceilingpic != frontsector->ceilingpic
			|| backsector->lightlevel != frontsector->lightlevel

			// killough 3/7/98: Add checks for (x,y) offsets
			|| backsector->ceiling_xoffs != frontsector->ceiling_xoffs
			|| backsector->ceiling_yoffs != frontsector->ceiling_yoffs

			// killough 4/15/98: prevent 2s normals
			// from bleeding through fake ceilings
			|| (frontsector->heightsec != -1 &&
				frontsector->ceilingpic!=skyflatnum)

			// killough 4/17/98: draw ceilings if different light levels
			|| backsector->ceilinglightsec != frontsector->ceilinglightsec
			;

		if (backsector->ceilingheight <= frontsector->floorheight
			|| backsector->floorheight >= frontsector->ceilingheight)
		{
			// closed door
			markceiling = markfloor = true;
		}
		

		if (worldhigh < worldtop)
		{
			// top texture
			toptexture = texturetranslation[sidedef->toptexture];
			if (linedef->flags & ML_DONTPEGTOP)
			{
				// top of texture at top
				rw_toptexturemid = worldtop;
			}
			else
			{
				vtop =
					backsector->ceilingheight
					+ textureheight[sidedef->toptexture];
				
				// bottom of texture
				rw_toptexturemid = vtop - viewz;		
			}
		}
		if (worldlow > worldbottom)
		{
			// bottom texture
			bottomtexture = texturetranslation[sidedef->bottomtexture];

			if (linedef->flags & ML_DONTPEGBOTTOM )
			{
				// bottom of texture at bottom
				// top of texture at top
				rw_bottomtexturemid = worldtop;
			}
			else		// top of texture at top
				rw_bottomtexturemid = worldlow;
		}
		rw_toptexturemid += sidedef->rowoffset;

		// killough 3/27/98: reduce offset
		{
			/* [RH] Not necessary unless I do BOOM's fix for wrapping
			 * textures of any height
			fixed_t h = textureheight[sidedef->toptexture];
			if (h & (h-FRACUNIT))
				rw_toptexturemid %= h;
			 */
		}

		rw_bottomtexturemid += sidedef->rowoffset;

		// killough 3/27/98: reduce offset
		{
			/* [RH] Not necessary unless I do BOOM's fix for wrapping
			 * textures of any height
			fixed_t h = textureheight[sidedef->bottomtexture];
			if (h & (h-FRACUNIT))
				rw_bottomtexturemid %= h;
			 */
		}
		
		// allocate space for masked texture tables
		if (sidedef->midtexture)
		{
			// masked midtexture
			maskedtexture = true;
			ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
			lastopening += rw_stopx - rw_x;
		}
	}
	
	// calculate rw_offset (only needed for textured lines)
	segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

	if (segtextured)
	{
		offsetangle = rw_normalangle-rw_angle1;
		
		if (offsetangle > ANG180)
			offsetangle = (unsigned)(-(int)offsetangle);
		else if (offsetangle > ANG90)
			offsetangle = ANG90;

		sineval = finesine[offsetangle >>ANGLETOFINESHIFT];
		rw_offset = FixedMul (hyp, sineval);

		if (rw_normalangle-rw_angle1 < ANG180)
			rw_offset = -rw_offset;

		rw_offset += sidedef->textureoffset + curline->offset;
		rw_centerangle = ANG90 + viewangle - rw_normalangle;
		
		// calculate light table
		//	use different light tables
		//	for horizontal / vertical / diagonal
		// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
		if (!fixedcolormap)
		{
			lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

			if (curline->v1->y == curline->v2->y)
				lightnum--;
			else if (curline->v1->x == curline->v2->x)
				lightnum++;

			if (lightnum < 0)			
				walllights = scalelight[0];
			else if (lightnum >= LIGHTLEVELS)
				walllights = scalelight[LIGHTLEVELS-1];
			else
				walllights = scalelight[lightnum];
		}
	}
	
	// if a floor / ceiling plane is on the wrong side
	//	of the view plane, it is definitely invisible
	//	and doesn't need to be marked.
	
  
	// killough 3/7/98: add deep water check
	if (frontsector->heightsec == -1)
	{
		if (frontsector->floorheight >= viewz)       // above view plane
			markfloor = false;
		if (frontsector->ceilingheight <= viewz &&
			frontsector->ceilingpic != skyflatnum)   // below view plane
			markceiling = false;
	}

	// calculate incremental stepping values for texture edges
	worldtop >>= 4;
	worldbottom >>= 4;
		
	topstep = -FixedMul (rw_scalestep, worldtop);
	topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

	bottomstep = -FixedMul (rw_scalestep,worldbottom);
	bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);
		
	if (backsector)
	{	
		worldhigh >>= 4;
		worldlow >>= 4;

		if (worldhigh < worldtop)
		{
			pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
			pixhighstep = -FixedMul (rw_scalestep,worldhigh);
		}
		
		if (worldlow > worldbottom)
		{
			pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
			pixlowstep = -FixedMul (rw_scalestep,worldlow);
		}
	}
	
	// render it
	if (markceiling)
	    if (ceilingplane)	// killough 4/11/98: add NULL ptr checks
			ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
		else
			markceiling = 0;
	
	if (markfloor)
	    if (floorplane)		// killough 4/11/98: add NULL ptr checks
			floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
		else
			markfloor = 0;

	R_RenderSegLoop ();

	
	// save sprite clipping info
	if ( ((ds_p->silhouette & SIL_TOP) || maskedtexture) && !ds_p->sprtopclip)
	{
		memcpy (lastopening, ceilingclip+start, sizeof(*lastopening)*(rw_stopx-start));
		ds_p->sprtopclip = lastopening - start;
		lastopening += rw_stopx - start;
	}
	
	if ( ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && !ds_p->sprbottomclip)
	{
		memcpy (lastopening, floorclip+start, sizeof(*lastopening)*(rw_stopx-start));
		ds_p->sprbottomclip = lastopening - start;
		lastopening += rw_stopx - start;		
	}

	if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
	{
		ds_p->silhouette |= SIL_TOP;
		ds_p->tsilheight = MININT;
	}
	if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
	{
		ds_p->silhouette |= SIL_BOTTOM;
		ds_p->bsilheight = MAXINT;
	}

	ds_p++;
}

