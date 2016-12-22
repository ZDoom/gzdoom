/*
**  Wall drawing stuff free of Build pollution
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include <stddef.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"

#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_draw.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"

namespace swrenderer
{
	using namespace drawerargs;

	extern FTexture *rw_pic;
	extern int wallshade;

struct WallSampler
{
	WallSampler() { }
	WallSampler(int y1, float swal, double yrepeat, fixed_t xoffset, FTexture *texture, const BYTE*(*getcol)(FTexture *texture, int x));

	uint32_t uv_pos;
	uint32_t uv_step;
	uint32_t uv_max;

	const BYTE *source;
	uint32_t height;
};

WallSampler::WallSampler(int y1, float swal, double yrepeat, fixed_t xoffset, FTexture *texture, const BYTE*(*getcol)(FTexture *texture, int x))
{
	height = texture->GetHeight();

	int uv_fracbits = 32 - texture->HeightBits;
	if (uv_fracbits != 32)
	{
		uv_max = height << uv_fracbits;

		// Find start uv in [0-base_height[ range.
		// Not using xs_ToFixed because it rounds the result and we need something that always rounds down to stay within the range.
		double uv_stepd = swal * yrepeat;
		double v = (dc_texturemid + uv_stepd * (y1 - CenterY + 0.5)) / height;
		v = v - floor(v);
		v *= height;
		v *= (1 << uv_fracbits);

		uv_pos = (uint32_t)v;
		uv_step = xs_ToFixed(uv_fracbits, uv_stepd);
		if (uv_step == 0) // To prevent divide by zero elsewhere
			uv_step = 1;
	}
	else
	{ // Hack for one pixel tall textures
		uv_pos = 0;
		uv_step = 0;
		uv_max = 1;
	}

	source = getcol(texture, xoffset >> FRACBITS);
}

// Draw a column with support for non-power-of-two ranges
static void Draw1Column(int x, int y1, int y2, WallSampler &sampler, void(*draw1column)())
{
	if (sampler.uv_max == 0 || sampler.uv_step == 0) // power of two
	{
		int count = y2 - y1;

		dc_source = sampler.source;
		dc_dest = (ylookup[y1] + x) + dc_destorg;
		dc_count = count;
		dc_iscale = sampler.uv_step;
		dc_texturefrac = sampler.uv_pos;
		draw1column();

		uint64_t step64 = sampler.uv_step;
		uint64_t pos64 = sampler.uv_pos;
		sampler.uv_pos = (uint32_t)(pos64 + step64 * count);
	}
	else
	{
		uint32_t uv_pos = sampler.uv_pos;

		uint32_t left = y2 - y1;
		while (left > 0)
		{
			uint32_t available = sampler.uv_max - uv_pos;
			uint32_t next_uv_wrap = available / sampler.uv_step;
			if (available % sampler.uv_step != 0)
				next_uv_wrap++;
			uint32_t count = MIN(left, next_uv_wrap);

			dc_source = sampler.source;
			dc_dest = (ylookup[y1] + x) + dc_destorg;
			dc_count = count;
			dc_iscale = sampler.uv_step;
			dc_texturefrac = uv_pos;
			draw1column();

			left -= count;
			uv_pos += sampler.uv_step * count;
			if (uv_pos >= sampler.uv_max)
				uv_pos -= sampler.uv_max;
		}

		sampler.uv_pos = uv_pos;
	}
}

// Draw four columns with support for non-power-of-two ranges
static void Draw4Columns(int x, int y1, int y2, WallSampler *sampler, void(*draw4columns)())
{
	if (sampler[0].uv_max == 0 || sampler[0].uv_step == 0) // power of two, no wrap handling needed
	{
		int count = y2 - y1;
		for (int i = 0; i < 4; i++)
		{
			dc_wall_source[i] = sampler[i].source;
			dc_wall_texturefrac[i] = sampler[i].uv_pos;
			dc_wall_iscale[i] = sampler[i].uv_step;

			uint64_t step64 = sampler[i].uv_step;
			uint64_t pos64 = sampler[i].uv_pos;
			sampler[i].uv_pos = (uint32_t)(pos64 + step64 * count);
		}
		dc_dest = (ylookup[y1] + x) + dc_destorg;
		dc_count = count;
		draw4columns();
	}
	else
	{
		dc_dest = (ylookup[y1] + x) + dc_destorg;
		for (int i = 0; i < 4; i++)
		{
			dc_wall_source[i] = sampler[i].source;
		}

		uint32_t left = y2 - y1;
		while (left > 0)
		{
			// Find which column wraps first
			uint32_t count = left;
			for (int i = 0; i < 4; i++)
			{
				uint32_t available = sampler[i].uv_max - sampler[i].uv_pos;
				uint32_t next_uv_wrap = available / sampler[i].uv_step;
				if (available % sampler[i].uv_step != 0)
					next_uv_wrap++;
				count = MIN(next_uv_wrap, count);
			}

			// Draw until that column wraps
			for (int i = 0; i < 4; i++)
			{
				dc_wall_texturefrac[i] = sampler[i].uv_pos;
				dc_wall_iscale[i] = sampler[i].uv_step;
			}
			dc_count = count;
			draw4columns();

			// Wrap the uv position
			for (int i = 0; i < 4; i++)
			{
				sampler[i].uv_pos += sampler[i].uv_step * count;
				if (sampler[i].uv_pos >= sampler[i].uv_max)
					sampler[i].uv_pos -= sampler[i].uv_max;
			}

			left -= count;
		}
	}
}

typedef void(*DrawColumnFuncPtr)();

static void ProcessWallWorker(
	int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat,
	const BYTE *(*getcol)(FTexture *tex, int x), DrawColumnFuncPtr draw1column, DrawColumnFuncPtr draw4columns)
{
	if (rw_pic->UseType == FTexture::TEX_Null)
		return;

	fixed_t xoffset = rw_offset;

	int fracbits = 32 - rw_pic->HeightBits;
	if (fracbits == 32)
	{ // Hack for one pixel tall textures
		fracbits = 0;
		yrepeat = 0;
		dc_texturemid = 0;
	}

	dc_wall_fracbits = fracbits;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		dc_wall_colormap[0] = dc_colormap;
		dc_wall_colormap[1] = dc_colormap;
		dc_wall_colormap[2] = dc_colormap;
		dc_wall_colormap[3] = dc_colormap;
	}

	if (fixedcolormap)
		dc_colormap = fixedcolormap;
	else
		dc_colormap = basecolormap->Maps;

	float light = rw_light;

	// Calculate where 4 column alignment begins and ends:
	int aligned_x1 = clamp((x1 + 3) / 4 * 4, x1, x2);
	int aligned_x2 = clamp(x2 / 4 * 4, x1, x2);

	// First unaligned columns:
	for (int x = x1; x < aligned_x1; x++, light += rw_lightstep)
	{
		int y1 = uwal[x];
		int y2 = dwal[x];
		if (y2 <= y1)
			continue;

		if (!fixed)
			dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

		WallSampler sampler(y1, swal[x], yrepeat, lwal[x] + xoffset, rw_pic, getcol);
		Draw1Column(x, y1, y2, sampler, draw1column);
	}

	// The aligned columns
	for (int x = aligned_x1; x < aligned_x2; x += 4)
	{
		// Find y1, y2, light and uv values for four columns:
		int y1[4] = { uwal[x], uwal[x + 1], uwal[x + 2], uwal[x + 3] };
		int y2[4] = { dwal[x], dwal[x + 1], dwal[x + 2], dwal[x + 3] };

		float lights[4];
		for (int i = 0; i < 4; i++)
		{
			lights[i] = light;
			light += rw_lightstep;
		}

		WallSampler sampler[4];
		for (int i = 0; i < 4; i++)
			sampler[i] = WallSampler(y1[i], swal[x + i], yrepeat, lwal[x + i] + xoffset, rw_pic, getcol);

		// Figure out where we vertically can start and stop drawing 4 columns in one go
		int middle_y1 = y1[0];
		int middle_y2 = y2[0];
		for (int i = 1; i < 4; i++)
		{
			middle_y1 = MAX(y1[i], middle_y1);
			middle_y2 = MIN(y2[i], middle_y2);
		}

		// If we got an empty column in our set we cannot draw 4 columns in one go:
		bool empty_column_in_set = false;
		for (int i = 0; i < 4; i++)
		{
			if (y2[i] <= y1[i])
				empty_column_in_set = true;
		}

		if (empty_column_in_set || middle_y2 <= middle_y1)
		{
			for (int i = 0; i < 4; i++)
			{
				if (y2[i] <= y1[i])
					continue;

				if (!fixed)
					dc_colormap = basecolormap->Maps + (GETPALOOKUP(lights[i], wallshade) << COLORMAPSHIFT);
				Draw1Column(x + i, y1[i], y2[i], sampler[i], draw1column);
			}
			continue;
		}

		// Draw the first rows where not all 4 columns are active
		for (int i = 0; i < 4; i++)
		{
			if (!fixed)
				dc_colormap = basecolormap->Maps + (GETPALOOKUP(lights[i], wallshade) << COLORMAPSHIFT);

			if (y1[i] < middle_y1)
				Draw1Column(x + i, y1[i], middle_y1, sampler[i], draw1column);
		}

		// Draw the area where all 4 columns are active
		if (!fixed)
		{
			for (int i = 0; i < 4; i++)
			{
				dc_wall_colormap[i] = basecolormap->Maps + (GETPALOOKUP(lights[i], wallshade) << COLORMAPSHIFT);
			}
		}
		Draw4Columns(x, middle_y1, middle_y2, sampler, draw4columns);

		// Draw the last rows where not all 4 columns are active
		for (int i = 0; i < 4; i++)
		{
			if (!fixed)
				dc_colormap = basecolormap->Maps + (GETPALOOKUP(lights[i], wallshade) << COLORMAPSHIFT);

			if (middle_y2 < y2[i])
				Draw1Column(x + i, middle_y2, y2[i], sampler[i], draw1column);
		}
	}

	// The last unaligned columns:
	for (int x = aligned_x2; x < x2; x++, light += rw_lightstep)
	{
		int y1 = uwal[x];
		int y2 = dwal[x];
		if (y2 <= y1)
			continue;

		if (!fixed)
			dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

		WallSampler sampler(y1, swal[x], yrepeat, lwal[x] + xoffset, rw_pic, getcol);
		Draw1Column(x, y1, y2, sampler, draw1column);
	}

	NetUpdate();
}

static void ProcessNormalWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
{
	ProcessWallWorker(x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol, R_DrawWallCol1, R_DrawWallCol4);
}

static void ProcessMaskedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
{
	if (!rw_pic->bMasked) // Textures that aren't masked can use the faster ProcessNormalWall.
	{
		ProcessNormalWall(x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
	}
	else
	{
		ProcessWallWorker(x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol, R_DrawWallMaskedCol1, R_DrawWallMaskedCol4);
	}
}

static void ProcessTranslucentWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
{
	void (*drawcol1)();
	void (*drawcol4)();
	if (!R_GetTransMaskDrawers(&drawcol1, &drawcol4))
	{
		// The current translucency is unsupported, so draw with regular ProcessMaskedWall instead.
		ProcessMaskedWall(x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
	}
	else
	{
		ProcessWallWorker(x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol, drawcol1, drawcol4);
	}
}

static void ProcessStripedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat)
{
	FDynamicColormap *startcolormap = basecolormap;
	int startshade = wallshade;
	bool fogginess = foggy;

	short most1[MAXWIDTH], most2[MAXWIDTH], most3[MAXWIDTH];
	short *up, *down;

	up = uwal;
	down = most1;

	assert(WallC.sx1 <= x1);
	assert(WallC.sx2 >= x2);

	// kg3D - fake floors instead of zdoom light list
	for (unsigned int i = 0; i < frontsector->e->XFloor.lightlist.Size(); i++)
	{
		int j = R_CreateWallSegmentYSloped (most3, frontsector->e->XFloor.lightlist[i].plane, &WallC);
		if (j != 3)
		{
			for (int j = x1; j < x2; ++j)
			{
				down[j] = clamp (most3[j], up[j], dwal[j]);
			}
			ProcessNormalWall (x1, x2, up, down, swal, lwal, yrepeat);
			up = down;
			down = (down == most1) ? most2 : most1;
		}

		lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
		basecolormap = lit->extra_colormap;
		wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(fogginess,
			*lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
 	}

	ProcessNormalWall (x1, x2, up, dwal, swal, lwal, yrepeat);
	basecolormap = startcolormap;
	wallshade = startshade;
}

static void ProcessWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, bool mask)
{
	if (mask)
	{
		if (colfunc == basecolfunc)
		{
			ProcessMaskedWall(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
		else
		{
			ProcessTranslucentWall(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
	}
	else
	{
		if (fixedcolormap != NULL || fixedlightlev >= 0 || !(frontsector->e && frontsector->e->XFloor.lightlist.Size()))
		{
			ProcessNormalWall(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
		else
		{
			ProcessStripedWall(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
	}
}

//=============================================================================
//
// ProcessWallNP2
//
// This is a wrapper around ProcessWall that helps it tile textures whose heights
// are not powers of 2. It divides the wall into texture-sized strips and calls
// ProcessNormalWall for each of those. Since only one repetition of the texture fits
// in each strip, ProcessWall will not tile.
//
//=============================================================================

static void ProcessWallNP2(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, double top, double bot, bool mask)
{
	short most1[MAXWIDTH], most2[MAXWIDTH], most3[MAXWIDTH];
	short *up, *down;
	double texheight = rw_pic->GetHeight();
	double partition;
	double scaledtexheight = texheight / yrepeat;

	if (yrepeat >= 0)
	{ // normal orientation: draw strips from top to bottom
		partition = top - fmod(top - dc_texturemid / yrepeat - ViewPos.Z, scaledtexheight);
		if (partition == top)
		{
			partition -= scaledtexheight;
		}
		up = uwal;
		down = most1;
		dc_texturemid = (partition - ViewPos.Z) * yrepeat + texheight;
		while (partition > bot)
		{
			int j = R_CreateWallSegmentY(most3, partition - ViewPos.Z, &WallC);
			if (j != 3)
			{
				for (int j = x1; j < x2; ++j)
				{
					down[j] = clamp(most3[j], up[j], dwal[j]);
				}
				ProcessWall(x1, x2, up, down, swal, lwal, yrepeat, mask);
				up = down;
				down = (down == most1) ? most2 : most1;
			}
			partition -= scaledtexheight;
			dc_texturemid -= texheight;
 		}
		ProcessWall(x1, x2, up, dwal, swal, lwal, yrepeat, mask);
	}
	else
	{ // upside down: draw strips from bottom to top
		partition = bot - fmod(bot - dc_texturemid / yrepeat - ViewPos.Z, scaledtexheight);
		up = most1;
		down = dwal;
		dc_texturemid = (partition - ViewPos.Z) * yrepeat + texheight;
		while (partition < top)
		{
			int j = R_CreateWallSegmentY(most3, partition - ViewPos.Z, &WallC);
			if (j != 12)
			{
				for (int j = x1; j < x2; ++j)
				{
					up[j] = clamp(most3[j], uwal[j], down[j]);
				}
				ProcessWall(x1, x2, up, down, swal, lwal, yrepeat, mask);
				down = up;
				up = (up == most1) ? most2 : most1;
			}
			partition -= scaledtexheight;
			dc_texturemid -= texheight;
 		}
		ProcessWall(x1, x2, uwal, down, swal, lwal, yrepeat, mask);
	}
}

void R_DrawDrawSeg(drawseg_t *ds, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat)
{
	if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
	{
		double frontcz1 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v1);
		double frontfz1 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v1);
		double frontcz2 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v2);
		double frontfz2 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v2);
		double top = MAX(frontcz1, frontcz2);
		double bot = MIN(frontfz1, frontfz2);
		if (fake3D & FAKE3D_CLIPTOP)
		{
			top = MIN(top, sclipTop);
		}
		if (fake3D & FAKE3D_CLIPBOTTOM)
		{
			bot = MAX(bot, sclipBottom);
		}
		ProcessWallNP2(x1, x2, uwal, dwal, swal, lwal, yrepeat, top, bot, true);
	}
	else
	{
		ProcessWall(x1, x2, uwal, dwal, swal, lwal, yrepeat, true);
	}
}


void R_DrawWallSegment(FTexture *rw_pic, int x1, int x2, short *walltop, short *wallbottom, float *swall, fixed_t *lwall, double yscale, double top, double bottom, bool mask)
{
	if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
	{
		ProcessWallNP2(x1, x2, walltop, wallbottom, swall, lwall, yscale, top, bottom, false);
	}
	else
	{
		ProcessWall(x1, x2, walltop, wallbottom, swall, lwall, yscale, false);
	}
}

void R_DrawSkySegment(visplane_t *pl, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	ProcessNormalWall(pl->left, pl->right, uwal, dwal, swal, lwal, yrepeat, getcol);
}



}