/*
** r_drawt.cpp
** Faster column drawers for modern processors
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
** These functions stretch columns into a temporary buffer and then
** map them to the screen. On modern machines, this is faster than drawing
** them directly to the screen.
**
** Will I be able to even understand any of this if I come back to it later?
** Let's hope so. :-)
*/

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"
#include "r_draw_pal.h"

// I should have commented this stuff better.
//
// dc_temp is the buffer R_DrawColumnHoriz writes into.
// dc_tspans points into it.
// dc_ctspan points into dc_tspans.
// horizspan also points into dc_tspans.

// dc_ctspan is advanced while drawing into dc_temp.
// horizspan is advanced up to dc_ctspan when drawing from dc_temp to the screen.

namespace swrenderer
{
	RtInitColsPalCommand::RtInitColsPalCommand(uint8_t *buff) : buff(buff)
	{
	}

	void RtInitColsPalCommand::Execute(DrawerThread *thread)
	{
		thread->dc_temp = buff == nullptr ? thread->dc_temp_buff : buff;
	}

	/////////////////////////////////////////////////////////////////////

	PalColumnHorizCommand::PalColumnHorizCommand()
	{
		using namespace drawerargs;
	
		_source = dc_source;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_color = dc_color;
		_x = dc_x;
		_yl = dc_yl;
	}
	
	void DrawColumnHorizPalCommand::Execute(DrawerThread *thread)
	{
		int count = _count;
		uint8_t *dest;
		fixed_t fracstep;
		fixed_t frac;

		count = thread->count_for_thread(_yl, count);
		if (count <= 0)
			return;

		fracstep = _iscale;
		frac = _texturefrac;

		const uint8_t *source = _source;

		int x = _x & 3;
		dest = &thread->dc_temp[x + thread->temp_line_for_thread(_yl) * 4];
		frac += fracstep * thread->skipped_by_thread(_yl);
		fracstep *= thread->num_cores;

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

	void FillColumnHorizPalCommand::Execute(DrawerThread *thread)
	{
		int count = _count;
		uint8_t color = _color;
		uint8_t *dest;

		count = thread->count_for_thread(_yl, count);
		if (count <= 0)
			return;

		int x = _x & 3;
		dest = &thread->dc_temp[x + thread->temp_line_for_thread(_yl) * 4];

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
	
	/////////////////////////////////////////////////////////////////////
	
	PalRtCommand::PalRtCommand(int hx, int sx, int yl, int yh) : hx(hx), sx(sx), yl(yl), yh(yh)
	{
		using namespace drawerargs;
	
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_colormap = dc_colormap;
		_srcblend = dc_srcblend;
		_destblend = dc_destblend;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_translation = dc_translation;
		_color = dc_color;
	}

	void DrawColumnRt1CopyPalCommand::Execute(DrawerThread *thread)
	{
		uint8_t *source;
		uint8_t *dest;
		int count;
		int pitch;

		count = yh - yl + 1;

		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;

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
	
	void DrawColumnRt4CopyPalCommand::Execute(DrawerThread *thread)
	{
		int *source;
		int *dest;
		int count;
		int pitch;

		count = yh - yl + 1;

		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = (int *)(ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg);
		source = (int *)(&thread->dc_temp[thread->temp_line_for_thread(yl)*4]);
		pitch = _pitch*thread->num_cores/sizeof(int);
	
		if (count & 1) {
			*dest = *source;
			source += 4/sizeof(int);
			dest += pitch;
		}
		if (!(count >>= 1))
			return;

		do {
			dest[0] = source[0];
			dest[pitch] = source[4/sizeof(int)];
			source += 8/sizeof(int);
			dest += pitch*2;
		} while (--count);
	}

	void DrawColumnRt1PalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int count;
		int pitch;

		count = yh - yl + 1;

		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		colormap = _colormap;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl) *4 + hx];
		pitch = _pitch*thread->num_cores;

		if (count & 1) {
			*dest = colormap[*source];
			source += 4;
			dest += pitch;
		}
		if (!(count >>= 1))
			return;

		do {
			dest[0] = colormap[source[0]];
			dest[pitch] = colormap[source[4]];
			source += 8;
			dest += pitch*2;
		} while (--count);
	}

	void DrawColumnRt4PalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int count;
		int pitch;

		count = yh - yl + 1;

		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		colormap = _colormap;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch*thread->num_cores;
	
		if (count & 1) {
			dest[0] = colormap[source[0]];
			dest[1] = colormap[source[1]];
			dest[2] = colormap[source[2]];
			dest[3] = colormap[source[3]];
			source += 4;
			dest += pitch;
		}
		if (!(count >>= 1))
			return;

		do {
			dest[0] = colormap[source[0]];
			dest[1] = colormap[source[1]];
			dest[2] = colormap[source[2]];
			dest[3] = colormap[source[3]];
			dest[pitch] = colormap[source[4]];
			dest[pitch+1] = colormap[source[5]];
			dest[pitch+2] = colormap[source[6]];
			dest[pitch+3] = colormap[source[7]];
			source += 8;
			dest += pitch*2;
		} while (--count);
	}

	void DrawColumnRt1TranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		uint8_t *source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		const uint8_t *translation = _translation;

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
			uint8_t b0, b1;

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

	void DrawColumnRt4TranslatedPalCommand::Execute(DrawerThread *thread)
	{
		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		uint8_t *source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		const uint8_t *translation = _translation;
		int c0, c1;
		uint8_t b0, b1;

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

	void DrawColumnRt1AddPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			uint32_t fg = colormap[*source];
			uint32_t bg = *dest;

			int r = MIN((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 63);
			int g = MIN((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 63);
			int b = MIN((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 63);
			*dest = RGB256k.RGB[r][g][b];
			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt4AddPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			for (int ks = 0; ks < 4; ks++)
			{	// [SP] this 4col function was a block of copy-pasted code. 4 times. I regret nothing.
				uint32_t fg = colormap[source[ks]];
				uint32_t bg = dest[ks];
				int r = MIN((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 63);
				int g = MIN((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 63);
				int b = MIN((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 63);
				dest[ks] = RGB256k.RGB[r][g][b];
			}

			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt1ShadedPalCommand::Execute(DrawerThread *thread)
	{
		uint32_t *fgstart;
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		fgstart = &Col2RGB8[0][_color];
		colormap = _colormap;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			uint32_t val = *source;
			int r = (palette[*dest].r * (255-val)) >> 10;
			int g = (palette[*dest].g * (255-val)) >> 10;
			int b = (palette[*dest].b * (255-val)) >> 10;
			*dest = RGB256k.RGB[clamp(r,0,63)][clamp(g,0,63)][clamp(b,0,63)];
			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt4ShadedPalCommand::Execute(DrawerThread *thread)
	{
		uint32_t *fgstart;
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		fgstart = &Col2RGB8[0][_color];
		colormap = _colormap;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch * thread->num_cores;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			uint32_t val;

			for (int ks = 0; ks < 4; ks++)
			{
				val = source[ks];
				int r = (palette[dest[ks]].r * (255-val)) >> 10;
				int g = (palette[dest[ks]].g * (255-val)) >> 10;
				int b = (palette[dest[ks]].b * (255-val)) >> 10;
				dest[ks] = RGB256k.RGB[clamp(r,0,63)][clamp(g,0,63)][clamp(b,0,63)];
			}

			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt1AddClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			int fg = *source;
			int bg = *dest;
			int r = MIN((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 63);
			int g = MIN((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 63);
			int b = MIN((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 63);
			*dest = RGB256k.RGB[r][g][b];
			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt4AddClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			for (int ks = 0; ks < 4; ks++)
			{
				int fg = source[ks];
				int bg = dest[ks];
				int r = MIN((palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 63);
				int g = MIN((palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 63);
				int b = MIN((palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 63);
				dest[ks] = RGB256k.RGB[r][g][b];
			}

			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt1SubClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		const uint32_t *fg2rgb = _srcblend;
		const uint32_t *bg2rgb = _destblend;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			int fg = *source;
			int bg = *dest;
			int r = MAX((palette[fg].r * _srcalpha - palette[bg].r * _destalpha)>>18, 0);
			int g = MAX((palette[fg].g * _srcalpha - palette[bg].g * _destalpha)>>18, 0);
			int b = MAX((palette[fg].b * _srcalpha - palette[bg].b * _destalpha)>>18, 0);
			*dest = RGB256k.RGB[r][g][b];
			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt4SubClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		const uint32_t *fg2rgb = _srcblend;
		const uint32_t *bg2rgb = _destblend;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			for (int ks = 0; ks < 4; ks++)
			{
				int fg = source[ks];
				int bg = dest[ks];
				int r = MAX((palette[fg].r * _srcalpha - palette[bg].r * _destalpha)>>18, 0);
				int g = MAX((palette[fg].g * _srcalpha - palette[bg].g * _destalpha)>>18, 0);
				int b = MAX((palette[fg].b * _srcalpha - palette[bg].b * _destalpha)>>18, 0);
				dest[ks] = RGB256k.RGB[r][g][b];
			}

			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt1RevSubClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		const uint32_t *fg2rgb = _srcblend;
		const uint32_t *bg2rgb = _destblend;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4 + hx];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			int fg = *source;
			int bg = *dest;
			int r = MAX((-palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
			int g = MAX((-palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
			int b = MAX((-palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
			*dest = RGB256k.RGB[r][g][b];
			source += 4;
			dest += pitch;
		} while (--count);
	}

	void DrawColumnRt4RevSubClampPalCommand::Execute(DrawerThread *thread)
	{
		const uint8_t *colormap;
		uint8_t *source;
		uint8_t *dest;
		int pitch;

		int count = yh - yl + 1;
		count = thread->count_for_thread(yl, count);
		if (count <= 0)
			return;

		const uint32_t *fg2rgb = _srcblend;
		const uint32_t *bg2rgb = _destblend;
		dest = ylookup[yl + thread->skipped_by_thread(yl)] + sx + _destorg;
		source = &thread->dc_temp[thread->temp_line_for_thread(yl)*4];
		pitch = _pitch * thread->num_cores;
		colormap = _colormap;
		const PalEntry *palette = GPalette.BaseColors;

		do {
			for (int ks = 0; ks < 4; ks++)
			{
				int fg = source[ks];
				int bg = dest[ks];
				int r = MAX((-palette[fg].r * _srcalpha + palette[bg].r * _destalpha)>>18, 0);
				int g = MAX((-palette[fg].g * _srcalpha + palette[bg].g * _destalpha)>>18, 0);
				int b = MAX((-palette[fg].b * _srcalpha + palette[bg].b * _destalpha)>>18, 0);
				dest[ks] = RGB256k.RGB[r][g][b];
			}

			source += 4;
			dest += pitch;
		} while (--count);
	}
}
