/*
**  Drawer commands for spans
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

/*
	Warning: this C++ source file has been auto-generated. Please modify the original php script that generated it.
*/

#pragma once

#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_spandrawer.h"

namespace swrenderer
{
	class DrawSpan32Command : public DrawerCommand
	{
	protected:
		SpanDrawerArgs args;

	public:
		DrawSpan32Command(const SpanDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			if (thread->line_skipped_by_thread(args.DestY())) return;
			
			uint32_t xbits = args.TextureWidthBits();
			uint32_t ybits = args.TextureHeightBits();
			uint32_t xstep = args.TextureUStep();
			uint32_t ystep = args.TextureVStep();
			uint32_t xfrac = args.TextureUPos();
			uint32_t yfrac = args.TextureVPos();
			uint32_t yshift = 32 - ybits;
			uint32_t xshift = yshift - xbits;
			uint32_t xmask = ((1 << xbits) - 1) << ybits;
			
			const uint32_t *source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (xbits <= 2 || ybits <= 2)
						break;

					source += (1 << (xbits)) * (1 << (ybits));
					xbits -= 1;
					ybits -= 1;
					level--;
				}
			}

			bool is_nearest_filter = !((magnifying && r_magfilter) || (!magnifying && r_minfilter));
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
			else
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i outcolor = fgcolor;
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
		}
		
		FString DebugInfo() override { return "DrawSpan32Command"; }
	};

	class DrawSpanMasked32Command : public DrawerCommand
	{
	protected:
		SpanDrawerArgs args;

	public:
		DrawSpanMasked32Command(const SpanDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			if (thread->line_skipped_by_thread(args.DestY())) return;
			
			uint32_t xbits = args.TextureWidthBits();
			uint32_t ybits = args.TextureHeightBits();
			uint32_t xstep = args.TextureUStep();
			uint32_t ystep = args.TextureVStep();
			uint32_t xfrac = args.TextureUPos();
			uint32_t yfrac = args.TextureVPos();
			uint32_t yshift = 32 - ybits;
			uint32_t xshift = yshift - xbits;
			uint32_t xmask = ((1 << xbits) - 1) << ybits;
			
			const uint32_t *source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (xbits <= 2 || ybits <= 2)
						break;

					source += (1 << (xbits)) * (1 << (ybits));
					xbits -= 1;
					ybits -= 1;
					level--;
				}
			}

			bool is_nearest_filter = !((magnifying && r_magfilter) || (!magnifying && r_minfilter));
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
			else
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_shufflehi_epi16(fgcolor, _MM_SHUFFLE(3,3,3,3));
						alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256
						__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
						
						fgcolor = _mm_mullo_epi16(fgcolor, alpha);
						bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
						__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
		}
		
		FString DebugInfo() override { return "DrawSpanMasked32Command"; }
	};

	class DrawSpanTranslucent32Command : public DrawerCommand
	{
	protected:
		SpanDrawerArgs args;

	public:
		DrawSpanTranslucent32Command(const SpanDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			if (thread->line_skipped_by_thread(args.DestY())) return;
			
			uint32_t xbits = args.TextureWidthBits();
			uint32_t ybits = args.TextureHeightBits();
			uint32_t xstep = args.TextureUStep();
			uint32_t ystep = args.TextureVStep();
			uint32_t xfrac = args.TextureUPos();
			uint32_t yfrac = args.TextureVPos();
			uint32_t yshift = 32 - ybits;
			uint32_t xshift = yshift - xbits;
			uint32_t xmask = ((1 << xbits) - 1) << ybits;
			
			const uint32_t *source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (xbits <= 2 || ybits <= 2)
						break;

					source += (1 << (xbits)) * (1 << (ybits));
					xbits -= 1;
					ybits -= 1;
					level--;
				}
			}

			bool is_nearest_filter = !((magnifying && r_magfilter) || (!magnifying && r_minfilter));
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
			else
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						__m128i fgalpha = _mm_set1_epi16(srcalpha);
						__m128i bgalpha = _mm_set1_epi16(destalpha);

						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);
						
						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
		}
		
		FString DebugInfo() override { return "DrawSpanTranslucent32Command"; }
	};

	class DrawSpanAddClamp32Command : public DrawerCommand
	{
	protected:
		SpanDrawerArgs args;

	public:
		DrawSpanAddClamp32Command(const SpanDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			if (thread->line_skipped_by_thread(args.DestY())) return;
			
			uint32_t xbits = args.TextureWidthBits();
			uint32_t ybits = args.TextureHeightBits();
			uint32_t xstep = args.TextureUStep();
			uint32_t ystep = args.TextureVStep();
			uint32_t xfrac = args.TextureUPos();
			uint32_t yfrac = args.TextureVPos();
			uint32_t yshift = 32 - ybits;
			uint32_t xshift = yshift - xbits;
			uint32_t xmask = ((1 << xbits) - 1) << ybits;
			
			const uint32_t *source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (xbits <= 2 || ybits <= 2)
						break;

					source += (1 << (xbits)) * (1 << (ybits));
					xbits -= 1;
					ybits -= 1;
					level--;
				}
			}

			bool is_nearest_filter = !((magnifying && r_magfilter) || (!magnifying && r_minfilter));
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
			else
			{
				if (is_nearest_filter)
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
				else
				{
				bool is_64x64 = xbits == 6 && ybits == 6;
				if (is_64x64)
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 26;
						uint32_t yybits = 26;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				else
				{
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpx = args.dc_viewpos.X;
					float stepvpx = args.dc_viewpos_step.X;
					__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
					__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);
					
					int count = args.DestX2() - args.DestX1() + 1;
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t *dest = (uint32_t*)RenderViewport::Instance()->GetDest(args.DestX1(), args.DestY());

					xfrac -= 1 << (31 - xbits);
					yfrac -= 1 << (31 - ybits);
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * 2;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						{
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						xfrac += xstep;
						yfrac += ystep;
						}
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
						viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						uint32_t xxbits = 32 - xbits;
						uint32_t yybits = 32 - ybits;
						uint32_t xxshift = (32 - xxbits);
						uint32_t yyshift = (32 - yybits);
						uint32_t xxmask = (1 << xxshift) - 1;
						uint32_t yymask = (1 << yyshift) - 1;
						uint32_t x = xfrac >> xxbits;
						uint32_t y = yfrac >> yybits;

						uint32_t p00 = source[((y & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p01 = source[(((y + 1) & yymask) + ((x & xxmask) << yyshift))];
						uint32_t p10 = source[((y & yymask) + (((x + 1) & xxmask) << yyshift))];
						uint32_t p11 = source[(((y + 1) & yymask) + (((x + 1) & xxmask) << yyshift))];

						uint32_t inv_b = (xfrac >> (xxbits - 4)) & 15;
						uint32_t inv_a = (yfrac >> (yybits - 4)) & 15;
						uint32_t a = 16 - inv_a;
						uint32_t b = 16 - inv_b;
						
						uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						ifgcolor[1] = 0;
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

						// Shade
						__m128i material = fgcolor;
						int blue0 = BPART(ifgcolor[0]);
						int green0 = GPART(ifgcolor[0]);
						int red0 = RPART(ifgcolor[0]);
						int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

						int blue1 = BPART(ifgcolor[1]);
						int green1 = GPART(ifgcolor[1]);
						int red1 = RPART(ifgcolor[1]);
						int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

						__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

						fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
						fgcolor = _mm_mullo_epi16(fgcolor, mlight);
						fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
						fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
						__m128i lit = _mm_setzero_si128();
						
						for (int i = 0; i != num_lights; i++)
						{
							__m128 light_x = _mm_set1_ps(lights[i].x);
							__m128 light_y = _mm_set1_ps(lights[i].y);
							__m128 light_z = _mm_set1_ps(lights[i].z);
							__m128 light_radius = _mm_set1_ps(lights[i].radius);
							__m128 m256 = _mm_set1_ps(256.0f);
							
							// L = light-pos
							// dist = sqrt(dot(L, L))
							// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
							__m128 Lyz2 = light_y; // L.y*L.y + L.z*L.z
							__m128 Lx = _mm_sub_ps(light_x, viewpos_x);
							__m128 dist2 = _mm_add_ps(Lyz2, _mm_mul_ps(Lx, Lx));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_z, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_z, _mm_setzero_ps());
							__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, simple_attenuation), _mm_andnot_ps(is_attenuated, point_attenuation)));
							attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1,1,1,1)));

							__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
							light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
							light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1,0,1,0));

							lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
						}
						
						fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
						fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));

						// Blend
						uint32_t alpha0 = APART(ifgcolor[0]);
						uint32_t alpha1 = APART(ifgcolor[1]);
						alpha0 += alpha0 >> 7; // 255->256
						alpha1 += alpha1 >> 7; // 255->256
						uint32_t inv_alpha0 = 256 - alpha0;
						uint32_t inv_alpha1 = 256 - alpha1;
						
						uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
						uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
						uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
						uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;
						
						__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
						__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);
						
						fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
						bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);
						
						__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
						__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
						__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());
						
						__m128i out_lo = _mm_add_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_add_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						dest[offset] = _mm_cvtsi128_si32(outcolor);
					}
				}
				}
			}
		}
		
		FString DebugInfo() override { return "DrawSpanAddClamp32Command"; }
	};

}
