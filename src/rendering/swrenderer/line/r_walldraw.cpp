//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

#include <stdlib.h>
#include <stddef.h>
#include <cmath>

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
#include "a_dynlight.h"
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
	RenderWallPart::RenderWallPart(RenderThread* thread)
	{
		Thread = thread;
	}

	void RenderWallPart::Render(const sector_t* lightsector, seg_t* curline, const FWallCoords& WallC, FSoftwareTexture* pic, int x1, int x2, const short* walltop, const short* wallbottom, const ProjectedWallTexcoords& texcoords, bool mask, bool additive, fixed_t alpha)
	{
		if (pic == nullptr)
			return;

		this->x1 = x1;
		this->x2 = x2;
		this->lightsector = lightsector;
		this->curline = curline;
		this->WallC = WallC;
		this->pic = pic;
		this->mask = mask;
		this->additive = additive;
		this->alpha = alpha;

		light_list = GetLightList();

		mLight.SetColormap(lightsector, curline);
		mLight.SetLightLeft(Thread, WallC);

		Thread->PrepareTexture(pic, DefaultRenderStyle()); // Get correct render style? Shaded won't get here.

		CameraLight* cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap() || cameraLight->FixedLightLevel() >= 0 || !(lightsector->e && lightsector->e->XFloor.lightlist.Size()))
		{
			ProcessNormalWall(walltop, wallbottom, texcoords);
		}
		else
		{
			ProcessStripedWall(walltop, wallbottom, texcoords);
		}
	}

	void RenderWallPart::ProcessStripedWall(const short* uwal, const short* dwal, const ProjectedWallTexcoords& texcoords)
	{
		RenderPortal* renderportal = Thread->Portal.get();

		ProjectedWallLine most1, most2, most3;
		const short* up = uwal;
		short* down = most1.ScreenY;

		for (unsigned int i = 0; i < lightsector->e->XFloor.lightlist.Size(); i++)
		{
			ProjectedWallCull j = most3.Project(Thread->Viewport.get(), lightsector->e->XFloor.lightlist[i].plane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			if (j != ProjectedWallCull::OutsideAbove)
			{
				for (int j = x1; j < x2; ++j)
				{
					down[j] = clamp(most3.ScreenY[j], up[j], dwal[j]);
				}
				ProcessNormalWall(up, down, texcoords);
				up = down;
				down = (down == most1.ScreenY) ? most2.ScreenY : most1.ScreenY;
			}

			mLight.SetColormap(lightsector, curline, &lightsector->e->XFloor.lightlist[i]);
		}

		ProcessNormalWall(up, dwal, texcoords);
	}

	static void DrawWallColumn32(RenderThread* thread, WallDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepX, uint32_t texelStepY, FSoftwareTexture* pic, int texwidth, int texheight)
	{
		double xmagnitude = fabs(static_cast<int32_t>(texelStepX) * (1.0 / 0x1'0000'0000LL));
		double ymagnitude = fabs(static_cast<int32_t>(texelStepY) * (1.0 / 0x1'0000'0000LL));
		double magnitude = MAX(ymagnitude, xmagnitude);
		double min_lod = -1000.0;
		double lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
		bool magnifying = lod < 0.0f;

		int mipmap_offset = 0;
		int mip_width = texwidth;
		int mip_height = texheight;
		if (r_mipmap && pic->Mipmapped() && mip_width > 1 && mip_height > 1)
		{
			int level = (int)lod;
			while (level > 0 && mip_width > 1 && mip_height > 1)
			{
				mipmap_offset += mip_width * mip_height;
				level--;
				mip_width = MAX(mip_width >> 1, 1);
				mip_height = MAX(mip_height >> 1, 1);
			}
		}

		const uint32_t* pixels = pic->GetPixelsBgra() + mipmap_offset;
		fixed_t xxoffset = (texelX >> 16) * mip_width;

		const uint8_t* source;
		const uint8_t* source2;
		uint32_t texturefracx;
		bool filter_nearest = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
		if (filter_nearest)
		{
			int tx = (xxoffset >> FRACBITS) % mip_width;
			source = (uint8_t*)(pixels + tx * mip_height);
			source2 = nullptr;
			texturefracx = 0;
		}
		else
		{
			xxoffset -= FRACUNIT / 2;
			int tx0 = (xxoffset >> FRACBITS) % mip_width;
			if (tx0 < 0)
				tx0 += mip_width;
			int tx1 = (tx0 + 1) % mip_width;
			source = (uint8_t*)(pixels + tx0 * mip_height);
			source2 = (uint8_t*)(pixels + tx1 * mip_height);
			texturefracx = (xxoffset >> (FRACBITS - 4)) & 15;
		}

		int count = y2 - y1;
		drawerargs.SetDest(thread->Viewport.get(), x, y1);
		drawerargs.SetCount(count);
		drawerargs.SetTexture(source, source2, mip_height);
		drawerargs.SetTextureUPos(texturefracx);
		drawerargs.SetTextureVPos(texelY);
		drawerargs.SetTextureVStep(texelStepY);
		drawerargs.DrawColumn(thread);
	}

	static void DrawWallColumn8(RenderThread* thread, WallDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepY, FSoftwareTexture* pic, int texwidth, int texheight, int fracbits, uint32_t uv_max)
	{
		texelY = (static_cast<uint64_t>(texelY)* texheight) >> (32 - fracbits);
		texelStepY = (static_cast<uint64_t>(texelStepY)* texheight) >> (32 - fracbits);

		const uint8_t* pixels = pic->GetColumn(DefaultRenderStyle(), ((texelX >> 16)* texwidth) >> 16, nullptr);

		drawerargs.SetTexture(pixels, nullptr, texheight);
		drawerargs.SetTextureVStep(texelStepY);

		if (uv_max == 0 || texelStepY == 0) // power of two
		{
			int count = y2 - y1;

			drawerargs.SetDest(thread->Viewport.get(), x, y1);
			drawerargs.SetCount(count);
			drawerargs.SetTextureVPos(texelY);
			drawerargs.DrawColumn(thread);
		}
		else
		{
			uint32_t left = y2 - y1;
			int y = y1;
			while (left > 0)
			{
				uint32_t available = uv_max - texelY;
				uint32_t next_uv_wrap = available / texelStepY;
				if (available % texelStepY != 0)
					next_uv_wrap++;
				uint32_t count = MIN(left, next_uv_wrap);

				drawerargs.SetDest(thread->Viewport.get(), x, y);
				drawerargs.SetCount(count);
				drawerargs.SetTextureVPos(texelY);
				drawerargs.DrawColumn(thread);

				y += count;
				left -= count;
				texelY += texelStepY * count;
				if (texelY >= uv_max)
					texelY -= uv_max;
			}
		}
	}

	void RenderWallPart::ProcessNormalWall(const short* uwal, const short* dwal, const ProjectedWallTexcoords& texcoords)
	{
		CameraLight* cameraLight = CameraLight::Instance();
		RenderViewport* viewport = Thread->Viewport.get();

		WallDrawerArgs drawerargs;

		// Textures that aren't masked can use the faster opaque drawer
		if (!pic->GetTexture()->isMasked() && mask && alpha >= OPAQUE && !additive)
		{
			drawerargs.SetStyle(true, false, OPAQUE, mLight.GetBaseColormap());
		}
		else
		{
			drawerargs.SetStyle(mask, additive, alpha, mLight.GetBaseColormap());
		}

		bool fixed = (cameraLight->FixedColormap() || cameraLight->FixedLightLevel() >= 0);

		bool haslights = r_dynlights && light_list;
		if (haslights)
		{
			float dx = WallC.tright.X - WallC.tleft.X;
			float dy = WallC.tright.Y - WallC.tleft.Y;
			float length = sqrt(dx * dx + dy * dy);
			drawerargs.dc_normal.X = dy / length;
			drawerargs.dc_normal.Y = -dx / length;
			drawerargs.dc_normal.Z = 0.0f;
		}

		int texwidth = pic->GetPhysicalWidth();
		int texheight = pic->GetPhysicalHeight();

		int fracbits = 32 - pic->GetHeightBits();
		if (fracbits == 32) fracbits = 0; // One pixel tall textures
		if (viewport->RenderTarget->IsBgra()) fracbits = 32;
		drawerargs.SetTextureFracBits(fracbits);

		uint32_t uv_max;
		int uv_fracbits = 32 - pic->GetHeightBits();
		if (uv_fracbits != 32)
			uv_max = texheight << uv_fracbits;

		float curlight = mLight.GetLightPos(x1);
		float lightstep = mLight.GetLightStep();

		float upos = texcoords.upos, ustepX = texcoords.ustepX, ustepY = texcoords.ustepY;
		float vpos = texcoords.vpos, vstepX = texcoords.vstepX, vstepY = texcoords.vstepY;
		float wpos = texcoords.wpos, wstepX = texcoords.wstepX, wstepY = texcoords.wstepY;
		float startX = texcoords.startX;

		upos += ustepX * (x1 + 0.5f - startX);
		vpos += vstepX * (x1 + 0.5f - startX);
		wpos += wstepX * (x1 + 0.5f - startX);

		float centerY = Thread->Viewport->CenterY;
		centerY -= 0.5f;

		for (int x = x1; x < x2; x++)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 > y1)
			{
				if (!fixed) drawerargs.SetLight(curlight, mLight.GetLightLevel(), mLight.GetFoggy(), viewport);
				if (haslights)
					SetLights(drawerargs, x, y1);
				else
					drawerargs.dc_num_lights = 0;

				float dy = (y1 - centerY);
				float u = upos + ustepY * dy;
				float v = vpos + vstepY * dy;
				float w = wpos + wstepY * dy;
				float scaleU = ustepX;
				float scaleV = vstepY;
				w = 1.0f / w;
				u *= w;
				v *= w;
				scaleU *= w;
				scaleV *= w;

				uint32_t texelX = (uint32_t)(int64_t)((u - std::floor(u)) * 0x1'0000'0000LL);
				uint32_t texelY = (uint32_t)(int64_t)((v - std::floor(v)) * 0x1'0000'0000LL);
				uint32_t texelStepX = (uint32_t)(int64_t)(scaleU * 0x1'0000'0000LL);
				uint32_t texelStepY = (uint32_t)(int64_t)(scaleV * 0x1'0000'0000LL);

				if (fracbits != 32)
					DrawWallColumn8(Thread, drawerargs, x, y1, y2, texelX, texelY, texelStepY, pic, texwidth, texheight, fracbits, uv_max);
				else
					DrawWallColumn32(Thread, drawerargs, x, y1, y2, texelX, texelY, texelStepX, texelStepY, pic, texwidth, texheight);
			}

			upos += ustepX;
			vpos += vstepX;
			wpos += wstepX;
			curlight += lightstep;
		}

		if (r_modelscene)
		{
			for (int x = x1; x < x2; x++)
			{
				int y1 = uwal[x];
				int y2 = dwal[x];
				if (y2 > y1)
				{
					int count = y2 - y1;

					float w1 = 1.0f / WallC.sz1;
					float w2 = 1.0f / WallC.sz2;
					float t = (x - WallC.sx1 + 0.5f) / (WallC.sx2 - WallC.sx1);
					float wcol = w1 * (1.0f - t) + w2 * t;
					float zcol = 1.0f / wcol;
					float zbufferdepth = 1.0f / (zcol / viewport->viewwindow.FocalTangent);

					drawerargs.SetDest(viewport, x, y1);
					drawerargs.SetCount(count);
					drawerargs.DrawDepthColumn(Thread, zbufferdepth);
				}
			}
		}
	}

	void RenderWallPart::SetLights(WallDrawerArgs &drawerargs, int x, int y1)
	{
		bool mirror = !!(Thread->Portal->MirrorFlags & RF_XFLIP);
		int tx = x;
		if (mirror)
			tx = viewwidth - tx - 1;

		RenderViewport *viewport = Thread->Viewport.get();

		// Find column position in view space
		float w1 = 1.0f / WallC.sz1;
		float w2 = 1.0f / WallC.sz2;
		float t = (x - WallC.sx1 + 0.5f) / (WallC.sx2 - WallC.sx1);
		float wcol = w1 * (1.0f - t) + w2 * t;
		float zcol = 1.0f / wcol;

		drawerargs.dc_viewpos.X = (float)((tx + 0.5 - viewport->CenterX) / viewport->CenterX * zcol);
		drawerargs.dc_viewpos.Y = zcol;
		drawerargs.dc_viewpos.Z = (float)((viewport->CenterY - y1 - 0.5) / viewport->InvZtoScale * zcol);
		drawerargs.dc_viewpos_step.Z = (float)(-zcol / viewport->InvZtoScale);

		// Calculate max lights that can touch column so we can allocate memory for the list
		int max_lights = 0;
		FLightNode *cur_node = light_list;
		while (cur_node)
		{
			if (cur_node->lightsource->IsActive())
				max_lights++;
			cur_node = cur_node->nextLight;
		}

		drawerargs.dc_num_lights = 0;
		drawerargs.dc_lights = Thread->FrameMemory->AllocMemory<DrawerLight>(max_lights);

		// Setup lights for column
		cur_node = light_list;
		while (cur_node)
		{
			if (cur_node->lightsource->IsActive())
			{
				double lightX = cur_node->lightsource->X() - viewport->viewpoint.Pos.X;
				double lightY = cur_node->lightsource->Y() - viewport->viewpoint.Pos.Y;
				double lightZ = cur_node->lightsource->Z() - viewport->viewpoint.Pos.Z;

				float lx = (float)(lightX * viewport->viewpoint.Sin - lightY * viewport->viewpoint.Cos) - drawerargs.dc_viewpos.X;
				float ly = (float)(lightX * viewport->viewpoint.TanCos + lightY * viewport->viewpoint.TanSin) - drawerargs.dc_viewpos.Y;
				float lz = (float)lightZ;

				// Precalculate the constant part of the dot here so the drawer doesn't have to.
				bool is_point_light = cur_node->lightsource->IsAttenuated();
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

	FLightNode* RenderWallPart::GetLightList()
	{
		CameraLight* cameraLight = CameraLight::Instance();
		if ((cameraLight->FixedLightLevel() >= 0) || cameraLight->FixedColormap())
			return nullptr; // [SP] Don't draw dynlights if invul/lightamp active
		else if (curline && curline->sidedef)
			return curline->sidedef->lighthead;
		else
			return nullptr;
	}
}
