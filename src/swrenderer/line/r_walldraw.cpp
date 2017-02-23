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

#include <stdlib.h>
#include <stddef.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"

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
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/r_renderthread.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	WallSampler::WallSampler(int y1, double texturemid, float swal, double yrepeat, fixed_t xoffset, double xmagnitude, FTexture *texture)
	{
		auto viewport = RenderViewport::Instance();
		
		xoffset += FLOAT2FIXED(xmagnitude * 0.5);

		if (!viewport->RenderTarget->IsBgra())
		{
			height = texture->GetHeight();

			int uv_fracbits = 32 - texture->HeightBits;
			if (uv_fracbits != 32)
			{
				uv_max = height << uv_fracbits;

				// Find start uv in [0-base_height[ range.
				// Not using xs_ToFixed because it rounds the result and we need something that always rounds down to stay within the range.
				double uv_stepd = swal * yrepeat;
				double v = (texturemid + uv_stepd * (y1 - viewport->CenterY + 0.5)) / height;
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

			int col = xoffset >> FRACBITS;

			// If the texture's width isn't a power of 2, then we need to make it a
			// positive offset for proper clamping.
			int width;
			if (col < 0 && (width = texture->GetWidth()) != (1 << texture->WidthBits))
			{
				col = width + (col % width);
			}

			if (viewport->RenderTarget->IsBgra())
				source = (const uint8_t *)texture->GetColumnBgra(col, nullptr);
			else
				source = texture->GetColumn(col, nullptr);

			source2 = nullptr;
			texturefracx = 0;
		}
		else
		{
			// Normalize to 0-1 range:
			double uv_stepd = swal * yrepeat;
			double v = (texturemid + uv_stepd * (y1 - viewport->CenterY + 0.5)) / texture->GetHeight();
			v = v - floor(v);
			double v_step = uv_stepd / texture->GetHeight();

			if (std::isnan(v) || std::isnan(v_step)) // this should never happen, but it apparently does..
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

	// Draw a column with support for non-power-of-two ranges
	void RenderWallPart::Draw1Column(int x, int y1, int y2, WallSampler &sampler)
	{
		if (r_dynlights && light_list)
		{
			auto viewport = RenderViewport::Instance();
			
			// Find column position in view space
			float w1 = 1.0f / WallC.sz1;
			float w2 = 1.0f / WallC.sz2;
			float t = (x - WallC.sx1 + 0.5f) / (WallC.sx2 - WallC.sx1);
			float wcol = w1 * (1.0f - t) + w2 * t;
			float zcol = 1.0f / wcol;
			drawerargs.dc_viewpos.X = (float)((x + 0.5 - viewport->CenterX) / viewport->CenterX * zcol);
			drawerargs.dc_viewpos.Y = zcol;
			drawerargs.dc_viewpos.Z = (float)((viewport->CenterY - y1 - 0.5) / viewport->InvZtoScale * zcol);
			drawerargs.dc_viewpos_step.Z = (float)(-zcol / viewport->InvZtoScale);

			// Calculate max lights that can touch column so we can allocate memory for the list
			int max_lights = 0;
			FLightNode *cur_node = light_list;
			while (cur_node)
			{
				if (!(cur_node->lightsource->flags2&MF2_DORMANT))
					max_lights++;
				cur_node = cur_node->nextLight;
			}

			drawerargs.dc_num_lights = 0;
			drawerargs.dc_lights = Thread->FrameMemory->AllocMemory<DrawerLight>(max_lights);

			// Setup lights for column
			cur_node = light_list;
			while (cur_node)
			{
				if (!(cur_node->lightsource->flags2&MF2_DORMANT))
				{
					double lightX = cur_node->lightsource->X() - ViewPos.X;
					double lightY = cur_node->lightsource->Y() - ViewPos.Y;
					double lightZ = cur_node->lightsource->Z() - ViewPos.Z;

					float lx = (float)(lightX * ViewSin - lightY * ViewCos) - drawerargs.dc_viewpos.X;
					float ly = (float)(lightX * ViewTanCos + lightY * ViewTanSin) - drawerargs.dc_viewpos.Y;
					float lz = (float)lightZ;

					// Precalculate the constant part of the dot here so the drawer doesn't have to.
					bool is_point_light = (cur_node->lightsource->flags4 & MF4_ATTENUATE) != 0;
					float lconstant = lx * lx + ly * ly;
					float nlconstant = is_point_light ? lx * drawerargs.dc_normal.X + ly * drawerargs.dc_normal.Y : 0.0f;

					// Include light only if it touches this column
					float radius = cur_node->lightsource->GetRadius();
					if (radius * radius >= lconstant && nlconstant >= 0.0f)
					{
						uint32_t red = cur_node->lightsource->GetRed();
						uint32_t green = cur_node->lightsource->GetGreen();
						uint32_t blue = cur_node->lightsource->GetBlue();

						auto &light = drawerargs.dc_lights[drawerargs.dc_num_lights++];
						light.x = lconstant;
						light.y = nlconstant;
						light.z = lz;
						light.radius = 256.0f / cur_node->lightsource->GetRadius();
						light.color = (red << 16) | (green << 8) | blue;
					}
				}

				cur_node = cur_node->nextLight;
			}
		}
		else
		{
			drawerargs.dc_num_lights = 0;
		}

		if (RenderViewport::Instance()->RenderTarget->IsBgra())
		{
			int count = y2 - y1;

			drawerargs.SetTexture(sampler.source, sampler.source2, sampler.height);
			drawerargs.SetTextureUPos(sampler.texturefracx);
			drawerargs.SetDest(x, y1);
			drawerargs.SetCount(count);
			drawerargs.SetTextureVStep(sampler.uv_step);
			drawerargs.SetTextureVPos(sampler.uv_pos);
			drawerargs.DrawColumn(Thread);

			uint64_t step64 = sampler.uv_step;
			uint64_t pos64 = sampler.uv_pos;
			sampler.uv_pos = (uint32_t)(pos64 + step64 * count);
		}
		else
		{
			if (sampler.uv_max == 0 || sampler.uv_step == 0) // power of two
			{
				int count = y2 - y1;

				drawerargs.SetTexture(sampler.source, sampler.source2, sampler.height);
				drawerargs.SetTextureUPos(sampler.texturefracx);
				drawerargs.SetDest(x, y1);
				drawerargs.SetCount(count);
				drawerargs.SetTextureVStep(sampler.uv_step);
				drawerargs.SetTextureVPos(sampler.uv_pos);
				drawerargs.DrawColumn(Thread);

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

					drawerargs.SetTexture(sampler.source, sampler.source2, sampler.height);
					drawerargs.SetTextureUPos(sampler.texturefracx);
					drawerargs.SetDest(x, y1);
					drawerargs.SetCount(count);
					drawerargs.SetTextureVStep(sampler.uv_step);
					drawerargs.SetTextureVPos(uv_pos);
					drawerargs.DrawColumn(Thread);

					left -= count;
					uv_pos += sampler.uv_step * count;
					if (uv_pos >= sampler.uv_max)
						uv_pos -= sampler.uv_max;
				}

				sampler.uv_pos = uv_pos;
			}
		}
	}

	void RenderWallPart::ProcessWallWorker(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal)
	{
		if (rw_pic->UseType == FTexture::TEX_Null)
			return;

		rw_pic->GetHeight(); // To ensure that rw_pic->HeightBits has been set
		int fracbits = 32 - rw_pic->HeightBits;
		if (fracbits == 32)
		{ // Hack for one pixel tall textures
			fracbits = 0;
			yrepeat = 0;
			texturemid = 0;
		}

		drawerargs.SetTextureFracBits(RenderViewport::Instance()->RenderTarget->IsBgra() ? FRACBITS : fracbits);

		CameraLight *cameraLight = CameraLight::Instance();
		bool fixed = (cameraLight->FixedColormap() != NULL || cameraLight->FixedLightLevel() >= 0);

		if (cameraLight->FixedColormap())
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
		else
			drawerargs.SetLight(basecolormap, 0, 0);

		float dx = WallC.tright.X - WallC.tleft.X;
		float dy = WallC.tright.Y - WallC.tleft.Y;
		float length = sqrt(dx * dx + dy * dy);
		drawerargs.dc_normal.X = dy / length;
		drawerargs.dc_normal.Y = -dx / length;
		drawerargs.dc_normal.Z = 0.0f;

		double xmagnitude = 1.0;

		float curlight = light;
		for (int x = x1; x < x2; x++, curlight += lightstep)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 <= y1)
				continue;

			if (!fixed)
				drawerargs.SetLight(basecolormap, curlight, wallshade);

			if (x + 1 < x2) xmagnitude = fabs(FIXED2DBL(lwal[x + 1]) - FIXED2DBL(lwal[x]));

			WallSampler sampler(y1, texturemid, swal[x], yrepeat, lwal[x] + xoffset, xmagnitude, rw_pic);
			Draw1Column(x, y1, y2, sampler);
		}

		if (Thread->MainThread)
			NetUpdate();
	}

	void RenderWallPart::ProcessNormalWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal)
	{
		ProcessWallWorker(uwal, dwal, texturemid, swal, lwal);
	}

	void RenderWallPart::ProcessStripedWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal)
	{
		ProjectedWallLine most1, most2, most3;
		const short *up;
		short *down;

		up = uwal;
		down = most1.ScreenY;

		assert(WallC.sx1 <= x1);
		assert(WallC.sx2 >= x2);
		
		RenderPortal *renderportal = Thread->Portal.get();

		// kg3D - fake floors instead of zdoom light list
		for (unsigned int i = 0; i < frontsector->e->XFloor.lightlist.Size(); i++)
		{
			ProjectedWallCull j = most3.Project(frontsector->e->XFloor.lightlist[i].plane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			if (j != ProjectedWallCull::OutsideAbove)
			{
				for (int j = x1; j < x2; ++j)
				{
					down[j] = clamp(most3.ScreenY[j], up[j], dwal[j]);
				}
				ProcessNormalWall(up, down, texturemid, swal, lwal);
				up = down;
				down = (down == most1.ScreenY) ? most2.ScreenY : most1.ScreenY;
			}

			lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
			basecolormap = lit->extra_colormap;
			wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != NULL) + R_ActualExtraLight(foggy));
		}

		ProcessNormalWall(up, dwal, texturemid, swal, lwal);
	}

	void RenderWallPart::ProcessWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal)
	{
		// Textures that aren't masked can use the faster ProcessNormalWall.
		if (!rw_pic->bMasked && drawerargs.IsMaskedDrawer())
		{
			drawerargs.SetStyle(true, false, OPAQUE);
		}

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap() != NULL || cameraLight->FixedLightLevel() >= 0 || !(frontsector->e && frontsector->e->XFloor.lightlist.Size()))
		{
			ProcessNormalWall(uwal, dwal, texturemid, swal, lwal);
		}
		else
		{
			ProcessStripedWall(uwal, dwal, texturemid, swal, lwal);
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

	void RenderWallPart::ProcessWallNP2(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal, double top, double bot)
	{
		ProjectedWallLine most1, most2, most3;
		double texheight = rw_pic->GetHeight();
		double partition;
		double scaledtexheight = texheight / yrepeat;

		if (yrepeat >= 0)
		{ // normal orientation: draw strips from top to bottom
			partition = top - fmod(top - texturemid / yrepeat - ViewPos.Z, scaledtexheight);
			if (partition == top)
			{
				partition -= scaledtexheight;
			}
			const short *up = uwal;
			short *down = most1.ScreenY;
			texturemid = (partition - ViewPos.Z) * yrepeat + texheight;
			while (partition > bot)
			{
				ProjectedWallCull j = most3.Project(partition - ViewPos.Z, &WallC);
				if (j != ProjectedWallCull::OutsideAbove)
				{
					for (int j = x1; j < x2; ++j)
					{
						down[j] = clamp(most3.ScreenY[j], up[j], dwal[j]);
					}
					ProcessWall(up, down, texturemid, swal, lwal);
					up = down;
					down = (down == most1.ScreenY) ? most2.ScreenY : most1.ScreenY;
				}
				partition -= scaledtexheight;
				texturemid -= texheight;
			}
			ProcessWall(up, dwal, texturemid, swal, lwal);
		}
		else
		{ // upside down: draw strips from bottom to top
			partition = bot - fmod(bot - texturemid / yrepeat - ViewPos.Z, scaledtexheight);
			short *up = most1.ScreenY;
			const short *down = dwal;
			texturemid = (partition - ViewPos.Z) * yrepeat + texheight;
			while (partition < top)
			{
				ProjectedWallCull j = most3.Project(partition - ViewPos.Z, &WallC);
				if (j != ProjectedWallCull::OutsideBelow)
				{
					for (int j = x1; j < x2; ++j)
					{
						up[j] = clamp(most3.ScreenY[j], uwal[j], down[j]);
					}
					ProcessWall(up, down, texturemid, swal, lwal);
					down = up;
					up = (up == most1.ScreenY) ? most2.ScreenY : most1.ScreenY;
				}
				partition -= scaledtexheight;
				texturemid -= texheight;
			}
			ProcessWall(uwal, down, texturemid, swal, lwal);
		}
	}

	void RenderWallPart::Render(const WallDrawerArgs &drawerargs, sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, FTexture *pic, int x1, int x2, const short *walltop, const short *wallbottom, double texturemid, float *swall, fixed_t *lwall, double yscale, double top, double bottom, bool mask, int wallshade, fixed_t xoffset, float light, float lightstep, FLightNode *light_list, bool foggy, FDynamicColormap *basecolormap)
	{
		this->drawerargs = drawerargs;
		this->x1 = x1;
		this->x2 = x2;
		this->frontsector = frontsector;
		this->curline = curline;
		this->WallC = WallC;
		this->yrepeat = yscale;
		this->wallshade = wallshade;
		this->xoffset = xoffset;
		this->light = light;
		this->lightstep = lightstep;
		this->foggy = foggy;
		this->basecolormap = basecolormap;
		this->light_list = light_list;
		this->rw_pic = pic;
		this->mask = mask;

		if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
		{
			ProcessWallNP2(walltop, wallbottom, texturemid, swall, lwall, top, bottom);
		}
		else
		{
			ProcessWall(walltop, wallbottom, texturemid, swall, lwall);
		}
	}

	RenderWallPart::RenderWallPart(RenderThread *thread)
	{
		Thread = thread;
	}
}
