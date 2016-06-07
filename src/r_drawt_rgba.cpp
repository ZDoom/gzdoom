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
#ifndef NO_SSE
#include <emmintrin.h>
#endif

extern unsigned int dc_tspans[4][MAXHEIGHT];
extern unsigned int *dc_ctspan[4];
extern unsigned int *horizspan[4];

/////////////////////////////////////////////////////////////////////////////

class RtCopy1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;

public:
	RtCopy1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch, sincr;

		count = thread->count_for_thread(yl, (yh - yl + 1));
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = thread->num_cores * 4;

		if (count & 1) {
			*dest = *source;
			source += sincr;
			dest += pitch;
		}
		if (count & 2) {
			dest[0] = source[0];
			dest[pitch] = source[sincr];
			source += sincr * 2;
			dest += pitch * 2;
		}
		if (!(count >>= 2))
			return;

		do {
			dest[0] = source[0];
			dest[pitch] = source[sincr];
			dest[pitch * 2] = source[sincr * 2];
			dest[pitch * 3] = source[sincr * 3];
			source += sincr * 4;
			dest += pitch * 4;
		} while (--count);
	}
};

class RtMap1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	fixed_t dc_light;
	ShadeConstants dc_shade_constants;
	BYTE *dc_destorg;
	int dc_pitch;

public:
	RtMap1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_light = ::dc_light;
		dc_shade_constants = ::dc_shade_constants;
		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = thread->num_cores * 4;

		if (count & 1) {
			*dest = shade_pal_index(*source, light, shade_constants);
			source += sincr;
			dest += pitch;
		}
		if (!(count >>= 1))
			return;

		do {
			dest[0] = shade_pal_index(source[0], light, shade_constants);
			dest[pitch] = shade_pal_index(source[sincr], light, shade_constants);
			source += sincr * 2;
			dest += pitch * 2;
		} while (--count);
	}
};

class RtMap4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	fixed_t dc_light;
	ShadeConstants dc_shade_constants;
	BYTE *dc_destorg;
	int dc_pitch;

public:
	RtMap4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_light = ::dc_light;
		dc_shade_constants = ::dc_shade_constants;
		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = thread->num_cores * 4;

		if (count & 1) {
			dest[0] = shade_pal_index(source[0], light, shade_constants);
			dest[1] = shade_pal_index(source[1], light, shade_constants);
			dest[2] = shade_pal_index(source[2], light, shade_constants);
			dest[3] = shade_pal_index(source[3], light, shade_constants);
			source += sincr;
			dest += pitch;
		}
		if (!(count >>= 1))
			return;

		do {
			dest[0] = shade_pal_index(source[0], light, shade_constants);
			dest[1] = shade_pal_index(source[1], light, shade_constants);
			dest[2] = shade_pal_index(source[2], light, shade_constants);
			dest[3] = shade_pal_index(source[3], light, shade_constants);
			dest[pitch] = shade_pal_index(source[sincr], light, shade_constants);
			dest[pitch + 1] = shade_pal_index(source[sincr + 1], light, shade_constants);
			dest[pitch + 2] = shade_pal_index(source[sincr + 2], light, shade_constants);
			dest[pitch + 3] = shade_pal_index(source[sincr + 3], light, shade_constants);
			source += sincr * 2;
			dest += pitch * 2;
		} while (--count);
	}
#else
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		ShadeConstants shade_constants = dc_shade_constants;
		uint32_t light = calc_light_multiplier(dc_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = thread->num_cores * 4;

		if (shade_constants.simple_shade)
		{
			SSE_SHADE_SIMPLE_INIT(light);

			if (count & 1) {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE_SIMPLE(fg);
				_mm_storeu_si128((__m128i*)dest, fg);

				source += sincr;
				dest += pitch;
			}
			if (!(count >>= 1))
				return;

			do {
				// shade_pal_index 0-3
				{
					uint32_t p0 = source[0];
					uint32_t p1 = source[1];
					uint32_t p2 = source[2];
					uint32_t p3 = source[3];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					SSE_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);
				}

				// shade_pal_index 4-7 (pitch)
				{
					uint32_t p0 = source[sincr];
					uint32_t p1 = source[sincr + 1];
					uint32_t p2 = source[sincr + 2];
					uint32_t p3 = source[sincr + 3];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					SSE_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)(dest + pitch), fg);
				}

				source += sincr * 2;
				dest += pitch * 2;
			} while (--count);
		}
		else
		{
			SSE_SHADE_INIT(light, shade_constants);

			if (count & 1) {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE(fg, shade_constants);
				_mm_storeu_si128((__m128i*)dest, fg);

				source += sincr;
				dest += pitch;
			}
			if (!(count >>= 1))
				return;

			do {
				// shade_pal_index 0-3
				{
					uint32_t p0 = source[0];
					uint32_t p1 = source[1];
					uint32_t p2 = source[2];
					uint32_t p3 = source[3];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					SSE_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);
				}

				// shade_pal_index 4-7 (pitch)
				{
					uint32_t p0 = source[sincr];
					uint32_t p1 = source[sincr + 1];
					uint32_t p2 = source[sincr + 2];
					uint32_t p3 = source[sincr + 3];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					SSE_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)(dest + pitch), fg);
				}

				source += sincr * 2;
				dest += pitch * 2;
			} while (--count);
		}
	}
#endif
};

class RtTranslate1colRGBACommand : public DrawerCommand
{
	const BYTE *translation;
	int hx;
	int yl;
	int yh;

public:
	RtTranslate1colRGBACommand(const BYTE *translation, int hx, int yl, int yh)
	{
		this->translation = translation;
		this->hx = hx;
		this->yl = yl;
		this->yh = yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = yh - yl + 1;
		uint32_t *source = &thread->dc_temp_rgba[yl*4 + hx];

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
};

class RtTranslate4colsRGBACommand : public DrawerCommand
{
	const BYTE *translation;
	int yl;
	int yh;

public:
	RtTranslate4colsRGBACommand(const BYTE *translation, int yl, int yh)
	{
		this->translation = translation;
		this->yl = yl;
		this->yh = yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = yh - yl + 1;
		uint32_t *source = &thread->dc_temp_rgba[yl*4];
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
};

class RtAdd1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	ShadeConstants dc_shade_constants;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;

public:
	RtAdd1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_shade_constants = ::dc_shade_constants;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			uint32_t fg = shade_pal_index(*source, light, shade_constants);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
			uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
			uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;

			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtAdd4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	ShadeConstants dc_shade_constants;

public:
	RtAdd4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_shade_constants = ::dc_shade_constants;
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = shade_pal_index(source[i], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = fg & 0xff;

				uint32_t bg_red = (dest[i] >> 16) & 0xff;
				uint32_t bg_green = (dest[i] >> 8) & 0xff;
				uint32_t bg_blue = (dest[i]) & 0xff;

				uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
				uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
				uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

				dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
			}

			source += sincr;
			dest += pitch;
		} while (--count);
	}
#else
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		ShadeConstants shade_constants = dc_shade_constants;

		if (shade_constants.simple_shade)
		{
			SSE_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			SSE_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
#endif
};

class RtShaded1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	lighttable_t *dc_colormap;
	BYTE *dc_destorg;
	int dc_pitch;
	int dc_color;
	fixed_t dc_light;

public:
	RtShaded1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_colormap = ::dc_colormap;
		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_color = ::dc_color;
		dc_light = ::dc_light;
	}

	void Execute(DrawerThread *thread) override
	{
		BYTE *colormap;
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		colormap = dc_colormap;
		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t fg = shade_pal_index_simple(dc_color, calc_light_multiplier(dc_light));
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
			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtShaded4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	lighttable_t *dc_colormap;
	int dc_color;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;

public:
	RtShaded4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_colormap = ::dc_colormap;
		dc_color = ::dc_color;
		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		BYTE *colormap;
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		colormap = dc_colormap;
		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t fg = shade_pal_index_simple(dc_color, calc_light_multiplier(dc_light));
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
			source += sincr;
			dest += pitch;
		} while (--count);
	}
#else
	void Execute(DrawerThread *thread) override
	{
		BYTE *colormap;
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		colormap = dc_colormap;
		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		__m128i fg = _mm_unpackhi_epi8(_mm_set1_epi32(shade_pal_index_simple(dc_color, calc_light_multiplier(dc_light))), _mm_setzero_si128());
		__m128i alpha_one = _mm_set1_epi16(64);

		do {
			uint32_t p0 = colormap[source[0]];
			uint32_t p1 = colormap[source[1]];
			uint32_t p2 = colormap[source[2]];
			uint32_t p3 = colormap[source[3]];

			__m128i alpha_hi = _mm_set_epi16(64, p3, p3, p3, 64, p2, p2, p2);
			__m128i alpha_lo = _mm_set_epi16(64, p1, p1, p1, 64, p0, p0, p0);
			__m128i inv_alpha_hi = _mm_subs_epu16(alpha_one, alpha_hi);
			__m128i inv_alpha_lo = _mm_subs_epu16(alpha_one, alpha_lo);

			// unpack bg:
			__m128i bg = _mm_loadu_si128((const __m128i*)dest);
			__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
			__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

			// (fg_red * alpha + bg_red * inv_alpha) / 64:
			__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg, alpha_hi), _mm_mullo_epi16(bg_hi, inv_alpha_hi)), 6);
			__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg, alpha_lo), _mm_mullo_epi16(bg_lo, inv_alpha_lo)), 6);

			__m128i color = _mm_packus_epi16(color_lo, color_hi);
			_mm_storeu_si128((__m128i*)dest, color);

			source += sincr;
			dest += pitch;
		} while (--count);
	}
#endif
};

class RtAddClamp1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	ShadeConstants dc_shade_constants;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;

public:
	RtAddClamp1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_shade_constants = ::dc_shade_constants;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			uint32_t fg = shade_pal_index(*source, light, shade_constants);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
			uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
			uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtAddClamp4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;
	ShadeConstants dc_shade_constants;

public:
	RtAddClamp4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
		dc_shade_constants = ::dc_shade_constants;
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = shade_pal_index(source[i], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = fg & 0xff;

				uint32_t bg_red = (dest[i] >> 16) & 0xff;
				uint32_t bg_green = (dest[i] >> 8) & 0xff;
				uint32_t bg_blue = (dest[i]) & 0xff;

				uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
				uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
				uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

				dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
			}
			source += sincr;
			dest += pitch;
		} while (--count);
	}
#else
	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		ShadeConstants shade_constants = dc_shade_constants;

		if (shade_constants.simple_shade)
		{
			SSE_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			SSE_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				SSE_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
#endif
};

class RtSubClamp1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;
	ShadeConstants dc_shade_constants;

public:
	RtSubClamp1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
		dc_shade_constants = ::dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			uint32_t fg = shade_pal_index(*source, light, shade_constants);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((0x10000 - fg_red * fg_alpha + bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>((0x10000 - fg_green * fg_alpha + bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>((0x10000 - fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtSubClamp4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;
	ShadeConstants dc_shade_constants;

public:
	RtSubClamp4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
		dc_shade_constants = ::dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = shade_pal_index(source[i], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = fg & 0xff;

				uint32_t bg_red = (dest[i] >> 16) & 0xff;
				uint32_t bg_green = (dest[i] >> 8) & 0xff;
				uint32_t bg_blue = (dest[i]) & 0xff;

				uint32_t red = clamp<uint32_t>((0x10000 - fg_red * fg_alpha + bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
				uint32_t green = clamp<uint32_t>((0x10000 - fg_green * fg_alpha + bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
				uint32_t blue = clamp<uint32_t>((0x10000 - fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

				dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
			}

			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtRevSubClamp1colRGBACommand : public DrawerCommand
{
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;
	ShadeConstants dc_shade_constants;

public:
	RtRevSubClamp1colRGBACommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
		dc_shade_constants = ::dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4 + hx] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			uint32_t fg = shade_pal_index(*source, light, shade_constants);
			uint32_t fg_red = (fg >> 16) & 0xff;
			uint32_t fg_green = (fg >> 8) & 0xff;
			uint32_t fg_blue = fg & 0xff;

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((0x10000 + fg_red * fg_alpha - bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>((0x10000 + fg_green * fg_alpha - bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>((0x10000 + fg_blue * fg_alpha - bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtRevSubClamp4colsRGBACommand : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE *dc_destorg;
	int dc_pitch;
	fixed_t dc_light;
	fixed_t dc_srcalpha;
	fixed_t dc_destalpha;
	ShadeConstants dc_shade_constants;

public:
	RtRevSubClamp4colsRGBACommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		dc_destorg = ::dc_destorg;
		dc_pitch = ::dc_pitch;
		dc_light = ::dc_light;
		dc_srcalpha = ::dc_srcalpha;
		dc_destalpha = ::dc_destalpha;
		dc_shade_constants = ::dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, dc_pitch, ylookup[yl] + sx + (uint32_t*)dc_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = dc_pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = calc_light_multiplier(dc_light);
		ShadeConstants shade_constants = dc_shade_constants;

		uint32_t fg_alpha = dc_srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = dc_destalpha >> (FRACBITS - 8);

		do {
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = shade_pal_index(source[i], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = fg & 0xff;

				uint32_t bg_red = (dest[i] >> 16) & 0xff;
				uint32_t bg_green = (dest[i] >> 8) & 0xff;
				uint32_t bg_blue = (dest[i]) & 0xff;

				uint32_t red = clamp<uint32_t>((0x10000 + fg_red * fg_alpha - bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
				uint32_t green = clamp<uint32_t>((0x10000 + fg_green * fg_alpha - bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
				uint32_t blue = clamp<uint32_t>((0x10000 + fg_blue * fg_alpha - bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

				dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
			}

			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class RtInitColsRGBACommand : public DrawerCommand
{
	BYTE *buff;

public:
	RtInitColsRGBACommand(BYTE *buff)
	{
		this->buff = buff;
	}

	void Execute(DrawerThread *thread) override
	{
		thread->dc_temp_rgba = buff == NULL ? thread->dc_temp_rgbabuff_rgba : (uint32_t*)buff;
	}
};

class DrawColumnHorizRGBACommand : public DrawerCommand
{
	int dc_count;
	fixed_t dc_iscale;
	fixed_t dc_texturefrac;
	const BYTE *dc_source;
	int dc_x;
	int dc_yl;
	int dc_yh;

public:
	DrawColumnHorizRGBACommand()
	{
		dc_count = ::dc_count;
		dc_iscale = ::dc_iscale;
		dc_texturefrac = ::dc_texturefrac;
		dc_source = ::dc_source;
		dc_x = ::dc_x;
		dc_yl = ::dc_yl;
		dc_yh = ::dc_yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = dc_count;
		uint32_t *dest;
		fixed_t fracstep;
		fixed_t frac;

		if (count <= 0)
			return;

		{
			int x = dc_x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * dc_yl];
		}
		fracstep = dc_iscale;
		frac = dc_texturefrac;

		{
			const BYTE *source = dc_source;

			if (count & 1) {
				*dest = source[frac >> FRACBITS]; dest += 4; frac += fracstep;
			}
			if (count & 2) {
				dest[0] = source[frac >> FRACBITS]; frac += fracstep;
				dest[4] = source[frac >> FRACBITS]; frac += fracstep;
				dest += 8;
			}
			if (count & 4) {
				dest[0] = source[frac >> FRACBITS]; frac += fracstep;
				dest[4] = source[frac >> FRACBITS]; frac += fracstep;
				dest[8] = source[frac >> FRACBITS]; frac += fracstep;
				dest[12] = source[frac >> FRACBITS]; frac += fracstep;
				dest += 16;
			}
			count >>= 3;
			if (!count) return;

			do
			{
				dest[0] = source[frac >> FRACBITS]; frac += fracstep;
				dest[4] = source[frac >> FRACBITS]; frac += fracstep;
				dest[8] = source[frac >> FRACBITS]; frac += fracstep;
				dest[12] = source[frac >> FRACBITS]; frac += fracstep;
				dest[16] = source[frac >> FRACBITS]; frac += fracstep;
				dest[20] = source[frac >> FRACBITS]; frac += fracstep;
				dest[24] = source[frac >> FRACBITS]; frac += fracstep;
				dest[28] = source[frac >> FRACBITS]; frac += fracstep;
				dest += 32;
			} while (--count);
		}
	}
};

class FillColumnHorizRGBACommand : public DrawerCommand
{
	int dc_x;
	int dc_yl;
	int dc_yh;
	int dc_count;
	int dc_color;

public:
	FillColumnHorizRGBACommand()
	{
		dc_x = ::dc_x;
		dc_count = ::dc_count;
		dc_color = ::dc_color;
		dc_yl = ::dc_yl;
		dc_yh = ::dc_yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = dc_count;
		int color = dc_color;
		uint32_t *dest;

		if (count <= 0)
			return;

		{
			int x = dc_x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * dc_yl];
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
};

/////////////////////////////////////////////////////////////////////////////

// Copies one span at hx to the screen at sx.
void rt_copy1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtCopy1colRGBACommand>(hx, sx, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtMap1colRGBACommand>(hx, sx, yl, yh);
}

// Maps all four spans to the screen starting at sx.
void rt_map4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtMap4colsRGBACommand>(sx, yl, yh);
}

void rt_Translate1col_RGBA_c(const BYTE *translation, int hx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtTranslate1colRGBACommand>(translation, hx, yl, yh);
}

void rt_Translate4cols_RGBA_c(const BYTE *translation, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtTranslate4colsRGBACommand>(translation, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtAdd1colRGBACommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx without clamping.
void rt_add4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtAdd4colsRGBACommand>(sx, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtShaded1colRGBACommand>(hx, sx, yl, yh);
}

// Shades all four spans to the screen starting at sx.
void rt_shaded4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtShaded4colsRGBACommand>(sx, yl, yh);
}

// Adds one span at hx to the screen at sx with clamping.
void rt_addclamp1col_RGBA_c (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtAddClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx with clamping.
void rt_addclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtAddClamp4colsRGBACommand>(sx, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtSubClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Subtracts all four spans to the screen starting at sx with clamping.
void rt_subclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtSubClamp4colsRGBACommand>(sx, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtRevSubClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Subtracts all four spans from the screen starting at sx with clamping.
void rt_revsubclamp4cols_RGBA_c (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtRevSubClamp4colsRGBACommand>(sx, yl, yh);
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
	for (int y = 3; y >= 0; y--)
		horizspan[y] = dc_ctspan[y] = &dc_tspans[y][0];

	DrawerCommandQueue::QueueCommand<RtInitColsRGBACommand>(buff);
}

void rt_span_coverage_rgba(int x, int start, int stop)
{
	unsigned int **tspan = &dc_ctspan[x & 3];
	(*tspan)[0] = start;
	(*tspan)[1] = stop;
	*tspan += 2;
}

// Stretches a column into a temporary buffer which is later
// drawn to the screen along with up to three other columns.
void R_DrawColumnHorizP_RGBA_C (void)
{
	int x = dc_x & 3;
	unsigned int **span = &dc_ctspan[x];
	(*span)[0] = dc_yl;
	(*span)[1] = dc_yh;
	*span += 2;

	DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand>();
}

// [RH] Just fills a column with a given color
void R_FillColumnHorizP_RGBA_C (void)
{
	int x = dc_x & 3;
	unsigned int **span = &dc_ctspan[x];
	(*span)[0] = dc_yl;
	(*span)[1] = dc_yh;
	*span += 2;

	DrawerCommandQueue::QueueCommand<FillColumnHorizRGBACommand>();
}
