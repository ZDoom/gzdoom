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
#include "r_draw_rgba.h"
#include "r_compiler/llvmdrawers.h"

extern unsigned int dc_tspans[4][MAXHEIGHT];
extern unsigned int *dc_ctspan[4];
extern unsigned int *horizspan[4];

/////////////////////////////////////////////////////////////////////////////

class DrawColumnRt1LLVMCommand : public DrawerCommand
{
protected:
	DrawColumnArgs args;

	WorkerThreadData ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		d.temp = thread->dc_temp_rgba;
		return d;
	}

public:
	DrawColumnRt1LLVMCommand(int hx, int sx, int yl, int yh)
	{
		args.dest = (uint32_t*)dc_destorg + ylookup[yl] + sx;
		args.source = nullptr;
		args.colormap = dc_colormap;
		args.translation = dc_translation;
		args.basecolors = (const uint32_t *)GPalette.BaseColors;
		args.pitch = dc_pitch;
		args.count = yh - yl + 1;
		args.dest_y = yl;
		args.iscale = dc_iscale;
		args.texturefrac = hx;
		args.light = LightBgra::calc_light_multiplier(dc_light);
		args.color = LightBgra::shade_pal_index_simple(dc_color, args.light);
		args.srccolor = dc_srccolor_bgra;
		args.srcalpha = dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = dc_destalpha >> (FRACBITS - 8);
		args.light_red = dc_shade_constants.light_red;
		args.light_green = dc_shade_constants.light_green;
		args.light_blue = dc_shade_constants.light_blue;
		args.light_alpha = dc_shade_constants.light_alpha;
		args.fade_red = dc_shade_constants.fade_red;
		args.fade_green = dc_shade_constants.fade_green;
		args.fade_blue = dc_shade_constants.fade_blue;
		args.fade_alpha = dc_shade_constants.fade_alpha;
		args.desaturate = dc_shade_constants.desaturate;
		args.flags = 0;
		if (dc_shade_constants.simple_shade)
			args.flags |= DrawColumnArgs::simple_shade;
	}

	void Execute(DrawerThread *thread) override
	{
		WorkerThreadData d = ThreadData(thread);
		LLVMDrawers::Instance()->DrawColumnRt1(&args, &d);
	}
};

#define DECLARE_DRAW_COMMAND(name, func, base) \
class name##LLVMCommand : public base \
{ \
public: \
	using base::base; \
	void Execute(DrawerThread *thread) override \
	{ \
		WorkerThreadData d = ThreadData(thread); \
		LLVMDrawers::Instance()->func(&args, &d); \
	} \
};

DECLARE_DRAW_COMMAND(DrawColumnRt1Copy, DrawColumnRt1Copy, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1Add, DrawColumnRt1Add, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1Shaded, DrawColumnRt1Shaded, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1AddClamp, DrawColumnRt1AddClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1SubClamp, DrawColumnRt1SubClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1RevSubClamp, DrawColumnRt1RevSubClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1Translated, DrawColumnRt1Translated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1TlatedAdd, DrawColumnRt1TlatedAdd, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1AddClampTranslated, DrawColumnRt1AddClampTranslated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1SubClampTranslated, DrawColumnRt1SubClampTranslated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt1RevSubClampTranslated, DrawColumnRt1RevSubClampTranslated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4, DrawColumnRt4, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4Copy, DrawColumnRt4Copy, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4Add, DrawColumnRt4Add, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4Shaded, DrawColumnRt4Shaded, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4AddClamp, DrawColumnRt4AddClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4SubClamp, DrawColumnRt4SubClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4RevSubClamp, DrawColumnRt4RevSubClamp, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4Translated, DrawColumnRt4Translated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4TlatedAdd, DrawColumnRt4TlatedAdd, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4AddClampTranslated, DrawColumnRt4AddClampTranslated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4SubClampTranslated, DrawColumnRt4SubClampTranslated, DrawColumnRt1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRt4RevSubClampTranslated, DrawColumnRt4RevSubClampTranslated, DrawColumnRt1LLVMCommand);

/////////////////////////////////////////////////////////////////////////////

class RtInitColsRGBACommand : public DrawerCommand
{
	BYTE * RESTRICT buff;

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

template<typename InputPixelType>
class DrawColumnHorizRGBACommand : public DrawerCommand
{
	int _count;
	fixed_t _iscale;
	fixed_t _texturefrac;
	const InputPixelType * RESTRICT _source;
	int _x;
	int _yl;
	int _yh;

public:
	DrawColumnHorizRGBACommand()
	{
		_count = dc_count;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = (const InputPixelType *)dc_source;
		_x = dc_x;
		_yl = dc_yl;
		_yh = dc_yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = _count;
		uint32_t *dest;
		fixed_t fracstep;
		fixed_t frac;

		if (count <= 0)
			return;

		{
			int x = _x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * _yl];
		}
		fracstep = _iscale;
		frac = _texturefrac;

		const InputPixelType *source = _source;

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
};

class FillColumnHorizRGBACommand : public DrawerCommand
{
	int _x;
	int _yl;
	int _yh;
	int _count;
	int _color;

public:
	FillColumnHorizRGBACommand()
	{
		_x = dc_x;
		_count = dc_count;
		_color = dc_color;
		_yl = dc_yl;
		_yh = dc_yh;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = _count;
		int color = _color;
		uint32_t *dest;

		if (count <= 0)
			return;

		{
			int x = _x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * _yl];
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
void rt_copy1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1CopyLLVMCommand>(hx, sx, yl, yh);
}

// Copies all four spans to the screen starting at sx.
void rt_copy4cols_rgba (int sx, int yl, int yh)
{
	// To do: we could do this with SSE using __m128i
	rt_copy1col_rgba(0, sx, yl, yh);
	rt_copy1col_rgba(1, sx + 1, yl, yh);
	rt_copy1col_rgba(2, sx + 2, yl, yh);
	rt_copy1col_rgba(3, sx + 3, yl, yh);
}

// Maps one span at hx to the screen at sx.
void rt_map1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1LLVMCommand>(hx, sx, yl, yh);
}

// Maps all four spans to the screen starting at sx.
void rt_map4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4LLVMCommand>(0, sx, yl, yh);
}

// Translates one span at hx to the screen at sx.
void rt_tlate1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1TranslatedLLVMCommand>(hx, sx, yl, yh);
}

// Translates all four spans to the screen starting at sx.
void rt_tlate4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4TranslatedLLVMCommand>(0, sx, yl, yh);
}

// Adds one span at hx to the screen at sx without clamping.
void rt_add1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1AddLLVMCommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx without clamping.
void rt_add4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4AddLLVMCommand>(0, sx, yl, yh);
}

// Translates and adds one span at hx to the screen at sx without clamping.
void rt_tlateadd1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampTranslatedLLVMCommand>(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx without clamping.
void rt_tlateadd4cols_rgba(int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampTranslatedLLVMCommand>(0, sx, yl, yh);
}

// Shades one span at hx to the screen at sx.
void rt_shaded1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1ShadedLLVMCommand>(hx, sx, yl, yh);
}

// Shades all four spans to the screen starting at sx.
void rt_shaded4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4ShadedLLVMCommand>(0, sx, yl, yh);
}

// Adds one span at hx to the screen at sx with clamping.
void rt_addclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampLLVMCommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx with clamping.
void rt_addclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampLLVMCommand>(0, sx, yl, yh);
}

// Translates and adds one span at hx to the screen at sx with clamping.
void rt_tlateaddclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampTranslatedLLVMCommand>(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx with clamping.
void rt_tlateaddclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampTranslatedLLVMCommand>(0, sx, yl, yh);
}

// Subtracts one span at hx to the screen at sx with clamping.
void rt_subclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampLLVMCommand>(hx, sx, yl, yh);
}

// Subtracts all four spans to the screen starting at sx with clamping.
void rt_subclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampLLVMCommand>(0, sx, yl, yh);
}

// Translates and subtracts one span at hx to the screen at sx with clamping.
void rt_tlatesubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampTranslatedLLVMCommand>(hx, sx, yl, yh);
}

// Translates and subtracts all four spans to the screen starting at sx with clamping.
void rt_tlatesubclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampTranslatedLLVMCommand>(0, sx, yl, yh);
}

// Subtracts one span at hx from the screen at sx with clamping.
void rt_revsubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampLLVMCommand>(hx, sx, yl, yh);
}

// Subtracts all four spans from the screen starting at sx with clamping.
void rt_revsubclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampLLVMCommand>(0, sx, yl, yh);
}

// Translates and subtracts one span at hx from the screen at sx with clamping.
void rt_tlaterevsubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampTranslatedLLVMCommand>(hx, sx, yl, yh);
}

// Translates and subtracts all four spans from the screen starting at sx with clamping.
void rt_tlaterevsubclamp4cols_rgba (int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<DrawColumnRt4RevSubClampTranslatedLLVMCommand>(0, sx, yl, yh);
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
void R_DrawColumnHoriz_rgba (void)
{
	if (dc_count <= 0)
		return;

	int x = dc_x & 3;
	unsigned int **span = &dc_ctspan[x];
	(*span)[0] = dc_yl;
	(*span)[1] = dc_yh;
	*span += 2;

	if (drawer_needs_pal_input)
		DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand<uint8_t>>();
	else
		DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand<uint32_t>>();
}

// [RH] Just fills a column with a given color
void R_FillColumnHoriz_rgba (void)
{
	if (dc_count <= 0)
		return;

	int x = dc_x & 3;
	unsigned int **span = &dc_ctspan[x];
	(*span)[0] = dc_yl;
	(*span)[1] = dc_yh;
	*span += 2;

	DrawerCommandQueue::QueueCommand<FillColumnHorizRGBACommand>();
}
