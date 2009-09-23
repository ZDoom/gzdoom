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

#include <stdlib.h>
#include <stddef.h>

#include "templates.h"
#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"

#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "v_palette.h"

#define WALLYREPEAT 8

//CVAR (Int, ty, 8, 0)
//CVAR (Int, tx, 8, 0)

#define HEIGHTBITS 12
#define HEIGHTSHIFT (FRACBITS-HEIGHTBITS)

// The 3072 below is just an arbitrary value picked to avoid
// drawing lines the player is too close to that would overflow
// the texture calculations.
#define TOO_CLOSE_Z 3072

extern fixed_t globaluclip, globaldclip;


// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

static bool		segtextured;	// True if any of the segs textures might be visible.
bool		markfloor;		// False if the back side is the same plane.
bool		markceiling;
FTexture *toptexture;
FTexture *bottomtexture;
FTexture *midtexture;
fixed_t rw_offset_top;
fixed_t rw_offset_mid;
fixed_t rw_offset_bottom;

int OWallMost (short *mostbuf, fixed_t z);
int WallMost (short *mostbuf, const secplane_t &plane);
void PrepWall (fixed_t *swall, fixed_t *lwall, fixed_t walxrepeat);
void PrepLWall (fixed_t *lwall, fixed_t walxrepeat);
extern fixed_t WallSZ1, WallSZ2, WallTX1, WallTX2, WallTY1, WallTY2, WallCX1, WallCX2, WallCY1, WallCY2;
extern int WallSX1, WallSX2;
extern float WallUoverZorg, WallUoverZstep, WallInvZorg, WallInvZstep, WallDepthScale, WallDepthOrg;

int		wallshade;

short	walltop[MAXWIDTH];	// [RH] record max extents of wall
short	wallbottom[MAXWIDTH];
short	wallupper[MAXWIDTH];
short	walllower[MAXWIDTH];
fixed_t	swall[MAXWIDTH];
fixed_t	lwall[MAXWIDTH];
fixed_t	lwallscale;

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

static fixed_t	rw_frontlowertop;

static int		rw_x;
static int		rw_stopx;
fixed_t			rw_offset;
static fixed_t	rw_scalestep;
static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;
static fixed_t	rw_midtexturescalex;
static fixed_t	rw_midtexturescaley;
static fixed_t	rw_toptexturescalex;
static fixed_t	rw_toptexturescaley;
static fixed_t	rw_bottomtexturescalex;
static fixed_t	rw_bottomtexturescaley;

FTexture		*rw_pic;

static fixed_t	*maskedtexturecol;
static FTexture	*WallSpriteTile;

static void R_RenderDecal (side_t *wall, DBaseDecal *first, drawseg_t *clipper, int pass);
static void WallSpriteColumn (void (*drawfunc)(const BYTE *column, const FTexture::Span *spans));

//=============================================================================
//
// CVAR r_fogboundary
//
// If true, makes fog look more "real" by shading the walls separating two
// sectors with different fog.
//=============================================================================

CVAR(Bool, r_fogboundary, true, 0)

inline bool IsFogBoundary (sector_t *front, sector_t *back)
{
	return r_fogboundary && fixedcolormap == NULL && front->ColorMap->Fade &&
		front->ColorMap->Fade != back->ColorMap->Fade &&
		(front->GetTexture(sector_t::ceiling) != skyflatnum || back->GetTexture(sector_t::ceiling) != skyflatnum);
}

//=============================================================================
//
// CVAR r_drawmirrors
//
// Set to false to disable rendering of mirrors
//=============================================================================

CVAR(Bool, r_drawmirrors, true, 0)

//
// R_RenderMaskedSegRange
//
fixed_t *MaskedSWall;
fixed_t MaskedScaleY;

static void BlastMaskedColumn (void (*blastfunc)(const BYTE *pixels, const FTexture::Span *spans), FTexture *tex)
{
	if (maskedtexturecol[dc_x] != FIXED_MAX)
	{
		// calculate lighting
		if (fixedcolormap == NULL && fixedlightlev < 0)
		{
			dc_colormap = basecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
		}

		dc_iscale = MulScale18 (MaskedSWall[dc_x], MaskedScaleY);
		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);
		
		// killough 1/25/98: here's where Medusa came in, because
		// it implicitly assumed that the column was all one patch.
		// Originally, Doom did not construct complete columns for
		// multipatched textures, so there were no header or trailer
		// bytes in the column referred to below, which explains
		// the Medusa effect. The fix is to construct true columns
		// when forming multipatched textures (see r_data.c).

		// draw the texture
		const FTexture::Span *spans;
		const BYTE *pixels = tex->GetColumn (maskedtexturecol[dc_x] >> FRACBITS, &spans);
		blastfunc (pixels, spans);
		maskedtexturecol[dc_x] = FIXED_MAX;
	}
	rw_light += rw_lightstep;
	spryscale += rw_scalestep;
}

void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2)
{
	FTexture	*tex;
	int			i;
	sector_t	tempsec;		// killough 4/13/98
	fixed_t		texheight, textop, texheightscale;

	sprflipvert = false;

	curline = ds->curline;

	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable translucency maps
	ESPSResult drawmode;

	drawmode = R_SetPatchStyle (LegacyRenderStyles[curline->linedef->flags & ML_ADDTRANS ? STYLE_Add : STYLE_Translucent],
		MIN(curline->linedef->Alpha, FRACUNIT),	0, 0);

	if ((drawmode == DontDraw && !ds->bFogBoundary))
	{
		return;
	}

	NetUpdate ();

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	tex = TexMan(curline->sidedef->GetTexture(side_t::mid));

	// killough 4/13/98: get correct lightlevel for 2s normal textures
	const sector_t *sec = R_FakeFlat (frontsector, &tempsec, NULL, NULL, false);

	basecolormap = sec->ColorMap;	// [RH] Set basecolormap

	wallshade = ds->shade;
	rw_lightstep = ds->lightstep;
	rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

	mfloorclip = openings + ds->sprbottomclip - ds->x1;
	mceilingclip = openings + ds->sprtopclip - ds->x1;

	// [RH] Draw fog partition
	if (ds->bFogBoundary)
	{
		R_DrawFogBoundary (x1, x2, mceilingclip, mfloorclip);
		if (ds->maskedtexturecol == -1)
		{
			goto clearfog;
		}
	}

	MaskedSWall = (fixed_t *)(openings + ds->swall) - ds->x1;
	MaskedScaleY = ds->yrepeat;
	maskedtexturecol = (fixed_t *)(openings + ds->maskedtexturecol) - ds->x1;
	spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
	rw_scalestep = ds->iscalestep;

	// find positioning
	texheight = tex->GetScaledHeight() << FRACBITS;
	texheightscale = curline->sidedef->GetTextureYScale(side_t::mid);
	if (texheightscale != FRACUNIT)
	{
		texheight = FixedDiv(texheight, texheightscale);
	}
	if (curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		dc_texturemid = MAX (frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor)) + texheight;
	}
	else
	{
		dc_texturemid = MIN (frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));
	}

	{ // encapsulate the lifetime of rowoffset
		fixed_t rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid);
		if (tex->bWorldPanning)
		{
			// rowoffset is added before the MulScale3 so that the masked texture will
			// still be positioned in world units rather than texels.
			dc_texturemid += rowoffset - viewz;
			textop = dc_texturemid;
			dc_texturemid = MulScale16 (dc_texturemid, MaskedScaleY);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			textop = dc_texturemid - viewz + SafeDivScale16 (rowoffset, MaskedScaleY);
			dc_texturemid = MulScale16 (dc_texturemid - viewz, MaskedScaleY) + rowoffset;
		}
	}

	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;

	if (!(curline->linedef->flags & ML_WRAP_MIDTEX) &&
		!(curline->sidedef->Flags & WALLF_WRAP_MIDTEX))
	{ // Texture does not wrap vertically.

		// [RH] Don't bother drawing segs that are completely offscreen
		if (MulScale12 (globaldclip, ds->sz1) < -textop &&
			MulScale12 (globaldclip, ds->sz2) < -textop)
		{ // Texture top is below the bottom of the screen
			goto clearfog;
		}

		if (MulScale12 (globaluclip, ds->sz1) > texheight - textop &&
			MulScale12 (globaluclip, ds->sz2) > texheight - textop)
		{ // Texture bottom is above the top of the screen
			goto clearfog;
		}

		WallSZ1 = ds->sz1;
		WallSZ2 = ds->sz2;
		WallSX1 = ds->sx1;
		WallSX2 = ds->sx2;

		OWallMost (wallupper, textop);
		OWallMost (walllower, textop - texheight);

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

		// draw the columns one at a time
		if (drawmode == DoDraw0)
		{
			for (dc_x = x1; dc_x <= x2; ++dc_x)
			{
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
			}
		}
		else
		{
			// [RH] Draw up to four columns at once
			int stop = (x2+1) & ~3;

			if (x1 > x2)
				goto clearfog;

			dc_x = x1;

			while ((dc_x < stop) && (dc_x & 3))
			{
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
				dc_x++;
			}

			while (dc_x < stop)
			{
				rt_initcols();
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex);
				rt_draw4cols (dc_x - 3);
				dc_x++;
			}

			while (dc_x <= x2)
			{
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
				dc_x++;
			}
		}
	}
	else
	{ // Texture does wrap vertically.

		rw_offset = 0;
		rw_pic = tex;
		if (colfunc == basecolfunc)
		{
			maskwallscan(x1, x2, mceilingclip, mfloorclip, MaskedSWall, maskedtexturecol, ds->yrepeat);
		}
		else
		{
			transmaskwallscan(x1, x2, mceilingclip, mfloorclip, MaskedSWall, maskedtexturecol, ds->yrepeat);
		}
	}

clearfog:
	R_FinishSetPatchStyle ();
	//if (ds->bFogBoundary)
	{
		clearbufshort (openings + ds->sprtopclip - ds->x1 + x1, x2-x1+1, viewheight);
	}
	return;
}

// prevlineasm1 is like vlineasm1 but skips the loop if only drawing one pixel
inline fixed_t prevline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
	return doprevline1 ();
}

void wallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
			   fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	int x, shiftval;
	int y1ve[4], y2ve[4], u4, d4, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	// This function also gets used to draw skies. Unlike BUILD, skies are
	// drawn by visplane instead of by bunch, so these checks are invalid.
	//if ((uwal[x1] > viewheight) && (uwal[x2] > viewheight)) return;
	//if ((dwal[x1] < 0) && (dwal[x2] < 0)) return;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setupvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = x1;
	//while ((umost[x] > dmost[x]) && (x <= x2)) x++;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x <= x2) && (x & 3); ++x)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;
		assert (y1ve[0] < viewheight);
		assert (y2ve[0] <= viewheight);

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + x + dc_destorg;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		dovline1();
	}

	for(; x <= x2-3; x += 4)
	{
		bad = 0;
		for (z = 3; z>= 0; --z)
		{
			y1ve[z] = uwal[x+z];//max(uwal[x+z],umost[x+z]);
			y2ve[z] = dwal[x+z];//min(dwal[x+z],dmost[x+z])-1;
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }
			assert (y1ve[z] < viewheight);
			assert (y2ve[z] <= viewheight);

			bufplce[z] = getcol (rw_pic, (lwal[x+z] + xoffset) >> FRACBITS);
			vince[z] = swal[x+z] * yrepeat;
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					prevline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+x+z+dc_destorg);
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				vplce[z] = prevline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+x+z+dc_destorg);
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+x+dc_destorg;
			dovline4();
		}

		BYTE *i = x+ylookup[d4]+dc_destorg;
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				prevline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
			}
		}
	}
	for(;x<=x2;x++)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;
		assert (y1ve[0] < viewheight);
		assert (y2ve[0] <= viewheight);

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + x + dc_destorg;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		dovline1();
	}

//unclock (WallScanCycles);

	NetUpdate ();
}

void wallscan_striped (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat)
{
	bool flooding = false;
	FDynamicColormap *startcolormap = basecolormap;
	int startshade = wallshade;
	bool fogginess = foggy;

	FDynamicColormap *floodcolormap = startcolormap;
	int floodshade = startshade;
	bool floodfoggy = foggy;

	short most1[MAXWIDTH], most2[MAXWIDTH], most3[MAXWIDTH];
	short *up, *down;

	FExtraLight *el = frontsector->ExtraLights;

	up = uwal;
	down = most1;

	assert(WallSX1 <= x1);
	assert(WallSX2 > x2);

	for (int i = 0; i < el->NumUsedLights; ++i)
	{
		if (flooding && !el->Lights[i].bFlooder)
		{
			continue;
		}

		if (!el->Lights[i].bOverlaps)
		{
			int j = WallMost (most3, el->Lights[i].Plane);

			if (j != 3 /*&& (most3[x1] > up[x1] || most3[x2] > up[x2])*/)
			{
				for (int j = x1; j <= x2; ++j)
				{
					down[j] = clamp (most3[j], up[j], dwal[j]);
				}
				wallscan (x1, x2, up, down, swal, lwal, yrepeat);

				up = down;
				down = (down == most1) ? most2 : most1;
			}
		}

		if (el->Lights[i].Master == NULL)
		{
			basecolormap = floodcolormap;
			wallshade = floodshade;
			fogginess = floodfoggy;
		}
		else
		{
			basecolormap = el->Lights[i].Master->ColorMap;
			fogginess = level.fadeto || basecolormap->Fade;
			wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(fogginess,
				el->Lights[i].Master->lightlevel) + r_actualextralight);
			if (el->Lights[i].bFlooder)
			{
				floodcolormap = basecolormap;
				floodshade = wallshade;
				flooding = true;
			}
		}
	}
	wallscan (x1, x2, up, dwal, swal, lwal, yrepeat);
	basecolormap = startcolormap;
	wallshade = startshade;
}

inline fixed_t mvline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
	return domvline1 ();
}

void maskwallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
	fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	int x, shiftval;
	BYTE *p;
	int y1ve[4], y2ve[4], u4, d4, startx, dax, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

	if (!rw_pic->bMasked)
	{ // Textures that aren't masked can use the faster wallscan.
		wallscan (x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setupmvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = startx = x1;
	p = x + dc_destorg;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x <= x2) && ((size_t)p & 3); ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		domvline1();
	}

	for(; x <= x2-3; x += 4, p+= 4)
	{
		bad = 0;
		for (z = 3, dax = x+3; z >= 0; --z, --dax)
		{
			y1ve[z] = uwal[dax];
			y2ve[z] = dwal[dax];
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }

			bufplce[z] = getcol (rw_pic, (lwal[dax] + xoffset) >> FRACBITS);
			vince[z] = swal[dax] * yrepeat;
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					mvline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				vplce[z] = mvline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+p;
			domvline4();
		}

		BYTE *i = p+ylookup[d4];
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				mvline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
			}
		}
	}
	for(; x <= x2; ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];
		y2ve[0] = dwal[x];
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		domvline1();
	}

//unclock(WallScanCycles);

	NetUpdate ();
}

inline void preptmvline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
}

void transmaskwallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
	fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	fixed_t (*tmvline1)();
	void (*tmvline4)();
	int x, shiftval;
	BYTE *p;
	int y1ve[4], y2ve[4], u4, d4, startx, dax, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

	if (!R_GetTransMaskDrawers (&tmvline1, &tmvline4))
	{
		// The current translucency is unsupported, so draw with regular maskwallscan instead.
		maskwallscan (x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setuptmvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = startx = x1;
	p = x + dc_destorg;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x <= x2) && ((size_t)p & 3); ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		tmvline1();
	}

	for(; x <= x2-3; x += 4, p+= 4)
	{
		bad = 0;
		for (z = 3, dax = x+3; z >= 0; --z, --dax)
		{
			y1ve[z] = uwal[dax];
			y2ve[z] = dwal[dax];
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }

			bufplce[z] = getcol (rw_pic, (lwal[dax] + xoffset) >> FRACBITS);
			vince[z] = swal[dax] * yrepeat;
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					preptmvline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
					tmvline1();
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				preptmvline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
				vplce[z] = tmvline1();
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+p;
			tmvline4();
		}

		BYTE *i = p+ylookup[d4];
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				preptmvline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
				tmvline1();
			}
		}
	}
	for(; x <= x2; ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];
		y2ve[0] = dwal[x];
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		tmvline1();
	}

//unclock(WallScanCycles);

	NetUpdate ();
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//
// [RH] Rewrote this to use Build's wallscan, so it's quite far
// removed from the original Doom routine.
//

void R_RenderSegLoop ()
{
	int x1 = rw_x;
	int x2 = rw_stopx;
	int x;
	fixed_t xscale, yscale;
	fixed_t xoffset = rw_offset;

	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;

	// clip wall to the floor and ceiling
	for (x = x1; x < x2; ++x)
	{
		if (walltop[x] < ceilingclip[x])
		{
			walltop[x] = ceilingclip[x];
		}
		if (wallbottom[x] > floorclip[x])
		{
			wallbottom[x] = floorclip[x];
		}
	}

	// mark ceiling areas
	if (markceiling)
	{
		for (x = x1; x < x2; ++x)
		{
			short top = ceilingclip[x];
			short bottom = MIN (walltop[x], floorclip[x]);
			if (top < bottom)
			{
				ceilingplane->top[x] = top;
				ceilingplane->bottom[x] = bottom;
			}
		}
	}

	// mark floor areas
	if (markfloor)
	{
		for (x = x1; x < x2; ++x)
		{
			short top = MAX (wallbottom[x], ceilingclip[x]);
			short bottom = floorclip[x];
			if (top < bottom)
			{
				assert (bottom <= viewheight);
				floorplane->top[x] = top;
				floorplane->bottom[x] = bottom;
			}
		}
	}

	// draw the wall tiers
	if (midtexture)
	{ // one sided line
		if (midtexture->UseType != FTexture::TEX_Null && viewactive)
		{
			dc_texturemid = rw_midtexturemid;
			rw_pic = midtexture;
			xscale = FixedMul(rw_pic->xScale, rw_midtexturescalex);
			yscale = FixedMul(rw_pic->yScale, rw_midtexturescaley);
			if (xscale != lwallscale)
			{
				PrepLWall (lwall, curline->sidedef->TexelLength*xscale);
				lwallscale = xscale;
			}
			if (midtexture->bWorldPanning)
			{
				rw_offset = MulScale16 (rw_offset_mid, xscale);
			}
			else
			{
				rw_offset = rw_offset_mid;
			}
			if (fixedcolormap != NULL || !frontsector->ExtraLights)
			{
				wallscan (x1, x2-1, walltop, wallbottom, swall, lwall, yscale);
			}
			else
			{
				wallscan_striped (x1, x2-1, walltop, wallbottom, swall, lwall, yscale);
			}
		}
		clearbufshort (ceilingclip+x1, x2-x1, viewheight);
		clearbufshort (floorclip+x1, x2-x1, 0xffff);
	}
	else
	{ // two sided line
		if (toptexture != NULL && toptexture->UseType != FTexture::TEX_Null)
		{ // top wall
			for (x = x1; x < x2; ++x)
			{
				wallupper[x] = MAX (MIN (wallupper[x], floorclip[x]), walltop[x]);
			}
			if (viewactive)
			{
				dc_texturemid = rw_toptexturemid;
				rw_pic = toptexture;
				xscale = FixedMul(rw_pic->xScale, rw_toptexturescalex);
				yscale = FixedMul(rw_pic->yScale, rw_toptexturescaley);
				if (xscale != lwallscale)
				{
					PrepLWall (lwall, curline->sidedef->TexelLength*xscale);
					lwallscale = xscale;
				}
				if (toptexture->bWorldPanning)
				{
					rw_offset = MulScale16 (rw_offset_top, xscale);
				}
				else
				{
					rw_offset = rw_offset_top;
				}
				if (fixedcolormap != NULL || !frontsector->ExtraLights)
				{
					wallscan (x1, x2-1, walltop, wallupper, swall, lwall, yscale);
				}
				else
				{
					wallscan_striped (x1, x2-1, walltop, wallupper, swall, lwall, yscale);
				}
			}
			memcpy (ceilingclip+x1, wallupper+x1, (x2-x1)*sizeof(short));
		}
		else if (markceiling)
		{ // no top wall
			memcpy (ceilingclip+x1, walltop+x1, (x2-x1)*sizeof(short));
		}

		
		if (bottomtexture != NULL && bottomtexture->UseType != FTexture::TEX_Null)
		{ // bottom wall
			for (x = x1; x < x2; ++x)
			{
				walllower[x] = MIN (MAX (walllower[x], ceilingclip[x]), wallbottom[x]);
			}
			if (viewactive)
			{
				dc_texturemid = rw_bottomtexturemid;
				rw_pic = bottomtexture;
				xscale = FixedMul(rw_pic->xScale, rw_bottomtexturescalex);
				yscale = FixedMul(rw_pic->yScale, rw_bottomtexturescaley);
				if (xscale != lwallscale)
				{
					PrepLWall (lwall, curline->sidedef->TexelLength*xscale);
					lwallscale = xscale;
				}
				if (bottomtexture->bWorldPanning)
				{
					rw_offset = MulScale16 (rw_offset_bottom, xscale);
				}
				else
				{
					rw_offset = rw_offset_bottom;
				}
				if (fixedcolormap != NULL || !frontsector->ExtraLights)
				{
					wallscan (x1, x2-1, walllower, wallbottom, swall, lwall, yscale);
				}
				else
				{
					wallscan_striped (x1, x2-1, walllower, wallbottom, swall, lwall, yscale);
				}
			}
			memcpy (floorclip+x1, walllower+x1, (x2-x1)*sizeof(short));
		}
		else if (markfloor)
		{ // no bottom wall
			memcpy (floorclip+x1, wallbottom+x1, (x2-x1)*sizeof(short));
		}
	}
	rw_offset = xoffset;
}

void R_NewWall (bool needlights)
{
	fixed_t rowoffset, yrepeat;

	rw_markmirror = false;

	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// mark the segment as visible for auto map
	if (!r_dontmaplines) linedef->flags |= ML_MAPPED;

	midtexture = toptexture = bottomtexture = 0;

	if (backsector == NULL)
	{
		// single sided line
		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;
		// [RH] Render mirrors later, but mark them now.
		if (linedef->special != Line_Mirror || !r_drawmirrors)
		{
			// [RH] Horizon lines do not need to be textured
			if (linedef->special != Line_Horizon)
			{
				midtexture = TexMan(sidedef->GetTexture(side_t::mid));
				rw_offset_mid = sidedef->GetTextureXOffset(side_t::mid);
				rowoffset = sidedef->GetTextureYOffset(side_t::mid);
				rw_midtexturescalex = sidedef->GetTextureXScale(side_t::mid);
				rw_midtexturescaley = sidedef->GetTextureYScale(side_t::mid);
				yrepeat = FixedMul(midtexture->yScale, rw_midtexturescaley);
				if (linedef->flags & ML_DONTPEGBOTTOM)
				{ // bottom of texture at bottom
					rw_midtexturemid = frontsector->GetPlaneTexZ(sector_t::floor) + (midtexture->GetHeight() << FRACBITS);
				}
				else
				{ // top of texture at top
					rw_midtexturemid = frontsector->GetPlaneTexZ(sector_t::ceiling);
					if (rowoffset < 0 && midtexture != NULL)
					{
						rowoffset += midtexture->GetHeight() << FRACBITS;
					}
				}
				if (midtexture->bWorldPanning)
				{
					rw_midtexturemid = MulScale16(rw_midtexturemid - viewz + rowoffset, yrepeat);
				}
				else
				{
					// rowoffset is added outside the multiply so that it positions the texture
					// by texels instead of world units.
					rw_midtexturemid = MulScale16(rw_midtexturemid - viewz, yrepeat) + rowoffset;
				}
			}
		}
		else
		{
			rw_markmirror = true;
		}
	}
	else
	{ // two-sided line
		// hack to allow height changes in outdoor areas

		rw_frontlowertop = frontsector->GetPlaneTexZ(sector_t::ceiling);

		if (frontsector->GetTexture(sector_t::ceiling) == skyflatnum &&
			backsector->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			if (rw_havehigh)
			{ // front ceiling is above back ceiling
				memcpy (&walltop[WallSX1], &wallupper[WallSX1], (WallSX2 - WallSX1)*sizeof(walltop[0]));
				rw_havehigh = false;
			}
			else if (rw_havelow && frontsector->ceilingplane != backsector->ceilingplane)
			{ // back ceiling is above front ceiling
				// The check for rw_havelow is not Doom-compliant, but it avoids HoM that
				// would otherwise occur because there is space made available for this
				// wall but nothing to draw for it.
				// Recalculate walltop so that the wall is clipped by the back sector's
				// ceiling instead of the front sector's ceiling.
				WallMost (walltop, backsector->ceilingplane);
			}
			// Putting sky ceilings on the front and back of a line alters the way unpegged
			// positioning works.
			rw_frontlowertop = backsector->GetPlaneTexZ(sector_t::ceiling);
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
				|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->GetXOffset(sector_t::floor) != frontsector->GetXOffset(sector_t::floor)
				|| backsector->GetYOffset(sector_t::floor) != frontsector->GetYOffset(sector_t::floor)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| frontsector->heightsec

				|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
				|| backsector->GetFlags(sector_t::floor) != frontsector->GetFlags(sector_t::floor)

				// [RH] Add checks for colormaps
				|| backsector->ColorMap != frontsector->ColorMap

				|| backsector->GetXScale(sector_t::floor) != frontsector->GetXScale(sector_t::floor)
				|| backsector->GetYScale(sector_t::floor) != frontsector->GetYScale(sector_t::floor)

				|| backsector->GetAngle(sector_t::floor) != frontsector->GetAngle(sector_t::floor)

				|| (sidedef->GetTexture(side_t::mid).isValid() &&
					((linedef->flags & (ML_CLIP_MIDTEX|ML_WRAP_MIDTEX)) ||
					 (sidedef->Flags & (WALLF_CLIP_MIDTEX|WALLF_WRAP_MIDTEX))))
				;

			markceiling = (frontsector->GetTexture(sector_t::ceiling) != skyflatnum ||
				backsector->GetTexture(sector_t::ceiling) != skyflatnum) &&
				(rw_mustmarkceiling
				|| backsector->ceilingplane != frontsector->ceilingplane
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->GetXOffset(sector_t::ceiling) != frontsector->GetXOffset(sector_t::ceiling)
				|| backsector->GetYOffset(sector_t::ceiling) != frontsector->GetYOffset(sector_t::ceiling)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (frontsector->heightsec && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

				|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
				|| backsector->GetFlags(sector_t::ceiling) != frontsector->GetFlags(sector_t::ceiling)

				// [RH] Add check for colormaps
				|| backsector->ColorMap != frontsector->ColorMap

				|| backsector->GetXScale(sector_t::ceiling) != frontsector->GetXScale(sector_t::ceiling)
				|| backsector->GetYScale(sector_t::ceiling) != frontsector->GetYScale(sector_t::ceiling)

				|| backsector->GetAngle(sector_t::ceiling) != frontsector->GetAngle(sector_t::ceiling)

				|| (sidedef->GetTexture(side_t::mid).isValid() &&
					((linedef->flags & (ML_CLIP_MIDTEX|ML_WRAP_MIDTEX)) ||
					(sidedef->Flags & (WALLF_CLIP_MIDTEX|WALLF_WRAP_MIDTEX))))
				);
		}

		if (rw_havehigh)
		{ // top texture
			toptexture = TexMan(sidedef->GetTexture(side_t::top));

			rw_offset_top = sidedef->GetTextureXOffset(side_t::top);
			rowoffset = sidedef->GetTextureYOffset(side_t::top);
			rw_toptexturescalex = sidedef->GetTextureXScale(side_t::top);
			rw_toptexturescaley = sidedef->GetTextureYScale(side_t::top);
			yrepeat = FixedMul(toptexture->yScale, rw_toptexturescaley);
			if (linedef->flags & ML_DONTPEGTOP)
			{ // top of texture at top
				rw_toptexturemid = MulScale16 (frontsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat);
				if (rowoffset < 0 && toptexture != NULL)
				{
					rowoffset += toptexture->GetHeight() << FRACBITS;
				}
			}
			else
			{ // bottom of texture at bottom
				rw_toptexturemid = MulScale16(backsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat) + (toptexture->GetHeight() << FRACBITS);
			}
			if (toptexture->bWorldPanning)
			{
				rw_toptexturemid += MulScale16(rowoffset, yrepeat);
			}
			else
			{
				rw_toptexturemid += rowoffset;
			}
		}
		if (rw_havelow)
		{ // bottom texture
			bottomtexture = TexMan(sidedef->GetTexture(side_t::bottom));

			rw_offset_bottom = sidedef->GetTextureXOffset(side_t::bottom);
			rowoffset = sidedef->GetTextureYOffset(side_t::bottom);
			rw_bottomtexturescalex = sidedef->GetTextureXScale(side_t::bottom);
			rw_bottomtexturescaley = sidedef->GetTextureYScale(side_t::bottom);
			yrepeat = FixedMul(bottomtexture->yScale, rw_bottomtexturescaley);
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				rw_bottomtexturemid = rw_frontlowertop;
			}
			else
			{ // top of texture at top
				rw_bottomtexturemid = backsector->GetPlaneTexZ(sector_t::floor);
				if (rowoffset < 0 && bottomtexture != NULL)
				{
					rowoffset += bottomtexture->GetHeight() << FRACBITS;
				}
			}
			if (bottomtexture->bWorldPanning)
			{
				rw_bottomtexturemid = MulScale16(rw_bottomtexturemid - viewz + rowoffset, yrepeat);
			}
			else
			{
				rw_bottomtexturemid = MulScale16(rw_bottomtexturemid - viewz, yrepeat) + rowoffset;
			}
		}
	}

	// if a floor / ceiling plane is on the wrong side of the view plane,
	// it is definitely invisible and doesn't need to be marked.

	// killough 3/7/98: add deep water check
	if (frontsector->GetHeightSec() == NULL)
	{
		if (frontsector->floorplane.ZatPoint (viewx, viewy) >= viewz)       // above view plane
			markfloor = false;
		if (frontsector->ceilingplane.ZatPoint (viewx, viewy) <= viewz &&
			frontsector->GetTexture(sector_t::ceiling) != skyflatnum)   // below view plane
			markceiling = false;
	}

	FTexture *midtex = TexMan(sidedef->GetTexture(side_t::mid));

	segtextured = midtex != NULL || toptexture != NULL || bottomtexture != NULL;

	// calculate light table
	if (needlights && (segtextured || (backsector && IsFogBoundary(frontsector, backsector))))
	{
		lwallscale =
			midtex ? FixedMul(midtex->xScale, sidedef->GetTextureXScale(side_t::mid)) :
			toptexture ? FixedMul(toptexture->xScale, sidedef->GetTextureXScale(side_t::top)) :
			bottomtexture ? FixedMul(bottomtexture->xScale, sidedef->GetTextureXScale(side_t::bottom)) :
			FRACUNIT;

		PrepWall (swall, lwall, sidedef->TexelLength * lwallscale);

		if (fixedcolormap == NULL && fixedlightlev < 0)
		{
			wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, frontsector->lightlevel)
				+ r_actualextralight);
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

CVAR(Bool, r_smoothlighting, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

int side_t::GetLightLevel (bool foggy, int baselight) const
{
	if (Flags & WALLF_ABSLIGHTING) 
	{
		baselight = (BYTE)Light;
	}


	if (!foggy) // Don't do relative lighting in foggy sectors
	{
		if (!(Flags & WALLF_NOFAKECONTRAST))
		{
			if (((level.flags2 & LEVEL2_SMOOTHLIGHTING) || (Flags & WALLF_SMOOTHLIGHTING) || r_smoothlighting) &&
				linedef->dx != 0)
			{
				baselight += int // OMG LEE KILLOUGH LIVES! :/
					(
						(float(level.WallHorizLight)
						+abs(atan(float(linedef->dy)/float(linedef->dx))/float(1.57079))
						*float(level.WallVertLight - level.WallHorizLight))
					);
			}
			else
			{
				baselight += linedef->dx==0? level.WallVertLight : 
							 linedef->dy==0? level.WallHorizLight : 0;
			}
		}
		if (!(Flags & WALLF_ABSLIGHTING))
		{
			baselight += this->Light;
		}
	}
	return clamp(baselight, 0, 255);
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
		drawsegs = (drawseg_t *)M_Realloc (drawsegs, newdrawsegs * sizeof(drawseg_t));
		firstdrawseg = drawsegs + firstofs;
		ds_p = drawsegs + MaxDrawSegs;
		MaxDrawSegs = newdrawsegs;
		DPrintf ("MaxDrawSegs increased to %zu\n", MaxDrawSegs);
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
		openings = (short *)M_Realloc (openings, maxopenings * sizeof(*openings));
		DPrintf ("MaxOpenings increased to %zu\n", maxopenings);
	}
}

//
// R_StoreWallRange
// A wall segment will be drawn between start and stop pixels (inclusive).
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
		R_NewWall (true);
	}

	rw_offset = sidedef->GetTextureXOffset(side_t::mid);
	rw_light = rw_lightleft + rw_lightstep * (start - WallSX1);

	ds_p->sx1 = WallSX1;
	ds_p->sx2 = WallSX2;
	ds_p->sz1 = WallSZ1;
	ds_p->sz2 = WallSZ2;
	ds_p->cx = WallTX1;
	ds_p->cy = WallTY1;
	ds_p->cdx = WallTX2 - WallTX1;
	ds_p->cdy = WallTY2 - WallTY1;
	ds_p->siz1 = (DWORD)DivScale32 (1, WallSZ1) >> 1;
	ds_p->siz2 = (DWORD)DivScale32 (1, WallSZ2) >> 1;
	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop-1;
	ds_p->curline = curline;
	rw_stopx = stop;
	ds_p->bFogBoundary = false;

	// killough 1/6/98, 2/1/98: remove limit on openings
	R_CheckOpenings ((stop - start)*6);

	ds_p->sprtopclip = ds_p->sprbottomclip = ds_p->maskedtexturecol = ds_p->swall = -1;

	if (rw_markmirror)
	{
		size_t drawsegnum = ds_p - drawsegs;
		WallMirrors.Push (drawsegnum);
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

		if (rw_frontfz1 > rw_backfz1 || rw_frontfz2 > rw_backfz2 ||
			backsector->floorplane.ZatPoint (viewx, viewy) > viewz)
		{
			ds_p->silhouette = SIL_BOTTOM;
		}

		if (rw_frontcz1 < rw_backcz1 || rw_frontcz2 < rw_backcz2 ||
			backsector->ceilingplane.ZatPoint (viewx, viewy) < viewz)
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
		if ((TexMan(sidedef->GetTexture(side_t::mid))->UseType != FTexture::TEX_Null || IsFogBoundary (frontsector, backsector)) &&
			(rw_ceilstat != 12 || !sidedef->GetTexture(side_t::top).isValid()) &&
			(rw_floorstat != 3 || !sidedef->GetTexture(side_t::bottom).isValid()) &&
			(WallSZ1 >= TOO_CLOSE_Z && WallSZ2 >= TOO_CLOSE_Z))
		{
			fixed_t *swal;
			fixed_t *lwal;
			int i;

			maskedtexture = true;

			ds_p->bFogBoundary = IsFogBoundary (frontsector, backsector);
			if (sidedef->GetTexture(side_t::mid).isValid())
			{
				ds_p->maskedtexturecol = R_NewOpening ((stop - start) * 2);
				ds_p->swall = R_NewOpening ((stop - start) * 2);

				lwal = (fixed_t *)(openings + ds_p->maskedtexturecol);
				swal = (fixed_t *)(openings + ds_p->swall);
				FTexture *pic = TexMan(sidedef->GetTexture(side_t::mid));
				fixed_t yrepeat = FixedMul(pic->yScale, sidedef->GetTextureYScale(side_t::mid));
				fixed_t xoffset = sidedef->GetTextureXOffset(side_t::mid);

				if (pic->bWorldPanning)
				{
					xoffset = MulScale16 (xoffset, lwallscale);
				}

				for (i = start; i < stop; i++)
				{
					*lwal++ = lwall[i] + xoffset;
					*swal++ = swall[i];
				}

				fixed_t istart = MulScale18 (*((fixed_t *)(openings + ds_p->swall)), yrepeat);
				fixed_t iend = MulScale18 (*(swal - 1), yrepeat);

				if (istart < 3 && istart >= 0) istart = 3;
				if (istart > -3 && istart < 0) istart = -3;
				if (iend < 3 && iend >= 0) iend = 3;
				if (iend > -3 && iend < 0) iend = -3;
				istart = DivScale32 (1, istart);
				iend = DivScale32 (1, iend);
				ds_p->yrepeat = yrepeat;
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
			ds_p->light = rw_light;
			ds_p->lightstep = rw_lightstep;
			ds_p->shade = wallshade;

			if (ds_p->bFogBoundary || ds_p->maskedtexturecol != -1)
			{
				size_t drawsegnum = ds_p - drawsegs;
				InterestingDrawsegs.Push (drawsegnum);
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

	if (maskedtexture && curline->sidedef->GetTexture(side_t::mid).isValid())
	{
		ds_p->silhouette |= SIL_TOP | SIL_BOTTOM;
	}

	// [RH] Draw any decals bound to the seg
	for (DBaseDecal *decal = curline->sidedef->AttachedDecals; decal != NULL; decal = decal->WallNext)
	{
		R_RenderDecal (curline->sidedef, decal, ds_p, 0);
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

void PrepWall (fixed_t *swall, fixed_t *lwall, fixed_t walxrepeat)
{ // swall = scale, lwall = texturecolumn
	int x;
	float top, bot, i;
	float xrepeat = (float)walxrepeat;
	float ol, l, topinc, botinc;

	i = (float)(WallSX1 - centerx);
	top = WallUoverZorg + WallUoverZstep * i;
	bot = WallInvZorg + WallInvZstep * i;
	topinc = WallUoverZstep * 4.f;
	botinc = WallInvZstep * 4.f;

	x = WallSX1;

	l = top / bot;
	swall[x] = quickertoint (l * WallDepthScale + WallDepthOrg);
	lwall[x] = quickertoint (l * xrepeat);
	// As long as l is invalid, step one column at a time so that
	// we can get as many correct texture columns as possible.
	while (l > 1.0 && x+1 < WallSX2)
	{
		l = (top += WallUoverZstep) / (bot += WallInvZstep);
		x++;
		swall[x] = quickertoint (l * WallDepthScale + WallDepthOrg);
		lwall[x] = quickertoint (l * xrepeat);
	}
	l *= xrepeat;
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
		l = (top + WallUoverZstep) / (bot + WallInvZstep);
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

	// fix for rounding errors
	fixed_t fix = (MirrorFlags & RF_XFLIP) ? walxrepeat-1 : 0;
	if (WallSX1 > 0)
	{
		for (x = WallSX1; x < WallSX2; x++)
		{
			if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
			{
				lwall[x] = fix;
			}
			else
			{
				break;
			}
		}
	}
	fix = walxrepeat - 1 - fix;
	for (x = WallSX2-1; x >= WallSX1; x--)
	{
		if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
		{
			lwall[x] = fix;
		}
		else
		{
			break;
		}
	}
}

void PrepLWall (fixed_t *lwall, fixed_t walxrepeat)
{ // lwall = texturecolumn
	int x;
	float top, bot, i;
	float xrepeat = (float)walxrepeat;
	float ol, l, topinc, botinc;

	i = (float)(WallSX1 - centerx);
	top = WallUoverZorg + WallUoverZstep * i;
	bot = WallInvZorg + WallInvZstep * i;
	topinc = WallUoverZstep * 4.f;
	botinc = WallInvZstep * 4.f;

	x = WallSX1;

	l = top / bot;
	lwall[x] = quickertoint (l * xrepeat);
	// As long as l is invalid, step one column at a time so that
	// we can get as many correct texture columns as possible.
	while (l > 1.0 && x+1 < WallSX2)
	{
		l = (top += WallUoverZstep) / (bot += WallInvZstep);
		lwall[++x] = quickertoint (l * xrepeat);
	}
	l *= xrepeat;
	while (x+4 < WallSX2)
	{
		top += topinc; bot += botinc;
		ol = l; l = top / bot;
		lwall[x+4] = quickertoint (l *= xrepeat);

		i = (ol+l) * 0.5f;
		lwall[x+2] = quickertoint (i);
		lwall[x+1] = quickertoint ((ol+i) * 0.5f);
		lwall[x+3] = quickertoint ((l+i) * 0.5f);
		x += 4;
	}
	if (x+2 < WallSX2)
	{
		top += topinc * 0.5f; bot += botinc * 0.5f;
		ol = l; l = top / bot;
		lwall[x+2] = quickertoint (l *= xrepeat);
		lwall[x+1] = quickertoint ((l+ol)*0.5f);
		x += 2;
	}
	if (x+1 < WallSX2)
	{
		l = (top + WallUoverZstep) / (bot + WallInvZstep);
		lwall[x+1] = quickertoint (l * xrepeat);
	}

	// fix for rounding errors
	fixed_t fix = (MirrorFlags & RF_XFLIP) ? walxrepeat-1 : 0;
	if (WallSX1 > 0)
	{
		for (x = WallSX1; x < WallSX2; x++)
		{
			if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
			{
				lwall[x] = fix;
			}
			else
			{
				break;
			}
		}
	}
	fix = walxrepeat - 1 - fix;
	for (x = WallSX2-1; x >= WallSX1; x--)
	{
		if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
		{
			lwall[x] = fix;
		}
		else
		{
			break;
		}
	}
}

// pass = 0: when seg is first drawn
//		= 1: drawing masked textures (including sprites)
// Currently, only pass = 0 is done or used

static void R_RenderDecal (side_t *wall, DBaseDecal *decal, drawseg_t *clipper, int pass)
{
	fixed_t lx, ly, lx2, ly2, decalx, decaly;
	int x1, x2;
	fixed_t xscale, yscale;
	fixed_t topoff;
	BYTE flipx;
	fixed_t zpos;
	int needrepeat = 0;
	sector_t *front, *back;

	if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
		return;

	// Determine actor z
	zpos = decal->Z;
	front = curline->frontsector;
	back = (curline->backsector != NULL) ? curline->backsector : curline->frontsector;
	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		zpos = decal->Z;
		break;
	case RF_RELUPPER:
		if (curline->linedef->flags & ML_DONTPEGTOP)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::ceiling);
		}
		break;
	case RF_RELLOWER:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::floor);
		}
		break;
	case RF_RELMID:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::floor);
		}
		else
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
	}

	xscale = decal->ScaleX;
	yscale = decal->ScaleY;

	WallSpriteTile = TexMan(decal->PicNum);
	flipx = (BYTE)(decal->RenderFlags & RF_XFLIP);

	if (WallSpriteTile == NULL || WallSpriteTile->UseType == FTexture::TEX_Null)
	{
		return;
	}

	// Determine left and right edges of sprite. Since this sprite is bound
	// to a wall, we use the wall's angle instead of the decal's. This is
	// pretty much the same as what R_AddLine() does.

	x2 = WallSpriteTile->GetWidth();
	x1 = WallSpriteTile->LeftOffset;
	x2 = x2 - x1;

	x1 *= xscale;
	x2 *= xscale;

	decal->GetXY (wall, decalx, decaly);

	angle_t ang = R_PointToAngle2 (curline->v1->x, curline->v1->y, curline->v2->x, curline->v2->y) >> ANGLETOFINESHIFT;
	lx  = decalx - FixedMul (x1, finecosine[ang]) - viewx;
	lx2 = decalx + FixedMul (x2, finecosine[ang]) - viewx;
	ly  = decaly - FixedMul (x1, finesine[ang]) - viewy;
	ly2 = decaly + FixedMul (x2, finesine[ang]) - viewy;

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
		if (WallTY1 == 0) return;
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

	if (WallSZ1 < TOO_CLOSE_Z)
		return;

	if (WallTX2 <= WallTY2)
	{
		if (WallTX2 < -WallTY2) return;	// right edge is off the left side
		if (WallTY2 == 0) return;
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

	if (x1 >= x2 || x1 > clipper->x2 || x2 <= clipper->x1 || WallSZ2 < TOO_CLOSE_Z)
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
	switch (decal->RenderFlags & RF_CLIPMASK)
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

	topoff = WallSpriteTile->TopOffset << FRACBITS;
	dc_texturemid = topoff + FixedDiv (zpos - viewz, yscale);

	// Clip sprite to drawseg
	if (x1 < clipper->x1)
	{
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
	PrepWall (swall, lwall, WallSpriteTile->GetWidth() << FRACBITS);
	swap (x1, WallSX1);
	swap (x2, WallSX2);

	if (flipx)
	{
		int i;
		int right = (WallSpriteTile->GetWidth() << FRACBITS) - 1;

		for (i = x1; i < x2; i++)
		{
			lwall[i] = right - lwall[i];
		}
	}

	// Prepare lighting
	bool calclighting = false;

	rw_light = rw_lightleft + (x1 - WallSX1) * rw_lightstep;
	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;
	else if (!foggy && (decal->RenderFlags & RF_FULLBRIGHT))
		dc_colormap = basecolormap->Maps;
	else
		calclighting = true;

	// Draw it
	if (decal->RenderFlags & RF_YFLIP)
	{
		sprflipvert = true;
		yscale = -yscale;
		dc_texturemid = dc_texturemid - (WallSpriteTile->GetHeight() << FRACBITS);
	}
	else
	{
		sprflipvert = false;
	}

	// rw_offset is used as the texture's vertical scale
	rw_offset = SafeDivScale30(1, yscale);

	do
	{
		dc_x = x1;
		ESPSResult mode;

		mode = R_SetPatchStyle (decal->RenderStyle, decal->Alpha, decal->Translation, decal->AlphaColor);

		if (mode == DontDraw)
		{
			needrepeat = 0;
		}
		else
		{
			int stop4;

			if (mode == DoDraw0)
			{ // 1 column at a time
				stop4 = dc_x;
			}
			else	 // DoDraw1
			{ // up to 4 columns at a time
				stop4 = x2 & ~3;
			}

			while ((dc_x < stop4) && (dc_x & 3))
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = basecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}

				WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}

			while (dc_x < stop4)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = basecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					WallSpriteColumn (R_DrawMaskedColumnHoriz);
					dc_x++;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = basecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}

				WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}
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
	hcolfunc_post4 = rt_map4cols;

	R_FinishSetPatchStyle ();
}

static void WallSpriteColumn (void (*drawfunc)(const BYTE *column, const FTexture::Span *spans))
{
	unsigned int texturecolumn = lwall[dc_x] >> FRACBITS;
	dc_iscale = MulScale16 (swall[dc_x], rw_offset);
	spryscale = SafeDivScale32 (1, dc_iscale);
	if (sprflipvert)
		sprtopscreen = centeryfrac + FixedMul (dc_texturemid, spryscale);
	else
		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);

	const BYTE *column;
	const FTexture::Span *spans;
	column = WallSpriteTile->GetColumn (texturecolumn, &spans);
	dc_texturefrac = 0;
	drawfunc (column, spans);
	rw_light += rw_lightstep;
}
