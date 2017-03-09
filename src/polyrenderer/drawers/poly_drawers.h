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

/*
	Warning: this C++ source file has been auto-generated. Please modify the original php script that generated it.
*/

#pragma once

#include "screen_triangle.h"

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

static void TriFill32Copy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32AlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32AddSolid(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32Add(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32Sub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32RevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32Stencil(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32Shaded(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32TranslateCopy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32TranslateAlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32TranslateAdd(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32TranslateSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32TranslateRevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32AddSrcColorOneMinusSrcColor(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill32Skycap(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Copy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32AlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32AddSolid(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;

					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Add(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Sub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32RevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Stencil(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t fgalpha = APART(fg);
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Shaded(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					uint32_t fg = color;
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					int sample = texPixels[texelX * texHeight + texelY];
					
					uint32_t fgalpha = sample;//clamp(sample, 0, 64) * 4;
					uint32_t inv_fgalpha = 256 - fgalpha;
					int a = (fgalpha * srcalpha + 128) >> 8;
					int inv_a = (destalpha * fgalpha + 256 * inv_fgalpha + 128) >> 8;
					
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32TranslateCopy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32TranslateAlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32TranslateAdd(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32TranslateSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32TranslateRevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint32_t * RESTRICT translation = (const uint32_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t a = APART(fg);
					a += a >> 7;
					uint32_t inv_a = 256 - a;
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32AddSrcColorOneMinusSrcColor(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					uint32_t inv_r = 256 - (r + (r >> 7));
					uint32_t inv_g = 256 - (g + (r >> 7));
					uint32_t inv_b = 256 - (b + (r >> 7));
					uint32_t bg = *destptr;
					uint32_t bg_red = RPART(bg);
					uint32_t bg_green = GPART(bg);
					uint32_t bg_blue = BPART(bg);
					r = r + ((bg_red * inv_r + 127) >> 8);
					g = g + ((bg_green * inv_g + 127) >> 8);
					b = b + ((bg_blue * inv_b + 127) >> 8);
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw32Skycap(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint32_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint32_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;

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

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint32_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
					int start_fade = 2; // How fast it should fade out

					int alpha_top = clamp(varyingPos[1] >> (16 - start_fade), 0, 256);
					int alpha_bottom = clamp(((2 << 24) - varyingPos[1]) >> (16 - start_fade), 0, 256);
					int a = MIN(alpha_top, alpha_bottom);
					int inv_a = 256 - a;
					
					uint32_t bg_red = RPART(color);
					uint32_t bg_green = GPART(color);
					uint32_t bg_blue = BPART(color);
					r = (r * a + bg_red * inv_a + 127) >> 8;
					g = (g * a + bg_green * inv_a + 127) >> 8;
					b = (b * a + bg_blue * inv_a + 127) >> 8;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Copy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8AlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8AddSolid(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Add(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Sub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8RevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Stencil(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Shaded(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8TranslateCopy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8TranslateAlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8TranslateAdd(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8TranslateSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8TranslateRevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8AddSrcColorOneMinusSrcColor(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriFill8Skycap(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Copy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8AlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8AddSolid(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Add(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Sub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8RevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Stencil(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Shaded(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					uint8_t fg = color;
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8TranslateCopy(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8TranslateAlphaBlend(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8TranslateAdd(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8TranslateSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8TranslateRevSub(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const uint8_t * RESTRICT translation = (const uint8_t *)args->translation;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					fg = translation[fg];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8AddSrcColorOneMinusSrcColor(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
static void TriDraw8Skycap(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;
	
	auto flags = args->uniforms->flags;
	bool is_simple_shade = (flags & TriUniforms::simple_shade) == TriUniforms::simple_shade;
	bool is_nearest_filter = (flags & TriUniforms::nearest_filter) == TriUniforms::nearest_filter;
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

	const uint8_t * RESTRICT texPixels = (const uint8_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint8_t * RESTRICT destOrg = (uint8_t*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	uint8_t color = args->uniforms->color;

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

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

				int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				int lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				for (int ix = 0; ix < 8; ix++)
				{
					uint8_t *destptr = dest + x * 8 + ix;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;

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
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
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

			int lightnext = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			int lightstep = (lightnext - lightpos) / 8;
			lightstep = lightstep & lightmask;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					uint8_t *destptr = dest + x;
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint8_t fg = texPixels[texelX * texHeight + texelY];
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
					*destptr = fg;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
	}
}
	
std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> ScreenTriangle::TriFill32 =
{
	&TriFill32Copy,
	&TriFill32AlphaBlend,
	&TriFill32AddSolid,
	&TriFill32Add,
	&TriFill32Sub,
	&TriFill32RevSub,
	&TriFill32Stencil,
	&TriFill32Shaded,
	&TriFill32TranslateCopy,
	&TriFill32TranslateAlphaBlend,
	&TriFill32TranslateAdd,
	&TriFill32TranslateSub,
	&TriFill32TranslateRevSub,
	&TriFill32AddSrcColorOneMinusSrcColor,
	&TriFill32Skycap,
};

std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> ScreenTriangle::TriDraw32 =
{
	&TriDraw32Copy,
	&TriDraw32AlphaBlend,
	&TriDraw32AddSolid,
	&TriDraw32Add,
	&TriDraw32Sub,
	&TriDraw32RevSub,
	&TriDraw32Stencil,
	&TriDraw32Shaded,
	&TriDraw32TranslateCopy,
	&TriDraw32TranslateAlphaBlend,
	&TriDraw32TranslateAdd,
	&TriDraw32TranslateSub,
	&TriDraw32TranslateRevSub,
	&TriDraw32AddSrcColorOneMinusSrcColor,
	&TriDraw32Skycap,
};

std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> ScreenTriangle::TriFill8 =
{
	&TriFill8Copy,
	&TriFill8AlphaBlend,
	&TriFill8AddSolid,
	&TriFill8Add,
	&TriFill8Sub,
	&TriFill8RevSub,
	&TriFill8Stencil,
	&TriFill8Shaded,
	&TriFill8TranslateCopy,
	&TriFill8TranslateAlphaBlend,
	&TriFill8TranslateAdd,
	&TriFill8TranslateSub,
	&TriFill8TranslateRevSub,
	&TriFill8AddSrcColorOneMinusSrcColor,
	&TriFill8Skycap,
};

std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> ScreenTriangle::TriDraw8 =
{
	&TriDraw8Copy,
	&TriDraw8AlphaBlend,
	&TriDraw8AddSolid,
	&TriDraw8Add,
	&TriDraw8Sub,
	&TriDraw8RevSub,
	&TriDraw8Stencil,
	&TriDraw8Shaded,
	&TriDraw8TranslateCopy,
	&TriDraw8TranslateAlphaBlend,
	&TriDraw8TranslateAdd,
	&TriDraw8TranslateSub,
	&TriDraw8TranslateRevSub,
	&TriDraw8AddSrcColorOneMinusSrcColor,
	&TriDraw8Skycap,
};

