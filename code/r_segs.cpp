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
//		All the clipping: columns, horizontal spans, sky columns.
//
// This file contains some code from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
//-----------------------------------------------------------------------------





#include "m_alloc.h"
#include <stdlib.h>
#include <stddef.h>

#include "templates.h"
#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"
#include "p_lnspec.h"

#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "w_wad.h"
#include "z_zone.h"

#define WALLYREPEAT 8

extern fixed_t globaluclip, globaldclip;


// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

static BOOL		segtextured;	// True if any of the segs textures might be visible.
static bool		markfloor;		// False if the back side is the same plane.
static bool		markceiling;
static int		toptexture;
static int		bottomtexture;
static int		midtexture;

int OWallMost (short *mostbuf, fixed_t z);
int WallMost (short *mostbuf, const secplane_t &plane);
void PrepWall (fixed_t *swall, fixed_t *lwall, int walxrepeat);
extern fixed_t WallSZ1, WallSZ2, WallTX1, WallTX2, WallTY1, WallTY2;
extern int WallSX1, WallSX2;
extern float WallUoverZorg, WallUoverZstep, WallInvZorg, WallInvZstep, WallDepthScale, WallDepthOrg;

static int		wallshade;

short	walltop[MAXWIDTH];	// [RH] record max extents of wall
short	wallbottom[MAXWIDTH];
short	wallupper[MAXWIDTH];
short	walllower[MAXWIDTH];
static fixed_t	swall[MAXWIDTH];
static fixed_t	lwall[MAXWIDTH];

//
// regular wall
//
extern fixed_t	rw_backcz1, rw_backcz2;
extern fixed_t	rw_backfz1, rw_backfz2;
extern fixed_t	rw_frontcz1, rw_frontcz2;
extern fixed_t	rw_frontfz1, rw_frontfz2;

int				rw_ceilstat, rw_floorstat;
bool			rw_mustmarkfloor, rw_mustmarkceiling;
bool			rw_prepped;
bool			rw_markmirror;
bool			rw_havehigh;
bool			rw_havelow;

fixed_t			rw_light;		// [RH] Scale lights with viewsize adjustments
fixed_t			rw_lightstep;
fixed_t			rw_lightleft;

static int		rw_x;
static int		rw_stopx;
static fixed_t	rw_offset;
static fixed_t	rw_scale;
static fixed_t	rw_scalestep;
static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;

static fixed_t	topfrac;
static fixed_t	topstep;

static fixed_t	bottomfrac;
static fixed_t	bottomstep;

static short	*maskedtexturecol;
static int		WallSpriteTile;

void (*R_RenderSegLoop)(void);

static void R_RenderBoundWallSprite (AActor *first, drawseg_t *clipper, int pass);
static void WallSpriteColumn (void (*drawfunc)(column_t *, int));

//=============================================================================
//
// CVAR r_drawmirrors
//
// Set to false to disable rendering of mirrors
//=============================================================================

CVAR(Bool, r_drawmirrors, true, 0);

//
// R_RenderMaskedSegRange
//
fixed_t *MaskedSWall;

static void BlastMaskedColumn (void (*blastfunc)(column_t *, int), int texnum)
{
	if (maskedtexturecol[dc_x] != SHRT_MAX)
	{
		// calculate lighting
		if (!fixedcolormap)
		{
			dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
		}

		dc_iscale = MaskedSWall[dc_x];
		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);
		
		// killough 1/25/98: here's where Medusa came in, because
		// it implicitly assumed that the column was all one patch.
		// Originally, Doom did not construct complete columns for
		// multipatched textures, so there were no header or trailer
		// bytes in the column referred to below, which explains
		// the Medusa effect. The fix is to construct true columns
		// when forming multipatched textures (see r_data.c).

		// draw the texture
		blastfunc ((column_t *)((byte *)R_GetColumn(texnum, maskedtexturecol[dc_x]) -3), FIXED_MAX);
		maskedtexturecol[dc_x] = SHRT_MAX;
	}
	rw_light += rw_lightstep;
	spryscale += rw_scalestep;
}

void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2)
{
	int 		lightnum;
	int 		texnum;
	int			i;
	sector_t	tempsec;		// killough 4/13/98

	sprflipvert = false;

	// Calculate light table.
	curline = ds->curline;

	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable
	//		translucency maps
	ESPSResult drawmode = R_SetPatchStyle (STYLE_Translucent,
		curline->linedef->alpha < 255 ? curline->linedef->alpha<<8 : FRACUNIT,
		NULL, 0);

	if (drawmode == DontDraw)
	{
		return;
	}

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	texnum = texturetranslation[curline->sidedef->midtexture];

	basecolormap = frontsector->floorcolormap->Maps;	// [RH] Set basecolormap

	// [RH] Get wall light level
	if (curline->sidedef->Flags & WALLF_ABSLIGHTING)
	{
		lightnum = (BYTE)curline->sidedef->Light;
	}
	else
	{
		// killough 4/13/98: get correct lightlevel for 2s normal textures
		lightnum = R_FakeFlat (frontsector, &tempsec, NULL, NULL, false)
				->lightlevel;

		if (!foggy) // Don't do relative lighting in foggy sectors
		{
			lightnum += curline->sidedef->Light * 2;
		}
	}
	wallshade = LIGHT2SHADE(lightnum + r_actualextralight);

	rw_lightstep = ds->lightstep;
	rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;
	MaskedSWall = (fixed_t *)(openings + ds->swall) - ds->x1;
	maskedtexturecol = openings + ds->maskedtexturecol - ds->x1;
	spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
	rw_scalestep = ds->iscalestep;

	mfloorclip = openings + ds->sprbottomclip - ds->x1;
	mceilingclip = openings + ds->sprtopclip - ds->x1;
	
	// find positioning
	if (curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		dc_texturemid = frontsector->floortexz > backsector->floortexz
			? frontsector->floortexz : backsector->floortexz;
		dc_texturemid += textureheight[texnum] - viewz;
	}
	else
	{
		dc_texturemid = frontsector->ceilingtexz < backsector->ceilingtexz
			? frontsector->ceilingtexz : backsector->ceilingtexz;
		dc_texturemid -= viewz;
	}
	dc_texturemid += curline->sidedef->rowoffset;

	// [RH] Don't bother drawing segs that are completely offscreen
	if ((MulScale12 (globaldclip, ds->neardepth) < -dc_texturemid &&
		 MulScale12 (globaldclip, ds->fardepth) < -dc_texturemid) ||
		(MulScale12 (globaluclip, ds->neardepth) > textureheight[texnum] - dc_texturemid &&
		 MulScale12 (globaluclip, ds->fardepth) > textureheight[texnum] - dc_texturemid))
	{
		clearbufshort (&maskedtexturecol[x1], x2 - x1 + 1, SHRT_MAX);
		R_FinishSetPatchStyle ();
		return;
	}

	WallSZ1 = ds->sz1;
	WallSZ2 = ds->sz2;
	WallSX1 = ds->sx1;
	WallSX2 = ds->sx2;

	OWallMost (wallupper, dc_texturemid);
	OWallMost (walllower, dc_texturemid - textureheight[texnum]);

	for (i = x1; i <= x2; i++)
	{
		if (wallupper[i] < mceilingclip[i])
			wallupper[i] = mceilingclip[i];
	}
	for (i = x1; i <= x2; i++)
	{
		if (walllower[i] > mfloorclip[i])
			walllower[i] = mfloorclip[i];
	}
	mfloorclip = walllower;
	mceilingclip = wallupper;

	if (fixedlightlev)
		dc_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		dc_colormap = fixedcolormap;
	
	// draw the columns one at a time
	if (drawmode == DoDraw0)
	{
		for (dc_x = x1; dc_x <= x2; dc_x++)
			BlastMaskedColumn (R_DrawMaskedColumn, texnum);
	}
	else
	{
		// [RH] Draw up to four columns at once
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
	R_FinishSetPatchStyle ();
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
#define HEIGHTSHIFT				(FRACBITS-HEIGHTBITS)

static void BlastColumn (void (*blastfunc)())
{
	fixed_t texturecolumn;
	int yl, yh;

	// mark floor / ceiling areas
	yl = walltop[rw_x];
	yh = wallbottom[rw_x]-1;

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
	texturecolumn = (lwall[rw_x] + rw_offset) >> FRACBITS;
	dc_iscale = MulScale5 (swall[rw_x], WALLYREPEAT);

	fixed_t texfracdiff = FixedMul (centeryfrac-FRACUNIT, dc_iscale);

	// [RH] record wall bounds
	walltop[rw_x] = yl;
	wallbottom[rw_x] = yh;

	// draw the wall tiers
	if (midtexture)
	{
		// single sided line
		dc_yl = yl;
		dc_yh = yh;
		dc_texturefrac = rw_midtexturemid + dc_yl * dc_iscale - texfracdiff;
		dc_source = R_GetColumn (midtexture, texturecolumn);
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
			int mid = wallupper[rw_x]-1;

			if (mid >= floorclip[rw_x])
				mid = floorclip[rw_x]-1;

			if (mid >= yl)
			{
				dc_yl = yl;
				dc_yh = mid;
				dc_texturefrac = rw_toptexturemid + dc_yl * dc_iscale - texfracdiff;
				dc_source = R_GetColumn (toptexture, texturecolumn);
				blastfunc ();
				ceilingclip[rw_x] = mid;
			}
			else
			{
				ceilingclip[rw_x] = yl - 1;
			}
		}
		else
		{
			// no top wall
			if (markceiling)
				ceilingclip[rw_x] = yl - 1;
		}
					
		if (bottomtexture)
		{
			// bottom wall
			int mid = walllower[rw_x];

			// no space above wall?
			if (mid <= ceilingclip[rw_x])
				mid = ceilingclip[rw_x] + 1;
			
			if (mid <= yh)
			{
				dc_yl = mid;
				dc_yh = yh;
				dc_texturefrac = rw_bottomtexturemid + dc_yl * dc_iscale - texfracdiff;
				dc_source = R_GetColumn (bottomtexture, texturecolumn);
				blastfunc ();
				floorclip[rw_x] = mid;
			}
			else
			{
				floorclip[rw_x] = yh + 1;
			}
			if (floorclip[rw_x] > viewheight)
				floorclip[rw_x] = floorclip[rw_x];
		}
		else
		{
			// no bottom wall
			if (markfloor)
				floorclip[rw_x] = yh + 1;
		}
	}

	rw_light += rw_lightstep;
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

	for ( ; rw_x < rw_stopx ; rw_x++)
	{
		dc_x = rw_x;
		if (!fixedcolormap)
		{
			// calculate lighting
			dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
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
//
// Footnote: Okay, maybe it's not the cache. I don't know why this is faster.
// On Pentiums it's sometimes slightly faster, sometimes slightly slower.

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
		dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
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
			dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
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
		dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
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

void R_NewWall ()
{
	rw_markmirror = false;

	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;

	midtexture = toptexture = bottomtexture = 0;

	if (backsector == NULL)
	{
		// single sided line
		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;
		// [RH] Render mirrors later, but mark them now.
		if (linedef->special != Line_Mirror || !*r_drawmirrors)
		{
			midtexture = texturetranslation[sidedef->midtexture];
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				rw_midtexturemid = frontsector->floortexz + textureheight[sidedef->midtexture] - viewz;
			}
			else
			{ // top of texture at top
				rw_midtexturemid = frontsector->ceilingtexz - viewz;
			}
			rw_midtexturemid += sidedef->rowoffset;
		}
		else
		{
			rw_markmirror = true;
		}
	}
	else
	{ // two-sided line
		// hack to allow height changes in outdoor areas
		//bool skyhack = false;

		if (rw_havehigh &&
			frontsector->ceilingpic == skyflatnum &&
			backsector->ceilingpic == skyflatnum)
		{
			memcpy (&walltop[WallSX1], &wallupper[WallSX1], (WallSX2 - WallSX1)*sizeof(walltop[0]));

			rw_havehigh = false;
			//skyhack = true;
		}

		if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
			(rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
		{
			// closed door
			markceiling = markfloor = true;
		}
		else
		{
			markfloor = rw_mustmarkfloor
				|| backsector->floorplane != frontsector->floorplane
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->floorpic != frontsector->floorpic

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->floor_xoffs != frontsector->floor_xoffs
				|| (backsector->floor_yoffs + backsector->base_floor_yoffs) != (frontsector->floor_yoffs + frontsector->base_floor_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| frontsector->heightsec

				|| backsector->FloorLight != frontsector->FloorLight
				|| backsector->FloorFlags != frontsector->FloorFlags

				// [RH] Add checks for colormaps
				|| backsector->floorcolormap != frontsector->floorcolormap

				|| backsector->floor_xscale != frontsector->floor_xscale
				|| backsector->floor_yscale != frontsector->floor_yscale

				|| (backsector->floor_angle + backsector->base_floor_angle) != (frontsector->floor_angle + frontsector->base_floor_angle)
				;

			markceiling = (frontsector->ceilingpic != skyflatnum ||
				backsector->ceilingpic != skyflatnum) &&
				(rw_mustmarkceiling
				|| backsector->ceilingplane != frontsector->ceilingplane
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->ceilingpic != frontsector->ceilingpic

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->ceiling_xoffs != frontsector->ceiling_xoffs
				|| (backsector->ceiling_yoffs + backsector->base_ceiling_yoffs) != (frontsector->ceiling_yoffs + frontsector->base_ceiling_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (frontsector->heightsec && frontsector->ceilingpic != skyflatnum)

				|| backsector->CeilingLight != frontsector->CeilingLight
				|| backsector->CeilingFlags != frontsector->CeilingFlags

				// [RH] Add check for colormaps
				|| backsector->ceilingcolormap != frontsector->ceilingcolormap

				|| backsector->ceiling_xscale != frontsector->ceiling_xscale
				|| backsector->ceiling_yscale != frontsector->ceiling_yscale

				|| (backsector->ceiling_angle + backsector->base_ceiling_angle) != (frontsector->ceiling_angle + frontsector->base_ceiling_angle)
				);
		}

		if (rw_havehigh)
		{ // top texture
			toptexture = texturetranslation[sidedef->toptexture];

			if (linedef->flags & ML_DONTPEGTOP)
			{ // top of texture at top
				rw_toptexturemid = frontsector->ceilingtexz - viewz;
			}
			else
			{ // bottom of texture at bottom
				rw_toptexturemid = backsector->ceilingtexz + textureheight[sidedef->toptexture] - viewz;		
			}
			rw_toptexturemid += sidedef->rowoffset;
		}
		if (rw_havelow)
		{ // bottom texture
			bottomtexture = texturetranslation[sidedef->bottomtexture];

			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				rw_bottomtexturemid = frontsector->ceilingtexz - viewz;
			}
			else
			{ // top of texture at top
				rw_bottomtexturemid = backsector->floortexz - viewz;
			}
			rw_bottomtexturemid += sidedef->rowoffset;
		}
	}

	// if a floor / ceiling plane is on the wrong side of the view plane,
	// it is definitely invisible and doesn't need to be marked.

	// killough 3/7/98: add deep water check
	if (frontsector->heightsec == NULL)
	{
		if (frontsector->floorplane.ZatPoint (viewx, viewy) >= viewz)       // above view plane
			markfloor = false;
		if (frontsector->ceilingplane.ZatPoint (viewx, viewy) <= viewz &&
			frontsector->ceilingpic != skyflatnum)   // below view plane
			markceiling = false;
	}

	segtextured = sidedef->midtexture | (toptexture | bottomtexture);

	// calculate light table
	if (segtextured)
	{
		PrepWall (swall, lwall, sidedef->TexelLength);

		if (!fixedcolormap)
		{
			int lightnum;

			// [RH] Get wall light level
			if (curline->sidedef->Flags & WALLF_ABSLIGHTING)
			{
				lightnum = (BYTE)curline->sidedef->Light;
			}
			else
			{
				lightnum = frontsector->lightlevel + r_actualextralight;
				if (!foggy) // Don't do relative lighting in foggy sectors
				{
					lightnum += curline->sidedef->Light * 2;
				}
			}
			wallshade = LIGHT2SHADE(lightnum + r_actualextralight);
			GlobVis = r_WallVisibility;
			rw_lightleft = SafeDivScale12 (GlobVis, WallSZ1);
			rw_lightstep = (SafeDivScale12 (GlobVis, WallSZ2) - rw_lightleft) / (WallSX2 - WallSX1);
		}
		else
		{
			rw_lightleft = FRACUNIT;
			rw_lightstep = 0;
		}
	}
}

//
// R_CheckDrawSegs
//

void R_CheckDrawSegs ()
{
	if (ds_p == &drawsegs[MaxDrawSegs])
	{ // [RH] Grab some more drawsegs
		size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs*2 : 32;
		ptrdiff_t firstofs = firstdrawseg - drawsegs;
		drawsegs = (drawseg_t *)Realloc (drawsegs, newdrawsegs * sizeof(drawseg_t));
		firstdrawseg = drawsegs + firstofs;
		ds_p = drawsegs + MaxDrawSegs;
		MaxDrawSegs = newdrawsegs;
		DPrintf ("MaxDrawSegs increased to %d\n", MaxDrawSegs);
	}
}

//
// R_CheckOpenings
//

void R_CheckOpenings (size_t need)
{	
	need += lastopening;

	if (need > maxopenings)
	{
		do
			maxopenings = maxopenings ? maxopenings*2 : 16384;
		while (need > maxopenings);
		openings = (short *)Realloc (openings, maxopenings * sizeof(*openings));
		DPrintf ("MaxOpenings increased to %u\n", maxopenings);
	}
}

//
// R_StoreWallRange
// A wall segment will be drawn
//	between start and stop pixels (inclusive).
//

void R_StoreWallRange (int start, int stop)
{
	bool maskedtexture = false;

#ifdef RANGECHECK
	if (start >= viewwidth || start >= stop)
		I_FatalError ("Bad R_StoreWallRange: %i to %i", start , stop);
#endif

	// don't overflow and crash
	R_CheckDrawSegs ();
	
	if (!rw_prepped)
	{
		rw_prepped = true;
		R_NewWall ();
	}

	rw_offset = sidedef->textureoffset;
	rw_light = rw_lightleft + rw_lightstep * (start - WallSX1);

	if (WallSZ1 < WallSZ2)
	{
		ds_p->neardepth = WallSZ1;
		ds_p->fardepth = WallSZ2;
	}
	else
	{
		ds_p->neardepth = WallSZ2;
		ds_p->fardepth = WallSZ1;
	}

	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop-1;
	ds_p->curline = curline;
	rw_stopx = stop;

	// killough 1/6/98, 2/1/98: remove limit on openings
	R_CheckOpenings ((stop - start)*6);

	ds_p->sprtopclip = ds_p->sprbottomclip = ds_p->maskedtexturecol = ds_p->swall = -1;

	if (rw_markmirror)
	{
		WallMirrors.Push (ds_p - drawsegs);
		ds_p->silhouette = SIL_BOTH;
	}
	else if (backsector == NULL)
	{
		ds_p->sprtopclip = R_NewOpening (stop - start);
		ds_p->sprbottomclip = R_NewOpening (stop - start);
		clearbufshort (openings + ds_p->sprtopclip, stop-start, viewheight);
		memset (openings + ds_p->sprbottomclip, -1, (stop-start)*sizeof(short));
		ds_p->silhouette = SIL_BOTH;
	}
	else
	{
		// two sided line
		ds_p->silhouette = 0;

		if (frontsector->floorplane.a != backsector->floorplane.a ||
			frontsector->floorplane.b != backsector->floorplane.b ||
			frontsector->floorplane.c != backsector->floorplane.c)
		{
			ds_p->silhouette = SIL_BOTTOM;
		}
		else
		if (rw_frontfz1 > rw_backfz1 || rw_backfz1 > viewz ||
			rw_frontfz2 > rw_backfz2 || rw_backfz2 > viewz)
		{
			ds_p->silhouette = SIL_BOTTOM;
		}

		if (frontsector->ceilingplane.a != backsector->ceilingplane.a ||
			frontsector->ceilingplane.b != backsector->ceilingplane.b ||
			frontsector->ceilingplane.c != backsector->ceilingplane.c)
		{
			ds_p->silhouette |= SIL_TOP;
		}
		else
		if (rw_frontcz1 < rw_backcz1 || rw_backcz1 < viewz ||
			rw_frontcz2 < rw_backcz2 || rw_backcz2 < viewz)
		{
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
			if (doorclosed || (rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2))
			{
				ds_p->sprbottomclip = R_NewOpening (stop - start);
				memset (openings + ds_p->sprbottomclip, -1, (stop-start)*sizeof(short));
				ds_p->silhouette |= SIL_BOTTOM;
			}
			if (doorclosed || (rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
			{						// killough 1/17/98, 2/8/98
				ds_p->sprtopclip = R_NewOpening (stop - start);
				clearbufshort (openings + ds_p->sprtopclip, stop - start, viewheight);
				ds_p->silhouette |= SIL_TOP;
			}
		}

		// allocate space for masked texture tables, if needed
		// [RH] Don't just allocate the space; fill it in too.
		if (sidedef->midtexture &&
			(rw_ceilstat != 12 || sidedef->toptexture == 0) &&
			(rw_floorstat != 3 || sidedef->bottomtexture == 0) &&
			(WallSZ1 >= 3072 && WallSZ2 >= 3072))
		{
			fixed_t *swal;
			short *lwal;
			int i;

			maskedtexture = true;
			ds_p->maskedtexturecol = R_NewOpening (stop - start);
			ds_p->swall = R_NewOpening ((stop - start) * 2);

			lwal = openings + ds_p->maskedtexturecol;
			swal = (fixed_t *)(openings + ds_p->swall);
			for (i = start; i < stop; i++)
			{
				*lwal++ = (short)((lwall[i] + rw_offset) >> FRACBITS);
				*swal++ = MulScale5 (swall[i], WALLYREPEAT);
			}
			ds_p->light = rw_light;
			ds_p->lightstep = rw_lightstep;
			ds_p->sx1 = WallSX1;
			ds_p->sx2 = WallSX2;
			ds_p->sz1 = WallSZ1;
			ds_p->sz2 = WallSZ2;

			fixed_t istart = *((fixed_t *)(openings + ds_p->swall));
			fixed_t iend = *(swal - 1);

			if (istart < 3 && istart >= 0) istart = 3;
			if (istart > -3 && istart < 0) istart = -3;
			if (iend < 3 && iend >= 0) iend = 3;
			if (iend > -3 && iend < 0) iend = -3;
			istart = DivScale32 (1, istart);
			iend = DivScale32 (1, iend);
			ds_p->iscale = istart;

			if (stop - start > 1)
			{
				ds_p->iscalestep = (iend - istart) / (stop - start - 1);
			}
			else
			{
				ds_p->iscalestep = 0;
			}
		}
	}
	
	// render it
	if (markceiling)
	{
		if (ceilingplane)
		{	// killough 4/11/98: add NULL ptr checks
			ceilingplane = R_CheckPlane (ceilingplane, start, stop-1);
		}
		else
		{
			markceiling = false;
		}
	}
	
	if (markfloor)
	{
		if (floorplane)
		{	// killough 4/11/98: add NULL ptr checks
			floorplane = R_CheckPlane (floorplane, start, stop-1);
		}
		else
		{
			markfloor = false;
		}
	}

	R_RenderSegLoop ();

	// save sprite clipping info
	if ( ((ds_p->silhouette & SIL_TOP) || maskedtexture) && ds_p->sprtopclip == -1)
	{
		ds_p->sprtopclip = R_NewOpening (stop - start);
		memcpy (openings + ds_p->sprtopclip, &ceilingclip[start], sizeof(short)*(stop-start));
	}

	if ( ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && ds_p->sprbottomclip == -1)
	{
		ds_p->sprbottomclip = R_NewOpening (stop - start);
		memcpy (openings + ds_p->sprbottomclip, &floorclip[start], sizeof(short)*(stop-start));
	}

	if (maskedtexture)
	{
		ds_p->silhouette |= SIL_TOP | SIL_BOTTOM;
	}

	// [RH] Draw any wall sprites bound to the seg
	AActor *decal = curline->sidedef->BoundActors;
	while (decal != NULL)
	{
		R_RenderBoundWallSprite (decal, ds_p, 0);
		decal = decal->snext;
	}

	ds_p++;
}

int OWallMost (short *mostbuf, fixed_t z)
{
	int bad, y, ix1, ix2, iy1, iy2;
	fixed_t s1, s2, s3, s4;

	z = -(z >> 4);
	s1 = MulScale16 (globaluclip, WallSZ1); s2 = MulScale16 (globaluclip, WallSZ2);
	s3 = MulScale16 (globaldclip, WallSZ1); s4 = MulScale16 (globaldclip, WallSZ2);
	bad = (z<s1)+((z<s2)<<1)+((z>s3)<<2)+((z>s4)<<3);

	if ((bad&3) == 3)
	{
		memset (&mostbuf[WallSX1], 0, (WallSX2 - WallSX1)*sizeof(mostbuf[0]));
		return bad;
	}

	if ((bad&12) == 12)
	{
		clearbufshort (&mostbuf[WallSX1], WallSX2 - WallSX1, viewheight);
		return bad;
	}

	ix1 = WallSX1; iy1 = WallSZ1;
	ix2 = WallSX2; iy2 = WallSZ2;

	if (bad & 3)
	{
		int t = DivScale30 (z-s1, s2-s1);
		int inty = WallSZ1 + MulScale30 (WallSZ2 - WallSZ1, t);
		int xcross = WallSX1 + Scale (MulScale30 (WallSZ2, t), WallSX2 - WallSX1, inty);

		if ((bad & 3) == 2)
		{
			if (WallSX1 <= xcross) { iy2 = inty; ix2 = xcross; }
			if (WallSX2 > xcross) memset (&mostbuf[xcross], 0, (WallSX2-xcross)*sizeof(mostbuf[0]));
		}
		else
		{
			if (xcross <= WallSX2) { iy1 = inty; ix1 = xcross; }
			if (xcross > WallSX1) memset (&mostbuf[WallSX1], 0, (xcross-WallSX1)*sizeof(mostbuf[0]));
		}
	}

	if (bad & 12)
	{
		int t = DivScale30 (z-s3, s4-s3);
		int inty = WallSZ1 + MulScale30 (WallSZ2 - WallSZ1, t);
		int xcross = WallSX1 + Scale (MulScale30 (WallSZ2, t), WallSX2 - WallSX1, inty);

		if ((bad & 12) == 8)
		{
			if (WallSX1 <= xcross) { iy2 = inty; ix2 = xcross; }
			if (WallSX2 > xcross) clearbufshort (&mostbuf[xcross], WallSX2 - xcross, viewheight);
		}
		else
		{
			if (xcross <= WallSX2) { iy1 = inty; ix1 = xcross; }
			if (xcross > WallSX1) clearbufshort (&mostbuf[WallSX1], xcross - WallSX1, viewheight);
		}
	}

	y = Scale (z, InvZtoScale, iy1);
	if (ix2 == ix1)
	{
		mostbuf[ix1] = (short)((y + centeryfrac) >> FRACBITS);
	}
	else
	{
		fixed_t yinc  = (Scale (z, InvZtoScale, iy2) - y) / (ix2 - ix1);
		qinterpolatedown16short (&mostbuf[ix1], ix2-ix1, y + centeryfrac, yinc);
	}

	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	else if (mostbuf[ix1] > viewheight) mostbuf[ix1] = (short)viewheight;
	if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
	else if (mostbuf[ix2] > viewheight) mostbuf[ix2] = (short)viewheight;

	return bad;
}

int WallMost (short *mostbuf, const secplane_t &plane)
{
	if ((plane.a | plane.b) == 0)
	{
		return OWallMost (mostbuf, ((plane.c < 0) ? plane.d : -plane.d) - viewz);
	}

	fixed_t x, y, den, z1, z2, oz1, oz2;
	fixed_t s1, s2, s3, s4;
	int bad, ix1, ix2, iy1, iy2;

	if (MirrorFlags & RF_XFLIP)
	{
		x = curline->v2->x;
		y = curline->v2->y;
		if (WallSX1 == 0 && 0 != (den = WallTX1 - WallTX2 + WallTY1 - WallTY2))
		{
			int frac = SafeDivScale30 (WallTY1 + WallTX1, den);
			x -= MulScale30 (frac, x - curline->v1->x);
			y -= MulScale30 (frac, y - curline->v1->y);
		}
		z1 = viewz - plane.ZatPoint (x, y);

		if (WallSX2 > WallSX1 + 1)
		{
			x = curline->v1->x;
			y = curline->v1->y;
			if (WallSX2 == viewwidth && 0 != (den = WallTX1 - WallTX2 - WallTY1 + WallTY2))
			{
				int frac = SafeDivScale30 (WallTY2 - WallTX2, den);
				x += MulScale30 (frac, curline->v2->x - x);
				y += MulScale30 (frac, curline->v2->y - y);
			}
			z2 = viewz - plane.ZatPoint (x, y);
		}
		else
		{
			z2 = z1;
		}
	}
	else
	{
		x = curline->v1->x;
		y = curline->v1->y;
		if (WallSX1 == 0 && 0 != (den = WallTX1 - WallTX2 + WallTY1 - WallTY2))
		{
			int frac = SafeDivScale30 (WallTY1 + WallTX1, den);
			x += MulScale30 (frac, curline->v2->x - x);
			y += MulScale30 (frac, curline->v2->y - y);
		}
		z1 = viewz - plane.ZatPoint (x, y);

		if (WallSX2 > WallSX1 + 1)
		{
			x = curline->v2->x;
			y = curline->v2->y;
			if (WallSX2 == viewwidth && 0 != (den = WallTX1 - WallTX2 - WallTY1 + WallTY2))
			{
				int frac = SafeDivScale30 (WallTY2 - WallTX2, den);
				x -= MulScale30 (frac, x - curline->v1->x);
				y -= MulScale30 (frac, y - curline->v1->y);
			}
			z2 = viewz - plane.ZatPoint (x, y);
		}
		else
		{
			z2 = z1;
		}
	}

	s1 = MulScale12 (globaluclip, WallSZ1); s2 = MulScale12 (globaluclip, WallSZ2);
	s3 = MulScale12 (globaldclip, WallSZ1); s4 = MulScale12 (globaldclip, WallSZ2);
	bad = (z1<s1)+((z2<s2)<<1)+((z1>s3)<<2)+((z2>s4)<<3);

	ix1 = WallSX1; ix2 = WallSX2;
	iy1 = WallSZ1; iy2 = WallSZ2;
	oz1 = z1; oz2 = z2;

	if ((bad&3) == 3)
	{
		memset (&mostbuf[ix1], -1, (ix2-ix1)*sizeof(mostbuf[0]));
		return bad;
	}

	if ((bad&12) == 12)
	{
		clearbufshort (&mostbuf[ix1], ix2-ix1, viewheight);
		return bad;
	}

	if (bad&3)
	{
			//inty = intz / (globaluclip>>16)
		int t = SafeDivScale30 (oz1-s1, s2-s1+oz1-oz2);
		int inty = WallSZ1 + MulScale30 (WallSZ2-WallSZ1,t);
		int intz = oz1 + MulScale30 (oz2-oz1,t);
		int xcross = WallSX1 + Scale (MulScale30 (WallSZ2, t), WallSX2-WallSX1, inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&3) == 2)
		{
			if (WallSX1 <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			memset (&mostbuf[xcross], 0, (WallSX2-xcross)*sizeof(mostbuf[0]));
		}
		else
		{
			if (xcross <= WallSX2) { z1 = intz; iy1 = inty; ix1 = xcross; }
			memset (&mostbuf[WallSX1], 0, (xcross-WallSX1)*sizeof(mostbuf[0]));
		}
	}

	if (bad&12)
	{
			//inty = intz / (globaldclip>>16)
		int t = SafeDivScale30 (oz1-s3, s4-s3+oz1-oz2);
		int inty = WallSZ1 + MulScale30 (WallSZ2-WallSZ1,t);
		int intz = oz1 + MulScale30 (oz2-oz1,t);
		int xcross = WallSX1 + Scale (MulScale30 (WallSZ2, t), WallSX2-WallSX1,inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&12) == 8)
		{
			if (WallSX1 <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			if (WallSX2 > xcross) clearbufshort (&mostbuf[xcross], WallSX2-xcross, viewheight);
		}
		else
		{
			if (xcross <= WallSX2) { z1 = intz; iy1 = inty; ix1 = xcross; }
			if (xcross > WallSX1) clearbufshort (&mostbuf[WallSX1], xcross-WallSX1, viewheight);
		}
	}

	y = Scale (z1>>4, InvZtoScale, iy1);
	if (ix2 == ix1)
	{
		mostbuf[ix1] = (short)((y + centeryfrac) >> FRACBITS);
	}
	else
	{
		fixed_t yinc = (Scale (z2>>4, InvZtoScale, iy2) - y) / (ix2-ix1);
		qinterpolatedown16short (&mostbuf[ix1], ix2-ix1, y + centeryfrac,yinc);
	}

	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	else if (mostbuf[ix1] > viewheight) mostbuf[ix1] = (short)viewheight;
	if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
	else if (mostbuf[ix2] > viewheight) mostbuf[ix2] = (short)viewheight;

	return bad;
}

void PrepWall (fixed_t *swall, fixed_t *lwall, int walxrepeat)
{ // swall = scale, lwall = texturecolumn
	int x;
	float top, bot, i;
	float xrepeat = (float)((unsigned)walxrepeat << 16);
	float ol, l, topinc, botinc;

	i = (float)(WallSX1 - centerx);
	top = WallUoverZorg + WallUoverZstep * i;
	bot = WallInvZorg + WallInvZstep * i;
	topinc = WallUoverZstep * 4.f;
	botinc = WallInvZstep * 4.f;

	x = WallSX1;

	l = top / bot;
	swall[x] = quickertoint (l * WallDepthScale + WallDepthOrg);
	lwall[x] = quickertoint (l *= xrepeat);
	while (x+4 < WallSX2)
	{
		top += topinc; bot += botinc;
		ol = l; l = top / bot;
		swall[x+4] = quickertoint (l * WallDepthScale + WallDepthOrg);
		lwall[x+4] = quickertoint (l *= xrepeat);

		i = (ol+l) * 0.5f;
		lwall[x+2] = quickertoint (i);
		lwall[x+1] = quickertoint ((ol+i) * 0.5f);
		lwall[x+3] = quickertoint ((l+i) * 0.5f);
		swall[x+2] = ((swall[x]+swall[x+4])>>1);
		swall[x+1] = ((swall[x]+swall[x+2])>>1);
		swall[x+3] = ((swall[x+4]+swall[x+2])>>1);
		x += 4;
	}
	if (x+2 < WallSX2)
	{
		top += topinc * 0.5f; bot += botinc * 0.5f;
		ol = l; l = top / bot;
		swall[x+2] = quickertoint (l * WallDepthScale + WallDepthOrg);
		lwall[x+2] = quickertoint (l *= xrepeat);

		lwall[x+1] = quickertoint ((l+ol)*0.5f);
		swall[x+1] = (swall[x]+swall[x+2])>>1;
		x += 2;
	}
	if (x+1 < WallSX2)
	{
		l = (top + topinc * 0.25f) / (bot + botinc * 0.25f);
		swall[x+1] = quickertoint (l * WallDepthScale + WallDepthOrg);
		lwall[x+1] = quickertoint (l * xrepeat);
	}
	/*
	for (x = WallSX1; x < WallSX2; x++)
	{
		frac = top / bot;
		lwall[x] = quickertoint (frac * xrepeat);
		swall[x] = quickertoint (frac * WallDepthScale + WallDepthOrg);
		top += WallUoverZstep;
		bot += WallInvZstep;
	}
	*/

	fixed_t lastl = walxrepeat << FRACBITS;

	// fix for rounding errors
	if (MirrorFlags & RF_XFLIP)
	{
		for (x = WallSX1; x < WallSX2 && lwall[x] >= lastl; x++)
			lwall[x] = lastl - FRACUNIT;
		for (x = WallSX2-1; x >= WallSX1 && lwall[x] < 0; x--)
			lwall[x] = 0;
	}
	else
	{
		for (x = WallSX1; x < WallSX2 && lwall[x] < 0; x++)
			lwall[x] = 0;
		for (x = WallSX2-1; x >= WallSX1 && lwall[x] >= lastl; x--)
			lwall[x] = lastl - FRACUNIT;
	}
}

// pass = 0: when seg is first drawn
//		= 1: drawing masked textures (including sprites)
// Currently, only pass = 0 is done or used

static void R_RenderBoundWallSprite (AActor *actor, drawseg_t *clipper, int pass)
{
	fixed_t lx, ly, lx2, ly2;
	int x1, x2;
	int xscale, yscale;
	fixed_t topoff;
	byte flipx;
	fixed_t zpos;
	int needrepeat = 0;
	sector_t *front, *back;

	if (actor->renderflags & RF_INVISIBLE)
		return;

	// Determine actor z
	zpos = actor->z;
	front = curline->frontsector;
	back = (curline->backsector != NULL) ? curline->backsector : curline->frontsector;
	switch (actor->renderflags & RF_RELMASK)
	{
	default:
		zpos = actor->z;
		break;
	case RF_RELUPPER:
		if (curline->linedef->flags & ML_DONTPEGTOP)
		{
			zpos = actor->z + front->ceilingtexz;
		}
		else
		{
			zpos = actor->z + back->ceilingtexz;
		}
		break;
	case RF_RELLOWER:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->z + front->ceilingtexz;
		}
		else
		{
			zpos = actor->z + back->floortexz;
		}
		break;
	case RF_RELMID:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->z + front->floortexz;
		}
		else
		{
			zpos = actor->z + front->ceilingtexz;
		}
	}

	xscale = actor->xscale + 1;
	yscale = actor->yscale + 1;

	if (actor->picnum != 0xffff)
	{
		WallSpriteTile = actor->picnum;
		flipx = false;
	}
	else
	{
		WallSpriteTile = sprites[actor->sprite].spriteframes[actor->frame].lump[0];
		flipx = sprites[actor->sprite].spriteframes[actor->frame].flip[0];
	}

	// Determine left and right edges of sprite. Since this sprite is bound
	// to a wall, we use the wall's angle instead of the actor's. This is
	// pretty much the same as what R_AddLine() does.

	if (TileSizes[WallSpriteTile].Width == 0xffff)
	{
		R_CacheTileNum (WallSpriteTile, PU_CACHE);
	}

	x1 = TileSizes[WallSpriteTile].LeftOffset;
	x2 = TileSizes[WallSpriteTile].Width - x1;

	x1 *= xscale;
	x2 *= xscale;

	lx  = actor->x - MulScale6 (x1, finecosine[curline->angle >> ANGLETOFINESHIFT]) - viewx;
	lx2 = actor->x + MulScale6 (x2, finecosine[curline->angle >> ANGLETOFINESHIFT]) - viewx;
	ly  = actor->y - MulScale6 (x1, finesine[curline->angle >> ANGLETOFINESHIFT]) - viewy;
	ly2 = actor->y + MulScale6 (x2, finesine[curline->angle >> ANGLETOFINESHIFT]) - viewy;

	WallTX1 = DMulScale20 (lx,  viewsin, -ly,  viewcos);
	WallTX2 = DMulScale20 (lx2, viewsin, -ly2, viewcos);

	WallTY1 = DMulScale20 (lx, viewtancos,  ly, viewtansin);
	WallTY2 = DMulScale20 (lx2, viewtancos, ly2, viewtansin);

	if (MirrorFlags & RF_XFLIP)
	{
		int t = 256-WallTX1;
		WallTX1 = 256-WallTX2;
		WallTX2 = t;
		swap (WallTY1, WallTY2);
	}

	if (WallTX1 >= -WallTY1)
	{
		if (WallTX1 > WallTY1) return;	// left edge is off the right side
		x1 = (centerxfrac + Scale (WallTX1, centerxfrac, WallTY1)) >> FRACBITS;
		if (WallTX1 >= 0) x1 = MIN (viewwidth, x1+1); // fix for signed divide
		WallSZ1 = WallTY1;
	}
	else
	{
		if (WallTX2 < -WallTY2) return;	// wall is off the left side
		fixed_t den = WallTX1 - WallTX2 - WallTY2 + WallTY1;	
		if (den == 0) return;
		x1 = 0;
		WallSZ1 = WallTY1 + Scale (WallTY2 - WallTY1, WallTX1 + WallTY1, den);
	}

	if (WallSZ1 < 32)
		return;

	if (WallTX2 <= WallTY2)
	{
		if (WallTX2 < -WallTY2) return;	// right edge is off the left side
		x2 = (centerxfrac + Scale (WallTX2, centerxfrac, WallTY2)) >> FRACBITS;
		if (WallTX2 >= 0) x2 = MIN (viewwidth, x2+1);	// fix for signed divide
		WallSZ2 = WallTY2;
	}
	else
	{
		if (WallTX1 > WallTY1) return;	// wall is off the right side
		fixed_t den = WallTY2 - WallTY1 - WallTX2 + WallTX1;
		if (den == 0) return;
		x2 = viewwidth;
		WallSZ2 = WallTY1 + Scale (WallTY2 - WallTY1, WallTX1 - WallTY1, den);
	}

	if (x1 >= x2 || x1 > clipper->x2 || x2 <= clipper->x1)
		return;

	if (MirrorFlags & RF_XFLIP)
	{
		WallUoverZorg = (float)WallTX2 * WallTMapScale;
		WallUoverZstep = (float)(-WallTY2) * 32.f;
		WallInvZorg = (float)(WallTX2 - WallTX1) * WallTMapScale;
		WallInvZstep = (float)(WallTY1 - WallTY2) * 32.f;
	}
	else
	{
		WallUoverZorg = (float)WallTX1 * WallTMapScale;
		WallUoverZstep = (float)(-WallTY1) * 32.f;
		WallInvZorg = (float)(WallTX1 - WallTX2) * WallTMapScale;
		WallInvZstep = (float)(WallTY2 - WallTY1) * 32.f;
	}
	WallDepthScale = WallInvZstep * WallTMapScale2;
	WallDepthOrg = -WallUoverZstep * WallTMapScale2;

	// Get the top and bottom clipping arrays
	switch (actor->renderflags & RF_CLIPMASK)
	{
	case RF_CLIPFULL:
		if (curline->backsector == NULL)
		{
			if (pass != 0)
			{
				return;
			}
			mceilingclip = walltop;
			mfloorclip = wallbottom;
		}
		else if (pass == 0)
		{
			mceilingclip = walltop;
			mfloorclip = ceilingclip;
			needrepeat = 1;
		}
		else
		{
			mceilingclip = openings + clipper->sprtopclip - clipper->x1;
			mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
		}
		break;

	case RF_CLIPUPPER:
		if (pass != 0)
		{
			return;
		}
		mceilingclip = walltop;
		mfloorclip = ceilingclip;
		break;

	case RF_CLIPMID:
		if (curline->backsector != NULL && pass != 2)
		{
			return;
		}
		mceilingclip = openings + clipper->sprtopclip - clipper->x1;
		mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
		break;

	case RF_CLIPLOWER:
		if (pass != 0)
		{
			return;
		}
		mceilingclip = floorclip;
		mfloorclip = wallbottom;
		break;
	}

	topoff = TileSizes[WallSpriteTile].TopOffset << FRACBITS;
	dc_texturemid = topoff + SafeDivScale6 (zpos - viewz, yscale);

	// Calculate top and bottom of sprite
	topfrac = zpos - viewz + MulScale6 (topoff, yscale);
	bottomfrac = topfrac - MulScale6 (TileSizes[WallSpriteTile].Height<<FRACBITS, yscale);
	topstep = -FixedMul (rw_scalestep, topfrac>>=HEIGHTSHIFT);
	topfrac = (centeryfrac>>HEIGHTSHIFT) - FixedMul (topfrac, rw_scale);
	bottomstep = -FixedMul (rw_scalestep, bottomfrac>>=HEIGHTSHIFT);
	bottomfrac = (centeryfrac>>HEIGHTSHIFT) - FixedMul (bottomfrac, rw_scale);

	// Scale the texture size
	rw_scale = MulScale6 (rw_scale, yscale);
	rw_scalestep = MulScale6 (rw_scalestep, yscale);

	// Clip sprite to drawseg
	if (x1 < clipper->x1)
	{
		int dist = clipper->x1 - x1;
		topfrac += topstep*dist;
		bottomfrac += bottomstep*dist;
		x1 = clipper->x1;
	}
	if (x2 > clipper->x2)
	{
		x2 = clipper->x2 + 1;
	}
	if (x1 >= x2)
	{
		return;
	}

	swap (x1, WallSX1);
	swap (x2, WallSX2);
	PrepWall (swall, lwall, TileSizes[WallSpriteTile].Width);
	swap (x1, WallSX1);
	swap (x2, WallSX2);

	if (flipx)
	{
		int i;
		int right = (TileSizes[WallSpriteTile].Width << FRACBITS) - 1;

		for (i = x1; i < x2; i++)
		{
			lwall[i] = right - lwall[i];
		}
	}

	// Prepare lighting
	bool calclighting = false;

	rw_light = rw_lightleft + (x1 - WallSX1) * rw_lightstep;
	if (fixedlightlev)
		dc_colormap = basecolormap + fixedlightlev;
	else if (fixedcolormap)
		dc_colormap = fixedcolormap;
	else if (!foggy && (actor->renderflags & RF_FULLBRIGHT))
		dc_colormap = basecolormap;
	else
		calclighting = true;

	// Draw it
	R_CacheTileNum (WallSpriteTile, PU_CACHE);
	dc_mask = 255;
	dc_x = x1;

	if (actor->renderflags & RF_YFLIP)
	{
		sprflipvert = true;
		yscale = -yscale;
		dc_texturemid = dc_texturemid - (TileSizes[WallSpriteTile].Height<<FRACBITS);
	}
	else
	{
		sprflipvert = false;
	}

	rw_offset = 16*FRACUNIT/yscale;

	do
	{
		switch (R_SetPatchStyle (actor->RenderStyle, actor->alpha,
			actor->translation, actor->alphacolor))
		{
		case DontDraw:
			needrepeat = 0;
			break;

		case DoDraw0:
			// 1 column at a time
			for (; dc_x < x2; dc_x++)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				WallSpriteColumn (R_DrawMaskedColumn);
			}
			break;

		case DoDraw1:
			// up to 4 columns at a time
			int stop = x2 & ~3;

			if (calclighting)
			{ // calculate lighting
				dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
			}

			if (dc_x & 1)
			{
				WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}

			if (dc_x & 2)
			{
				if (dc_x < x2 - 1)
				{
					rt_initcols();
					WallSpriteColumn (R_DrawMaskedColumnHoriz);
					dc_x++;
					WallSpriteColumn (R_DrawMaskedColumnHoriz);
					rt_draw2cols ((dc_x - 1) & 3, dc_x - 1);
					dc_x++;
				}
				else if (dc_x == x2 - 1)
				{
					WallSpriteColumn (R_DrawMaskedColumn);
					dc_x++;
				}
			}

			while (dc_x < stop)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				rt_initcols();
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				dc_x++;
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				dc_x++;
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				dc_x++;
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				rt_draw4cols (dc_x - 3);
				dc_x++;
			}

			if (calclighting)
			{ // calculate lighting
				dc_colormap = basecolormap + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
			}

			if (x2 - dc_x == 1)
			{
				WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}
			else if (x2 - dc_x >= 2)
			{
				rt_initcols();
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				dc_x++;
				WallSpriteColumn (R_DrawMaskedColumnHoriz);
				rt_draw2cols ((dc_x - 1) & 3, dc_x - 1);
				if (++dc_x < x2)
				{
					WallSpriteColumn (R_DrawMaskedColumn);
					dc_x++;
				}
			}
			break;
		}

		// If this sprite is RF_CLIPFULL on a two-sided line, needrepeat will
		// be set 1 if we need to draw on the lower wall. In all other cases,
		// needrepeat will be 0, and the while will fail.
		mceilingclip = floorclip;
		mfloorclip = wallbottom;
		R_FinishSetPatchStyle ();
	} while (needrepeat--);

	colfunc = basecolfunc;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post2 = rt_map2cols;
	hcolfunc_post4 = rt_map4cols;

	R_FinishSetPatchStyle ();
}

static void WallSpriteColumn (void (*drawfunc)(column_t *, int))
{
	unsigned int texturecolumn = lwall[dc_x] >> FRACBITS;
	dc_iscale = MulScale16 (swall[dc_x], rw_offset);
	spryscale = SafeDivScale32 (1, dc_iscale);
	//sprtopscreen = sprflipvert ? bottomfrac << HEIGHTSHIFT : topfrac << HEIGHTSHIFT;
	if (sprflipvert)
		sprtopscreen = centeryfrac + FixedMul (dc_texturemid, spryscale);
	else
		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);
	drawfunc ((column_t *)(LONG(TileCache[WallSpriteTile]->columnofs[texturecolumn])
		+ (byte *)TileCache[WallSpriteTile]), viewheight);

	rw_light += rw_lightstep;
	topfrac += topstep;
	bottomfrac += bottomstep;
}
