/*
** r_drawt_rgba.cpp
** Faster column drawers for modern processors, true color edition
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** True color versions of the similar functions in r_drawt.cpp
** Please see r_drawt.cpp for a description of the globals used.
*/

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"

uint32_t dc_temp_rgbabuff_rgba[MAXHEIGHT*4];
uint32_t *dc_temp_rgba;

// Defined in r_draw_t.cpp:
extern unsigned int dc_tspans[4][MAXHEIGHT];
extern unsigned int *dc_ctspan[4];
extern unsigned int *horizspan[4];

// Copies one span at hx to the screen at sx.
void rt_copy1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;

	if (count & 1) {
		*dest = *source;
		source += 4;
		dest += pitch;
	}
	if (count & 2) {
		dest[0] = source[0];
		dest[pitch] = source[4];
		source += 8;
		dest += pitch*2;
	}
	if (!(count >>= 2))
		return;

	do {
		dest[0] = source[0];
		dest[pitch] = source[4];
		dest[pitch*2] = source[8];
		dest[pitch*3] = source[12];
		source += 16;
		dest += pitch*4;
	} while (--count);
}

// Copies all four spans to the screen starting at sx.
void rt_copy4cols_RGBA_c (int sx, int yl, int yh)
{
	// To do: we could do this with SSE using __m128i
	rt_copy1col_RGBA_c(0, sx, yl, yh);
	rt_copy1col_RGBA_c(1, sx + 1, yl, yh);
	rt_copy1col_RGBA_c(2, sx + 2, yl, yh);
	rt_copy1col_RGBA_c(3, sx + 3, yl, yh);
}

// Maps one span at hx to the screen at sx.
void rt_map1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	uint32_t light = calc_light_multiplier(dc_light);

	colormap = dc_colormap;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;

	if (count & 1) {
		*dest = shade_pal_index(colormap[*source], light);
		source += 4;
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = shade_pal_index(colormap[source[0]], light);
		dest[pitch] = shade_pal_index(colormap[source[4]], light);
		source += 8;
		dest += pitch*2;
	} while (--count);
}

// Maps all four spans to the screen starting at sx.
void rt_map4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	uint32_t light = calc_light_multiplier(dc_light);

	colormap = dc_colormap;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;
	
	if (count & 1) {
		dest[0] = shade_pal_index(colormap[source[0]], light);
		dest[1] = shade_pal_index(colormap[source[1]], light);
		dest[2] = shade_pal_index(colormap[source[2]], light);
		dest[3] = shade_pal_index(colormap[source[3]], light);
		source += 4;
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = shade_pal_index(colormap[source[0]], light);
		dest[1] = shade_pal_index(colormap[source[1]], light);
		dest[2] = shade_pal_index(colormap[source[2]], light);
		dest[3] = shade_pal_index(colormap[source[3]], light);
		dest[pitch] = shade_pal_index(colormap[source[4]], light);
		dest[pitch + 1] = shade_pal_index(colormap[source[5]], light);
		dest[pitch + 2] = shade_pal_index(colormap[source[6]], light);
		dest[pitch + 3] = shade_pal_index(colormap[source[7]], light);
		source += 8;
		dest += pitch*2;
	} while (--count);
}

void rt_Translate1col_RGBA_c(const BYTE *translation, int hx, int yl, int yh)
{
	int count = yh - yl + 1;
	uint32_t *source = &dc_temp_rgba[yl*4 + hx];

	// Things we do to hit the compiler's optimizer with a clue bat:
	// 1. Parallelism is explicitly spelled out by using a separate
	//    C instruction for each assembly instruction. GCC lets me
	//    have four temporaries, but VC++ spills to the stack with
	//    more than two. Two is probably optimal, anyway.
	// 2. The results of the translation lookups are explicitly
	//    stored in byte-sized variables. This causes the VC++ code
	//    to use byte mov instructions in most cases; for apparently
	//    random reasons, it will use movzx for some places. GCC
	//    ignores this and uses movzx always.

	// Do 8 rows at a time.
	for (int count8 = count >> 3; count8; --count8)
	{
		int c0, c1;
		BYTE b0, b1;

		c0 = source[0];			c1 = source[4];
		b0 = translation[c0];	b1 = translation[c1];
		source[0] = b0;			source[4] = b1;

		c0 = source[8];			c1 = source[12];
		b0 = translation[c0];	b1 = translation[c1];
		source[8] = b0;			source[12] = b1;

		c0 = source[16];		c1 = source[20];
		b0 = translation[c0];	b1 = translation[c1];
		source[16] = b0;		source[20] = b1;

		c0 = source[24];		c1 = source[28];
		b0 = translation[c0];	b1 = translation[c1];
		source[24] = b0;		source[28] = b1;

		source += 32;
	}
	// Finish by doing 1 row at a time.
	for (count &= 7; count; --count, source += 4)
	{
		source[0] = translation[source[0]];
	}
}

void rt_Translate4cols_RGBA_c(const BYTE *translation, int yl, int yh)
{
	int count = yh - yl + 1;
	uint32_t *source = &dc_temp_rgba[yl*4];
	int c0, c1;
	BYTE b0, b1;

	// Do 2 rows at a time.
	for (int count8 = count >> 1; count8; --count8)
	{
		c0 = source[0];			c1 = source[1];
		b0 = translation[c0];	b1 = translation[c1];
		source[0] = b0;			source[1] = b1;

		c0 = source[2];			c1 = source[3];
		b0 = translation[c0];	b1 = translation[c1];
		source[2] = b0;			source[3] = b1;

		c0 = source[4];			c1 = source[5];
		b0 = translation[c0];	b1 = translation[c1];
		source[4] = b0;			source[5] = b1;

		c0 = source[6];			c1 = source[7];
		b0 = translation[c0];	b1 = translation[c1];
		source[6] = b0;			source[7] = b1;

		source += 8;
	}
	// Do the final row if count was odd.
	if (count & 1)
	{
		c0 = source[0];			c1 = source[1];
		b0 = translation[c0];	b1 = translation[c1];
		source[0] = b0;			source[1] = b1;

		c0 = source[2];			c1 = source[3];
		b0 = translation[c0];	b1 = translation[c1];
		source[2] = b0;			source[3] = b1;
	}
}

// Translates one span at hx to the screen at sx.
void rt_tlate1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_RGBA_c(dc_translation, hx, yl, yh);
	rt_map1col(hx, sx, yl, yh);
}

// Translates all four spans to the screen starting at sx.
void rt_tlate4cols_RGBA_c (int sx, int yl, int yh)
{
	rt_Translate4cols_RGBA_c(dc_translation, yl, yh);
	rt_map4cols(sx, yl, yh);
}

// Adds one span at hx to the screen at sx without clamping.
void rt_add1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		uint32_t fg = shade_pal_index(colormap[*source], light);
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t bg_red = (*dest >> 16) & 0xff;
		uint32_t bg_green = (*dest >> 8) & 0xff;
		uint32_t bg_blue = (*dest) & 0xff;

		uint32_t red = clamp<uint32_t>(fg_red + bg_red, 0, 255);
		uint32_t green = clamp<uint32_t>(fg_green + bg_green, 0, 255);
		uint32_t blue = clamp<uint32_t>(fg_blue + bg_blue, 0, 255);

		*dest = 0xff000000 | (red << 16) | (green << 8) | blue;

		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds all four spans to the screen starting at sx without clamping.
void rt_add4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		for (int i = 0; i < 4; i++)
		{
			uint32_t fg = shade_pal_index(colormap[source[i]], light);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (dest[i] >> 16) & 0xff;
			uint32_t bg_green = (dest[i] >> 8) & 0xff;
			uint32_t bg_blue = (dest[i]) & 0xff;

			uint32_t red = clamp<uint32_t>(fg_red + bg_red, 0, 255);
			uint32_t green = clamp<uint32_t>(fg_green + bg_green, 0, 255);
			uint32_t blue = clamp<uint32_t>(fg_blue + bg_blue, 0, 255);

			dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
		}

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds one span at hx to the screen at sx without clamping.
void rt_tlateadd1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_RGBA_c(dc_translation, hx, yl, yh);
	rt_add1col(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx without clamping.
void rt_tlateadd4cols_RGBA_c(int sx, int yl, int yh)
{
	rt_Translate4cols_RGBA_c(dc_translation, yl, yh);
	rt_add4cols(sx, yl, yh);
}

// Shades one span at hx to the screen at sx.
void rt_shaded1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;

	uint32_t fg = shade_pal_index(dc_color, calc_light_multiplier(0));
	uint32_t fg_red = (fg >> 16) & 0xff;
	uint32_t fg_green = (fg >> 8) & 0xff;
	uint32_t fg_blue = fg & 0xff;

	do {
		uint32_t alpha = colormap[*source];
		uint32_t inv_alpha = 64 - alpha;

		uint32_t bg_red = (*dest >> 16) & 0xff;
		uint32_t bg_green = (*dest >> 8) & 0xff;
		uint32_t bg_blue = (*dest) & 0xff;

		uint32_t red = (fg_red * alpha + bg_red * inv_alpha) / 64;
		uint32_t green = (fg_green * alpha + bg_green * inv_alpha) / 64;
		uint32_t blue = (fg_blue * alpha + bg_blue * inv_alpha) / 64;

		*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		source += 4;
		dest += pitch;
	} while (--count);
}

// Shades all four spans to the screen starting at sx.
void rt_shaded4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;

	uint32_t fg = shade_pal_index(dc_color, calc_light_multiplier(0));
	uint32_t fg_red = (fg >> 16) & 0xff;
	uint32_t fg_green = (fg >> 8) & 0xff;
	uint32_t fg_blue = fg & 0xff;

	do {
		for (int i = 0; i < 4; i++)
		{
			uint32_t alpha = colormap[source[i]];
			uint32_t inv_alpha = 64 - alpha;

			uint32_t bg_red = (dest[i] >> 16) & 0xff;
			uint32_t bg_green = (dest[i] >> 8) & 0xff;
			uint32_t bg_blue = (dest[i]) & 0xff;

			uint32_t red = (fg_red * alpha + bg_red * inv_alpha) / 64;
			uint32_t green = (fg_green * alpha + bg_green * inv_alpha) / 64;
			uint32_t blue = (fg_blue * alpha + bg_blue * inv_alpha) / 64;

			dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
		}
		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds one span at hx to the screen at sx with clamping.
void rt_addclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		uint32_t fg = shade_pal_index(colormap[*source], light);
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t bg_red = (*dest >> 16) & 0xff;
		uint32_t bg_green = (*dest >> 8) & 0xff;
		uint32_t bg_blue = (*dest) & 0xff;

		uint32_t red = clamp<uint32_t>(fg_red + bg_red, 0, 255);
		uint32_t green = clamp<uint32_t>(fg_green + bg_green, 0, 255);
		uint32_t blue = clamp<uint32_t>(fg_blue + bg_blue, 0, 255);

		*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds all four spans to the screen starting at sx with clamping.
void rt_addclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		for (int i = 0; i < 4; i++)
		{
			uint32_t fg = shade_pal_index(colormap[source[i]], light);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (dest[i] >> 16) & 0xff;
			uint32_t bg_green = (dest[i] >> 8) & 0xff;
			uint32_t bg_blue = (dest[i]) & 0xff;

			uint32_t red = clamp<uint32_t>(fg_red + bg_red, 0, 255);
			uint32_t green = clamp<uint32_t>(fg_green + bg_green, 0, 255);
			uint32_t blue = clamp<uint32_t>(fg_blue + bg_blue, 0, 255);

			dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
		}
		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds one span at hx to the screen at sx with clamping.
void rt_tlateaddclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_RGBA_c(dc_translation, hx, yl, yh);
	rt_addclamp1col_RGBA_c(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx with clamping.
void rt_tlateaddclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	rt_Translate4cols_RGBA_c(dc_translation, yl, yh);
	rt_addclamp4cols(sx, yl, yh);
}

// Subtracts one span at hx to the screen at sx with clamping.
void rt_subclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		uint32_t fg = shade_pal_index(colormap[*source], light);
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t bg_red = (*dest >> 16) & 0xff;
		uint32_t bg_green = (*dest >> 8) & 0xff;
		uint32_t bg_blue = (*dest) & 0xff;

		uint32_t red = clamp<uint32_t>(256 - fg_red + bg_red, 256, 256 + 255) - 256;
		uint32_t green = clamp<uint32_t>(256 - fg_green + bg_green, 256, 256 + 255) - 256;
		uint32_t blue = clamp<uint32_t>(256 - fg_blue + bg_blue, 256, 256 + 255) - 256;

		*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		source += 4;
		dest += pitch;
	} while (--count);
}

// Subtracts all four spans to the screen starting at sx with clamping.
void rt_subclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		for (int i = 0; i < 4; i++)
		{
			uint32_t fg = shade_pal_index(colormap[source[i]], light);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (dest[i] >> 16) & 0xff;
			uint32_t bg_green = (dest[i] >> 8) & 0xff;
			uint32_t bg_blue = (dest[i]) & 0xff;

			uint32_t red = clamp<uint32_t>(256 - fg_red + bg_red, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>(256 - fg_green + bg_green, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>(256 - fg_blue + bg_blue, 256, 256 + 255) - 256;

			dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
		}

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and subtracts one span at hx to the screen at sx with clamping.
void rt_tlatesubclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_RGBA_c(dc_translation, hx, yl, yh);
	rt_subclamp1col_RGBA_c(hx, sx, yl, yh);
}

// Translates and subtracts all four spans to the screen starting at sx with clamping.
void rt_tlatesubclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	rt_Translate4cols_RGBA_c(dc_translation, yl, yh);
	rt_subclamp4cols_RGBA_c(sx, yl, yh);
}

// Subtracts one span at hx from the screen at sx with clamping.
void rt_revsubclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		uint32_t fg = shade_pal_index(colormap[*source], light);
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t bg_red = (*dest >> 16) & 0xff;
		uint32_t bg_green = (*dest >> 8) & 0xff;
		uint32_t bg_blue = (*dest) & 0xff;

		uint32_t red = clamp<uint32_t>(256 + fg_red - bg_red, 256, 256 + 255) - 256;
		uint32_t green = clamp<uint32_t>(256 + fg_green - bg_green, 256, 256 + 255) - 256;
		uint32_t blue = clamp<uint32_t>(256 + fg_blue - bg_blue, 256, 256 + 255) - 256;

		*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		source += 4;
		dest += pitch;
	} while (--count);
}

// Subtracts all four spans from the screen starting at sx with clamping.
void rt_revsubclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	BYTE *colormap;
	uint32_t *source;
	uint32_t *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx + (uint32_t*)dc_destorg;
	source = &dc_temp_rgba[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	uint32_t light = calc_light_multiplier(dc_light);

	do {
		for (int i = 0; i < 4; i++)
		{
			uint32_t fg = shade_pal_index(colormap[source[i]], light);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (dest[i] >> 16) & 0xff;
			uint32_t bg_green = (dest[i] >> 8) & 0xff;
			uint32_t bg_blue = (dest[i]) & 0xff;

			uint32_t red = clamp<uint32_t>(256 + fg_red - bg_red, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>(256 + fg_green - bg_green, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>(256 + fg_blue - bg_blue, 256, 256 + 255) - 256;

			dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
		}

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and subtracts one span at hx from the screen at sx with clamping.
void rt_tlaterevsubclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_RGBA_c(dc_translation, hx, yl, yh);
	rt_revsubclamp1col_RGBA_c(hx, sx, yl, yh);
}

// Translates and subtracts all four spans from the screen starting at sx with clamping.
void rt_tlaterevsubclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	rt_Translate4cols_RGBA_c(dc_translation, yl, yh);
	rt_revsubclamp4cols_RGBA_c(sx, yl, yh);
}

// Before each pass through a rendering loop that uses these routines,
// call this function to set up the span pointers.
void rt_initcols_rgba (BYTE *buff)
{
	int y;

	dc_temp_rgba = buff == NULL ? dc_temp_rgbabuff_rgba : (uint32_t*)buff;
	for (y = 3; y >= 0; y--)
		horizspan[y] = dc_ctspan[y] = &dc_tspans[y][0];
}

// Stretches a column into a temporary buffer which is later
// drawn to the screen along with up to three other columns.
void R_DrawColumnHorizP_RGBA_C (void)
{
	int count = dc_count;
	uint32_t *dest;
	fixed_t fracstep;
	fixed_t frac;

	if (count <= 0)
		return;

	{
		int x = dc_x & 3;
		unsigned int **span;
		
		span = &dc_ctspan[x];
		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;
		dest = &dc_temp_rgba[x + 4*dc_yl];
	}
	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		const BYTE *source = dc_source;

		if (count & 1) {
			*dest = source[frac>>FRACBITS]; dest += 4; frac += fracstep;
		}
		if (count & 2) {
			dest[0] = source[frac>>FRACBITS]; frac += fracstep;
			dest[4] = source[frac>>FRACBITS]; frac += fracstep;
			dest += 8;
		}
		if (count & 4) {
			dest[0] = source[frac>>FRACBITS]; frac += fracstep;
			dest[4] = source[frac>>FRACBITS]; frac += fracstep;
			dest[8] = source[frac>>FRACBITS]; frac += fracstep;
			dest[12]= source[frac>>FRACBITS]; frac += fracstep;
			dest += 16;
		}
		count >>= 3;
		if (!count) return;

		do
		{
			dest[0] = source[frac>>FRACBITS]; frac += fracstep;
			dest[4] = source[frac>>FRACBITS]; frac += fracstep;
			dest[8] = source[frac>>FRACBITS]; frac += fracstep;
			dest[12]= source[frac>>FRACBITS]; frac += fracstep;
			dest[16]= source[frac>>FRACBITS]; frac += fracstep;
			dest[20]= source[frac>>FRACBITS]; frac += fracstep;
			dest[24]= source[frac>>FRACBITS]; frac += fracstep;
			dest[28]= source[frac>>FRACBITS]; frac += fracstep;
			dest += 32;
		} while (--count);
	}
}

// [RH] Just fills a column with a given color
void R_FillColumnHorizP_RGBA_C (void)
{
	int count = dc_count;
	BYTE color = dc_color;
	uint32_t *dest;

	if (count <= 0)
		return;

	{
		int x = dc_x & 3;
		unsigned int **span = &dc_ctspan[x];

		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;
		dest = &dc_temp_rgba[x + 4*dc_yl];
	}

	if (count & 1) {
		*dest = color;
		dest += 4;
	}
	if (!(count >>= 1))
		return;
	do {
		dest[0] = color; dest[4] = color;
		dest += 8;
	} while (--count);
}
