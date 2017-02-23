/*
**  Drawer commands for walls
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
#include "swrenderer/viewport/r_walldrawer.h"

namespace swrenderer
{
	class DrawWall32Command : public DrawerCommand
	{
	protected:
		WallDrawerArgs args;

	public:
		DrawWall32Command(const WallDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
		
		FString DebugInfo() override { return "DrawWall32Command"; }
	};
	
	class DrawWallMasked32Command : public DrawerCommand
	{
	protected:
		WallDrawerArgs args;

	public:
		DrawWallMasked32Command(const WallDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
		
		FString DebugInfo() override { return "DrawWallMasked32Command"; }
	};
	
	class DrawWallAddClamp32Command : public DrawerCommand
	{
	protected:
		WallDrawerArgs args;

	public:
		DrawWallAddClamp32Command(const WallDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
		
		FString DebugInfo() override { return "DrawWallAddClamp32Command"; }
	};
	
	class DrawWallSubClamp32Command : public DrawerCommand
	{
	protected:
		WallDrawerArgs args;

	public:
		DrawWallSubClamp32Command(const WallDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

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
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(fg_lo, bg_lo);
						__m128i out_hi = _mm_sub_epi32(fg_hi, bg_hi);

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
		
		FString DebugInfo() override { return "DrawWallSubClamp32Command"; }
	};
	
	class DrawWallRevSubClamp32Command : public DrawerCommand
	{
	protected:
		WallDrawerArgs args;

	public:
		DrawWallRevSubClamp32Command(const WallDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
		{
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

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
				const uint32_t *source = (const uint32_t*)args.TexturePixels();
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);
				if (is_nearest_filter)
				{
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
						unsigned int sampleout = source[sample_index];
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

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
					int textureheight = args.TextureHeight();
					uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;
					
					// Shade constants
					int light = 256 - (args.Light() >> (FRACBITS - 8));
					__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
					__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
					__m128i inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
					__m128i shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
					shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
					__m128i shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
					int desaturate = shade_constants.desaturate;

					int count = args.Count();
					int pitch = RenderViewport::Instance()->RenderTarget->GetPitch();
					uint32_t fracstep = args.TextureVStep();
					uint32_t frac = args.TextureVPos();
					uint32_t texturefracx = args.TextureUPos();
					uint32_t *dest = (uint32_t*)args.Dest();
					int dest_y = args.DestY();
					
					auto lights = args.dc_lights;
					auto num_lights = args.dc_num_lights;
					float vpz = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
					float stepvpz = args.dc_viewpos_step.Z * thread->num_cores;
					__m128 viewpos_z = _mm_setr_ps(vpz, vpz + stepvpz, 0.0f, 0.0f);
					__m128 step_viewpos_z = _mm_set1_ps(stepvpz * 2.0f);

					count = thread->count_for_thread(dest_y, count);
					if (count <= 0) return;
					frac += thread->skipped_by_thread(dest_y) * fracstep;
					dest = thread->dest_for_thread(dest_y, pitch, dest);
					fracstep *= thread->num_cores;
					pitch *= thread->num_cores;
					frac -= one / 2;
					uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
					uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
					
					int ssecount = count / 2;
					for (int index = 0; index < ssecount; index++)
					{
						int offset = index * pitch * 2;
						uint32_t desttmp[2];
						desttmp[0] = dest[offset];
						desttmp[1] = dest[offset + pitch];
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[0] = sampleout;
						frac += fracstep;
						}
						{
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

						unsigned int sampleout = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
						ifgcolor[1] = sampleout;
						frac += fracstep;
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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

						out_lo = _mm_srai_epi32(out_lo, 8);
						out_hi = _mm_srai_epi32(out_hi, 8);
						__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
						outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
						outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));

						_mm_storel_epi64((__m128i*)desttmp, outcolor);
						dest[offset] = desttmp[0];
						dest[offset + pitch] = desttmp[1];
						viewpos_z = _mm_add_ps(viewpos_z, step_viewpos_z);
					}
					
					if (ssecount * 2 != count)
					{
						int index = ssecount * 2;
						int offset = index * pitch;
						__m128i bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
						
						// Sample
						unsigned int ifgcolor[2];
						unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
						unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
						unsigned int y0 = frac_y0 >> FRACBITS;
						unsigned int y1 = frac_y1 >> FRACBITS;

						unsigned int p00 = source[y0];
						unsigned int p01 = source[y1];
						unsigned int p10 = source2[y0];
						unsigned int p11 = source2[y1];

						unsigned int inv_b = texturefracx;
						unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
						unsigned int a = 16 - inv_a;
						unsigned int b = 16 - inv_b;
						
						unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
						unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

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
							__m128 Lxy2 = light_x; // L.x*L.x + L.y*L.y
							__m128 Lz = _mm_sub_ps(light_z, viewpos_z);
							__m128 dist2 = _mm_add_ps(Lxy2, _mm_mul_ps(Lz, Lz));
							__m128 rcp_dist = _mm_rsqrt_ps(dist2);
							__m128 dist = _mm_mul_ps(dist2, rcp_dist);
							__m128 distance_attenuation = _mm_sub_ps(m256, _mm_min_ps(_mm_mul_ps(dist, light_radius), m256));

							// The simple light type
							__m128 simple_attenuation = distance_attenuation;

							// The point light type
							// diffuse = dot(N,L) * attenuation
							__m128 point_attenuation = _mm_mul_ps(_mm_mul_ps(light_y, rcp_dist), distance_attenuation);
							
							__m128 is_attenuated = _mm_cmpeq_ps(light_y, _mm_setzero_ps());
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
						
						__m128i out_lo = _mm_sub_epi32(bg_lo, fg_lo);
						__m128i out_hi = _mm_sub_epi32(bg_hi, fg_hi);

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
		
		FString DebugInfo() override { return "DrawWallRevSubClamp32Command"; }
	};
	
}
