/*
** r_draw.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2016 Magnus Norddahl
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stddef.h>

#include "templates.h"
#include "doomdef.h"

#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_draw.h"
#include "r_draw_rgba.h"
#include "r_draw_pal.h"
#include "r_thread.h"
#include "swrenderer/scene/r_light.h"
#include "playsim/a_dynlight.h"
#include "polyrenderer/drawers/poly_thread.h"

CVAR(Bool, r_dynlights, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, r_fuzzscale, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

namespace swrenderer
{
	uint8_t shadetables[NUMCOLORMAPS * 16 * 256];
	FDynamicColormap ShadeFakeColormap[16];
	uint8_t identitymap[256];
	FDynamicColormap identitycolormap;
	int fuzzoffset[FUZZTABLE + 1];
	int fuzzpos;
	int fuzzviewheight;

	int fuzz_random_x_offset[FUZZ_RANDOM_X_SIZE] =
	{
		75, 76, 21, 91, 56, 33, 62, 99, 61, 79,
		95, 54, 41, 18, 69, 43, 49, 59, 10, 84,
		94, 17, 57, 46,  9, 39, 55, 34,100, 81,
		73, 88, 92,  3, 63, 36,  7, 28, 13, 80,
		16, 96, 78, 29, 71, 58, 89, 24,  1, 35,
		52, 82,  4, 14, 22, 53, 38, 66, 12, 72,
		90, 44, 77, 83,  6, 27, 48, 30, 42, 32,
		65, 15, 97, 20, 67, 74, 98, 85, 60, 68,
		19, 26,  8, 87, 86, 64, 11, 37, 31, 47,
		25,  5, 50, 51, 23,  2, 93, 70, 40, 45
	};

	uint32_t particle_texture[NUM_PARTICLE_TEXTURES][PARTICLE_TEXTURE_SIZE * PARTICLE_TEXTURE_SIZE];

	short zeroarray[MAXWIDTH] = { 0 };
	short screenheightarray[MAXWIDTH];

	void R_InitShadeMaps()
	{
		int i, j;
		// set up shading tables for shaded columns
		// 16 colormap sets, progressing from full alpha to minimum visible alpha

		uint8_t *table = shadetables;

		// Full alpha
		for (i = 0; i < 16; ++i)
		{
			ShadeFakeColormap[i].Color = ~0u;
			ShadeFakeColormap[i].Desaturate = ~0u;
			ShadeFakeColormap[i].Next = NULL;
			ShadeFakeColormap[i].Maps = table;

			for (j = 0; j < NUMCOLORMAPS; ++j)
			{
				int a = (NUMCOLORMAPS - j) * 256 / NUMCOLORMAPS * (16 - i);
				for (int k = 0; k < 256; ++k)
				{
					uint8_t v = (((k + 2) * a) + 256) >> 14;
					table[k] = MIN<uint8_t>(v, 64);
				}
				table += 256;
			}
		}
		for (i = 0; i < NUMCOLORMAPS * 16 * 256; ++i)
		{
			assert(shadetables[i] <= 64);
		}

		// Set up a guaranteed identity map
		for (i = 0; i < 256; ++i)
		{
			identitymap[i] = i;
		}
		identitycolormap.Maps = identitymap;
	}

	void R_InitFuzzTable(int fuzzoff)
	{
		/*
			FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
			FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
			FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
			FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
		*/

		static const int8_t fuzzinit[FUZZTABLE] = {
			1,-1, 1,-1, 1, 1,-1,
			1, 1,-1, 1, 1, 1,-1,
			1, 1, 1,-1,-1,-1,-1,
			1,-1,-1, 1, 1, 1, 1,-1,
			1,-1, 1, 1,-1,-1, 1,
			1,-1,-1,-1,-1, 1, 1,
			1, 1,-1, 1, 1,-1, 1 
		};

#ifdef ORIGINAL_FUZZ
		for (int i = 0; i < FUZZTABLE; i++)
		{
			fuzzoffset[i] = fuzzinit[i] * fuzzoff;
		}
#else
		int8_t fuzzcount[FUZZTABLE + 1];
		for (int i = 0; i < FUZZTABLE + 1; i++) fuzzcount[i] = 0;
		fuzzcount[0] = 1;
		for (int i = 1; i < FUZZTABLE; i++)
			fuzzcount[i] = fuzzcount[i + fuzzinit[i]] + 1;

		for (int i = 0; i < FUZZTABLE; i++)
		{
			float shade = 1.0f - 6.0f / NUMCOLORMAPS;
			float resultshade = 1.0;
			for (int j = 0; j < fuzzcount[i]; j++)
				resultshade *= shade;
			fuzzoffset[i] = clamp((int)((1.0f - resultshade) * NUMCOLORMAPS + 0.5f), 0, NUMCOLORMAPS - 1);
		}
#endif
	}

	void R_InitParticleTexture()
	{
		static_assert(NUM_PARTICLE_TEXTURES == 3, "R_InitParticleTexture must be updated if NUM_PARTICLE_TEXTURES is changed");

		double center = PARTICLE_TEXTURE_SIZE * 0.5f;
		for (int y = 0; y < PARTICLE_TEXTURE_SIZE; y++)
		{
			for (int x = 0; x < PARTICLE_TEXTURE_SIZE; x++)
			{
				double dx = (center - x - 0.5f) / center;
				double dy = (center - y - 0.5f) / center;
				double dist2 = dx * dx + dy * dy;
				double round_alpha = clamp<double>(1.7f - dist2 * 1.7f, 0.0f, 1.0f);
				double smooth_alpha = clamp<double>(1.1f - dist2 * 1.1f, 0.0f, 1.0f);

				particle_texture[0][x + y * PARTICLE_TEXTURE_SIZE] = 128;
				particle_texture[1][x + y * PARTICLE_TEXTURE_SIZE] = (int)(round_alpha * 128.0f + 0.5f);
				particle_texture[2][x + y * PARTICLE_TEXTURE_SIZE] = (int)(smooth_alpha * 128.0f + 0.5f);
			}
		}
	}

	void R_UpdateFuzzPosFrameStart()
	{
		if (r_fuzzscale || V_IsPolyRenderer())
		{
			static int next_random = 0;

			fuzzpos = (fuzzpos + fuzz_random_x_offset[next_random] * FUZZTABLE / 100) % FUZZTABLE;

			next_random++;
			if (next_random == FUZZ_RANDOM_X_SIZE)
				next_random = 0;
		}
	}

	void R_UpdateFuzzPos(const SpriteDrawerArgs &args)
	{
		if (!r_fuzzscale && !V_IsPolyRenderer())
		{
			int yl = MAX(args.FuzzY1(), 1);
			int yh = MIN(args.FuzzY2(), fuzzviewheight);
			if (yl <= yh)
				fuzzpos = (fuzzpos + yh - yl + 1) % FUZZTABLE;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	DrawWallCommand::DrawWallCommand(const WallDrawerArgs& args) : wallargs(args)
	{
	}

	void DrawWallCommand::Execute(DrawerThread* thread)
	{
		if (!thread->columndrawer)
			thread->columndrawer = std::make_shared<WallColumnDrawerArgs>();

		WallColumnDrawerArgs& drawerargs = *thread->columndrawer.get();
		drawerargs.wallargs = &wallargs;

		bool haslights = r_dynlights && wallargs.lightlist;
		if (haslights)
		{
			float dx = wallargs.WallC.tright.X - wallargs.WallC.tleft.X;
			float dy = wallargs.WallC.tright.Y - wallargs.WallC.tleft.Y;
			float length = sqrt(dx * dx + dy * dy);
			drawerargs.dc_normal.X = dy / length;
			drawerargs.dc_normal.Y = -dx / length;
			drawerargs.dc_normal.Z = 0.0f;
		}

		drawerargs.SetTextureFracBits(wallargs.fracbits);

		float curlight = wallargs.lightpos;
		float lightstep = wallargs.lightstep;
		int shade = wallargs.Shade();

		if (wallargs.fixedlight)
		{
			curlight = wallargs.FixedLight();
			lightstep = 0;
		}

		float upos = wallargs.texcoords.upos, ustepX = wallargs.texcoords.ustepX, ustepY = wallargs.texcoords.ustepY;
		float vpos = wallargs.texcoords.vpos, vstepX = wallargs.texcoords.vstepX, vstepY = wallargs.texcoords.vstepY;
		float wpos = wallargs.texcoords.wpos, wstepX = wallargs.texcoords.wstepX, wstepY = wallargs.texcoords.wstepY;
		float startX = wallargs.texcoords.startX;

		int x1 = wallargs.x1;
		int x2 = wallargs.x2;

		upos += ustepX * (x1 + 0.5f - startX);
		vpos += vstepX * (x1 + 0.5f - startX);
		wpos += wstepX * (x1 + 0.5f - startX);

		float centerY = wallargs.CenterY;
		centerY -= 0.5f;

		auto uwal = wallargs.uwal;
		auto dwal = wallargs.dwal;
		for (int x = x1; x < x2; x++)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 > y1)
			{
				drawerargs.SetLight(curlight, shade);
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

				if (wallargs.fracbits != 32)
					DrawWallColumn8(thread, drawerargs, x, y1, y2, texelX, texelY, texelStepY);
				else
					DrawWallColumn32(thread, drawerargs, x, y1, y2, texelX, texelY, texelStepX, texelStepY);
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

					float w1 = 1.0f / wallargs.WallC.sz1;
					float w2 = 1.0f / wallargs.WallC.sz2;
					float t = (x - wallargs.WallC.sx1 + 0.5f) / (wallargs.WallC.sx2 - wallargs.WallC.sx1);
					float wcol = w1 * (1.0f - t) + w2 * t;
					float zcol = 1.0f / wcol;
					float zbufferdepth = 1.0f / (zcol / wallargs.FocalTangent);

					drawerargs.SetDest(x, y1);
					drawerargs.SetCount(count);
					DrawDepthColumn(thread, drawerargs, zbufferdepth);
				}
			}
		}
	}

	void DrawWallCommand::DrawWallColumn32(DrawerThread* thread, WallColumnDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepX, uint32_t texelStepY)
	{
		int texwidth = wallargs.texwidth;
		int texheight = wallargs.texheight;

		double xmagnitude = fabs(static_cast<int32_t>(texelStepX)* (1.0 / 0x1'0000'0000LL));
		double ymagnitude = fabs(static_cast<int32_t>(texelStepY)* (1.0 / 0x1'0000'0000LL));
		double magnitude = MAX(ymagnitude, xmagnitude);
		double min_lod = -1000.0;
		double lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
		bool magnifying = lod < 0.0f;

		int mipmap_offset = 0;
		int mip_width = texwidth;
		int mip_height = texheight;
		if (wallargs.mipmapped && mip_width > 1 && mip_height > 1)
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

		const uint32_t* pixels = static_cast<const uint32_t*>(wallargs.texpixels) + mipmap_offset;
		fixed_t xxoffset = (texelX >> 16)* mip_width;

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
		drawerargs.SetDest(x, y1);
		drawerargs.SetCount(count);
		drawerargs.SetTexture(source, source2, mip_height);
		drawerargs.SetTextureUPos(texturefracx);
		drawerargs.SetTextureVPos(texelY);
		drawerargs.SetTextureVStep(texelStepY);
		DrawColumn(thread, drawerargs);
	}

	void DrawWallCommand::DrawWallColumn8(DrawerThread* thread, WallColumnDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepY)
	{
		int texwidth = wallargs.texwidth;
		int texheight = wallargs.texheight;
		int fracbits = wallargs.fracbits;
		uint32_t uv_max = texheight << fracbits;

		const uint8_t* pixels = static_cast<const uint8_t*>(wallargs.texpixels) + (((texelX >> 16)* texwidth) >> 16)* texheight;

		texelY = (static_cast<uint64_t>(texelY)* texheight) >> (32 - fracbits);
		texelStepY = (static_cast<uint64_t>(texelStepY)* texheight) >> (32 - fracbits);

		drawerargs.SetTexture(pixels, nullptr, texheight);
		drawerargs.SetTextureVStep(texelStepY);

		if (uv_max == 0 || texelStepY == 0) // power of two
		{
			int count = y2 - y1;

			drawerargs.SetDest(x, y1);
			drawerargs.SetCount(count);
			drawerargs.SetTextureVPos(texelY);
			DrawColumn(thread, drawerargs);
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

				drawerargs.SetDest(x, y);
				drawerargs.SetCount(count);
				drawerargs.SetTextureVPos(texelY);
				DrawColumn(thread, drawerargs);

				y += count;
				left -= count;
				texelY += texelStepY * count;
				if (texelY >= uv_max)
					texelY -= uv_max;
			}
		}
	}

	void DrawWallCommand::DrawDepthColumn(DrawerThread* thread, const WallColumnDrawerArgs& args, float idepth)
	{
		int x, y, count;

		auto rendertarget = args.Viewport()->RenderTarget;
		if (rendertarget->IsBgra())
		{
			uint32_t* destorg = (uint32_t*)rendertarget->GetPixels();
			destorg += viewwindowx + viewwindowy * rendertarget->GetPitch();
			uint32_t* dest = (uint32_t*)args.Dest();
			int offset = (int)(ptrdiff_t)(dest - destorg);
			x = offset % rendertarget->GetPitch();
			y = offset / rendertarget->GetPitch();
		}
		else
		{
			uint8_t* destorg = rendertarget->GetPixels();
			destorg += viewwindowx + viewwindowy * rendertarget->GetPitch();
			uint8_t* dest = (uint8_t*)args.Dest();
			int offset = (int)(ptrdiff_t)(dest - destorg);
			x = offset % rendertarget->GetPitch();
			y = offset / rendertarget->GetPitch();
		}
		count = args.Count();

		auto zbuffer = PolyTriangleThreadData::Get(thread)->depthstencil;
		int pitch = zbuffer->Width();
		float* values = zbuffer->DepthValues() + y * pitch + x;
		int cnt = count;

		values = thread->dest_for_thread(y, pitch, values);
		cnt = thread->count_for_thread(y, cnt);
		pitch *= thread->num_cores;

		float depth = idepth;
		for (int i = 0; i < cnt; i++)
		{
			*values = depth;
			values += pitch;
		}
	}

	void DrawWallCommand::SetLights(WallColumnDrawerArgs& drawerargs, int x, int y1)
	{
		bool mirror = !!(wallargs.PortalMirrorFlags & RF_XFLIP);
		int tx = x;
		if (mirror)
			tx = viewwidth - tx - 1;

		// Find column position in view space
		float w1 = 1.0f / wallargs.WallC.sz1;
		float w2 = 1.0f / wallargs.WallC.sz2;
		float t = (x - wallargs.WallC.sx1 + 0.5f) / (wallargs.WallC.sx2 - wallargs.WallC.sx1);
		float wcol = w1 * (1.0f - t) + w2 * t;
		float zcol = 1.0f / wcol;

		drawerargs.dc_viewpos.X = (float)((tx + 0.5 - wallargs.CenterX) / wallargs.CenterX * zcol);
		drawerargs.dc_viewpos.Y = zcol;
		drawerargs.dc_viewpos.Z = (float)((wallargs.CenterY - y1 - 0.5) / wallargs.InvZtoScale * zcol);
		drawerargs.dc_viewpos_step.Z = (float)(-zcol / wallargs.InvZtoScale);

		drawerargs.dc_num_lights = 0;

		// Setup lights for column
		FLightNode* cur_node = drawerargs.LightList();
		while (cur_node)
		{
			if (cur_node->lightsource->IsActive())
			{
				double lightX = cur_node->lightsource->X() - wallargs.ViewpointPos.X;
				double lightY = cur_node->lightsource->Y() - wallargs.ViewpointPos.Y;
				double lightZ = cur_node->lightsource->Z() - wallargs.ViewpointPos.Z;

				float lx = (float)(lightX * wallargs.Sin - lightY * wallargs.Cos) - drawerargs.dc_viewpos.X;
				float ly = (float)(lightX * wallargs.TanCos + lightY * wallargs.TanSin) - drawerargs.dc_viewpos.Y;
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

					auto& light = drawerargs.dc_lights[drawerargs.dc_num_lights++];
					light.x = lconstant;
					light.y = nlconstant;
					light.z = lz;
					light.radius = 256.0f / cur_node->lightsource->GetRadius();
					light.color = (red << 16) | (green << 8) | blue;

					if (drawerargs.dc_num_lights == WallColumnDrawerArgs::MAX_DRAWER_LIGHTS)
						break;
				}
			}

			cur_node = cur_node->nextLight;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	class DepthSkyColumnCommand : public DrawerCommand
	{
	public:
		DepthSkyColumnCommand(const SkyDrawerArgs &args, float idepth) : idepth(idepth)
		{
			auto rendertarget = args.Viewport()->RenderTarget;
			if (rendertarget->IsBgra())
			{
				uint32_t *destorg = (uint32_t*)rendertarget->GetPixels();
				destorg += viewwindowx + viewwindowy * rendertarget->GetPitch();
				uint32_t *dest = (uint32_t*)args.Dest();
				int offset = (int)(ptrdiff_t)(dest - destorg);
				x = offset % rendertarget->GetPitch();
				y = offset / rendertarget->GetPitch();
			}
			else
			{
				uint8_t *destorg = rendertarget->GetPixels();
				destorg += viewwindowx + viewwindowy * rendertarget->GetPitch();
				uint8_t *dest = (uint8_t*)args.Dest();
				int offset = (int)(ptrdiff_t)(dest - destorg);
				x = offset % rendertarget->GetPitch();
				y = offset / rendertarget->GetPitch();
			}
			count = args.Count();
		}

		void Execute(DrawerThread *thread) override
		{
			auto zbuffer = PolyTriangleThreadData::Get(thread)->depthstencil;
			int pitch = zbuffer->Width();
			float *values = zbuffer->DepthValues() + y * pitch + x;
			int cnt = count;

			values = thread->dest_for_thread(y, pitch, values);
			cnt = thread->count_for_thread(y, cnt);
			pitch *= thread->num_cores;

			float depth = idepth;
			for (int i = 0; i < cnt; i++)
			{
				*values = depth;
				values += pitch;
			}
		}

	private:
		int x, y, count;
		float idepth;
	};

	// #define DEPTH_DEBUG

	class DepthSpanCommand : public DrawerCommand
	{
	public:
		DepthSpanCommand(const SpanDrawerArgs &args, float idepth1, float idepth2) : idepth1(idepth1), idepth2(idepth2)
		{
			y = args.DestY();
			x1 = args.DestX1();
			x2 = args.DestX2();
			#ifdef DEPTH_DEBUG
			dest = (uint32_t*)args.Viewport()->GetDest(0, args.DestY());
			#endif
		}

		void Execute(DrawerThread *thread) override
		{
			if (thread->skipped_by_thread(y))
				return;

			auto zbuffer = PolyTriangleThreadData::Get(thread)->depthstencil;
			int pitch = zbuffer->Width();
			float *values = zbuffer->DepthValues() + y * pitch;
			int end = x2;

			if (idepth1 == idepth2)
			{
				float depth = idepth1;
				#ifdef DEPTH_DEBUG
				uint32_t gray = clamp<int32_t>((int32_t)(1.0f / depth / 4.0f), 0, 255);
				uint32_t color = MAKEARGB(255, gray, gray, gray);
				#endif
				for (int x = x1; x <= end; x++)
				{
					values[x] = depth;
					#ifdef DEPTH_DEBUG
					dest[x] = color;
					#endif
				}
			}
			else
			{
				float depth = idepth1;
				float step = (idepth2 - idepth1) / (x2 - x1 + 1);
				for (int x = x1; x <= end; x++)
				{
					#ifdef DEPTH_DEBUG
					uint32_t gray = clamp<int32_t>((int32_t)(1.0f / depth / 4.0f), 0, 255);
					uint32_t color = MAKEARGB(255, gray, gray, gray);
					dest[x] = color;
					#endif

					values[x] = depth;
					depth += step;
				}
			}
		}

	private:
		int y, x1, x2;
		float idepth1, idepth2;
		#ifdef DEPTH_DEBUG
		uint32_t *dest;
		#endif
	};

	void SWPixelFormatDrawers::DrawDepthSkyColumn(const SkyDrawerArgs &args, float idepth)
	{
		Queue->Push<DepthSkyColumnCommand>(args, idepth);
	}

	void SWPixelFormatDrawers::DrawDepthSpan(const SpanDrawerArgs &args, float idepth1, float idepth2)
	{
		Queue->Push<DepthSpanCommand>(args, idepth1, idepth2);
	}
}
