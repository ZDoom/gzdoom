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

template<typename BlendT, typename SamplerT>
class TriScreenDrawer32
{
public:
	static void Execute(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		bool is_simple_shade = args->uniforms->SimpleShade();

		if (SamplerT::Mode == (int)Samplers::Texture)
		{
			bool is_nearest_filter = args->uniforms->NearestFilter();

			if (is_simple_shade)
			{
				if (is_nearest_filter)
					Loop<SimpleShade, NearestFilter>(args, thread);
				else
					Loop<SimpleShade, LinearFilter>(args, thread);
			}
			else
			{
				if (is_nearest_filter)
					Loop<AdvancedShade, NearestFilter>(args, thread);
				else
					Loop<AdvancedShade, LinearFilter>(args, thread);
			}
		}
		else // no linear filtering for translated, shaded, stencil, fill or skycap
		{
			if (is_simple_shade)
			{
				Loop<SimpleShade, NearestFilter>(args, thread);
			}
			else
			{
				Loop<AdvancedShade, NearestFilter>(args, thread);
			}
		}
	}

	template<typename ShadeModeT, typename FilterModeT>
	FORCEINLINE static void VECTORCALL Loop(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		int numSpans = thread->NumFullSpans;
		auto fullSpans = thread->FullSpans;
		int numBlocks = thread->NumPartialBlocks;
		auto partialBlocks = thread->PartialBlocks;
		int startX = thread->StartX;
		int startY = thread->StartY;

		bool is_fixed_light = args->uniforms->FixedLight();
		uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
		uint32_t srcalpha = args->uniforms->SrcAlpha();
		uint32_t destalpha = args->uniforms->DestAlpha();

		// Calculate gradients
		const TriVertex &v1 = *args->v1;
		const TriVertex &v2 = *args->v2;
		const TriVertex &v3 = *args->v3;
		ScreenTriangleStepVariables gradientX;
		ScreenTriangleStepVariables gradientY;
		ScreenTriangleStepVariables start;
		gradientX.W = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
		gradientY.W = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
		start.W = v1.w + gradientX.W * (startX - v1.x) + gradientY.W * (startY - v1.y);
		for (int i = 0; i < TriVertex::NumVarying; i++)
		{
			gradientX.Varying[i] = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
			gradientY.Varying[i] = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
			start.Varying[i] = v1.varying[i] * v1.w + gradientX.Varying[i] * (startX - v1.x) + gradientY.Varying[i] * (startY - v1.y);
		}

		// Output
		uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
		int pitch = args->pitch;

		// Light
		uint32_t light = args->uniforms->Light();
		float shade = 2.0f - (light + 12.0f) / 128.0f;
		float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
		light += (light >> 7); // 255 -> 256

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
		__m128i inv_desaturate, shade_fade, shade_light;
		int desaturate;
		if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			inv_desaturate = _mm_setr_epi16(256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256, 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate(), 256 - args->uniforms->ShadeDesaturate());
			shade_fade = _mm_set_epi16(args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue(), args->uniforms->ShadeFadeAlpha(), args->uniforms->ShadeFadeRed(), args->uniforms->ShadeFadeGreen(), args->uniforms->ShadeFadeBlue());
			shade_light = _mm_set_epi16(args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue(), args->uniforms->ShadeLightAlpha(), args->uniforms->ShadeLightRed(), args->uniforms->ShadeLightGreen(), args->uniforms->ShadeLightBlue());
			desaturate = args->uniforms->ShadeDesaturate();
		}
		else
		{
			inv_desaturate = _mm_setzero_si128();
			shade_fade = _mm_setzero_si128();
			shade_fade = _mm_setzero_si128();
			shade_light = _mm_setzero_si128();
			desaturate = 0;
		}

		for (int i = 0; i < numSpans; i++)
		{
			const auto &span = fullSpans[i];

			uint32_t *dest = destOrg + span.X + span.Y * pitch;
			int width = span.Length;
			int height = 8;

			ScreenTriangleStepVariables blockPosY;
			blockPosY.W = start.W + gradientX.W * (span.X - startX) + gradientY.W * (span.Y - startY);
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] = start.Varying[j] + gradientX.Varying[j] * (span.X - startX) + gradientY.Varying[j] * (span.Y - startY);

			for (int y = 0; y < height; y++)
			{
				ScreenTriangleStepVariables blockPosX = blockPosY;

				float rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingPos[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

				fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				for (int x = 0; x < width; x++)
				{
					blockPosX.W += gradientX.W * 8;
					for (int j = 0; j < TriVertex::NumVarying; j++)
						blockPosX.Varying[j] += gradientX.Varying[j] * 8;

					rcpW = 0x01000000 / blockPosX.W;
					int32_t varyingStep[TriVertex::NumVarying];
					for (int j = 0; j < TriVertex::NumVarying; j++)
					{
						int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
						varyingStep[j] = (nextPos - varyingPos[j]) / 8;
					}

					fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
					fixed_t lightstep = (lightnext - lightpos) / 8;
					lightstep = lightstep & lightmask;

					for (int ix = 0; ix < 4; ix++)
					{
						// Load bgcolor
						__m128i bgcolor;
						if (BlendT::Mode != (int)BlendModes::Opaque)
							bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(dest + x * 8 + ix * 2)), _mm_setzero_si128());
						else
							bgcolor = _mm_setzero_si128();

						// Sample fgcolor
						unsigned int ifgcolor[2], ifgshade[2];
						ifgcolor[0] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
						ifgshade[0] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
						for (int j = 0; j < TriVertex::NumVarying; j++)
							varyingPos[j] += varyingStep[j];

						ifgcolor[1] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
						ifgshade[1] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
						for (int j = 0; j < TriVertex::NumVarying; j++)
							varyingPos[j] += varyingStep[j];

						// Setup light
						int lightpos0 = lightpos >> 8;
						lightpos += lightstep;
						int lightpos1 = lightpos >> 8;
						lightpos += lightstep;
						__m128i mlight = _mm_set_epi16(256, lightpos1, lightpos1, lightpos1, 256, lightpos0, lightpos0, lightpos0);

						__m128i shade_fade_lit;
						if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
						{
							__m128i inv_light = _mm_sub_epi16(_mm_set_epi16(0, 256, 256, 256, 0, 256, 256, 256), mlight);
							shade_fade_lit = _mm_mullo_epi16(shade_fade, inv_light);
						}
						else
						{
							shade_fade_lit = _mm_setzero_si128();
						}

						// Shade and blend
						__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());
						fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade_lit, shade_light);
						__m128i outcolor = Blend(fgcolor, bgcolor, ifgcolor[0], ifgcolor[1], ifgshade[0], ifgshade[1], srcalpha, destalpha);

						// Store result
						_mm_storel_epi64((__m128i*)(dest + x * 8 + ix * 2), outcolor);
					}
				}

				blockPosY.W += gradientY.W;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosY.Varying[j] += gradientY.Varying[j];

				dest += pitch;
			}
		}

		for (int i = 0; i < numBlocks; i++)
		{
			const auto &block = partialBlocks[i];

			ScreenTriangleStepVariables blockPosY;
			blockPosY.W = start.W + gradientX.W * (block.X - startX) + gradientY.W * (block.Y - startY);
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] = start.Varying[j] + gradientX.Varying[j] * (block.X - startX) + gradientY.Varying[j] * (block.Y - startY);

			uint32_t *dest = destOrg + block.X + block.Y * pitch;
			uint32_t mask0 = block.Mask0;
			uint32_t mask1 = block.Mask1;

			// mask0 loop:
			for (int y = 0; y < 4; y++)
			{
				ScreenTriangleStepVariables blockPosX = blockPosY;

				float rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingPos[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

				fixed_t lightpos = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				blockPosX.W += gradientX.W * 8;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosX.Varying[j] += gradientX.Varying[j] * 8;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingStep[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
				{
					int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
					varyingStep[j] = (nextPos - varyingPos[j]) / 8;
				}

				fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				fixed_t lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int x = 0; x < 4; x++)
				{
					// Load bgcolor
					uint32_t desttmp[2];
					if (mask0 & (1 << 31)) desttmp[0] = dest[x * 2];
					if (mask0 & (1 << 30)) desttmp[1] = dest[x * 2 + 1];

					__m128i bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
						bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
					else
						bgcolor = _mm_setzero_si128();

					// Sample fgcolor
					unsigned int ifgcolor[2], ifgshade[2];
					ifgcolor[0] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					ifgshade[0] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];

					ifgcolor[1] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					ifgshade[1] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos1 = lightpos >> 8;
					lightpos += lightstep;
					__m128i mlight = _mm_set_epi16(256, lightpos1, lightpos1, lightpos1, 256, lightpos0, lightpos0, lightpos0);

					__m128i shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						__m128i inv_light = _mm_sub_epi16(_mm_set_epi16(0, 256, 256, 256, 0, 256, 256, 256), mlight);
						shade_fade_lit = _mm_mullo_epi16(shade_fade, inv_light);
					}
					else
					{
						shade_fade_lit = _mm_setzero_si128();
					}

					// Shade and blend
					__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());
					fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade_lit, shade_light);
					__m128i outcolor = Blend(fgcolor, bgcolor, ifgcolor[0], ifgcolor[1], ifgshade[0], ifgshade[1], srcalpha, destalpha);

					// Store result
					_mm_storel_epi64((__m128i*)desttmp, outcolor);
					if (mask0 & (1 << 31)) dest[x * 2] = desttmp[0];
					if (mask0 & (1 << 30)) dest[x * 2 + 1] = desttmp[1];

					mask0 <<= 2;
				}

				blockPosY.W += gradientY.W;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosY.Varying[j] += gradientY.Varying[j];

				dest += pitch;
			}

			// mask1 loop:
			for (int y = 0; y < 4; y++)
			{
				ScreenTriangleStepVariables blockPosX = blockPosY;

				float rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingPos[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

				fixed_t lightpos = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				blockPosX.W += gradientX.W * 8;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosX.Varying[j] += gradientX.Varying[j] * 8;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingStep[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
				{
					int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
					varyingStep[j] = (nextPos - varyingPos[j]) / 8;
				}

				fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				fixed_t lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int x = 0; x < 4; x++)
				{
					// Load bgcolor
					uint32_t desttmp[2];
					if (mask1 & (1 << 31)) desttmp[0] = dest[x * 2];
					if (mask1 & (1 << 30)) desttmp[1] = dest[x * 2 + 1];

					__m128i bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
						bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
					else
						bgcolor = _mm_setzero_si128();

					// Sample fgcolor
					unsigned int ifgcolor[2], ifgshade[2];
					ifgcolor[0] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					ifgshade[0] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];

					ifgcolor[1] = Sample<FilterModeT>(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					ifgshade[1] = SampleShade(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight);
					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					int lightpos1 = lightpos >> 8;
					lightpos += lightstep;
					__m128i mlight = _mm_set_epi16(256, lightpos1, lightpos1, lightpos1, 256, lightpos0, lightpos0, lightpos0);

					__m128i shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						__m128i inv_light = _mm_sub_epi16(_mm_set_epi16(0, 256, 256, 256, 0, 256, 256, 256), mlight);
						shade_fade_lit = _mm_mullo_epi16(shade_fade, inv_light);
					}
					else
					{
						shade_fade_lit = _mm_setzero_si128();
					}

					// Shade and blend
					__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());
					fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade_lit, shade_light);
					__m128i outcolor = Blend(fgcolor, bgcolor, ifgcolor[0], ifgcolor[1], ifgshade[0], ifgshade[1], srcalpha, destalpha);

					// Store result
					_mm_storel_epi64((__m128i*)desttmp, outcolor);
					if (mask1 & (1 << 31)) dest[x * 2] = desttmp[0];
					if (mask1 & (1 << 30)) dest[x * 2 + 1] = desttmp[1];

					mask1 <<= 2;
				}

				blockPosY.W += gradientY.W;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosY.Varying[j] += gradientY.Varying[j];

				dest += pitch;
			}
		}
	}

private:
	template<typename FilterModeT>
	FORCEINLINE static unsigned int VECTORCALL Sample(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight, uint32_t oneU, uint32_t oneV, uint32_t color, const uint32_t *translation)
	{
		using namespace TriScreenDrawerModes;

		uint32_t texel;
		if (SamplerT::Mode == (int)Samplers::Shaded || SamplerT::Mode == (int)Samplers::Stencil || SamplerT::Mode == (int)Samplers::Fill)
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

	FORCEINLINE static unsigned int VECTORCALL SampleShade(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight)
	{
		using namespace TriScreenDrawerModes;

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
		else
		{
			return 0;
		}
	}

	template<typename ShadeModeT>
	FORCEINLINE static __m128i VECTORCALL Shade(__m128i fgcolor, __m128i mlight, unsigned int ifgcolor0, unsigned int ifgcolor1, int desaturate, __m128i inv_desaturate, __m128i shade_fade, __m128i shade_light)
	{
		using namespace TriScreenDrawerModes;

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
		return fgcolor;
	}

	FORCEINLINE static __m128i VECTORCALL Blend(__m128i fgcolor, __m128i bgcolor, unsigned int ifgcolor0, unsigned int ifgcolor1, unsigned int ifgshade0, unsigned int ifgshade1, uint32_t srcalpha, uint32_t destalpha)
	{
		using namespace TriScreenDrawerModes;

		if (BlendT::Mode == (int)BlendModes::Opaque)
		{
			__m128i outcolor = fgcolor;
			outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Masked)
		{
			__m128i mask = _mm_cmpeq_epi32(_mm_packus_epi16(fgcolor, _mm_setzero_si128()), _mm_setzero_si128());
			mask = _mm_unpacklo_epi8(mask, _mm_setzero_si128());
			__m128i outcolor = _mm_or_si128(_mm_and_si128(mask, bgcolor), _mm_andnot_si128(mask, fgcolor));
			outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
			outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddSrcColorOneMinusSrcColor)
		{
			__m128i inv_srccolor = _mm_sub_epi16(_mm_set1_epi16(256), _mm_add_epi16(fgcolor, _mm_srli_epi16(fgcolor, 7)));
			__m128i outcolor = _mm_add_epi16(fgcolor, _mm_srli_epi16(_mm_mullo_epi16(bgcolor, inv_srccolor), 8));
			outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Shaded)
		{
			ifgshade0 = (ifgshade0 * srcalpha + 128) >> 8;
			ifgshade1 = (ifgshade1 * srcalpha + 128) >> 8;
			__m128i alpha = _mm_set_epi16(ifgshade1, ifgshade1, ifgshade1, ifgshade1, ifgshade0, ifgshade0, ifgshade0, ifgshade0);
			__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);

			fgcolor = _mm_mullo_epi16(fgcolor, alpha);
			bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
			__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
			outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
			outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddClampShaded)
		{
			ifgshade0 = (ifgshade0 * srcalpha + 128) >> 8;
			ifgshade1 = (ifgshade1 * srcalpha + 128) >> 8;
			__m128i alpha = _mm_set_epi16(ifgshade1, ifgshade1, ifgshade1, ifgshade1, ifgshade0, ifgshade0, ifgshade0, ifgshade0);

			fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, alpha), 8);
			__m128i outcolor = _mm_add_epi16(fgcolor, bgcolor);
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
			if (BlendT::Mode == (int)BlendModes::AddClamp)
			{
				out_lo = _mm_add_epi32(fg_lo, bg_lo);
				out_hi = _mm_add_epi32(fg_hi, bg_hi);
			}
			else if (BlendT::Mode == (int)BlendModes::SubClamp)
			{
				out_lo = _mm_sub_epi32(fg_lo, bg_lo);
				out_hi = _mm_sub_epi32(fg_hi, bg_hi);
			}
			else if (BlendT::Mode == (int)BlendModes::RevSubClamp)
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

	static float FindGradientX(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
	{
		float top = (c1 - c2) * (y0 - y2) - (c0 - c2) * (y1 - y2);
		float bottom = (x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2);
		return top / bottom;
	}

	static float FindGradientY(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
	{
		float top = (c1 - c2) * (x0 - x2) - (c0 - c2) * (x1 - x2);
		float bottom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
		return top / bottom;
	}
};
