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

<?
OutputDrawers(true, true, false);
OutputDrawers(true, false, false);
OutputDrawers(false, true, false);
OutputDrawers(false, false, false);
OutputDrawers(true, true, true);
OutputDrawers(true, false, true);
OutputDrawers(false, true, true);
OutputDrawers(false, false, true);

function OutputDrawers($isTruecolor, $isColorFill, $isListEntry)
{
	$namePrefix = "";
	if ($isTruecolor == true && $isColorFill == true)
	{
		$namePrefix = "TriFill32";
	}
	else if ($isTruecolor == true)
	{
		$namePrefix = "TriDraw32";
	}
	else if ($isColorFill == true)
	{
		$namePrefix = "TriFill8";
	}
	else
	{
		$namePrefix = "TriDraw8";
	}

	if ($isListEntry)
	{ ?>
std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> ScreenTriangle::<?=$namePrefix?> =
{
<?	}
	
	OutputDrawer($namePrefix."Copy", "opaque", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."AlphaBlend", "masked", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."AddSolid", "translucent", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."Add", "add", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."Sub", "sub", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."RevSub", "revsub", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."Stencil", "stencil", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."Shaded", "shaded", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."TranslateCopy", "opaque", true, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."TranslateAlphaBlend", "masked", true, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."TranslateAdd", "add", true, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."TranslateSub", "sub", true, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."TranslateRevSub", "revsub", true, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."AddSrcColorOneMinusSrcColor", "addsrccolor", false, $isTruecolor, $isColorFill, $isListEntry);
	OutputDrawer($namePrefix."Skycap", "skycap", false, $isTruecolor, $isColorFill, $isListEntry);
	
	if ($isListEntry)
	{ ?>
};

<?	}
}

function OutputDrawer($drawerName, $blendmode, $isTranslated, $isTruecolor, $isColorFill, $isListEntry)
{
	if ($isListEntry)
	{ ?>
	&<?=$drawerName?>,
<?	}
	else
	{
		GenerateDrawer($drawerName, $blendmode, $isTranslated, $isTruecolor, $isColorFill);
	}
}

function GenerateDrawer($drawerName, $blendmode, $isTranslated, $isTruecolor, $isColorFill)
{
	$pixeltype = $isTruecolor ? "uint32_t" : "uint8_t";
?>
static void <?=$drawerName?>(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
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

<?	if ($isTranslated || $blendmode == "shaded")
	{ ?>
	const uint8_t * RESTRICT texPixels = args->texturePixels;
	const <?=$pixeltype?> * RESTRICT translation = (const <?=$pixeltype?> *)args->translation;
<?	}
	else
	{ ?>
	const <?=$pixeltype?> * RESTRICT texPixels = (const <?=$pixeltype?> *)args->texturePixels;
<?	}?>
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	<?=$pixeltype?> * RESTRICT destOrg = (<?=$pixeltype?>*)args->dest;
	int pitch = args->pitch;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = args->uniforms->globvis * (1.0f / 32.0f);
	
	<?=$pixeltype?> color = args->uniforms->color;

	for (int i = 0; i < numSpans; i++)
	{
		const auto &span = fullSpans[i];

		<?=$pixeltype?> *dest = destOrg + span.X + span.Y * pitch;
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
					<?=$pixeltype?> *destptr = dest + x * 8 + ix;
<?					ProcessPixel($blendmode, $isTranslated, $isTruecolor, $isColorFill, $pixeltype); ?>
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

		<?=$pixeltype?> *dest = destOrg + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
<?
	for ($i = 0; $i < 2; $i++)
	{
		$coveragemask = ($i == 0) ? "mask0" : "mask1";
?>
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
				if (<?=$coveragemask?> & (1 << 31))
				{
					<?=$pixeltype?> *destptr = dest + x;
<?					ProcessPixel($blendmode, $isTranslated, $isTruecolor, $isColorFill, $pixeltype); ?>
					*destptr = fg;
				}
				<?=$coveragemask?> <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
		}
<?	} ?>
	}
}
	
<?
}

function ProcessPixel($blendmode, $isTranslated, $isTruecolor, $isColorFill, $pixeltype)
{
	if ($isColorFill || $blendmode == "shaded")
	{ ?>
					<?=$pixeltype?> fg = color;
<?	}
	else
	{ ?>
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					<?=$pixeltype?> fg = texPixels[texelX * texHeight + texelY];
<?	}

	if ($isTranslated)
	{ ?>
					fg = translation[fg];
<?	}

	if ($isTruecolor)
	{ ?>
					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = (r * lightpos) >> 16;
					g = (g * lightpos) >> 16;
					b = (b * lightpos) >> 16;
<?					TruecolorBlend($blendmode); ?>
					fg = 0xff000000 | (r << 16) | (g << 8) | b;
<?
	}
	else
	{ ?>
					int colormapindex = MIN(((256 - (lightpos >> 8)) * 32) >> 8, 31) << 8;
					fg = colormaps[colormapindex + fg];
<?	}
}

function TruecolorBlend($blendmode)
{
		if ($blendmode == "opaque")
		{
		}
		else if ($blendmode == "masked")
		{ ?>
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
<?		}
		else if ($blendmode == "translucent")
		{ ?>

<?		}
		else if ($blendmode == "shaded")
		{ ?>
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
<?		}
		else if ($blendmode == "stencil")
		{ ?>
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
<?		}
		else if ($blendmode == "addsrccolor")
		{ ?>
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
<?		}
		else if ($blendmode == "skycap")
		{ ?>
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
<?		}
		else
		{ ?>
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
<?		}
}

?>