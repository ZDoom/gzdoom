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
#include <stddef.h>

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
int*			walllights;		// [RH] Changed from lighttable_t** to int*

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

// [RH] Selects which version of the seg renderers to use.
cvar_t *r_columnmethod;
void (*R_RenderSegLoop)(void);


//
// R_RenderMaskedSegRange
//
static void BlastMaskedColumn (void (*blastfunc)(column_t *column), int texnum)
{
	if (maskedtexturecol[dc_x] != MAXSHORT)
	{
		// calculate lighting
		if (!fixedcolormap)
		{
			unsigned index = (spryscale*lightscalexmul)>>LIGHTSCALESHIFT;	// [RH]

			if (index >= MAXLIGHTSCALE)
				index = MAXLIGHTSCALE-1;

			dc_colormap = walllights[index] + basecolormap;	// [RH] add basecolormap
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
// [RH] This doesn't work properly as-is with freelook. Probably just me.
//				if (t + (__int64) textureheight[texnum] * spryscale < 0 ||
//					 t > (__int64) screen.height << FRACBITS*2)
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
		blastfunc ((column_t *)((byte *)R_GetColumn(texnum, maskedtexturecol[dc_x]) -3));
		maskedtexturecol[dc_x] = MAXSHORT;
	}
	spryscale += rw_scalestep;
}

void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2)
{
	int 		lightnum;
	int 		texnum;
	sector_t	tempsec;		// killough 4/13/98
	
	// Calculate light table.
	// Use different light tables
	//	 for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	curline = ds->curline;

	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable
	//		translucency maps
	if (!r_columnmethod->value) {
		if (curline->linedef->lucency < 240) {
			colfunc = lucentcolfunc;
			dc_transmap = TransTable + ((curline->linedef->lucency << 10) & 0x30000);
		} else
			colfunc = basecolfunc;
		// killough 4/11/98: end translucent 2s normal code
	} else {
		// [RH] Alternate drawer functions
		if (curline->linedef->lucency < 240) {
			colfunc = lucentcolfunc;
			hcolfunc_post1 = rt_lucent1col;
			hcolfunc_post2 = rt_lucent2cols;
			hcolfunc_post4 = rt_lucent4cols;
			dc_transmap = TransTable + ((curline->linedef->lucency << 10) & 0x30000);
		} else {
			hcolfunc_post1 = rt_map1col;
			hcolfunc_post2 = rt_map2cols;
			hcolfunc_post4 = rt_map4cols;
		}
	}

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	texnum = texturetranslation[curline->sidedef->midtexture];

	basecolormap = frontsector->colormap->maps;	// [RH] Set basecolormap

	// killough 4/13/98: get correct lightlevel for 2s normal textures
	lightnum = (R_FakeFlat(frontsector, &tempsec, NULL, NULL, false)
			->lightlevel >> LIGHTSEGSHIFT) + (foggy ? 0 : extralight);

	// [RH] Only do it if not foggy and allowed
	if (!foggy && !(level.flags & LEVEL_EVENLIGHTING)) {
		if (curline->v1->y == curline->v2->y)
			lightnum--;
		else if (curline->v1->x == curline->v2->x)
			lightnum++;
	}

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
						
	if (fixedlightlev)
		dc_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		dc_colormap = fixedcolormap;
	
	// draw the columns
	if (!r_columnmethod->value) {
		for (dc_x = x1; dc_x <= x2; dc_x++)
			BlastMaskedColumn (R_DrawMaskedColumn, texnum);
	} else {
		// [RH] Draw them through the temporary buffer
		int stop = (++x2) & ~3;

		if (x1 >= x2)
			return;

		dc_x = x1;

		if (dc_x & 1) {
			BlastMaskedColumn (R_DrawMaskedColumn, texnum);
			dc_x++;
		}

		if (dc_x & 2) {
			if (dc_x < x2 - 1) {
				rt_initcols();
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
				dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
				rt_draw2cols ((dc_x - 1) & 3, dc_x - 1);
				dc_x++;
			} else if (dc_x == x2 - 1) {
				BlastMaskedColumn (R_DrawMaskedColumn, texnum);
				dc_x++;
			}
		}

		while (dc_x < stop) {
			rt_initcols();
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			dc_x++;
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			dc_x++;
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			dc_x++;
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			rt_draw4cols (dc_x - 3);
			dc_x++;
		}

		if (x2 - dc_x == 1) {
			BlastMaskedColumn (R_DrawMaskedColumn, texnum);
		} else if (x2 - dc_x >= 2) {
			rt_initcols();
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			dc_x++;
			BlastMaskedColumn (R_DrawMaskedColumnHoriz, texnum);
			rt_draw2cols ((dc_x - 1) & 3, dc_x - 1);
			if (++dc_x < x2) {
				BlastMaskedColumn (R_DrawMaskedColumn, texnum);
			}
		}
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

static void BlastColumn (void (*blastfunc)())
{
	fixed_t texturecolumn;
	int yl, yh;

	// mark floor / ceiling areas
	yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

	// no space above wall?
	if (yl < ceilingclip[rw_x]+1)
		yl = ceilingclip[rw_x]+1;
	
	if (markceiling)
	{
		int top = ceilingclip[rw_x]+1;
		int bottom = yl-1;

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
		int top = yh+1;
		int bottom = floorclip[rw_x]-1;
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
		texturecolumn = rw_offset-FixedMul(finetangent[(rw_centerangle + xtoviewangle[rw_x])>>ANGLETOFINESHIFT],rw_distance);
		texturecolumn >>= FRACBITS;

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
		blastfunc ();
		ceilingclip[rw_x] = viewheight;
		floorclip[rw_x] = -1;
	}
	else
	{
		// two sided line
		if (toptexture)
		{
			// top wall
			int mid = pixhigh>>HEIGHTBITS;
			pixhigh += pixhighstep;

			if (mid >= floorclip[rw_x])
				mid = floorclip[rw_x]-1;

			if (mid >= yl)
			{
				dc_yl = yl;
				dc_yh = mid;
				dc_texturemid = rw_toptexturemid;
				dc_source = R_GetColumn(toptexture,texturecolumn);
				blastfunc ();
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
			int mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
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
				blastfunc ();
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


// [RH] This is DOOM's original R_RenderSegLoop() with most of the work
//		having been split off into a separate BlastColumn() function. It
//		just draws the columns straight to the frame buffer, and at least
//		on a Pentium II, can be slower than R_RenderSegLoop2().
void R_RenderSegLoop1 (void)
{
	if (fixedlightlev)
		dc_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		dc_colormap = fixedcolormap;
	else if (!walllights)
		walllights = scalelight[0];

	for ( ; rw_x < rw_stopx ; rw_x++)
	{
		dc_x = rw_x;
		if (!fixedcolormap) {
			// calculate lighting
			unsigned index = (rw_scale*lightscalexmul)>>LIGHTSCALESHIFT;

			if (index >= MAXLIGHTSCALE)
				index = MAXLIGHTSCALE-1;

			dc_colormap = walllights[index] + basecolormap;	// [RH] add basecolormap
		}
		BlastColumn (colfunc);
	}
}

// [RH] This is a cache optimized version of R_RenderSegLoop(). It first
//		draws columns into a temporary buffer with a pitch of 4 and then
//		copies them to the framebuffer using a bunch of byte, word, and
//		longword moves. This may seem like a lot of extra work just to
//		draw columns to the screen (and it is), but it's actually faster
//		than drawing them directly to the screen like R_RenderSegLoop1().
//		On a Pentium II 300, using this code with rendering functions in
//		C is about twice as fast as using R_RenderSegLoop1() with an
//		assembly rendering function.

void R_RenderSegLoop2 (void)
{
	int stop = rw_stopx & ~3;

	if (rw_x >= rw_stopx)
		return;

	if (fixedlightlev)
		dc_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		dc_colormap = fixedcolormap;
	else {
		// calculate lighting
		unsigned index = (rw_scale*lightscalexmul)>>LIGHTSCALESHIFT;

		if (index >= MAXLIGHTSCALE)
			index = MAXLIGHTSCALE-1;

		if (!walllights)
			walllights = scalelight[0];

		dc_colormap = walllights[index] + basecolormap;	// [RH] add basecolormap
	}

	if (rw_x & 1) {
		dc_x = rw_x;
		BlastColumn (colfunc);
		rw_x++;
	}

	if (rw_x & 2) {
		if (rw_x < rw_stopx - 1) {
			rt_initcols();
			dc_x = 0;
			BlastColumn (hcolfunc_pre);
			rw_x++;
			dc_x = 1;
			BlastColumn (hcolfunc_pre);
			rt_draw2cols (0, rw_x - 1);
			rw_x++;
		} else if (rw_x == rw_stopx - 1) {
			dc_x = rw_x;
			BlastColumn (colfunc);
			rw_x++;
		}
	}

	while (rw_x < stop) {
		if (!fixedcolormap) {
			// calculate lighting
			unsigned index = (rw_scale*lightscalexmul)>>LIGHTSCALESHIFT;

			if (index >= MAXLIGHTSCALE)
				index = MAXLIGHTSCALE-1;

			dc_colormap = walllights[index] + basecolormap;	// [RH] add basecolormap
		}
		rt_initcols();
		dc_x = 0;
		BlastColumn (hcolfunc_pre);
		rw_x++;
		dc_x = 1;
		BlastColumn (hcolfunc_pre);
		rw_x++;
		dc_x = 2;
		BlastColumn (hcolfunc_pre);
		rw_x++;
		dc_x = 3;
		BlastColumn (hcolfunc_pre);
		rt_draw4cols (rw_x - 3);
		rw_x++;
	}

	if (!fixedcolormap) {
		// calculate lighting
		unsigned index = (rw_scale*lightscalexmul)>>LIGHTSCALESHIFT;

		if (index >= MAXLIGHTSCALE)
			index = MAXLIGHTSCALE-1;

		dc_colormap = walllights[index] + basecolormap;	// [RH] add basecolormap
	}

	if (rw_stopx - rw_x == 1) {
		dc_x = rw_x;
		BlastColumn (colfunc);
		rw_x++;
	} else if (rw_stopx - rw_x >= 2) {
		rt_initcols();
		dc_x = 0;
		BlastColumn (hcolfunc_pre);
		rw_x++;
		dc_x = 1;
		BlastColumn (hcolfunc_pre);
		rt_draw2cols (0, rw_x - 1);
		if (++rw_x < rw_stopx) {
			dc_x = rw_x;
			BlastColumn (colfunc);
			rw_x++;
		}
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
	int					flatcolor;		// [RH] Color of planes if r_drawflat is 1

#ifdef RANGECHECK
	if (start >=viewwidth || start > stop)
		I_FatalError ("Bad R_StoreWallRange: %i to %i", start , stop);
#endif

	// don't overflow and crash
	if (ds_p == &drawsegs[MaxDrawSegs]) {
		// [RH] Grab some more drawsegs
		size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs*2 : 32;
		drawsegs = Realloc (drawsegs, newdrawsegs * sizeof(drawseg_t));
		ds_p = drawsegs + MaxDrawSegs;
		MaxDrawSegs = newdrawsegs;
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
	hyp = (viewx == curline->v1->x && viewy == curline->v1->y) ?
		0 : R_PointToDist (curline->v1->x, curline->v1->y);
	sineval = finesine[distangle>>ANGLETOFINESHIFT];
	rw_distance = FixedMul (hyp, sineval);

	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop;
	ds_p->curline = curline;
	rw_stopx = stop+1;

	{	// killough 1/6/98, 2/1/98: remove limit on openings
		ptrdiff_t pos = lastopening - openings;
		size_t need = (rw_stopx - start)*4 + pos;

		if (need > maxopenings)
		{
			drawseg_t *ds;
			short *oldopenings = openings;
			short *oldlast = lastopening;

			do
				maxopenings = maxopenings ? maxopenings*2 : 16384;
			while (need > maxopenings);
			openings = Realloc (openings, maxopenings * sizeof(*openings));
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
	
	if (stop > start)
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
			fixed_t vtop = frontsector->floorheight + textureheight[sidedef->midtexture];
			// bottom of texture at bottom
			rw_midtexturemid = vtop - viewz;	
		}
		else
		{
			// top of texture at top
			rw_midtexturemid = worldtop;
		}

		rw_midtexturemid += sidedef->rowoffset;

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
		if (frontsector->ceilingpic == skyflatnum && backsector->ceilingpic == skyflatnum)
		{
			worldtop = worldhigh;
		}

		if (spanfunc == R_FillSpan) {
			markfloor = markceiling = frontsector != backsector;
			flatcolor = ((frontsector - sectors) & 31) * 4;
		} else {
			markfloor = worldlow != worldbottom
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->floorpic != frontsector->floorpic

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->floor_xoffs != frontsector->floor_xoffs
				|| backsector->floor_yoffs != frontsector->floor_yoffs

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| frontsector->heightsec != -1

				// killough 4/17/98: draw floors if different light levels
				|| backsector->floorlightsec != frontsector->floorlightsec

				// [RH] Add checks for colormaps
				|| backsector->colormap != frontsector->colormap
				;

			markceiling = worldhigh != worldtop
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->ceilingpic != frontsector->ceilingpic

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->ceiling_xoffs != frontsector->ceiling_xoffs
				|| backsector->ceiling_yoffs != frontsector->ceiling_yoffs

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (frontsector->heightsec != -1 &&
					frontsector->ceilingpic!=skyflatnum)

				// killough 4/17/98: draw ceilings if different light levels
				|| backsector->ceilinglightsec != frontsector->ceilinglightsec

				// [RH] Add check for colormaps
				|| backsector->colormap != frontsector->colormap
				;
		}

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
				fixed_t vtop = backsector->ceilingheight + textureheight[sidedef->toptexture];
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
		rw_bottomtexturemid += sidedef->rowoffset;

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
			int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)
					+ (foggy ? 0 : extralight);

			// [RH] Only do it if not foggy and allowed
			if (!foggy && !(level.flags & LEVEL_EVENLIGHTING)) {
				if (curline->v1->y == curline->v2->y)
					lightnum--;
				else if (curline->v1->x == curline->v2->x)
					lightnum++;
			}

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

	bottomstep = -FixedMul (rw_scalestep, worldbottom);
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
	if (markceiling) {
		if (ceilingplane) {	// killough 4/11/98: add NULL ptr checks
			ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
			ceilingplane->color = flatcolor;	// [RH] for r_drawflat 1
		} else
			markceiling = 0;
	}
	
	if (markfloor) {
		if (floorplane)	{	// killough 4/11/98: add NULL ptr checks
			floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
			floorplane->color = flatcolor;	// [RH] for r_drawflat 1
		} else
			markfloor = 0;
	}

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

