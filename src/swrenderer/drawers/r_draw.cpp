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
#include "i_system.h"
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

	class DepthColumnCommand : public DrawerCommand
	{
	public:
		DepthColumnCommand(const WallDrawerArgs &args, float idepth) : idepth(idepth)
		{
			auto rendertarget = args.Viewport()->RenderTarget;
			if (rendertarget->IsBgra())
			{
				uint32_t *destorg = (uint32_t*)rendertarget->GetPixels();
				destorg += viewwindowx + viewwindowy * rendertarget->GetPitch();
				uint32_t *dest = (uint32_t*)args.Dest();
				int offset  = (int)(ptrdiff_t)(dest - destorg);
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

		DepthColumnCommand(const SkyDrawerArgs &args, float idepth) : idepth(idepth)
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
			auto zbuffer = PolyZBuffer::Instance();
			int pitch = PolyStencilBuffer::Instance()->Width();
			float *values = zbuffer->Values() + y * pitch + x;
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

			auto zbuffer = PolyZBuffer::Instance();
			int pitch = PolyStencilBuffer::Instance()->Width();
			float *values = zbuffer->Values() + y * pitch;
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
		Queue->Push<DepthColumnCommand>(args, idepth);
	}

	void SWPixelFormatDrawers::DrawDepthWallColumn(const WallDrawerArgs &args, float idepth)
	{
		Queue->Push<DepthColumnCommand>(args, idepth);
	}

	void SWPixelFormatDrawers::DrawDepthSpan(const SpanDrawerArgs &args, float idepth1, float idepth2)
	{
		Queue->Push<DepthSpanCommand>(args, idepth1, idepth2);
	}
}
