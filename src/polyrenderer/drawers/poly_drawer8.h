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
class TriScreenDrawer8
{
public:
	static void Execute(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		int numSpans = thread->NumFullSpans;
		auto fullSpans = thread->FullSpans;
		int numBlocks = thread->NumPartialBlocks;
		auto partialBlocks = thread->PartialBlocks;
		int startX = thread->StartX;
		int startY = thread->StartY;

		auto flags = args->uniforms->flags;
		bool is_fixed_light = (flags & TriUniforms::fixed_light) == TriUniforms::fixed_light;
		uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
		auto colormaps = args->colormaps;
		uint32_t srcalpha = args->uniforms->srcalpha;
		uint32_t destalpha = args->uniforms->destalpha;

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
		uint8_t * RESTRICT destOrg = args->dest;
		int pitch = args->pitch;

		// Light
		uint32_t light = args->uniforms->light;
		float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
		float globVis = args->uniforms->globvis * (1.0f / 32.0f);

		// Sampling stuff
		uint32_t color = args->uniforms->color;
		const uint8_t * RESTRICT translation = args->translation;
		const uint8_t * RESTRICT texPixels = args->texturePixels;
		uint32_t texWidth = args->textureWidth;
		uint32_t texHeight = args->textureHeight;

		for (int i = 0; i < numSpans; i++)
		{
			const auto &span = fullSpans[i];

			uint8_t *dest = destOrg + span.X + span.Y * pitch;
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

					for (int ix = 0; ix < 8; ix++)
					{
						int lightshade = lightpos >> 8;
						uint8_t bgcolor = dest[x * 8 + ix];
						uint8_t fgcolor = Sample(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, color, translation);
						dest[x * 8 + ix] = ShadeAndBlend(fgcolor, bgcolor, lightshade, colormaps, srcalpha, destalpha);
						for (int j = 0; j < TriVertex::NumVarying; j++)
							varyingPos[j] += varyingStep[j];
						lightpos += lightstep;
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

			uint8_t *dest = destOrg + block.X + block.Y * pitch;
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

				for (int x = 0; x < 8; x++)
				{
					if (mask0 & (1 << 31))
					{
						int lightshade = lightpos >> 8;
						uint8_t bgcolor = dest[x];
						uint8_t fgcolor = Sample(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, color, translation);
						dest[x] = ShadeAndBlend(fgcolor, bgcolor, lightshade, colormaps, srcalpha, destalpha);
					}

					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];
					lightpos += lightstep;

					mask0 <<= 1;
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

				for (int x = 0; x < 8; x++)
				{
					if (mask1 & (1 << 31))
					{
						int lightshade = lightpos >> 8;
						uint8_t bgcolor = dest[x];
						uint8_t fgcolor = Sample(varyingPos[0], varyingPos[1], texPixels, texWidth, texHeight, color, translation);
						dest[x] = ShadeAndBlend(fgcolor, bgcolor, lightshade, colormaps, srcalpha, destalpha);
					}

					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];
					lightpos += lightstep;

					mask1 <<= 1;
				}

				blockPosY.W += gradientY.W;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosY.Varying[j] += gradientY.Varying[j];

				dest += pitch;
			}
		}
	}

private:
	FORCEINLINE static unsigned int VECTORCALL Sample(int32_t u, int32_t v, const uint8_t *texPixels, int texWidth, int texHeight, uint32_t color, const uint8_t *translation)
	{
		using namespace TriScreenDrawerModes;

		uint8_t texel;
		if (SamplerT::Mode == (int)Samplers::Shaded || SamplerT::Mode == (int)Samplers::Fill)
		{
			return color;
		}
		else if (SamplerT::Mode == (int)Samplers::Translated)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			return translation[texPixels[texelX * texHeight + texelY]];
		}
		else
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			texel = texPixels[texelX * texHeight + texelY];
		}

		if (SamplerT::Mode == (int)Samplers::Skycap)
		{
			/*
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
			*/
			return texel;
		}
		else
		{
			return texel;
		}
	}

	FORCEINLINE static uint8_t ShadeAndBlend(uint8_t fgcolor, uint8_t bgcolor, uint32_t lightshade, const uint8_t *colormaps, uint32_t srcalpha, uint32_t destalpha)
	{
		using namespace TriScreenDrawerModes;

		lightshade = ((256 - lightshade) * (NUMCOLORMAPS - 1) + (NUMCOLORMAPS - 1) / 2) / 256;
		uint8_t shadedfg = colormaps[lightshade * 256 + fgcolor];

		if (BlendT::Mode == (int)BlendModes::Opaque)
		{
			return shadedfg;
		}
		else if (BlendT::Mode == (int)BlendModes::Masked)
		{
			return (fgcolor != 0) ? shadedfg : bgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddSrcColorOneMinusSrcColor)
		{
			return (fgcolor != 0) ? shadedfg : bgcolor;
		}
		else
		{
			if (BlendT::Mode == (int)BlendModes::AddClamp)
			{
			}
			else if (BlendT::Mode == (int)BlendModes::SubClamp)
			{
			}
			else if (BlendT::Mode == (int)BlendModes::RevSubClamp)
			{
			}
			return (fgcolor != 0) ? shadedfg : bgcolor;
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
