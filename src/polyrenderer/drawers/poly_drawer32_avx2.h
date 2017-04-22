/*
**  Projected triangle drawer
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

#include "screen_triangle.h"

#ifdef _MSC_VER
#pragma warning(disable: 4752) // warning C4752 : found Intel(R) Advanced Vector Extensions; consider using /arch:AVX
#endif

namespace TriScreenDrawerModes
{
	template<typename SamplerT, typename FilterModeT>
	FORCEINLINE unsigned int VECTORCALL Sample32_AVX2(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight, uint32_t oneU, uint32_t oneV, uint32_t color, const uint32_t *translation)
	{
		uint32_t texel;
		if (SamplerT::Mode == (int)Samplers::Shaded || SamplerT::Mode == (int)Samplers::Stencil || SamplerT::Mode == (int)Samplers::Fill || SamplerT::Mode == (int)Samplers::Fuzz)
		{
			return color;
		}
		else if (SamplerT::Mode == (int)Samplers::Translated)
		{
			const uint8_t *texpal = (const uint8_t *)texPixels;
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			return translation[texpal[texelX * texHeight + texelY]];
		}
		else if (FilterModeT::Mode == (int)FilterModes::Nearest)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			texel = texPixels[texelX * texHeight + texelY];
		}
		else
		{
			u -= oneU >> 1;
			v -= oneV >> 1;

			unsigned int frac_x0 = (((uint32_t)u << 8) >> FRACBITS) * texWidth;
			unsigned int frac_x1 = ((((uint32_t)u << 8) + oneU) >> FRACBITS) * texWidth;
			unsigned int frac_y0 = (((uint32_t)v << 8) >> FRACBITS) * texHeight;
			unsigned int frac_y1 = ((((uint32_t)v << 8) + oneV) >> FRACBITS) * texHeight;
			unsigned int x0 = frac_x0 >> FRACBITS;
			unsigned int x1 = frac_x1 >> FRACBITS;
			unsigned int y0 = frac_y0 >> FRACBITS;
			unsigned int y1 = frac_y1 >> FRACBITS;

			unsigned int p00 = texPixels[x0 * texHeight + y0];
			unsigned int p01 = texPixels[x0 * texHeight + y1];
			unsigned int p10 = texPixels[x1 * texHeight + y0];
			unsigned int p11 = texPixels[x1 * texHeight + y1];

			unsigned int inv_a = (frac_x1 >> (FRACBITS - 4)) & 15;
			unsigned int inv_b = (frac_y1 >> (FRACBITS - 4)) & 15;
			unsigned int a = 16 - inv_a;
			unsigned int b = 16 - inv_b;

			unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

			texel = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
		}

		if (SamplerT::Mode == (int)Samplers::Skycap)
		{
			int start_fade = 2; // How fast it should fade out

			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			uint32_t r = RPART(texel);
			uint32_t g = GPART(texel);
			uint32_t b = BPART(texel);
			uint32_t fg_a = APART(texel);
			uint32_t bg_red = RPART(color);
			uint32_t bg_green = GPART(color);
			uint32_t bg_blue = BPART(color);
			r = (r * a + bg_red * inv_a + 127) >> 8;
			g = (g * a + bg_green * inv_a + 127) >> 8;
			b = (b * a + bg_blue * inv_a + 127) >> 8;
			return MAKEARGB(fg_a, r, g, b);
		}
		else
		{
			return texel;
		}
	}

	template<typename SamplerT>
	FORCEINLINE unsigned int VECTORCALL SampleShade32_AVX2(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight, int &fuzzpos)
	{
		if (SamplerT::Mode == (int)Samplers::Shaded)
		{
			const uint8_t *texpal = (const uint8_t *)texPixels;
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = texpal[texelX * texHeight + texelY];
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			return sampleshadeout;
		}
		else if (SamplerT::Mode == (int)Samplers::Stencil)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			return sampleshadeout;
		}
		else if (SamplerT::Mode == (int)Samplers::Fuzz)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			sampleshadeout = (sampleshadeout * fuzzcolormap[fuzzpos++]) >> 5;
			if (fuzzpos >= FUZZTABLE) fuzzpos = 0;
			return sampleshadeout;
		}
		else
		{
			return 0;
		}
	}

	template<typename ShadeModeT>
	FORCEINLINE __m256i VECTORCALL Shade32_AVX2(__m256i fgcolor, __m256i mlight, __m256i desaturate, __m256i inv_desaturate, __m256i shade_fade, __m256i shade_light)
	{
		if (ShadeModeT::Mode == (int)ShadeMode::Simple)
		{
			fgcolor = _mm256_srli_epi16(_mm256_mullo_epi16(fgcolor, mlight), 8);
		}
		else if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			__m256i intensity = _mm256_mullo_epi16(fgcolor, _mm256_set_epi16(0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37));
			intensity = _mm256_add_epi16(intensity, _mm256_srli_epi64(intensity, 32));
			intensity = _mm256_add_epi16(intensity, _mm256_srli_epi64(intensity, 16));
			intensity = _mm256_srli_epi16(intensity, 8);
			intensity = _mm256_mullo_epi16(intensity, desaturate);
			intensity = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(intensity, _MM_SHUFFLE(3, 0, 0, 0)), _MM_SHUFFLE(3, 0, 0, 0));

			fgcolor = _mm256_srli_epi16(_mm256_add_epi16(_mm256_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
			fgcolor = _mm256_mullo_epi16(fgcolor, mlight);
			fgcolor = _mm256_srli_epi16(_mm256_add_epi16(shade_fade, fgcolor), 8);
			fgcolor = _mm256_srli_epi16(_mm256_mullo_epi16(fgcolor, shade_light), 8);
		}
		return fgcolor;
	}

	template<typename BlendT>
	FORCEINLINE __m256i VECTORCALL Blend32_AVX2(__m256i fgcolor, __m256i bgcolor, __m256i ifgcolor, __m256i ifgshade, __m256i srcalpha, __m256i destalpha)
	{
		if (BlendT::Mode == (int)BlendModes::Opaque)
		{
			__m256i outcolor = fgcolor;
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Masked)
		{
			__m256i mask = _mm256_cmpeq_epi32(_mm256_packus_epi16(fgcolor, _mm256_setzero_si256()), _mm256_setzero_si256());
			mask = _mm256_unpacklo_epi8(mask, _mm256_setzero_si256());
			__m256i outcolor = _mm256_or_si256(_mm256_and_si256(mask, bgcolor), _mm256_andnot_si256(mask, fgcolor));
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			outcolor = _mm256_or_si256(outcolor, _mm256_set1_epi32(0xff000000));
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddSrcColorOneMinusSrcColor)
		{
			__m256i inv_srccolor = _mm256_sub_epi16(_mm256_set1_epi16(256), _mm256_add_epi16(fgcolor, _mm256_srli_epi16(fgcolor, 7)));
			__m256i outcolor = _mm256_add_epi16(fgcolor, _mm256_srli_epi16(_mm256_mullo_epi16(bgcolor, inv_srccolor), 8));
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Shaded)
		{
			ifgshade = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mul_epu32(ifgshade, srcalpha), _mm256_set1_epi32(128)), 8);
			__m256i alpha = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(ifgshade, _MM_SHUFFLE(0, 0, 0, 0)), _MM_SHUFFLE(0, 0, 0, 0));
			__m256i inv_alpha = _mm256_sub_epi16(_mm256_set1_epi16(256), alpha);

			fgcolor = _mm256_mullo_epi16(fgcolor, alpha);
			bgcolor = _mm256_mullo_epi16(bgcolor, inv_alpha);
			__m256i outcolor = _mm256_srli_epi16(_mm256_add_epi16(fgcolor, bgcolor), 8);
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			outcolor = _mm256_or_si256(outcolor, _mm256_set1_epi32(0xff000000));
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddClampShaded)
		{
			ifgshade = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mul_epu32(ifgshade, srcalpha), _mm256_set1_epi32(128)), 8);
			__m256i alpha = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(ifgshade, _MM_SHUFFLE(0, 0, 0, 0)), _MM_SHUFFLE(0, 0, 0, 0));
			__m256i inv_alpha = _mm256_sub_epi16(_mm256_set1_epi16(256), alpha);

			fgcolor = _mm256_srli_epi16(_mm256_mullo_epi16(fgcolor, alpha), 8);
			__m256i outcolor = _mm256_add_epi16(fgcolor, bgcolor);
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			outcolor = _mm256_or_si256(outcolor, _mm256_set1_epi32(0xff000000));
			return outcolor;
		}
		else
		{
			__m256i alpha = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(ifgcolor, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
			alpha = _mm256_srli_epi16(_mm256_add_epi16(alpha, _mm256_srli_epi16(alpha, 7)), 1); // 255->128
			__m256i inv_alpha = _mm256_sub_epi16(_mm256_set1_epi16(128), alpha);

			__m256i bgalpha = _mm256_srli_epi16(_mm256_add_epi16(_mm256_add_epi16(_mm256_mullo_epi16(destalpha, alpha), _mm256_slli_epi16(inv_alpha, 8)), _mm256_set1_epi32(64)), 7);
			__m256i fgalpha = _mm256_srli_epi16(_mm256_add_epi16(_mm256_mullo_epi16(srcalpha, alpha), _mm256_set1_epi32(64)), 7);

			fgcolor = _mm256_mullo_epi16(fgcolor, fgalpha);
			bgcolor = _mm256_mullo_epi16(bgcolor, bgalpha);

			__m256i fg_lo = _mm256_unpacklo_epi16(fgcolor, _mm256_setzero_si256());
			__m256i bg_lo = _mm256_unpacklo_epi16(bgcolor, _mm256_setzero_si256());
			__m256i fg_hi = _mm256_unpackhi_epi16(fgcolor, _mm256_setzero_si256());
			__m256i bg_hi = _mm256_unpackhi_epi16(bgcolor, _mm256_setzero_si256());

			__m256i out_lo, out_hi;
			if (BlendT::Mode == (int)BlendModes::AddClamp)
			{
				out_lo = _mm256_add_epi32(fg_lo, bg_lo);
				out_hi = _mm256_add_epi32(fg_hi, bg_hi);
			}
			else if (BlendT::Mode == (int)BlendModes::SubClamp)
			{
				out_lo = _mm256_sub_epi32(fg_lo, bg_lo);
				out_hi = _mm256_sub_epi32(fg_hi, bg_hi);
			}
			else if (BlendT::Mode == (int)BlendModes::RevSubClamp)
			{
				out_lo = _mm256_sub_epi32(bg_lo, fg_lo);
				out_hi = _mm256_sub_epi32(bg_hi, fg_hi);
			}

			out_lo = _mm256_srai_epi32(out_lo, 8);
			out_hi = _mm256_srai_epi32(out_hi, 8);
			__m256i outcolor = _mm256_packs_epi32(out_lo, out_hi);
			outcolor = _mm256_packus_epi16(outcolor, _mm256_setzero_si256());
			outcolor = _mm256_or_si256(outcolor, _mm256_set1_epi32(0xff000000));
			return outcolor;
		}
	}
}

template<typename BlendT, typename SamplerT>
class TriScreenDrawer32_AVX2
{
public:
	static void Execute(int x, int y, uint32_t mask0, uint32_t mask1, const TriDrawTriangleArgs *args)
	{
		using namespace TriScreenDrawerModes;

		bool is_simple_shade = args->uniforms->SimpleShade();

		if (SamplerT::Mode == (int)Samplers::Texture)
		{
			bool is_nearest_filter = args->uniforms->NearestFilter();

			if (is_simple_shade)
			{
				if (is_nearest_filter)
					DrawBlock<SimpleShade, NearestFilter>(x, y, mask0, mask1, args);
				else
					DrawBlock<SimpleShade, LinearFilter>(x, y, mask0, mask1, args);
			}
			else
			{
				if (is_nearest_filter)
					DrawBlock<AdvancedShade, NearestFilter>(x, y, mask0, mask1, args);
				else
					DrawBlock<AdvancedShade, LinearFilter>(x, y, mask0, mask1, args);
			}
		}
		else if (SamplerT::Mode == (int)Samplers::Fuzz)
		{
			DrawBlock<NoShade, NearestFilter>(x, y, mask0, mask1, args);
		}
		else // no linear filtering for translated, shaded, stencil, fill or skycap
		{
			if (is_simple_shade)
			{
				DrawBlock<SimpleShade, NearestFilter>(x, y, mask0, mask1, args);
			}
			else
			{
				DrawBlock<AdvancedShade, NearestFilter>(x, y, mask0, mask1, args);
			}
		}
	}

private:
	template<typename ShadeModeT, typename FilterModeT>
	FORCEINLINE static void VECTORCALL DrawBlock(int destX, int destY, uint32_t mask0, uint32_t mask1, const TriDrawTriangleArgs *args)
	{
		using namespace TriScreenDrawerModes;

		bool is_fixed_light = args->uniforms->FixedLight();
		__m128i lightmask = _mm_set1_epi32(is_fixed_light ? 0 : 0xffffffff);
		__m256i srcalpha = _mm256_set1_epi16(args->uniforms->SrcAlpha());
		__m256i destalpha = _mm256_set1_epi16(args->uniforms->DestAlpha());

		int fuzzpos = (ScreenTriangle::FuzzStart + destX * 123 + destY) % FUZZTABLE;

		// Light
		uint32_t light = args->uniforms->Light();
		float shade = MIN(2.0f - (light + 12.0f) / 128.0f, 31.0f / 32.0f);
		float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
		light += (light >> 7); // 255 -> 256
		light <<= 8;
		__m128i fixedlight = _mm_set1_epi32(light);

		// Calculate gradients
		const TriVertex &v1 = *args->v1;
		__m128 gradientX = _mm_setr_ps(args->gradientX.W, args->gradientX.U, args->gradientX.V, 0.0f);
		__m128 gradientY = _mm_setr_ps(args->gradientY.W, args->gradientY.U, args->gradientY.V, 0.0f);
		__m128 blockPosY = _mm_add_ps(_mm_add_ps(
			_mm_setr_ps(v1.w, v1.u * v1.w, v1.v * v1.w, globVis),
			_mm_mul_ps(gradientX, _mm_set1_ps(destX - v1.x))),
			_mm_mul_ps(gradientY, _mm_set1_ps(destY - v1.y)));
		gradientX = _mm_mul_ps(gradientX, _mm_set1_ps(8.0f));

		// Output
		uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
		int pitch = args->pitch;
		uint32_t *dest = destOrg + destX + destY * pitch;
		int offset_next_line = pitch - 8;

		// Sampling stuff
		uint32_t color = args->uniforms->Color();
		const uint32_t * RESTRICT translation = (const uint32_t *)args->uniforms->Translation();
		const uint32_t * RESTRICT texPixels = (const uint32_t *)args->uniforms->TexturePixels();
		uint32_t texWidth = args->uniforms->TextureWidth();
		uint32_t texHeight = args->uniforms->TextureHeight();
		uint32_t oneU, oneV;
		if (SamplerT::Mode != (int)Samplers::Fill)
		{
			oneU = ((0x800000 + texWidth - 1) / texWidth) * 2 + 1;
			oneV = ((0x800000 + texHeight - 1) / texHeight) * 2 + 1;
		}
		else
		{
			oneU = 0;
			oneV = 0;
		}

		// Shade constants
		__m256i inv_desaturate, shade_fade, shade_light;
		__m256i desaturate;
		if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			inv_desaturate = _mm256_setr_epi16(
				256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(),
				256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(),
				256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(),
				256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate());
			shade_fade = _mm256_set_epi16(
				args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue(),
				args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue(),
				args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue(),
				args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue());
			shade_light = _mm256_set_epi16(
				args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue(),
				args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue(),
				args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue(),
				args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue());
			desaturate = _mm256_sub_epi16(_mm256_set1_epi16(256), inv_desaturate);
		}
		else
		{
			inv_desaturate = _mm256_setzero_si256();
			shade_fade = _mm256_setzero_si256();
			shade_fade = _mm256_setzero_si256();
			shade_light = _mm256_setzero_si256();
			desaturate = _mm256_setzero_si256();
		}

		if (mask0 == 0xffffffff && mask1 == 0xffffffff)
		{
			for (int y = 0; y < 8; y++)
			{
				__m128 blockPosX = _mm_add_ps(blockPosY, gradientX);
				__m128 W = _mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 rcpW = _mm_div_ps(_mm_set1_ps((float)0x01000000), W);
				__m128i posUV = _mm_cvtps_epi32(_mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(2, 1, 2, 1)), rcpW));

				__m128 vis = _mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(3, 3, 3, 3)), W);
				__m128i lightpospair = _mm_sub_epi32(
					_mm_set1_epi32(FRACUNIT),
					_mm_cvtps_epi32(_mm_mul_ps(
						_mm_max_ps(_mm_sub_ps(_mm_set1_ps(shade), _mm_min_ps(_mm_set1_ps(24.0f / 32.0f), vis)), _mm_setzero_ps()),
						_mm_set1_ps((float)FRACUNIT))));
				lightpospair = _mm_or_si128(_mm_and_si128(lightmask, lightpospair), _mm_andnot_si128(lightmask, fixedlight));

				int32_t posU = _mm_cvtsi128_si32(posUV);
				int32_t posV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 4));
				int32_t nextU = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 8));
				int32_t nextV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 12));
				int32_t lightpos = _mm_cvtsi128_si32(lightpospair);
				int32_t lightnext = _mm_cvtsi128_si32(_mm_srli_si128(lightpospair, 8));
				int32_t stepU = (nextU - posU) >> 3;
				int32_t stepV = (nextV - posV) >> 3;
				fixed_t lightstep = (lightnext - lightpos) >> 3;

				for (int ix = 0; ix < 2; ix++)
				{
					// Load bgcolor
					__m256i bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
					{
						__m128i bgpacked = _mm_loadu_si128((__m128i*)dest);
						bgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(bgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(bgpacked, _mm_setzero_si128()));
					}
					else
						bgcolor = _mm256_setzero_si256();

					// Sample fgcolor
					unsigned int ifgcolor0 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade0 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor1 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade1 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor2 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade2 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor3 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade3 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos1 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos2 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos3 = lightpos >> 8;
					lightpos += lightstep;
					__m256i mlight = _mm256_set_epi16(
						256, lightpos3, lightpos3, lightpos3,
						256, lightpos2, lightpos2, lightpos2,
						256, lightpos1, lightpos1, lightpos1,
						256, lightpos0, lightpos0, lightpos0);

					__m256i shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						__m256i inv_light = _mm256_sub_epi16(_mm256_set_epi16(0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256), mlight);
						shade_fade_lit = _mm256_mullo_epi16(shade_fade, inv_light);
					}
					else
					{
						shade_fade_lit = _mm256_setzero_si256();
					}

					// Shade and blend
					__m128i fgpacked = _mm_set_epi32(ifgcolor3, ifgcolor2, ifgcolor1, ifgcolor0);
					__m128i shadepacked = _mm_set_epi32(ifgshade3, ifgshade2, ifgshade1, ifgshade0);
					__m256i mifgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(fgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(fgpacked, _mm_setzero_si128()));
					__m256i mifgshade = _mm256_set_m128i(_mm_unpackhi_epi32(shadepacked, shadepacked), _mm_unpacklo_epi32(shadepacked, shadepacked));
					__m256i fgcolor = mifgcolor;
					fgcolor = Shade32_AVX2<ShadeModeT>(fgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light);
					__m256i outcolor = Blend32_AVX2<BlendT>(fgcolor, bgcolor, mifgcolor, mifgshade, srcalpha, destalpha);

					// Store result
					_mm_storeu_si128((__m128i*)dest, _mm_or_si128(_mm256_extracti128_si256(outcolor, 0), _mm_slli_si128(_mm256_extracti128_si256(outcolor, 1), 8)));
					dest += 4;
				}

				blockPosY = _mm_add_ps(blockPosY, gradientY);

				dest += offset_next_line;
			}
		}
		else
		{
			// mask0 loop:
			for (int y = 0; y < 4; y++)
			{
				__m128 blockPosX = _mm_add_ps(blockPosY, gradientX);
				__m128 W = _mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 rcpW = _mm_div_ps(_mm_set1_ps((float)0x01000000), W);
				__m128i posUV = _mm_cvtps_epi32(_mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(2, 1, 2, 1)), rcpW));

				__m128 vis = _mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(3, 3, 3, 3)), W);
				__m128i lightpospair = _mm_sub_epi32(
					_mm_set1_epi32(FRACUNIT),
					_mm_cvtps_epi32(_mm_mul_ps(
						_mm_max_ps(_mm_sub_ps(_mm_set1_ps(shade), _mm_min_ps(_mm_set1_ps(24.0f / 32.0f), vis)), _mm_setzero_ps()),
						_mm_set1_ps((float)FRACUNIT))));
				lightpospair = _mm_or_si128(_mm_and_si128(lightmask, lightpospair), _mm_andnot_si128(lightmask, fixedlight));

				int32_t posU = _mm_cvtsi128_si32(posUV);
				int32_t posV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 4));
				int32_t nextU = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 8));
				int32_t nextV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 12));
				int32_t lightpos = _mm_cvtsi128_si32(lightpospair);
				int32_t lightnext = _mm_cvtsi128_si32(_mm_srli_si128(lightpospair, 8));
				int32_t stepU = (nextU - posU) >> 3;
				int32_t stepV = (nextV - posV) >> 3;
				fixed_t lightstep = (lightnext - lightpos) >> 3;

				for (int x = 0; x < 2; x++)
				{
					// Load bgcolor
					uint32_t desttmp[4];
					__m256i bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
					{
						if (mask0 & (1 << 31)) desttmp[0] = dest[0];
						if (mask0 & (1 << 30)) desttmp[1] = dest[1];
						if (mask0 & (1 << 29)) desttmp[2] = dest[2];
						if (mask0 & (1 << 28)) desttmp[3] = dest[3];

						__m128i bgpacked = _mm_loadu_si128((__m128i*)(desttmp));
						bgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(bgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(bgpacked, _mm_setzero_si128()));
					}
					else
						bgcolor = _mm256_setzero_si256();

					// Sample fgcolor
					unsigned int ifgcolor0 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade0 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor1 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade1 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor2 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade2 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor3 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade3 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos1 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos2 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos3 = lightpos >> 8;
					lightpos += lightstep;
					__m256i mlight = _mm256_set_epi16(
						256, lightpos3, lightpos3, lightpos3,
						256, lightpos2, lightpos2, lightpos2,
						256, lightpos1, lightpos1, lightpos1,
						256, lightpos0, lightpos0, lightpos0);

					__m256i shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						__m256i inv_light = _mm256_sub_epi16(_mm256_set_epi16(0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256), mlight);
						shade_fade_lit = _mm256_mullo_epi16(shade_fade, inv_light);
					}
					else
					{
						shade_fade_lit = _mm256_setzero_si256();
					}

					// Shade and blend
					__m128i fgpacked = _mm_set_epi32(ifgcolor3, ifgcolor2, ifgcolor1, ifgcolor0);
					__m128i shadepacked = _mm_set_epi32(ifgshade3, ifgshade2, ifgshade1, ifgshade0);
					__m256i mifgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(fgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(fgpacked, _mm_setzero_si128()));
					__m256i mifgshade = _mm256_set_m128i(_mm_unpackhi_epi32(shadepacked, shadepacked), _mm_unpacklo_epi32(shadepacked, shadepacked));
					__m256i fgcolor = mifgcolor;
					fgcolor = Shade32_AVX2<ShadeModeT>(fgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light);
					__m256i outcolor = Blend32_AVX2<BlendT>(fgcolor, bgcolor, mifgcolor, mifgshade, srcalpha, destalpha);

					// Store result
					_mm_storeu_si128((__m128i*)desttmp, _mm_or_si128(_mm256_extracti128_si256(outcolor, 0), _mm_slli_si128(_mm256_extracti128_si256(outcolor, 1), 8)));
					if (mask0 & (1 << 31)) dest[0] = desttmp[0];
					if (mask0 & (1 << 30)) dest[1] = desttmp[1];
					if (mask0 & (1 << 29)) dest[2] = desttmp[2];
					if (mask0 & (1 << 28)) dest[3] = desttmp[3];

					mask0 <<= 4;
					dest += 4;
				}

				blockPosY = _mm_add_ps(blockPosY, gradientY);

				dest += offset_next_line;
			}

			// mask1 loop:
			for (int y = 0; y < 4; y++)
			{
				__m128 blockPosX = _mm_add_ps(blockPosY, gradientX);
				__m128 W = _mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 rcpW = _mm_div_ps(_mm_set1_ps((float)0x01000000), W);
				__m128i posUV = _mm_cvtps_epi32(_mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(2, 1, 2, 1)), rcpW));

				__m128 vis = _mm_mul_ps(_mm_shuffle_ps(blockPosY, blockPosX, _MM_SHUFFLE(3, 3, 3, 3)), W);
				__m128i lightpospair = _mm_sub_epi32(
					_mm_set1_epi32(FRACUNIT),
					_mm_cvtps_epi32(_mm_mul_ps(
						_mm_max_ps(_mm_sub_ps(_mm_set1_ps(shade), _mm_min_ps(_mm_set1_ps(24.0f / 32.0f), vis)), _mm_setzero_ps()),
						_mm_set1_ps((float)FRACUNIT))));
				lightpospair = _mm_or_si128(_mm_and_si128(lightmask, lightpospair), _mm_andnot_si128(lightmask, fixedlight));

				int32_t posU = _mm_cvtsi128_si32(posUV);
				int32_t posV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 4));
				int32_t nextU = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 8));
				int32_t nextV = _mm_cvtsi128_si32(_mm_srli_si128(posUV, 12));
				int32_t lightpos = _mm_cvtsi128_si32(lightpospair);
				int32_t lightnext = _mm_cvtsi128_si32(_mm_srli_si128(lightpospair, 8));
				int32_t stepU = (nextU - posU) >> 3;
				int32_t stepV = (nextV - posV) >> 3;
				fixed_t lightstep = (lightnext - lightpos) >> 3;

				for (int x = 0; x < 2; x++)
				{
					// Load bgcolor
					uint32_t desttmp[4];
					__m256i bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
					{
						if (mask1 & (1 << 31)) desttmp[0] = dest[0];
						if (mask1 & (1 << 30)) desttmp[1] = dest[1];
						if (mask1 & (1 << 29)) desttmp[2] = dest[2];
						if (mask1 & (1 << 28)) desttmp[3] = dest[3];

						__m128i bgpacked = _mm_loadu_si128((__m128i*)(desttmp));
						bgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(bgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(bgpacked, _mm_setzero_si128()));
					}
					else
						bgcolor = _mm256_setzero_si256();

					// Sample fgcolor
					unsigned int ifgcolor0 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade0 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor1 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade1 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor2 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade2 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					unsigned int ifgcolor3 = Sample32_AVX2<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade3 = SampleShade32_AVX2<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos1 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos2 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos3 = lightpos >> 8;
					lightpos += lightstep;
					__m256i mlight = _mm256_set_epi16(
						256, lightpos3, lightpos3, lightpos3,
						256, lightpos2, lightpos2, lightpos2,
						256, lightpos1, lightpos1, lightpos1,
						256, lightpos0, lightpos0, lightpos0);

					__m256i shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						__m256i inv_light = _mm256_sub_epi16(_mm256_set_epi16(0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256, 0, 256, 256, 256), mlight);
						shade_fade_lit = _mm256_mullo_epi16(shade_fade, inv_light);
					}
					else
					{
						shade_fade_lit = _mm256_setzero_si256();
					}

					// Shade and blend
					__m128i fgpacked = _mm_set_epi32(ifgcolor3, ifgcolor2, ifgcolor1, ifgcolor0);
					__m128i shadepacked = _mm_set_epi32(ifgshade3, ifgshade2, ifgshade1, ifgshade0);
					__m256i mifgcolor = _mm256_set_m128i(_mm_unpackhi_epi8(fgpacked, _mm_setzero_si128()), _mm_unpacklo_epi8(fgpacked, _mm_setzero_si128()));
					__m256i mifgshade = _mm256_set_m128i(_mm_unpackhi_epi32(shadepacked, shadepacked), _mm_unpacklo_epi32(shadepacked, shadepacked));
					__m256i fgcolor = mifgcolor;
					fgcolor = Shade32_AVX2<ShadeModeT>(fgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light);
					__m256i outcolor = Blend32_AVX2<BlendT>(fgcolor, bgcolor, mifgcolor, mifgshade, srcalpha, destalpha);

					// Store result
					_mm_storeu_si128((__m128i*)desttmp, _mm_or_si128(_mm256_extracti128_si256(outcolor, 0), _mm_slli_si128(_mm256_extracti128_si256(outcolor, 1), 8)));
					if (mask1 & (1 << 31)) dest[0] = desttmp[0];
					if (mask1 & (1 << 30)) dest[1] = desttmp[1];
					if (mask1 & (1 << 29)) dest[2] = desttmp[2];
					if (mask1 & (1 << 28)) dest[3] = desttmp[3];

					mask1 <<= 4;
					dest += 4;
				}

				blockPosY = _mm_add_ps(blockPosY, gradientY);

				dest += offset_next_line;
			}
		}
	}
};
