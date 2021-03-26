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

#pragma once

#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_spandrawer.h"

namespace swrenderer
{
	namespace DrawSpan32TModes
	{
		enum class SpanBlendModes { Opaque, Masked, Translucent, AddClamp, SubClamp, RevSubClamp };
		struct OpaqueSpan { static const int Mode = (int)SpanBlendModes::Opaque; };
		struct MaskedSpan { static const int Mode = (int)SpanBlendModes::Masked; };
		struct TranslucentSpan { static const int Mode = (int)SpanBlendModes::Translucent; };
		struct AddClampSpan { static const int Mode = (int)SpanBlendModes::AddClamp; };
		struct SubClampSpan { static const int Mode = (int)SpanBlendModes::SubClamp; };
		struct RevSubClampSpan { static const int Mode = (int)SpanBlendModes::RevSubClamp; };

		enum class FilterModes { Nearest, Linear };
		struct NearestFilter { static const int Mode = (int)FilterModes::Nearest; };
		struct LinearFilter { static const int Mode = (int)FilterModes::Linear; };

		enum class ShadeMode { Simple, Advanced };
		struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
		struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };

		enum class SpanTextureSize { SizeAny, Size64x64 };
		struct TextureSizeAny { static const int Mode = (int)SpanTextureSize::SizeAny; };
		struct TextureSize64x64 { static const int Mode = (int)SpanTextureSize::Size64x64; };
	}

	template<typename BlendT>
	class DrawSpan32T
	{
	public:
		struct TextureData
		{
			uint32_t width;
			uint32_t height;
			uint32_t xone;
			uint32_t yone;
			uint32_t xstep;
			uint32_t ystep;
			uint32_t xfrac;
			uint32_t yfrac;
			const uint32_t *source;
		};

		static void DrawColumn(const SpanDrawerArgs& args)
		{
			using namespace DrawSpan32TModes;

			TextureData texdata;
			texdata.width = args.TextureWidth();
			texdata.height = args.TextureHeight();
			texdata.xstep = args.TextureUStep();
			texdata.ystep = args.TextureVStep();
			texdata.xfrac = args.TextureUPos();
			texdata.yfrac = args.TextureVPos();
			
			texdata.source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (texdata.width <= 2 || texdata.height <= 2)
						break;

					texdata.source += texdata.width * texdata.height;
					texdata.width = MAX<uint32_t>(texdata.width / 2, 1);
					texdata.height = MAX<uint32_t>(texdata.height / 2, 1);
					level--;
				}
			}

			texdata.xone = (0x80000000u / texdata.width) << 1;
			texdata.yone = (0x80000000u / texdata.height) << 1;

			bool is_nearest_filter = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
			bool is_64x64 = texdata.width == 64 && texdata.height == 64;
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
					if (is_64x64)
						Loop<SimpleShade, NearestFilter, TextureSize64x64>(args, texdata, shade_constants);
					else
						Loop<SimpleShade, NearestFilter, TextureSizeAny>(args, texdata, shade_constants);
				}
				else
				{
					if (is_64x64)
						Loop<SimpleShade, LinearFilter, TextureSize64x64>(args, texdata, shade_constants);
					else
						Loop<SimpleShade, LinearFilter, TextureSizeAny>(args, texdata, shade_constants);
				}
			}
			else
			{
				if (is_nearest_filter)
				{
					if (is_64x64)
						Loop<AdvancedShade, NearestFilter, TextureSize64x64>(args, texdata, shade_constants);
					else
						Loop<AdvancedShade, NearestFilter, TextureSizeAny>(args, texdata, shade_constants);
				}
				else
				{
					if (is_64x64)
						Loop<AdvancedShade, LinearFilter, TextureSize64x64>(args, texdata, shade_constants);
					else
						Loop<AdvancedShade, LinearFilter, TextureSizeAny>(args, texdata, shade_constants);
				}
			}
		}

		template<typename ShadeModeT, typename FilterModeT, typename TextureSizeT>
		FORCEINLINE static void VECTORCALL Loop(const SpanDrawerArgs& args, TextureData texdata, ShadeConstants shade_constants)
		{
			using namespace DrawSpan32TModes;

			// Shade constants
			int light = 256 - (args.Light() >> (FRACBITS - 8));
			__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);
			__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);

			__m128i inv_desaturate, shade_fade, shade_light;
			int desaturate;
			if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
			{
				inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
				shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
				shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
				shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
				desaturate = shade_constants.desaturate;
			}
			else
			{
				inv_desaturate = _mm_setzero_si128();
				shade_fade = _mm_setzero_si128();
				shade_fade = _mm_setzero_si128();
				shade_light = _mm_setzero_si128();
				desaturate = 0;
			}

			auto lights = args.dc_lights;
			auto num_lights = args.dc_num_lights;
			float vpx = args.dc_viewpos.X;
			float stepvpx = args.dc_viewpos_step.X;
			__m128 viewpos_x = _mm_setr_ps(vpx, vpx + stepvpx, 0.0f, 0.0f);
			__m128 step_viewpos_x = _mm_set1_ps(stepvpx * 2.0f);

			int count = args.DestX2() - args.DestX1() + 1;
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			uint32_t *dest = (uint32_t*)args.Viewport()->GetDest(args.DestX1(), args.DestY());

			if (FilterModeT::Mode == (int)FilterModes::Linear)
			{
				texdata.xfrac -= texdata.xone / 2;
				texdata.yfrac -= texdata.yone / 2;
			}

			uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
			uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);

			int ssecount = count / 2;
			for (int index = 0; index < ssecount; index++)
			{
				int offset = index * 2;

				__m128i bgcolor;
				if (BlendT::Mode != (int)SpanBlendModes::Opaque)
				{
					bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + offset)), _mm_setzero_si128());
				}
				else
				{
					bgcolor = _mm_setzero_si128();
				}
						
				unsigned int ifgcolor[2];
				ifgcolor[0] = Sample<FilterModeT, TextureSizeT>(texdata.width, texdata.height, texdata.xone, texdata.yone, texdata.xstep, texdata.ystep, texdata.xfrac, texdata.yfrac, texdata.source);
				texdata.xfrac += texdata.xstep;
				texdata.yfrac += texdata.ystep;

				ifgcolor[1] = Sample<FilterModeT, TextureSizeT>(texdata.width, texdata.height, texdata.xone, texdata.yone, texdata.xstep, texdata.ystep, texdata.xfrac, texdata.yfrac, texdata.source);
				texdata.xfrac += texdata.xstep;
				texdata.yfrac += texdata.ystep;

				__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

				fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade, shade_light, lights, num_lights, viewpos_x);
				__m128i outcolor = Blend(fgcolor, bgcolor, srcalpha, destalpha, ifgcolor[0], ifgcolor[1]);

				_mm_storel_epi64((__m128i*)(dest + offset), outcolor);
				viewpos_x = _mm_add_ps(viewpos_x, step_viewpos_x);
			}

			if (ssecount * 2 != count)
			{
				int index = ssecount * 2;
				int offset = index;

				__m128i bgcolor;
				if (BlendT::Mode != (int)SpanBlendModes::Opaque)
				{
					bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
				}
				else
				{
					bgcolor = _mm_setzero_si128();
				}

				// Sample
				unsigned int ifgcolor[2];
				ifgcolor[0] = Sample<FilterModeT, TextureSizeT>(texdata.width, texdata.height, texdata.xone, texdata.yone, texdata.xstep, texdata.ystep, texdata.xfrac, texdata.yfrac, texdata.source);
				ifgcolor[1] = 0;

				__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

				fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade, shade_light, lights, num_lights, viewpos_x);
				__m128i outcolor = Blend(fgcolor, bgcolor, srcalpha, destalpha, ifgcolor[0], ifgcolor[1]);

				dest[offset] = _mm_cvtsi128_si32(outcolor);
			}

		}

		template<typename FilterModeT, typename TextureSizeT>
		FORCEINLINE static unsigned int VECTORCALL Sample(uint32_t width, uint32_t height, uint32_t xone, uint32_t yone, uint32_t xstep, uint32_t ystep, uint32_t xfrac, uint32_t yfrac, const uint32_t *source)
		{
			using namespace DrawSpan32TModes;

			if (FilterModeT::Mode == (int)FilterModes::Nearest && TextureSizeT::Mode == (int)SpanTextureSize::Size64x64)
			{
				int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
				return source[sample_index];
			}
			else if (FilterModeT::Mode == (int)FilterModes::Nearest)
			{
				uint32_t x = ((xfrac >> 16) * width) >> 16;
				uint32_t y = ((yfrac >> 16) * height) >> 16;
				int sample_index = x * height + y;
				return source[sample_index];
			}
			else
			{
				uint32_t p00, p01, p10, p11;
				uint32_t frac_x, frac_y;
				if (TextureSizeT::Mode == (int)SpanTextureSize::Size64x64)
				{
					frac_x = xfrac >> 16 << 6;
					frac_y = yfrac >> 16 << 6;
					uint32_t x0 = frac_x >> 16;
					uint32_t y0 = frac_y >> 16;
					uint32_t x1 = (x0 + 1) & 0x3f;
					uint32_t y1 = (y0 + 1) & 0x3f;
					p00 = source[(y0 + (x0 << 6))];
					p01 = source[(y1 + (x0 << 6))];
					p10 = source[(y0 + (x1 << 6))];
					p11 = source[(y1 + (x1 << 6))];
				}
				else
				{
					frac_x = (xfrac >> 16) * width;
					frac_y = (yfrac >> 16) * height;
					uint32_t x0 = frac_x >> 16;
					uint32_t y0 = frac_y >> 16;
					uint32_t x1 = (((xfrac + xone) >> 16) * width) >> 16;
					uint32_t y1 = (((yfrac + yone) >> 16) * height) >> 16;
					p00 = source[y0 + x0 * height];
					p01 = source[y1 + x0 * height];
					p10 = source[y0 + x1 * height];
					p11 = source[y1 + x1 * height];
				}

				uint32_t inv_b = (frac_x >> 12) & 15;
				uint32_t inv_a = (frac_y >> 12) & 15;
				uint32_t a = 16 - inv_a;
				uint32_t b = 16 - inv_b;

				uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

				return (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
			}
		}

		template<typename ShadeModeT>
		FORCEINLINE static __m128i VECTORCALL Shade(__m128i fgcolor, __m128i mlight, unsigned int ifgcolor0, unsigned int ifgcolor1, int desaturate, __m128i inv_desaturate, __m128i shade_fade, __m128i shade_light, const DrawerLight *lights, int num_lights, __m128 viewpos_x)
		{
			using namespace DrawSpan32TModes;

			__m128i material = fgcolor;
			if (ShadeModeT::Mode == (int)ShadeMode::Simple)
			{
				fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
			}
			else
			{
				int blue0 = BPART(ifgcolor0);
				int green0 = GPART(ifgcolor0);
				int red0 = RPART(ifgcolor0);
				int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

				int blue1 = BPART(ifgcolor1);
				int green1 = GPART(ifgcolor1);
				int red1 = RPART(ifgcolor1);
				int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

				__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

				fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
				fgcolor = _mm_mullo_epi16(fgcolor, mlight);
				fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
				fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);
			}

			return AddLights(material, fgcolor, lights, num_lights, viewpos_x);
		}

		FORCEINLINE static __m128i VECTORCALL AddLights(__m128i material, __m128i fgcolor, const DrawerLight *lights, int num_lights, __m128 viewpos_x)
		{
			using namespace DrawSpan32TModes;

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
				attenuation = _mm_packs_epi32(_mm_shuffle_epi32(attenuation, _MM_SHUFFLE(0, 0, 0, 0)), _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1, 1, 1, 1)));

				__m128i light_color = _mm_cvtsi32_si128(lights[i].color);
				light_color = _mm_unpacklo_epi8(light_color, _mm_setzero_si128());
				light_color = _mm_shuffle_epi32(light_color, _MM_SHUFFLE(1, 0, 1, 0));

				lit = _mm_add_epi16(lit, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenuation), 8));
			}

			lit = _mm_min_epi16(lit, _mm_set1_epi16(256));

			fgcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(material, lit), 8));
			fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));
			return fgcolor;
		}

		FORCEINLINE static __m128i VECTORCALL Blend(__m128i fgcolor, __m128i bgcolor, uint32_t srcalpha, uint32_t destalpha, unsigned int ifgcolor0, unsigned int ifgcolor1)
		{
			using namespace DrawSpan32TModes;

			if (BlendT::Mode == (int)SpanBlendModes::Opaque)
			{
				__m128i outcolor = fgcolor;
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
			else if (BlendT::Mode == (int)SpanBlendModes::Masked)
			{
#if 0 // leaving this in for alpha texture support (todo: fix in texture manager later?)
				__m128i alpha = _mm_shufflelo_epi16(fgcolor, _MM_SHUFFLE(3, 3, 3, 3));
				alpha = _mm_shufflehi_epi16(alpha, _MM_SHUFFLE(3, 3, 3, 3));
				alpha = _mm_add_epi16(alpha, _mm_srli_epi16(alpha, 7)); // 255 -> 256

				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);

				fgcolor = _mm_mullo_epi16(fgcolor, alpha);
				bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
				__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
#endif
				__m128i mask = _mm_cmpeq_epi32(_mm_packus_epi16(fgcolor, _mm_setzero_si128()), _mm_setzero_si128());
				mask = _mm_unpacklo_epi8(mask, _mm_setzero_si128());
				__m128i outcolor = _mm_or_si128(_mm_and_si128(mask, bgcolor), _mm_andnot_si128(mask, fgcolor));
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
			else if (BlendT::Mode == (int)SpanBlendModes::Translucent)
			{
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
				return outcolor;
			}
			else
			{
				uint32_t alpha0 = APART(ifgcolor0);
				uint32_t alpha1 = APART(ifgcolor1);
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

				__m128i out_lo, out_hi;
				if (BlendT::Mode == (int)SpanBlendModes::AddClamp)
				{
					out_lo = _mm_add_epi32(fg_lo, bg_lo);
					out_hi = _mm_add_epi32(fg_hi, bg_hi);
				}
				else if (BlendT::Mode == (int)SpanBlendModes::SubClamp)
				{
					out_lo = _mm_sub_epi32(fg_lo, bg_lo);
					out_hi = _mm_sub_epi32(fg_hi, bg_hi);
				}
				else if (BlendT::Mode == (int)SpanBlendModes::RevSubClamp)
				{
					out_lo = _mm_sub_epi32(bg_lo, fg_lo);
					out_hi = _mm_sub_epi32(bg_hi, fg_hi);
				}

				out_lo = _mm_srai_epi32(out_lo, 8);
				out_hi = _mm_srai_epi32(out_hi, 8);
				__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
		}
	};

	typedef DrawSpan32T<DrawSpan32TModes::OpaqueSpan> DrawSpan32Command;
	typedef DrawSpan32T<DrawSpan32TModes::MaskedSpan> DrawSpanMasked32Command;
	typedef DrawSpan32T<DrawSpan32TModes::TranslucentSpan> DrawSpanTranslucent32Command;
	typedef DrawSpan32T<DrawSpan32TModes::AddClampSpan> DrawSpanAddClamp32Command;
	typedef DrawSpan32T<DrawSpan32TModes::SubClampSpan> DrawSpanSubClamp32Command;
	typedef DrawSpan32T<DrawSpan32TModes::RevSubClampSpan> DrawSpanRevSubClamp32Command;
}
