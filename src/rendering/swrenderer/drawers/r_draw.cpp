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
#include <cmath>


#include "doomdef.h"

#include "filesystem.h"
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

CVAR(Bool, r_dynlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, r_fuzzscale, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

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
					table[k] = min<uint8_t>(v, 64);
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
		if (r_fuzzscale)
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
		if (!r_fuzzscale)
		{
			int yl = max(args.FuzzY1(), 1);
			int yh = min(args.FuzzY2(), fuzzviewheight);
			if (yl <= yh)
				fuzzpos = (fuzzpos + yh - yl + 1) % FUZZTABLE;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	void SWPixelFormatDrawers::SetLights(WallColumnDrawerArgs& drawerargs, int x, int y1, const WallDrawerArgs& wallargs)
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

		if(drawerargs.LightList())
		{
			TMap<FDynamicLight *, std::unique_ptr<FLightNode>>::Iterator it(*drawerargs.LightList());
			TMap<FDynamicLight *, std::unique_ptr<FLightNode>>::Pair *pair;
			while (it.NextPair(pair))
			{
				auto cur_node = pair->Value.get();
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
			}
		}
	}

}
