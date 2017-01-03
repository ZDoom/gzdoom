/*
**  Wall drawing stuff
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

#include "swrenderer/r_main.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_walldraw.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "gl/dynlights/gl_dynlight.h"
#include "swrenderer/drawers/r_drawers.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_wallsetup.h"

namespace swrenderer
{
	using namespace drawerargs;

	namespace
	{
		FTexture *rw_pic;
	}

	static const uint8_t *R_GetColumn(FTexture *tex, int col)
	{
		int width;

		// If the texture's width isn't a power of 2, then we need to make it a
		// positive offset for proper clamping.
		if (col < 0 && (width = tex->GetWidth()) != (1 << tex->WidthBits))
		{
			col = width + (col % width);
		}

		if (r_swtruecolor)
			return (const uint8_t *)tex->GetColumnBgra(col, nullptr);
		else
			return tex->GetColumn(col, nullptr);
	}

	WallSampler::WallSampler(int y1, float swal, double yrepeat, fixed_t xoffset, double xmagnitude, FTexture *texture, const BYTE*(*getcol)(FTexture *texture, int x))
	{
		xoffset += FLOAT2FIXED(xmagnitude * 0.5);

		if (!r_swtruecolor)
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
			source2 = nullptr;
			texturefracx = 0;
		}
		else
		{
			// Normalize to 0-1 range:
			double uv_stepd = swal * yrepeat;
			double v = (dc_texturemid + uv_stepd * (y1 - CenterY + 0.5)) / texture->GetHeight();
			v = v - floor(v);
			double v_step = uv_stepd / texture->GetHeight();

			if (isnan(v) || isnan(v_step)) // this should never happen, but it apparently does..
			{
				uv_stepd = 0.0;
				v = 0.0;
				v_step = 0.0;
			}

			// Convert to uint32:
			uv_pos = (uint32_t)(v * 0x100000000LL);
			uv_step = (uint32_t)(v_step * 0x100000000LL);
			uv_max = 0;

			// Texture mipmap and filter selection:
			if (getcol != R_GetColumn)
			{
				source = getcol(texture, xoffset >> FRACBITS);
				source2 = nullptr;
				height = texture->GetHeight();
				texturefracx = 0;
			}
			else
			{
				double ymagnitude = fabs(uv_stepd);
				double magnitude = MAX(ymagnitude, xmagnitude);
				double min_lod = -1000.0;
				double lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
				bool magnifying = lod < 0.0f;

				int mipmap_offset = 0;
				int mip_width = texture->GetWidth();
				int mip_height = texture->GetHeight();
				if (r_mipmap && texture->Mipmapped() && mip_width > 1 && mip_height > 1)
				{
					uint32_t xpos = (uint32_t)((((uint64_t)xoffset) << FRACBITS) / mip_width);

					int level = (int)lod;
					while (level > 0 && mip_width > 1 && mip_height > 1)
					{
						mipmap_offset += mip_width * mip_height;
						level--;
						mip_width = MAX(mip_width >> 1, 1);
						mip_height = MAX(mip_height >> 1, 1);
					}
					xoffset = (xpos >> FRACBITS) * mip_width;
				}

				const uint32_t *pixels = texture->GetPixelsBgra() + mipmap_offset;

				bool filter_nearest = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
				if (filter_nearest)
				{
					int tx = (xoffset >> FRACBITS) % mip_width;
					if (tx < 0)
						tx += mip_width;
					source = (BYTE*)(pixels + tx * mip_height);
					source2 = nullptr;
					height = mip_height;
					texturefracx = 0;
				}
				else
				{
					xoffset -= FRACUNIT / 2;
					int tx0 = (xoffset >> FRACBITS) % mip_width;
					if (tx0 < 0)
						tx0 += mip_width;
					int tx1 = (tx0 + 1) % mip_width;
					source = (BYTE*)(pixels + tx0 * mip_height);
					source2 = (BYTE*)(pixels + tx1 * mip_height);
					height = mip_height;
					texturefracx = (xoffset >> (FRACBITS - 4)) & 15;
				}
			}
		}
	}

	// Draw a column with support for non-power-of-two ranges
	static void Draw1Column(const FWallCoords &WallC, int x, int y1, int y2, WallSampler &sampler, DrawerFunc draw1column)
	{
		if (r_dynlights && dc_light_list)
		{
			// Find column position in view space
			float w1 = 1.0f / WallC.sz1;
			float w2 = 1.0f / WallC.sz2;
			float t = (x - WallC.sx1 + 0.5f) / (WallC.sx2 - WallC.sx1);
			float wcol = w1 * (1.0f - t) + w2 * t;
			float zcol = 1.0f / wcol;
			dc_viewpos.X = (float)((x + 0.5 - CenterX) / CenterX * zcol);
			dc_viewpos.Y = zcol;
			dc_viewpos.Z = (float)((CenterY - y1 - 0.5) / InvZtoScale * zcol);
			dc_viewpos_step.Z = (float)(-zcol / InvZtoScale);

			static TriLight lightbuffer[64 * 1024];
			static int nextlightindex = 0;

			// Setup lights for column
			dc_num_lights = 0;
			dc_lights = lightbuffer + nextlightindex;
			FLightNode *cur_node = dc_light_list;
			while (cur_node && nextlightindex < 64 * 1024)
			{
				if (!(cur_node->lightsource->flags2&MF2_DORMANT))
				{
					double lightX = cur_node->lightsource->X() - ViewPos.X;
					double lightY = cur_node->lightsource->Y() - ViewPos.Y;
					double lightZ = cur_node->lightsource->Z() - ViewPos.Z;

					float lx = (float)(lightX * ViewSin - lightY * ViewCos) - dc_viewpos.X;
					float ly = (float)(lightX * ViewTanCos + lightY * ViewTanSin) - dc_viewpos.Y;
					float lz = (float)lightZ;

					// Precalculate the constant part of the dot here so the drawer doesn't have to.
					bool is_point_light = (cur_node->lightsource->flags4 & MF4_ATTENUATE) != 0;
					float lconstant = lx * lx + ly * ly;
					float nlconstant = is_point_light ? lx * dc_normal.X + ly * dc_normal.Y : 0.0f;

					// Include light only if it touches this column
					float radius = cur_node->lightsource->GetRadius();
					if (radius * radius >= lconstant && nlconstant >= 0.0f)
					{
						uint32_t red = cur_node->lightsource->GetRed();
						uint32_t green = cur_node->lightsource->GetGreen();
						uint32_t blue = cur_node->lightsource->GetBlue();

						nextlightindex++;
						auto &light = dc_lights[dc_num_lights++];
						light.x = lconstant;
						light.y = nlconstant;
						light.z = lz;
						light.radius = 256.0f / cur_node->lightsource->GetRadius();
						light.color = (red << 16) | (green << 8) | blue;
					}
				}

				cur_node = cur_node->nextLight;
			}

			if (nextlightindex == 64 * 1024)
				nextlightindex = 0;
		}
		else
		{
			dc_num_lights = 0;
		}

		if (r_swtruecolor)
		{
			int count = y2 - y1;

			dc_source = sampler.source;
			dc_source2 = sampler.source2;
			dc_texturefracx = sampler.texturefracx;
			dc_dest = (ylookup[y1] + x) * 4 + dc_destorg;
			dc_count = count;
			dc_iscale = sampler.uv_step;
			dc_texturefrac = sampler.uv_pos;
			dc_textureheight = sampler.height;
			(R_Drawers()->*draw1column)();

			uint64_t step64 = sampler.uv_step;
			uint64_t pos64 = sampler.uv_pos;
			sampler.uv_pos = (uint32_t)(pos64 + step64 * count);
		}
		else
		{
			if (sampler.uv_max == 0 || sampler.uv_step == 0) // power of two
			{
				int count = y2 - y1;

				dc_source = sampler.source;
				dc_source2 = sampler.source2;
				dc_texturefracx = sampler.texturefracx;
				dc_dest = (ylookup[y1] + x) + dc_destorg;
				dc_count = count;
				dc_iscale = sampler.uv_step;
				dc_texturefrac = sampler.uv_pos;
				(R_Drawers()->*draw1column)();

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
					dc_source2 = sampler.source2;
					dc_texturefracx = sampler.texturefracx;
					dc_dest = (ylookup[y1] + x) + dc_destorg;
					dc_count = count;
					dc_iscale = sampler.uv_step;
					dc_texturefrac = uv_pos;
					(R_Drawers()->*draw1column)();

					left -= count;
					uv_pos += sampler.uv_step * count;
					if (uv_pos >= sampler.uv_max)
						uv_pos -= sampler.uv_max;
				}

				sampler.uv_pos = uv_pos;
			}
		}
	}

	static void ProcessWallWorker(
		const FWallCoords &WallC,
		int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep,
		const BYTE *(*getcol)(FTexture *tex, int x), DrawerFunc drawcolumn)
	{
		if (rw_pic->UseType == FTexture::TEX_Null)
			return;

		rw_pic->GetHeight(); // To ensure that rw_pic->HeightBits has been set
		int fracbits = 32 - rw_pic->HeightBits;
		if (fracbits == 32)
		{ // Hack for one pixel tall textures
			fracbits = 0;
			yrepeat = 0;
			dc_texturemid = 0;
		}

		dc_wall_fracbits = r_swtruecolor ? FRACBITS : fracbits;

		bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
		if (fixed)
		{
			dc_wall_colormap[0] = dc_colormap;
			dc_wall_colormap[1] = dc_colormap;
			dc_wall_colormap[2] = dc_colormap;
			dc_wall_colormap[3] = dc_colormap;
			dc_wall_light[0] = 0;
			dc_wall_light[1] = 0;
			dc_wall_light[2] = 0;
			dc_wall_light[3] = 0;
		}

		if (fixedcolormap)
			R_SetColorMapLight(fixedcolormap, 0, 0);
		else
			R_SetColorMapLight(basecolormap, 0, 0);

		float dx = WallC.tright.X - WallC.tleft.X;
		float dy = WallC.tright.Y - WallC.tleft.Y;
		float length = sqrt(dx * dx + dy * dy);
		dc_normal.X = dy / length;
		dc_normal.Y = -dx / length;
		dc_normal.Z = 0.0f;

		double xmagnitude = 1.0;

		for (int x = x1; x < x2; x++, light += lightstep)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 <= y1)
				continue;

			if (!fixed)
				R_SetColorMapLight(basecolormap, light, wallshade);

			if (x + 1 < x2) xmagnitude = fabs(FIXED2DBL(lwal[x + 1]) - FIXED2DBL(lwal[x]));

			WallSampler sampler(y1, swal[x], yrepeat, lwal[x] + xoffset, xmagnitude, rw_pic, getcol);
			Draw1Column(WallC, x, y1, y2, sampler, drawcolumn);
		}

		NetUpdate();
	}

	static void ProcessNormalWall(const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
	{
		ProcessWallWorker(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol, &SWPixelFormatDrawers::DrawWallColumn);
	}

	static void ProcessMaskedWall(const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
	{
		if (!rw_pic->bMasked) // Textures that aren't masked can use the faster ProcessNormalWall.
		{
			ProcessNormalWall(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol);
		}
		else
		{
			ProcessWallWorker(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol, &SWPixelFormatDrawers::DrawWallMaskedColumn);
		}
	}

	static void ProcessTranslucentWall(const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, const BYTE *(*getcol)(FTexture *tex, int x) = R_GetColumn)
	{
		DrawerFunc drawcol1 = R_GetTransMaskDrawer();
		if (drawcol1 == nullptr)
		{
			// The current translucency is unsupported, so draw with regular ProcessMaskedWall instead.
			ProcessMaskedWall(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol);
		}
		else
		{
			ProcessWallWorker(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol, drawcol1);
		}
	}

	static void ProcessStripedWall(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep)
	{
		FDynamicColormap *startcolormap = basecolormap;
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
			int j = R_CreateWallSegmentYSloped(most3, frontsector->e->XFloor.lightlist[i].plane, &WallC, curline, MirrorFlags & RF_XFLIP);
			if (j != 3)
			{
				for (int j = x1; j < x2; ++j)
				{
					down[j] = clamp(most3[j], up[j], dwal[j]);
				}
				ProcessNormalWall(WallC, x1, x2, up, down, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
				up = down;
				down = (down == most1) ? most2 : most1;
			}

			lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
			basecolormap = lit->extra_colormap;
			wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(fogginess, *lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
		}

		ProcessNormalWall(WallC, x1, x2, up, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
		basecolormap = startcolormap;
	}

	static void ProcessWall(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, bool mask)
	{
		if (mask)
		{
			if (colfunc == basecolfunc)
			{
				ProcessMaskedWall(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
			}
			else
			{
				ProcessTranslucentWall(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
			}
		}
		else
		{
			if (fixedcolormap != NULL || fixedlightlev >= 0 || !(frontsector->e && frontsector->e->XFloor.lightlist.Size()))
			{
				ProcessNormalWall(WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
			}
			else
			{
				ProcessStripedWall(frontsector, curline, WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep);
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

	static void ProcessWallNP2(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, double top, double bot, int wallshade, fixed_t xoffset, float light, float lightstep, bool mask)
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
					ProcessWall(frontsector, curline, WallC, x1, x2, up, down, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, mask);
					up = down;
					down = (down == most1) ? most2 : most1;
				}
				partition -= scaledtexheight;
				dc_texturemid -= texheight;
			}
			ProcessWall(frontsector, curline, WallC, x1, x2, up, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, mask);
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
					ProcessWall(frontsector, curline, WallC, x1, x2, up, down, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, mask);
					down = up;
					up = (up == most1) ? most2 : most1;
				}
				partition -= scaledtexheight;
				dc_texturemid -= texheight;
			}
			ProcessWall(frontsector, curline, WallC, x1, x2, uwal, down, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, mask);
		}
	}

	void R_DrawDrawSeg(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, FTexture *pic, drawseg_t *ds, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep)
	{
		rw_pic = pic;
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
			ProcessWallNP2(frontsector, curline, WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, top, bot, wallshade, xoffset, light, lightstep, true);
		}
		else
		{
			ProcessWall(frontsector, curline, WallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, true);
		}
	}


	void R_DrawWallSegment(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, FTexture *pic, int x1, int x2, short *walltop, short *wallbottom, float *swall, fixed_t *lwall, double yscale, double top, double bottom, bool mask, int wallshade, fixed_t xoffset, float light, float lightstep, FLightNode *light_list)
	{
		rw_pic = pic;
		dc_light_list = light_list;
		if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
		{
			ProcessWallNP2(frontsector, curline, WallC, x1, x2, walltop, wallbottom, swall, lwall, yscale, top, bottom, wallshade, xoffset, light, lightstep, false);
		}
		else
		{
			ProcessWall(frontsector, curline, WallC, x1, x2, walltop, wallbottom, swall, lwall, yscale, wallshade, xoffset, light, lightstep, false);
		}
		dc_light_list = nullptr;
	}

	void R_DrawSkySegment(FTexture *pic, int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, const uint8_t *(*getcol)(FTexture *tex, int x))
	{
		rw_pic = pic;
		FWallCoords wallC; // Not used. To do: don't use r_walldraw to draw the sky!!
		ProcessNormalWall(wallC, x1, x2, uwal, dwal, swal, lwal, yrepeat, wallshade, xoffset, light, lightstep, getcol);
	}
}
