/*
** r_draw_pal.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2016 Magnus Norddahl
** Copyright 2016 Rachael Alexanderson
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

#ifndef NO_SSE
#include <xmmintrin.h>
#endif
#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "v_video.h"
#include "r_draw_pal.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/scene/r_light.h"

// [SP] r_blendmethod - false = rgb555 matching (ZDoom classic), true = rgb666 (refactored)
CVAR(Bool, r_blendmethod, false, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)
EXTERN_CVAR(Int, gl_particles_style)

/*
	[RH] This translucency algorithm is based on DOSDoom 0.65's, but uses
	a 32k RGB table instead of an 8k one. At least on my machine, it's
	slightly faster (probably because it uses only one shift instead of
	two), and it looks considerably less green at the ends of the
	translucency range. The extra size doesn't appear to be an issue.

	The following note is from DOSDoom 0.65:

	New translucency algorithm, by Erik Sandberg:

	Basically, we compute the red, green and blue values for each pixel, and
	then use a RGB table to check which one of the palette colours that best
	represents those RGB values. The RGB table is 8k big, with 4 R-bits,
	5 G-bits and 4 B-bits. A 4k table gives a bit too bad precision, and a 32k
	table takes up more memory and results in more cache misses, so an 8k
	table seemed to be quite ultimate.

	The computation of the RGB for each pixel is accelerated by using two
	1k tables for each translucency level.
	The xth element of one of these tables contains the r, g and b values for
	the colour x, weighted for the current translucency level (for example,
	the weighted rgb values for background colour at 75% translucency are 1/4
	of the original rgb values). The rgb values are stored as three
	low-precision fixed point values, packed into one long per colour:
	Bit 0-4:   Frac part of blue  (5 bits)
	Bit 5-8:   Int  part of blue  (4 bits)
	Bit 9-13:  Frac part of red   (5 bits)
	Bit 14-17: Int  part of red   (4 bits)
	Bit 18-22: Frac part of green (5 bits)
	Bit 23-27: Int  part of green (5 bits)
	Bit 28-31: All zeros          (4 bits)

	The point of this format is that the two colours now can be added, and
	then be converted to a RGB table index very easily: First, we just set
	all the frac bits and the four upper zero bits to 1. It's now possible
	to get the RGB table index by anding the current value >> 5 with the
	current value >> 19. When asm-optimised, this should be the fastest
	algorithm that uses RGB tables.
*/

namespace swrenderer
{
	PalWall1Command::PalWall1Command(const WallDrawerArgs &args) : args(args)
	{
	}

	uint8_t PalWall1Command::AddLights(const DrawerLight *lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material)
	{
		uint32_t lit_r = 0;
		uint32_t lit_g = 0;
		uint32_t lit_b = 0;

		for (int i = 0; i < num_lights; i++)
		{
			uint32_t light_color_r = RPART(lights[i].color);
			uint32_t light_color_g = GPART(lights[i].color);
			uint32_t light_color_b = BPART(lights[i].color);

			// L = light-pos
			// dist = sqrt(dot(L, L))
			// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
			float Lxy2 = lights[i].x; // L.x*L.x + L.y*L.y
			float Lz = lights[i].z - viewpos_z;
			float dist2 = Lxy2 + Lz * Lz;
#ifdef NO_SSE
			float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
			float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
#endif
			float dist = dist2 * rcp_dist;
			float distance_attenuation = (256.0f - MIN(dist * lights[i].radius, 256.0f));

			// The simple light type
			float simple_attenuation = distance_attenuation;

			// The point light type
			// diffuse = dot(N,L) * attenuation
			float point_attenuation = lights[i].y * rcp_dist * distance_attenuation;
			uint32_t attenuation = (uint32_t)(lights[i].y == 0.0f ? simple_attenuation : point_attenuation);

			lit_r += (light_color_r * attenuation) >> 8;
			lit_g += (light_color_g * attenuation) >> 8;
			lit_b += (light_color_b * attenuation) >> 8;
		}

		if (lit_r == 0 && lit_g == 0 && lit_b == 0)
			return fg;

		uint32_t material_r = GPalette.BaseColors[material].r;
		uint32_t material_g = GPalette.BaseColors[material].g;
		uint32_t material_b = GPalette.BaseColors[material].b;

		lit_r = MIN<uint32_t>(GPalette.BaseColors[fg].r + ((lit_r * material_r) >> 8), 255);
		lit_g = MIN<uint32_t>(GPalette.BaseColors[fg].g + ((lit_g * material_g) >> 8), 255);
		lit_b = MIN<uint32_t>(GPalette.BaseColors[fg].b + ((lit_b * material_b) >> 8), 255);

		return RGB256k.All[((lit_r >> 2) << 12) | ((lit_g >> 2) << 6) | (lit_b >> 2)];
	}

	void DrawWall1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		DrawerLight *dynlights = args.dc_lights;
		int num_dynlights = args.dc_num_lights;
		float viewpos_z = args.dc_viewpos.Z;
		float step_viewpos_z = args.dc_viewpos_step.Z;

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		if (num_dynlights == 0)
		{
			do
			{
				*dest = colormap[source[frac >> bits]];
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
		else
		{
			float viewpos_z = args.dc_viewpos.Z;
			float step_viewpos_z = args.dc_viewpos_step.Z;

			viewpos_z += step_viewpos_z * thread->skipped_by_thread(args.DestY());
			step_viewpos_z *= thread->num_cores;

			do
			{
				*dest = AddLights(dynlights, num_dynlights, viewpos_z, colormap[source[frac >> bits]], source[frac >> bits]);
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	void DrawWallMasked1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		DrawerLight *dynlights = args.dc_lights;
		int num_dynlights = args.dc_num_lights;
		float viewpos_z = args.dc_viewpos.Z;
		float step_viewpos_z = args.dc_viewpos_step.Z;

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		if (num_dynlights == 0)
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					*dest = colormap[pix];
				}
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
		else
		{
			float viewpos_z = args.dc_viewpos.Z;
			float step_viewpos_z = args.dc_viewpos_step.Z;

			viewpos_z += step_viewpos_z * thread->skipped_by_thread(args.DestY());
			step_viewpos_z *= thread->num_cores;

			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					*dest = AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix);
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	void DrawWallAdd1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		if (!r_blendmethod)
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = colormap[pix];

					uint32_t fg = fg2rgb[lit];
					uint32_t bg = bg2rgb[*dest];
					fg = (fg + bg) | 0x1f07c1f;
					*dest = RGB32k.All[fg & (fg >> 15)];
				}
				frac += fracstep;
				dest += pitch;
			} while (--count);

		}
		else
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = colormap[pix];

					uint32_t r = MIN(GPalette.BaseColors[lit].r + GPalette.BaseColors[*dest].r, 255);
					uint32_t g = MIN(GPalette.BaseColors[lit].g + GPalette.BaseColors[*dest].g, 255);
					uint32_t b = MIN(GPalette.BaseColors[lit].b + GPalette.BaseColors[*dest].b, 255);
					*dest = RGB256k.RGB[r>>2][g>>2][b>>2];
				}
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	void DrawWallAddClamp1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		DrawerLight *dynlights = args.dc_lights;
		int num_dynlights = args.dc_num_lights;
		float viewpos_z = args.dc_viewpos.Z;
		float step_viewpos_z = args.dc_viewpos_step.Z;

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;
		viewpos_z += step_viewpos_z * thread->skipped_by_thread(args.DestY());
		step_viewpos_z *= thread->num_cores;

		if (!r_blendmethod)
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					uint32_t a = fg2rgb[lit] + bg2rgb[*dest];
					uint32_t b = a;

					a |= 0x01f07c1f;
					b &= 0x40100400;
					a &= 0x3fffffff;
					b = b - (b >> 5);
					a |= b;
					*dest = RGB32k.All[a & (a >> 15)];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
		else
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					uint32_t r = MIN(GPalette.BaseColors[lit].r + GPalette.BaseColors[*dest].r, 255);
					uint32_t g = MIN(GPalette.BaseColors[lit].g + GPalette.BaseColors[*dest].g, 255);
					uint32_t b = MIN(GPalette.BaseColors[lit].b + GPalette.BaseColors[*dest].b, 255);
					*dest = RGB256k.RGB[r>>2][g>>2][b>>2];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	void DrawWallSubClamp1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		DrawerLight *dynlights = args.dc_lights;
		int num_dynlights = args.dc_num_lights;
		float viewpos_z = args.dc_viewpos.Z;
		float step_viewpos_z = args.dc_viewpos_step.Z;

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;
		viewpos_z += step_viewpos_z * thread->skipped_by_thread(args.DestY());
		step_viewpos_z *= thread->num_cores;

		if (!r_blendmethod)
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					uint32_t a = (fg2rgb[lit] | 0x40100400) - bg2rgb[*dest];
					uint32_t b = a;

					b &= 0x40100400;
					b = b - (b >> 5);
					a &= b;
					a |= 0x01f07c1f;
					*dest = RGB32k.All[a & (a >> 15)];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
		else
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					int r = clamp(-GPalette.BaseColors[lit].r + GPalette.BaseColors[*dest].r, 0, 255);
					int g = clamp(-GPalette.BaseColors[lit].g + GPalette.BaseColors[*dest].g, 0, 255);
					int b = clamp(-GPalette.BaseColors[lit].b + GPalette.BaseColors[*dest].b, 0, 255);
					*dest = RGB256k.RGB[r>>2][g>>2][b>>2];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	void DrawWallRevSubClamp1PalCommand::Execute(DrawerThread *thread)
	{
		uint32_t fracstep = args.TextureVStep();
		uint32_t frac = args.TextureVPos();
		uint8_t *colormap = args.Colormap(args.Viewport());
		int count = args.Count();
		const uint8_t *source = args.TexturePixels();
		uint8_t *dest = args.Dest();
		int bits = args.TextureFracBits();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		DrawerLight *dynlights = args.dc_lights;
		int num_dynlights = args.dc_num_lights;
		float viewpos_z = args.dc_viewpos.Z;
		float step_viewpos_z = args.dc_viewpos_step.Z;

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;
		viewpos_z += step_viewpos_z * thread->skipped_by_thread(args.DestY());
		step_viewpos_z *= thread->num_cores;

		if (!r_blendmethod)
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					uint32_t a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[lit];
					uint32_t b = a;

					b &= 0x40100400;
					b = b - (b >> 5);
					a &= b;
					a |= 0x01f07c1f;
					*dest = RGB32k.All[a & (a >> 15)];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
		else
		{
			do
			{
				uint8_t pix = source[frac >> bits];
				if (pix != 0)
				{
					uint8_t lit = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_z, colormap[pix], pix) : colormap[pix];

					int r = clamp(GPalette.BaseColors[lit].r - GPalette.BaseColors[*dest].r, 0, 255);
					int g = clamp(GPalette.BaseColors[lit].g - GPalette.BaseColors[*dest].g, 0, 255);
					int b = clamp(GPalette.BaseColors[lit].b - GPalette.BaseColors[*dest].b, 0, 255);
					*dest = RGB256k.RGB[r>>2][g>>2][b>>2];
				}
				viewpos_z += step_viewpos_z;
				frac += fracstep;
				dest += pitch;
			} while (--count);
		}
	}

	/////////////////////////////////////////////////////////////////////////

	PalSkyCommand::PalSkyCommand(const SkyDrawerArgs &args) : args(args)
	{
	}

	void DrawSingleSky1PalCommand::Execute(DrawerThread *thread)
	{
		uint8_t *dest = args.Dest();
		int count = args.Count();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		const uint8_t *source0 = args.FrontTexturePixels();
		int textureheight0 = args.FrontTextureHeight();

		int32_t frac = args.TextureVPos();
		int32_t fracstep = args.TextureVStep();

		// Find bands for top solid color, top fade, center textured, bottom fade, bottom solid color:
		int start_fade = 2; // How fast it should fade out
		int fade_length = (1 << (24 - start_fade));
		int start_fadetop_y = (-frac) / fracstep;
		int end_fadetop_y = (fade_length - frac) / fracstep;
		int start_fadebottom_y = ((2 << 24) - fade_length - frac) / fracstep;
		int end_fadebottom_y = ((2 << 24) - frac) / fracstep;
		start_fadetop_y = clamp(start_fadetop_y, 0, count);
		end_fadetop_y = clamp(end_fadetop_y, 0, count);
		start_fadebottom_y = clamp(start_fadebottom_y, 0, count);
		end_fadebottom_y = clamp(end_fadebottom_y, 0, count);

		int num_cores = thread->num_cores;
		int skipped = thread->skipped_by_thread(args.DestY());
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * skipped;
		fracstep *= num_cores;
		pitch *= num_cores;

		if (!args.FadeSky())
		{
			count = thread->count_for_thread(args.DestY(), count);

			for (int index = 0; index < count; index++)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				*dest = source0[sample_index];
				dest += pitch;
				frac += fracstep;
			}

			return;
		}

		uint32_t solid_top = args.SolidTopColor();
		uint32_t solid_bottom = args.SolidBottomColor();

		int solid_top_r = RPART(solid_top);
		int solid_top_g = GPART(solid_top);
		int solid_top_b = BPART(solid_top);
		int solid_bottom_r = RPART(solid_bottom);
		int solid_bottom_g = GPART(solid_bottom);
		int solid_bottom_b = BPART(solid_bottom);
		uint8_t solid_top_fill = RGB32k.RGB[(solid_top_r >> 3)][(solid_top_g >> 3)][(solid_top_b >> 3)];
		uint8_t solid_bottom_fill = RGB32k.RGB[(solid_bottom_r >> 3)][(solid_bottom_g >> 3)][(solid_bottom_b >> 3)];

		const uint32_t *palette = (const uint32_t *)GPalette.BaseColors;

		int index = skipped;

		// Top solid color:
		while (index < start_fadetop_y)
		{
			*dest = solid_top_fill;
			dest += pitch;
			frac += fracstep;
			index += num_cores;
		}

		// Top fade:
		while (index < end_fadetop_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			uint8_t fg = source0[sample_index];

			uint32_t c = palette[fg];
			int alpha_top = MAX(MIN(frac >> (16 - start_fade), 256), 0);
			int inv_alpha_top = 256 - alpha_top;
			int c_red = RPART(c);
			int c_green = GPART(c);
			int c_blue = BPART(c);
			c_red = (c_red * alpha_top + solid_top_r * inv_alpha_top) >> 8;
			c_green = (c_green * alpha_top + solid_top_g * inv_alpha_top) >> 8;
			c_blue = (c_blue * alpha_top + solid_top_b * inv_alpha_top) >> 8;
			*dest = RGB256k.RGB[(c_red >> 2)][(c_green >> 2)][(c_blue >> 2)];

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Textured center:
		while (index < start_fadebottom_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			*dest = source0[sample_index];

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Fade bottom:
		while (index < end_fadebottom_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			uint8_t fg = source0[sample_index];

			uint32_t c = palette[fg];
			int alpha_bottom = MAX(MIN(((2 << 24) - frac) >> (16 - start_fade), 256), 0);
			int inv_alpha_bottom = 256 - alpha_bottom;
			int c_red = RPART(c);
			int c_green = GPART(c);
			int c_blue = BPART(c);
			c_red = (c_red * alpha_bottom + solid_bottom_r * inv_alpha_bottom) >> 8;
			c_green = (c_green * alpha_bottom + solid_bottom_g * inv_alpha_bottom) >> 8;
			c_blue = (c_blue * alpha_bottom + solid_bottom_b * inv_alpha_bottom) >> 8;
			*dest = RGB256k.RGB[(c_red >> 2)][(c_green >> 2)][(c_blue >> 2)];

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Bottom solid color:
		while (index < count)
		{
			*dest = solid_bottom_fill;
			dest += pitch;
			index += num_cores;
		}
	}

	void DrawDoubleSky1PalCommand::Execute(DrawerThread *thread)
	{
		uint8_t *dest = args.Dest();
		int count = args.Count();
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		const uint8_t *source0 = args.FrontTexturePixels();
		const uint8_t *source1 = args.BackTexturePixels();
		int textureheight0 = args.FrontTextureHeight();
		uint32_t maxtextureheight1 = args.BackTextureHeight() - 1;

		int32_t frac = args.TextureVPos();
		int32_t fracstep = args.TextureVStep();

		// Find bands for top solid color, top fade, center textured, bottom fade, bottom solid color:
		int start_fade = 2; // How fast it should fade out
		int fade_length = (1 << (24 - start_fade));
		int start_fadetop_y = (-frac) / fracstep;
		int end_fadetop_y = (fade_length - frac) / fracstep;
		int start_fadebottom_y = ((2 << 24) - fade_length - frac) / fracstep;
		int end_fadebottom_y = ((2 << 24) - frac) / fracstep;
		start_fadetop_y = clamp(start_fadetop_y, 0, count);
		end_fadetop_y = clamp(end_fadetop_y, 0, count);
		start_fadebottom_y = clamp(start_fadebottom_y, 0, count);
		end_fadebottom_y = clamp(end_fadebottom_y, 0, count);

		int num_cores = thread->num_cores;
		int skipped = thread->skipped_by_thread(args.DestY());
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * skipped;
		fracstep *= num_cores;
		pitch *= num_cores;

		if (!args.FadeSky())
		{
			count = thread->count_for_thread(args.DestY(), count);

			for (int index = 0; index < count; index++)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint8_t fg = source0[sample_index];
				if (fg == 0)
				{
					uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
					fg = source1[sample_index2];
				}

				*dest = fg;
				dest += pitch;
				frac += fracstep;
			}

			return;
		}

		uint32_t solid_top = args.SolidTopColor();
		uint32_t solid_bottom = args.SolidBottomColor();

		int solid_top_r = RPART(solid_top);
		int solid_top_g = GPART(solid_top);
		int solid_top_b = BPART(solid_top);
		int solid_bottom_r = RPART(solid_bottom);
		int solid_bottom_g = GPART(solid_bottom);
		int solid_bottom_b = BPART(solid_bottom);
		uint8_t solid_top_fill = RGB32k.RGB[(solid_top_r >> 3)][(solid_top_g >> 3)][(solid_top_b >> 3)];
		uint8_t solid_bottom_fill = RGB32k.RGB[(solid_bottom_r >> 3)][(solid_bottom_g >> 3)][(solid_bottom_b >> 3)];

		const uint32_t *palette = (const uint32_t *)GPalette.BaseColors;

		int index = skipped;

		// Top solid color:
		while (index < start_fadetop_y)
		{
			*dest = solid_top_fill;
			dest += pitch;
			frac += fracstep;
			index += num_cores;
		}

		// Top fade:
		while (index < end_fadetop_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			uint8_t fg = source0[sample_index];
			if (fg == 0)
			{
				uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
				fg = source1[sample_index2];
			}

			uint32_t c = palette[fg];
			int alpha_top = MAX(MIN(frac >> (16 - start_fade), 256), 0);
			int inv_alpha_top = 256 - alpha_top;
			int c_red = RPART(c);
			int c_green = GPART(c);
			int c_blue = BPART(c);
			c_red = (c_red * alpha_top + solid_top_r * inv_alpha_top) >> 8;
			c_green = (c_green * alpha_top + solid_top_g * inv_alpha_top) >> 8;
			c_blue = (c_blue * alpha_top + solid_top_b * inv_alpha_top) >> 8;
			*dest = RGB256k.RGB[(c_red >> 2)][(c_green >> 2)][(c_blue >> 2)];

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Textured center:
		while (index < start_fadebottom_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			uint8_t fg = source0[sample_index];
			if (fg == 0)
			{
				uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
				fg = source1[sample_index2];
			}
			*dest = fg;

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Fade bottom:
		while (index < end_fadebottom_y)
		{
			uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
			uint8_t fg = source0[sample_index];
			if (fg == 0)
			{
				uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
				fg = source1[sample_index2];
			}

			uint32_t c = palette[fg];
			int alpha_bottom = MAX(MIN(((2 << 24) - frac) >> (16 - start_fade), 256), 0);
			int inv_alpha_bottom = 256 - alpha_bottom;
			int c_red = RPART(c);
			int c_green = GPART(c);
			int c_blue = BPART(c);
			c_red = (c_red * alpha_bottom + solid_bottom_r * inv_alpha_bottom) >> 8;
			c_green = (c_green * alpha_bottom + solid_bottom_g * inv_alpha_bottom) >> 8;
			c_blue = (c_blue * alpha_bottom + solid_bottom_b * inv_alpha_bottom) >> 8;
			*dest = RGB256k.RGB[(c_red >> 2)][(c_green >> 2)][(c_blue >> 2)];

			frac += fracstep;
			dest += pitch;
			index += num_cores;
		}

		// Bottom solid color:
		while (index < count)
		{
			*dest = solid_bottom_fill;
			dest += pitch;
			index += num_cores;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	PalColumnCommand::PalColumnCommand(const SpriteDrawerArgs &args) : args(args)
	{
	}

	uint8_t PalColumnCommand::AddLights(uint8_t fg, uint8_t material, uint32_t lit_r, uint32_t lit_g, uint32_t lit_b)
	{
		if (lit_r == 0 && lit_g == 0 && lit_b == 0)
			return fg;

		uint32_t material_r = GPalette.BaseColors[material].r;
		uint32_t material_g = GPalette.BaseColors[material].g;
		uint32_t material_b = GPalette.BaseColors[material].b;

		lit_r = MIN<uint32_t>(GPalette.BaseColors[fg].r + ((lit_r * material_r) >> 8), 255);
		lit_g = MIN<uint32_t>(GPalette.BaseColors[fg].g + ((lit_g * material_g) >> 8), 255);
		lit_b = MIN<uint32_t>(GPalette.BaseColors[fg].b + ((lit_b * material_b) >> 8), 255);

		return RGB256k.All[((lit_r >> 2) << 12) | ((lit_g >> 2) << 6) | (lit_b >> 2)];
	}

	void DrawColumnPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();

		// Framebuffer destination address.
		dest = args.Dest();

		// Determine scaling,
		//	which is the only mapping to be done.
		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		// [RH] Get local copies of these variables so that the compiler
		//		has a better chance of optimizing this well.
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		
		uint32_t dynlight = args.DynamicLight();
		if (dynlight == 0)
		{
			do
			{
				*dest = colormap[source[frac >> FRACBITS]];

				dest += pitch;
				frac += fracstep;

			} while (--count);
		}
		else
		{
			uint32_t lit_r = RPART(dynlight);
			uint32_t lit_g = GPART(dynlight);
			uint32_t lit_b = BPART(dynlight);
			uint32_t light = 256 - (args.Light() >> (FRACBITS - 8));
			lit_r = MIN<uint32_t>(light + lit_r, 256);
			lit_g = MIN<uint32_t>(light + lit_g, 256);
			lit_b = MIN<uint32_t>(light + lit_b, 256);
			lit_r = lit_r - light;
			lit_g = lit_g - light;
			lit_b = lit_b - light;
			
			do
			{
				auto material = source[frac >> FRACBITS];
				auto fg = colormap[material];
				*dest = AddLights(fg, material, lit_r, lit_g, lit_b);

				dest += pitch;
				frac += fracstep;

			} while (--count);
		}
	}

	void FillColumnPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;

		count = args.Count();
		dest = args.Dest();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		pitch *= thread->num_cores;

		uint8_t color = args.SolidColor();
		do
		{
			*dest = color;
			dest += pitch;
		} while (--count);
	}

	void FillColumnAddPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;

		count = args.Count();
		dest = args.Dest();
		uint32_t *bg2rgb;
		uint32_t fg;

		bg2rgb = args.DestBlend();
		fg = args.SrcColorIndex();
		int pitch = args.Viewport()->RenderTarget->GetPitch();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		pitch *= thread->num_cores;

		const PalEntry* pal = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t bg;
				bg = (fg + bg2rgb[*dest]) | 0x1f07c1f;
				*dest = RGB32k.All[bg & (bg >> 15)];
				dest += pitch;
			} while (--count);
		}
		else
		{
			uint32_t srccolor = args.SrcColorIndex();
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			int src_r = ((srccolor >> 16) & 0xff) * srcalpha;
			int src_g = ((srccolor >> 0) & 0xff) * srcalpha;
			int src_b = ((srccolor >> 8) & 0xff) * srcalpha;
			do
			{
				int r = clamp((src_r + pal[*dest].r * destalpha)>>18, 0, 255);
				int g = clamp((src_g + pal[*dest].g * destalpha)>>18, 0, 255);
				int b = clamp((src_b + pal[*dest].b * destalpha)>>18, 0, 255);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
			} while (--count);
		}
	}

	void FillColumnAddClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;

		count = args.Count();

		dest = args.Dest();
		uint32_t *bg2rgb;
		uint32_t fg;

		bg2rgb = args.DestBlend();
		fg = args.SrcColorIndex();
		int pitch = args.Viewport()->RenderTarget->GetPitch();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		pitch *= thread->num_cores;

		const PalEntry* pal = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = fg + bg2rgb[*dest];
				uint32_t b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
			} while (--count);
		}
		else
		{
			uint32_t srccolor = args.SrcColorIndex();
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			int src_r = ((srccolor >> 16) & 0xff) * srcalpha;
			int src_g = ((srccolor >> 0) & 0xff) * srcalpha;
			int src_b = ((srccolor >> 8) & 0xff) * srcalpha;
			do
			{
				int r = clamp((src_r + pal[*dest].r * destalpha)>>18, 0, 255);
				int g = clamp((src_g + pal[*dest].g * destalpha)>>18, 0, 255);
				int b = clamp((src_b + pal[*dest].b * destalpha)>>18, 0, 255);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
			} while (--count);
		}
	}

	void FillColumnSubClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;

		count = args.Count();

		dest = args.Dest();
		uint32_t *bg2rgb = args.DestBlend();
		uint32_t fg = args.SrcColorIndex();

		int pitch = args.Viewport()->RenderTarget->GetPitch();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		pitch *= thread->num_cores;

		const PalEntry* palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = fg - bg2rgb[*dest];
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
			} while (--count);
		}
		else
		{
			uint32_t srccolor = args.SrcColorIndex();
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int src_r = ((srccolor >> 16) & 0xff) * srcalpha;
				int src_g = ((srccolor >> 0) & 0xff) * srcalpha;
				int src_b = ((srccolor >> 8) & 0xff) * srcalpha;
				int bg = *dest;
				int r = MAX((-src_r + palette[bg].r * destalpha)>>18, 0);
				int g = MAX((-src_g + palette[bg].g * destalpha)>>18, 0);
				int b = MAX((-src_b + palette[bg].b * destalpha)>>18, 0);

				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
			} while (--count);
		}
	}

	void FillColumnRevSubClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;

		count = args.Count();
		if (count <= 0)
			return;

		dest = args.Dest();
		uint32_t *bg2rgb = args.DestBlend();
		uint32_t fg = args.SrcColorIndex();

		int pitch = args.Viewport()->RenderTarget->GetPitch();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		pitch *= thread->num_cores;

		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = (bg2rgb[*dest] | 0x40100400) - fg;
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
			} while (--count);
		}
		else
		{
			uint32_t srccolor = args.SrcColorIndex();
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int src_r = ((srccolor >> 16) & 0xff) * srcalpha;
				int src_g = ((srccolor >> 0) & 0xff) * srcalpha;
				int src_b = ((srccolor >> 8) & 0xff) * srcalpha;
				int bg = *dest;
				int r = MAX((src_r - palette[bg].r * destalpha)>>18, 0);
				int g = MAX((src_g - palette[bg].g * destalpha)>>18, 0);
				int b = MAX((src_b - palette[bg].b * destalpha)>>18, 0);

				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
			} while (--count);
		}
	}

	void DrawColumnAddPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t fg = colormap[source[frac >> FRACBITS]];
				uint32_t bg = *dest;

				fg = fg2rgb[fg];
				bg = bg2rgb[bg];
				fg = (fg + bg) | 0x1f07c1f;
				*dest = RGB32k.All[fg & (fg >> 15)];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			uint32_t srccolor = args.SrcColorIndex();
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				uint32_t fg = colormap[source[frac >> FRACBITS]];
				uint32_t bg = *dest;
				uint32_t r = MIN((palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 63);
				uint32_t g = MIN((palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 63);
				uint32_t b = MIN((palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 63);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnTranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int 				count;
		uint8_t*				dest;
		fixed_t 			frac;
		fixed_t 			fracstep;

		count = args.Count();

		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		// [RH] Local copies of global vars to improve compiler optimizations
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *translation = args.TranslationMap();
		const uint8_t *source = args.TexturePixels();

		do
		{
			*dest = colormap[translation[source[frac >> FRACBITS]]];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}

	void DrawColumnTlatedAddPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const uint8_t *translation = args.TranslationMap();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();

		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t fg = colormap[translation[source[frac >> FRACBITS]]];
				uint32_t bg = *dest;

				fg = fg2rgb[fg];
				bg = bg2rgb[bg];
				fg = (fg + bg) | 0x1f07c1f;
				*dest = RGB32k.All[fg & (fg >> 15)];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				uint32_t fg = colormap[translation[source[frac >> FRACBITS]]];
				uint32_t bg = *dest;
				uint32_t r = MIN((palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 63);
				uint32_t g = MIN((palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 63);
				uint32_t b = MIN((palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 63);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnShadedPalCommand::Execute(DrawerThread *thread)
	{
		int  count;
		uint8_t *dest;
		fixed_t frac, fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *source = args.TexturePixels();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		uint32_t *fgstart = &Col2RGB8[0][args.SolidColor()];
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t val = colormap[source[frac >> FRACBITS]];
				uint32_t fg = fgstart[val << 8];
				val = (Col2RGB8[64 - val][*dest] + fg) | 0x1f07c1f;
				*dest = RGB32k.All[val & (val >> 15)];

				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			int color = args.SolidColor();
			do
			{
				uint32_t val = colormap[source[frac >> FRACBITS]] << 2;

				int r = (palette[*dest].r * (256-val) + palette[color].r * val) >> 10;
				int g = (palette[*dest].g * (256-val) + palette[color].g * val) >> 10;
				int b = (palette[*dest].b * (256-val) + palette[color].b * val) >> 10;
				*dest = RGB256k.RGB[clamp(r,0,63)][clamp(g,0,63)][clamp(b,0,63)];

				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnAddClampShadedPalCommand::Execute(DrawerThread *thread)
	{
		int  count;
		uint8_t *dest;
		fixed_t frac, fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *source = args.TexturePixels();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		//uint32_t *fgstart = &Col2RGB8[0][args.SolidColor()]; // if someone wants to write the 555's, be my guest.
		const PalEntry *palette = GPalette.BaseColors;

		int color = args.SolidColor();
		do
		{
			uint32_t val = colormap[source[frac >> FRACBITS]] << 2;

			int r = (palette[*dest].r * (256) + palette[color].r * val) >> 10;
			int g = (palette[*dest].g * (256) + palette[color].g * val) >> 10;
			int b = (palette[*dest].b * (256) + palette[color].b * val) >> 10;
			*dest = RGB256k.RGB[clamp(r,0,63)][clamp(g,0,63)][clamp(b,0,63)];

			dest += pitch;
			frac += fracstep;
		} while (--count);
	}

	void DrawColumnAddClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = fg2rgb[colormap[source[frac >> FRACBITS]]] + bg2rgb[*dest];
				uint32_t b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[source[frac >> FRACBITS]];
				int bg = *dest;
				int r = MIN((palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 63);
				int g = MIN((palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 63);
				int b = MIN((palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 63);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnAddClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *translation = args.TranslationMap();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = fg2rgb[colormap[translation[source[frac >> FRACBITS]]]] + bg2rgb[*dest];
				uint32_t b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				*dest = RGB32k.All[(a >> 15) & a];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[translation[source[frac >> FRACBITS]]];
				int bg = *dest;
				int r = MIN((palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 63);
				int g = MIN((palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 63);
				int b = MIN((palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 63);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnSubClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = (fg2rgb[colormap[source[frac >> FRACBITS]]] | 0x40100400) - bg2rgb[*dest];
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[source[frac >> FRACBITS]];
				int bg = *dest;
				int r = MAX((palette[fg].r * srcalpha - palette[bg].r * destalpha)>>18, 0);
				int g = MAX((palette[fg].g * srcalpha - palette[bg].g * destalpha)>>18, 0);
				int b = MAX((palette[fg].b * srcalpha - palette[bg].b * destalpha)>>18, 0);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnSubClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *translation = args.TranslationMap();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = (fg2rgb[colormap[translation[source[frac >> FRACBITS]]]] | 0x40100400) - bg2rgb[*dest];
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[(a >> 15) & a];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[translation[source[frac >> FRACBITS]]];
				int bg = *dest;
				int r = MAX((palette[fg].r * srcalpha - palette[bg].r * destalpha)>>18, 0);
				int g = MAX((palette[fg].g * srcalpha - palette[bg].g * destalpha)>>18, 0);
				int b = MAX((palette[fg].b * srcalpha - palette[bg].b * destalpha)>>18, 0);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnRevSubClampPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[colormap[source[frac >> FRACBITS]]];
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[a & (a >> 15)];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[source[frac >> FRACBITS]];
				int bg = *dest;
				int r = MAX((-palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 0);
				int g = MAX((-palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 0);
				int b = MAX((-palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 0);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	void DrawColumnRevSubClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int count;
		uint8_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = args.Count();
		dest = args.Dest();

		fracstep = args.TextureVStep();
		frac = args.TextureVPos();

		count = thread->count_for_thread(args.DestY(), count);
		if (count <= 0)
			return;

		int pitch = args.Viewport()->RenderTarget->GetPitch();
		dest = thread->dest_for_thread(args.DestY(), pitch, dest);
		frac += fracstep * thread->skipped_by_thread(args.DestY());
		fracstep *= thread->num_cores;
		pitch *= thread->num_cores;

		const uint8_t *translation = args.TranslationMap();
		const uint8_t *colormap = args.Colormap(args.Viewport());
		const uint8_t *source = args.TexturePixels();
		uint32_t *fg2rgb = args.SrcBlend();
		uint32_t *bg2rgb = args.DestBlend();
		const PalEntry *palette = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			do
			{
				uint32_t a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[colormap[translation[source[frac >> FRACBITS]]]];
				uint32_t b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				*dest = RGB32k.All[(a >> 15) & a];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
		else
		{
			fixed_t srcalpha = args.SrcAlpha();
			fixed_t destalpha = args.DestAlpha();
			do
			{
				int fg = colormap[translation[source[frac >> FRACBITS]]];
				int bg = *dest;
				int r = MAX((-palette[fg].r * srcalpha + palette[bg].r * destalpha)>>18, 0);
				int g = MAX((-palette[fg].g * srcalpha + palette[bg].g * destalpha)>>18, 0);
				int b = MAX((-palette[fg].b * srcalpha + palette[bg].b * destalpha)>>18, 0);
				*dest = RGB256k.RGB[r][g][b];
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawScaledFuzzColumnPalCommand::DrawScaledFuzzColumnPalCommand(const SpriteDrawerArgs &drawerargs)
	{
		_x = drawerargs.FuzzX();
		_yl = drawerargs.FuzzY1();
		_yh = drawerargs.FuzzY2();
		_destorg = drawerargs.Viewport()->GetDest(0, 0);
		_pitch = drawerargs.Viewport()->RenderTarget->GetPitch();
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void DrawScaledFuzzColumnPalCommand::Execute(DrawerThread *thread)
	{
		int x = _x;
		int yl = MAX(_yl, 1);
		int yh = MIN(_yh, _fuzzviewheight);

		int count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0) return;

		int pitch = _pitch;
		uint8_t *dest = _pitch * yl + x + (uint8_t*)_destorg;

		int scaled_x = x * 200 / _fuzzviewheight;
		int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

		fixed_t fuzzstep = (200 << FRACBITS) / _fuzzviewheight;
		fixed_t fuzzcount = FUZZTABLE << FRACBITS;
		fixed_t fuzz = (fuzz_x << FRACBITS) + yl * fuzzstep;

		dest = thread->dest_for_thread(yl, pitch, dest);
		pitch *= thread->num_cores;

		fuzz += fuzzstep * thread->skipped_by_thread(yl);
		fuzz %= fuzzcount;
		fuzzstep *= thread->num_cores;

		uint8_t *map = NormalLight.Maps;

		while (count > 0)
		{
			int offset = fuzzoffset[fuzz >> FRACBITS] << 8;
			*dest = map[offset + *dest];
			dest += pitch;

			fuzz += fuzzstep;
			if (fuzz >= fuzzcount) fuzz -= fuzzcount;

			count--;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	DrawFuzzColumnPalCommand::DrawFuzzColumnPalCommand(const SpriteDrawerArgs &args)
	{
		_yl = args.FuzzY1();
		_yh = args.FuzzY2();
		_x = args.FuzzX();
		_destorg = args.Viewport()->GetDest(0, 0);
		_pitch = args.Viewport()->RenderTarget->GetPitch();
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void DrawFuzzColumnPalCommand::Execute(DrawerThread *thread)
	{
		int yl = MAX(_yl, 1);
		int yh = MIN(_yh, _fuzzviewheight);

		int count = thread->count_for_thread(yl, yh - yl + 1);

		// Zero length.
		if (count <= 0)
			return;

		int pitch = _pitch;
		uint8_t *dest = thread->dest_for_thread(yl, pitch, yl * pitch + _x + _destorg);

		pitch = pitch * thread->num_cores;
		int fuzzstep = thread->num_cores;
		int fuzz = (_fuzzpos + thread->skipped_by_thread(yl)) % FUZZTABLE;

#ifndef ORIGINAL_FUZZ

		uint8_t *map = NormalLight.Maps;

		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = MIN(count, next_wrap);
			count -= cnt;
			do
			{
				int offset = fuzzoffset[fuzz] << 8;

				*dest = map[offset + *dest];
				dest += pitch;
				fuzz += fuzzstep;
			} while (--cnt);

			fuzz %= FUZZTABLE;
		}

#else

		uint8_t *map = &NormalLight.Maps[6 * 256];

		yl += thread->skipped_by_thread(yl);

		// Handle the case where we would go out of bounds at the top:
		if (yl < fuzzstep)
		{
			uint8_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep + pitch;
			//assert(static_cast<int>((srcdest - (uint8_t*)dc_destorg) / (pitch)) < viewheight);

			*dest = map[*srcdest];
			dest += pitch;
			fuzz += fuzzstep;
			fuzz %= FUZZTABLE;

			count--;
			if (count == 0)
				return;
		}

		bool lowerbounds = (yl + (count + fuzzstep - 1) * fuzzstep > _fuzzviewheight);
		if (lowerbounds)
			count--;

		// Fuzz where fuzzoffset stays within bounds
		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = MIN(count, next_wrap);
			count -= cnt;
			do
			{
				uint8_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep;
				//assert(static_cast<int>((srcdest - (uint8_t*)dc_destorg) / (pitch)) < viewheight);

				*dest = map[*srcdest];
				dest += pitch;
				fuzz += fuzzstep;
			} while (--cnt);

			fuzz %= FUZZTABLE;
		}

		// Handle the case where we would go out of bounds at the bottom
		if (lowerbounds)
		{
			uint8_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep - pitch;
			//assert(static_cast<int>((srcdest - (uint8_t*)dc_destorg) / (pitch)) < viewheight);

			*dest = map[*srcdest];
		}
#endif
	}

	/////////////////////////////////////////////////////////////////////////

	PalSpanCommand::PalSpanCommand(const SpanDrawerArgs &args)
	{
		_source = args.TexturePixels();
		_colormap = args.Colormap(args.Viewport());
		_xfrac = args.TextureUPos();
		_yfrac = args.TextureVPos();
		_y = args.DestY();
		_x1 = args.DestX1();
		_x2 = args.DestX2();
		_dest = args.Viewport()->GetDest(_x1, _y);
		_xstep = args.TextureUStep();
		_ystep = args.TextureVStep();
		_srcwidth = args.TextureWidth();
		_srcheight = args.TextureHeight();
		_srcblend = args.SrcBlend();
		_destblend = args.DestBlend();
		_color = args.SolidColor();
		_srcalpha = args.SrcAlpha();
		_destalpha = args.DestAlpha();
		_dynlights = args.dc_lights;
		_num_dynlights = args.dc_num_lights;
		_viewpos_x = args.dc_viewpos.X;
		_step_viewpos_x = args.dc_viewpos_step.X;
	}

	uint8_t PalSpanCommand::AddLights(const DrawerLight *lights, int num_lights, float viewpos_x, uint8_t fg, uint8_t material)
	{
		uint32_t lit_r = 0;
		uint32_t lit_g = 0;
		uint32_t lit_b = 0;

		for (int i = 0; i < num_lights; i++)
		{
			uint32_t light_color_r = RPART(lights[i].color);
			uint32_t light_color_g = GPART(lights[i].color);
			uint32_t light_color_b = BPART(lights[i].color);

			// L = light-pos
			// dist = sqrt(dot(L, L))
			// attenuation = 1 - MIN(dist * (1/radius), 1)
			float Lyz2 = lights[i].y; // L.y*L.y + L.z*L.z
			float Lx = lights[i].x - viewpos_x;
			float dist2 = Lyz2 + Lx * Lx;
#ifdef NO_SSE
			float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
			float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_load_ss(&dist2)));
#endif
			float dist = dist2 * rcp_dist;
			float distance_attenuation = (256.0f - MIN(dist * lights[i].radius, 256.0f));

			// The simple light type
			float simple_attenuation = distance_attenuation;

			// The point light type
			// diffuse = dot(N,L) * attenuation
			float point_attenuation = lights[i].z * rcp_dist * distance_attenuation;
			uint32_t attenuation = (uint32_t)(lights[i].z == 0.0f ? simple_attenuation : point_attenuation);

			lit_r += (light_color_r * attenuation) >> 8;
			lit_g += (light_color_g * attenuation) >> 8;
			lit_b += (light_color_b * attenuation) >> 8;
		}

		if (lit_r == 0 && lit_g == 0 && lit_b == 0)
			return fg;

		uint32_t material_r = GPalette.BaseColors[material].r;
		uint32_t material_g = GPalette.BaseColors[material].g;
		uint32_t material_b = GPalette.BaseColors[material].b;

		lit_r = MIN<uint32_t>(GPalette.BaseColors[fg].r + ((lit_r * material_r) >> 8), 255);
		lit_g = MIN<uint32_t>(GPalette.BaseColors[fg].g + ((lit_g * material_g) >> 8), 255);
		lit_b = MIN<uint32_t>(GPalette.BaseColors[fg].b + ((lit_b * material_b) >> 8), 255);

		return RGB256k.All[((lit_r >> 2) << 12) | ((lit_g >> 2) << 6) | (lit_b >> 2)];
	}

	void DrawSpanPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		if (_srcwidth == 64 && _srcheight == 64 && num_dynlights == 0)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				// Lookup pixel from flat texture tile,
				//  re-index using light/colormap.
				*dest++ = colormap[source[spot]];

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else if (_srcwidth == 64 && _srcheight == 64)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				// Lookup pixel from flat texture tile,
				//  re-index using light/colormap.
				*dest++ = AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
				viewpos_x += step_viewpos_x;
			} while (--count);
		}
		else
		{
			uint32_t srcwidth = _srcwidth;
			uint32_t srcheight = _srcheight;

			do
			{
				// Current texture index in u,v.
				spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);

				// Lookup pixel from flat texture tile,
				//  re-index using light/colormap.
				*dest++ = AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
				viewpos_x += step_viewpos_x;
			} while (--count);
		}
	}

	void DrawSpanMaskedPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		if (_srcwidth == 64 && _srcheight == 64)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				int texdata;

				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
				texdata = source[spot];
				if (texdata != 0)
				{
					*dest = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
				viewpos_x += step_viewpos_x;
			} while (--count);
		}
		else
		{
			uint32_t srcwidth = _srcwidth;
			uint32_t srcheight = _srcheight;

			do
			{
				int texdata;

				spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
				texdata = source[spot];
				if (texdata != 0)
				{
					*dest = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
				viewpos_x += step_viewpos_x;
			} while (--count);
		}
	}

	void DrawSpanTranslucentPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;
		uint32_t *fg2rgb = _srcblend;
		uint32_t *bg2rgb = _destblend;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		const PalEntry *palette = GPalette.BaseColors;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		if (!r_blendmethod)
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					fg = fg2rgb[fg];
					bg = bg2rgb[bg];
					fg = (fg + bg) | 0x1f07c1f;
					*dest++ = RGB32k.All[fg & (fg >> 15)];
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					fg = fg2rgb[fg];
					bg = bg2rgb[bg];
					fg = (fg + bg) | 0x1f07c1f;
					*dest++ = RGB32k.All[fg & (fg >> 15)];
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
		else
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
					int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
					int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
					*dest++ = RGB256k.RGB[r][g][b];

					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
					int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
					int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
					*dest++ = RGB256k.RGB[r][g][b];

					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
	}

	void DrawSpanMaskedTranslucentPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;
		uint32_t *fg2rgb = _srcblend;
		uint32_t *bg2rgb = _destblend;

		const PalEntry *palette = GPalette.BaseColors;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (!r_blendmethod)
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					uint8_t texdata;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						fg = fg2rgb[fg];
						bg = bg2rgb[bg];
						fg = (fg + bg) | 0x1f07c1f;
						*dest = RGB32k.All[fg & (fg >> 15)];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					uint8_t texdata;

					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						fg = fg2rgb[fg];
						bg = bg2rgb[bg];
						fg = (fg + bg) | 0x1f07c1f;
						*dest = RGB32k.All[fg & (fg >> 15)];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
		else
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					uint8_t texdata;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
						int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
						int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
						*dest = RGB256k.RGB[r][g][b];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					uint8_t texdata;

					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
						int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
						int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
						*dest = RGB256k.RGB[r][g][b];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
	}

	void DrawSpanAddClampPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;
		uint32_t *fg2rgb = _srcblend;
		uint32_t *bg2rgb = _destblend;
		const PalEntry *palette = GPalette.BaseColors;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (!r_blendmethod)
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t a = fg2rgb[fg] + bg2rgb[*dest];
					uint32_t b = a;

					a |= 0x01f07c1f;
					b &= 0x40100400;
					a &= 0x3fffffff;
					b = b - (b >> 5);
					a |= b;
					*dest++ = RGB32k.All[a & (a >> 15)];
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t a = fg2rgb[fg] + bg2rgb[*dest];
					uint32_t b = a;

					a |= 0x01f07c1f;
					b &= 0x40100400;
					a &= 0x3fffffff;
					b = b - (b >> 5);
					a |= b;
					*dest++ = RGB32k.All[a & (a >> 15)];
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
		else
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
					int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
					int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
					*dest++ = RGB256k.RGB[r][g][b];

					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[source[spot]], source[spot]) : colormap[source[spot]];
					uint32_t bg = *dest;
					int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
					int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
					int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
					*dest++ = RGB256k.RGB[r][g][b];

					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
	}

	void DrawSpanMaskedAddClampPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t xfrac;
		uint32_t yfrac;
		uint32_t xstep;
		uint32_t ystep;
		uint8_t *dest;
		const uint8_t *source = _source;
		const uint8_t *colormap = _colormap;
		int count;
		int spot;
		uint32_t *fg2rgb = _srcblend;
		uint32_t *bg2rgb = _destblend;
		const PalEntry *palette = GPalette.BaseColors;

		const DrawerLight *dynlights = _dynlights;
		int num_dynlights = _num_dynlights;
		float viewpos_x = _viewpos_x;
		float step_viewpos_x = _step_viewpos_x;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = _dest;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (!r_blendmethod)
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					uint8_t texdata;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t a = fg2rgb[fg] + bg2rgb[*dest];
						uint32_t b = a;
	
						a |= 0x01f07c1f;
						b &= 0x40100400;
						a &= 0x3fffffff;
						b = b - (b >> 5);
						a |= b;
						*dest = RGB32k.All[a & (a >> 15)];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					uint8_t texdata;

					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t a = fg2rgb[fg] + bg2rgb[*dest];
						uint32_t b = a;
	
						a |= 0x01f07c1f;
						b &= 0x40100400;
						a &= 0x3fffffff;
						b = b - (b >> 5);
						a |= b;
						*dest = RGB32k.All[a & (a >> 15)];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
		else
		{
			if (_srcwidth == 64 && _srcheight == 64)
			{
				// 64x64 is the most common case by far, so special case it.
				do
				{
					uint8_t texdata;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
						int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
						int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
						*dest = RGB256k.RGB[r][g][b];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
			else
			{
				uint32_t srcwidth = _srcwidth;
				uint32_t srcheight = _srcheight;

				do
				{
					uint8_t texdata;

					spot = (((xfrac >> 16) * srcwidth) >> 16) * srcheight + (((yfrac >> 16) * srcheight) >> 16);
					texdata = source[spot];
					if (texdata != 0)
					{
						uint32_t fg = num_dynlights != 0 ? AddLights(dynlights, num_dynlights, viewpos_x, colormap[texdata], texdata) : colormap[texdata];
						uint32_t bg = *dest;
						int r = MAX((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
						int g = MAX((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
						int b = MAX((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
						*dest = RGB256k.RGB[r][g][b];
					}
					dest++;
					xfrac += xstep;
					yfrac += ystep;
					viewpos_x += step_viewpos_x;
				} while (--count);
			}
		}
	}

	void FillSpanPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		memset(_dest, _color, _x2 - _x1 + 1);
	}

	/////////////////////////////////////////////////////////////////////////

	DrawTiltedSpanPalCommand::DrawTiltedSpanPalCommand(const SpanDrawerArgs &args, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap)
		: plane_sz(plane_sz), plane_su(plane_su), plane_sv(plane_sv), plane_shade(plane_shade), planeshade(planeshade), planelightfloat(planelightfloat), pviewx(pviewx), pviewy(pviewy)
	{
		y = args.DestY();
		x1 = args.DestX1();
		x2 = args.DestX2();
		viewport = args.Viewport();
		_colormap = args.Colormap(args.Viewport());
		_dest = args.Viewport()->GetDest(x1, y);
		_ybits = args.TextureHeightBits();
		_xbits = args.TextureWidthBits();
		_source = args.TexturePixels();
		basecolormapdata = basecolormap->Maps;
	}

	void DrawTiltedSpanPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(y))
			return;

		const uint8_t **tiltlighting = thread->tiltlighting;

		int width = x2 - x1;
		double iz, uz, vz;
		uint8_t *fb;
		uint32_t u, v;
		int i;

		iz = plane_sz[2] + plane_sz[1] * (viewport->viewwindow.centery - y) + plane_sz[0] * (x1 - viewport->viewwindow.centerx);

		// Lighting is simple. It's just linear interpolation from start to end
		if (plane_shade)
		{
			uz = (iz + plane_sz[0] * width) * planelightfloat;
			vz = iz * planelightfloat;
			CalcTiltedLighting(vz, uz, width, thread);
		}
		else
		{
			for (int i = 0; i <= width; ++i)
			{
				tiltlighting[i] = _colormap;
			}
		}

		uz = plane_su[2] + plane_su[1] * (viewport->viewwindow.centery - y) + plane_su[0] * (x1 - viewport->viewwindow.centerx);
		vz = plane_sv[2] + plane_sv[1] * (viewport->viewwindow.centery - y) + plane_sv[0] * (x1 - viewport->viewwindow.centerx);

		fb = _dest;

		uint8_t vshift = 32 - _ybits;
		uint8_t ushift = vshift - _xbits;
		int umask = ((1 << _xbits) - 1) << _ybits;

		#if 0
		// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
		i = 0;
		do
		{
			double z = 1.f / iz;

			u = int64_t(uz*z) + pviewx;
			v = int64_t(vz*z) + pviewy;
			R_SetDSColorMapLight(tiltlighting[i], 0, 0);
			fb[i++] = ds_colormap[ds_source[(v >> vshift) | ((u >> ushift) & umask)]];
			iz += plane_sz[0];
			uz += plane_su[0];
			vz += plane_sv[0];
		} while (--width >= 0);
		#else
		//#define SPANSIZE 32
		//#define INVSPAN 0.03125f
		//#define SPANSIZE 8
		//#define INVSPAN 0.125f
		#define SPANSIZE 16
		#define INVSPAN	0.0625f

		double startz = 1.f / iz;
		double startu = uz*startz;
		double startv = vz*startz;
		double izstep, uzstep, vzstep;

		izstep = plane_sz[0] * SPANSIZE;
		uzstep = plane_su[0] * SPANSIZE;
		vzstep = plane_sv[0] * SPANSIZE;
		x1 = 0;
		width++;

		while (width >= SPANSIZE)
		{
			iz += izstep;
			uz += uzstep;
			vz += vzstep;

			double endz = 1.f / iz;
			double endu = uz*endz;
			double endv = vz*endz;
			uint32_t stepu = (uint32_t)int64_t((endu - startu) * INVSPAN);
			uint32_t stepv = (uint32_t)int64_t((endv - startv) * INVSPAN);
			u = (uint32_t)(int64_t(startu) + pviewx);
			v = (uint32_t)(int64_t(startv) + pviewy);

			for (i = SPANSIZE - 1; i >= 0; i--)
			{
				fb[x1] = *(tiltlighting[x1] + _source[(v >> vshift) | ((u >> ushift) & umask)]);
				x1++;
				u += stepu;
				v += stepv;
			}
			startu = endu;
			startv = endv;
			width -= SPANSIZE;
		}
		if (width > 0)
		{
			if (width == 1)
			{
				u = (uint32_t)int64_t(startu);
				v = (uint32_t)int64_t(startv);
				fb[x1] = *(tiltlighting[x1] + _source[(v >> vshift) | ((u >> ushift) & umask)]);
			}
			else
			{
				double left = width;
				iz += plane_sz[0] * left;
				uz += plane_su[0] * left;
				vz += plane_sv[0] * left;

				double endz = 1.f / iz;
				double endu = uz*endz;
				double endv = vz*endz;
				left = 1.f / left;
				uint32_t stepu = (uint32_t)int64_t((endu - startu) * left);
				uint32_t stepv = (uint32_t)int64_t((endv - startv) * left);
				u = (uint32_t)(int64_t(startu) + pviewx);
				v = (uint32_t)(int64_t(startv) + pviewy);

				for (; width != 0; width--)
				{
					fb[x1] = *(tiltlighting[x1] + _source[(v >> vshift) | ((u >> ushift) & umask)]);
					x1++;
					u += stepu;
					v += stepv;
				}
			}
		}
		#endif
	}

	// Calculates the lighting for one row of a tilted plane. If the definition
	// of GETPALOOKUP changes, this needs to change, too.
	void DrawTiltedSpanPalCommand::CalcTiltedLighting(double lstart, double lend, int width, DrawerThread *thread)
	{
		const uint8_t **tiltlighting = thread->tiltlighting;

		uint8_t *lightstart = basecolormapdata + (GETPALOOKUP(lstart, planeshade) << COLORMAPSHIFT);
		uint8_t *lightend = basecolormapdata + (GETPALOOKUP(lend, planeshade) << COLORMAPSHIFT);

		if (width == 0 || lightstart == lightend)
		{
			for (int i = 0; i <= width; i++)
			{
				tiltlighting[i] = lightstart;
			}
		}
		else
		{
			double lstep = (lend - lstart) / width;
			double lval = lstart;
			for (int i = 0; i <= width; i++)
			{
				tiltlighting[i] = basecolormapdata + (GETPALOOKUP(lval, planeshade) << COLORMAPSHIFT);
				lval += lstep;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////

	DrawColoredSpanPalCommand::DrawColoredSpanPalCommand(const SpanDrawerArgs &args) : PalSpanCommand(args)
	{
		y = args.DestY();
		x1 = args.DestX1();
		x2 = args.DestX2();
		color = args.SolidColor();
		dest = args.Viewport()->GetDest(x1, y);
	}

	void DrawColoredSpanPalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(y))
			return;

		memset(_dest, color, x2 - x1 + 1);
	}

	/////////////////////////////////////////////////////////////////////////

	DrawFogBoundaryLinePalCommand::DrawFogBoundaryLinePalCommand(const SpanDrawerArgs &args) : PalSpanCommand(args)
	{
		y = args.DestY();
		x1 = args.DestX1();
		x2 = args.DestX2();
		_colormap = args.Colormap(args.Viewport());
		_dest = args.Viewport()->GetDest(0, y);
	}

	void DrawFogBoundaryLinePalCommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(y))
			return;

		const uint8_t *colormap = _colormap;
		uint8_t *dest = _dest;
		int x = x1;
		do
		{
			dest[x] = colormap[dest[x]];
		} while (++x <= x2);
	}
	
	/////////////////////////////////////////////////////////////////////////////

	DrawParticleColumnPalCommand::DrawParticleColumnPalCommand(uint8_t *dest, int dest_y, int pitch, int count, uint32_t fg, uint32_t alpha, uint32_t fracposx)
	{
		_dest = dest;
		_pitch = pitch;
		_count = count;
		_fg = fg;
		_alpha = alpha;
		_fracposx = fracposx;
		_dest_y = dest_y;
	}

	void DrawParticleColumnPalCommand::Execute(DrawerThread *thread)
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		int pitch = _pitch;
		uint8_t *dest = thread->dest_for_thread(_dest_y, pitch, _dest);
		pitch = pitch * thread->num_cores;

		int particle_texture_index = MIN<int>(gl_particles_style, NUM_PARTICLE_TEXTURES - 1);
		const uint32_t *source = &particle_texture[particle_texture_index][(_fracposx >> FRACBITS) * PARTICLE_TEXTURE_SIZE];
		uint32_t particle_alpha = _alpha;

		uint32_t fracstep = PARTICLE_TEXTURE_SIZE * FRACUNIT / _count;
		uint32_t fracpos = fracstep * thread->skipped_by_thread(_dest_y) + fracstep / 2;
		fracstep *= thread->num_cores;

		uint32_t fg_red = (_fg >> 16) & 0xff;
		uint32_t fg_green = (_fg >> 8) & 0xff;
		uint32_t fg_blue = _fg & 0xff;

		for (int y = 0; y < count; y++)
		{
			uint32_t alpha = (source[fracpos >> FRACBITS] * particle_alpha) >> 7;
			uint32_t inv_alpha = 256 - alpha;

			int bg = *dest;
			uint32_t bg_red = GPalette.BaseColors[bg].r;
			uint32_t bg_green = GPalette.BaseColors[bg].g;
			uint32_t bg_blue = GPalette.BaseColors[bg].b;

			uint32_t red = (fg_red * alpha + bg_red * inv_alpha) / 256;
			uint32_t green = (fg_green * alpha + bg_green * inv_alpha) / 256;
			uint32_t blue = (fg_blue * alpha + bg_blue * inv_alpha) / 256;

			*dest = RGB256k.All[((red >> 2) << 12) | ((green >> 2) << 6) | (blue >> 2)];
			dest += pitch;
			fracpos += fracstep;
		}
	}

	FString DrawParticleColumnPalCommand::DebugInfo()
	{
		return "DrawParticle";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawVoxelBlocksPalCommand::DrawVoxelBlocksPalCommand(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount) : args(args), blocks(blocks), blockcount(blockcount)
	{
	}

	void DrawVoxelBlocksPalCommand::Execute(DrawerThread *thread)
	{
		int destpitch = args.Viewport()->RenderTarget->GetPitch();
		uint8_t *destorig = args.Viewport()->RenderTarget->GetPixels();
		const uint8_t *colormap = args.Colormap(args.Viewport());

		for (int i = 0; i < blockcount; i++)
		{
			const VoxelBlock &block = blocks[i];

			const uint8_t *source = block.voxels;

			fixed_t fracpos = block.vPos;
			fixed_t iscale = block.vStep;
			int count = block.height;
			int width = block.width;
			int pitch = destpitch;
			uint8_t *dest = destorig + (block.x + block.y * pitch);

			count = thread->count_for_thread(block.y, count);
			dest = thread->dest_for_thread(block.y, pitch, dest);
			fracpos += iscale * thread->skipped_by_thread(block.y);
			iscale *= thread->num_cores;
			pitch *= thread->num_cores;

			if (width == 1)
			{
				while (count > 0)
				{
					uint8_t color = colormap[source[fracpos >> FRACBITS]];
					*dest = color;
					dest += pitch;
					fracpos += iscale;
					count--;
				}
			}
			else if (width == 2)
			{
				while (count > 0)
				{
					uint8_t color = colormap[source[fracpos >> FRACBITS]];
					dest[0] = color;
					dest[1] = color;
					dest += pitch;
					fracpos += iscale;
					count--;
				}
			}
			else if (width == 3)
			{
				while (count > 0)
				{
					uint8_t color = colormap[source[fracpos >> FRACBITS]];
					dest[0] = color;
					dest[1] = color;
					dest[2] = color;
					dest += pitch;
					fracpos += iscale;
					count--;
				}
			}
			else if (width == 4)
			{
				while (count > 0)
				{
					uint8_t color = colormap[source[fracpos >> FRACBITS]];
					dest[0] = color;
					dest[1] = color;
					dest[2] = color;
					dest[3] = color;
					dest += pitch;
					fracpos += iscale;
					count--;
				}
			}
			else
			{
				while (count > 0)
				{
					uint8_t color = colormap[source[fracpos >> FRACBITS]];

					for (int x = 0; x < width; x++)
						dest[x] = color;

					dest += pitch;
					fracpos += iscale;
					count--;
				}
			}
		}
	}

	FString DrawVoxelBlocksPalCommand::DebugInfo()
	{
		return "DrawVoxelBlocks";
	}
}
