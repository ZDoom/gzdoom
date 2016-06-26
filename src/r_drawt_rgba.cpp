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
#ifndef NO_SSE
#include <emmintrin.h>
#endif

extern unsigned int dc_tspans[4][MAXHEIGHT];
extern unsigned int *dc_ctspan[4];
extern unsigned int *horizspan[4];

#ifndef NO_SSE

#ifdef _MSC_VER
#pragma warning(disable: 4101) // warning C4101: unreferenced local variable
#endif

// Generate SSE drawers:
#define VecCommand(name) name##_SSE_Command
#define VEC_SHADE_VARS SSE_SHADE_VARS
#define VEC_SHADE_SIMPLE_INIT SSE_SHADE_SIMPLE_INIT
#define VEC_SHADE_SIMPLE_INIT4 SSE_SHADE_SIMPLE_INIT4
#define VEC_SHADE_SIMPLE SSE_SHADE_SIMPLE
#define VEC_SHADE_INIT SSE_SHADE_INIT
#define VEC_SHADE_INIT4 SSE_SHADE_INIT4
#define VEC_SHADE SSE_SHADE
#include "r_drawt_rgba_sse.h"
/*
// Generate AVX drawers:
#undef VecCommand
#undef VEC_SHADE_SIMPLE_INIT
#undef VEC_SHADE_SIMPLE_INIT4
#undef VEC_SHADE_SIMPLE
#undef VEC_SHADE_INIT
#undef VEC_SHADE_INIT4
#undef VEC_SHADE
#define VecCommand(name) name##_AVX_Command
#define VEC_SHADE_SIMPLE_INIT AVX_LINEAR_SHADE_SIMPLE_INIT
#define VEC_SHADE_SIMPLE_INIT4 AVX_LINEAR_SHADE_SIMPLE_INIT4
#define VEC_SHADE_SIMPLE AVX_LINEAR_SHADE_SIMPLE
#define VEC_SHADE_INIT AVX_LINEAR_SHADE_INIT
#define VEC_SHADE_INIT4 AVX_LINEAR_SHADE_INIT4
#define VEC_SHADE AVX_LINEAR_SHADE
#include "r_drawt_rgba_sse.h"
*/
#endif

/////////////////////////////////////////////////////////////////////////////

class DrawerRt1colCommand : public DrawerCommand
{
public:
	int hx;
	int sx;
	int yl;
	int yh;
	BYTE * RESTRICT _destorg;
	int _pitch;

	uint32_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _colormap;

	uint32_t _srcalpha;
	uint32_t _destalpha;

	DrawerRt1colCommand(int hx, int sx, int yl, int yh)
	{
		this->hx = hx;
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_destorg = dc_destorg;
		_pitch = dc_pitch;

		_light = LightBgra::calc_light_multiplier(dc_light);
		_shade_constants = dc_shade_constants;
		_colormap = dc_colormap;

		_srcalpha = dc_srcalpha >> (FRACBITS - 8);
		_destalpha = dc_destalpha >> (FRACBITS - 8);
	}

	class LoopIterator
	{
	public:
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch, sincr;

		LoopIterator(DrawerRt1colCommand *command, DrawerThread *thread)
		{
			count = thread->count_for_thread(command->yl, (command->yh - command->yl + 1));
			if (count <= 0)
				return;

			dest = thread->dest_for_thread(command->yl, command->_pitch, ylookup[command->yl] + command->sx + (uint32_t*)command->_destorg);
			source = &thread->dc_temp_rgba[command->yl * 4 + command->hx] + thread->skipped_by_thread(command->yl) * 4;
			pitch = command->_pitch * thread->num_cores;
			sincr = thread->num_cores * 4;
		}

		explicit operator bool()
		{
			return count > 0;
		}

		bool next()
		{
			dest += pitch;
			source += sincr;
			return (--count) != 0;
		}
	};
};

class DrawerRt4colsCommand : public DrawerCommand
{
public:
	int sx;
	int yl;
	int yh;
	uint32_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _destorg;
	int _pitch;
	BYTE * RESTRICT _colormap;
	uint32_t _srcalpha;
	uint32_t _destalpha;

	DrawerRt4colsCommand(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_light = LightBgra::calc_light_multiplier(dc_light);
		_shade_constants = dc_shade_constants;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_colormap = dc_colormap;

		_srcalpha = dc_srcalpha >> (FRACBITS - 8);
		_destalpha = dc_destalpha >> (FRACBITS - 8);
	}

	class LoopIterator
	{
	public:
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		LoopIterator(DrawerRt4colsCommand *command, DrawerThread *thread)
		{
			count = thread->count_for_thread(command->yl, command->yh - command->yl + 1);
			if (count <= 0)
				return;

			dest = thread->dest_for_thread(command->yl, command->_pitch, ylookup[command->yl] + command->sx + (uint32_t*)command->_destorg);
			source = &thread->dc_temp_rgba[command->yl * 4] + thread->skipped_by_thread(command->yl) * 4;
			pitch = command->_pitch * thread->num_cores;
			sincr = thread->num_cores * 4;
		}

		explicit operator bool()
		{
			return count > 0;
		}

		bool next()
		{
			dest += pitch;
			source += sincr;
			return (--count) != 0;
		}
	};
};

class RtCopy1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtCopy1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = GPalette.BaseColors[*loop.source];
			*loop.dest = BlendBgra::copy(fg);
		} while (loop.next());
	}
};

class RtMap1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtMap1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = LightBgra::shade_pal_index(_colormap[*loop.source], _light, _shade_constants);
			*loop.dest = BlendBgra::copy(fg);
		} while (loop.next());
	}
};

class RtMap4colsRGBACommand : public DrawerRt4colsCommand
{
public:
	RtMap4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = LightBgra::shade_pal_index(_colormap[loop.source[i]], _light, _shade_constants);
				loop.dest[i] = BlendBgra::copy(fg);
			}
		} while (loop.next());
	}
};

class RtAdd1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtAdd1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = LightBgra::shade_pal_index(_colormap[*loop.source], _light, _shade_constants);
			*loop.dest = BlendBgra::add(fg, *loop.dest, _srcalpha, _destalpha);
		} while (loop.next());
	}
};

class RtAdd4colsRGBACommand : public DrawerRt4colsCommand
{
public:
	RtAdd4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = LightBgra::shade_pal_index(_colormap[loop.source[i]], _light, _shade_constants);
				loop.dest[i] = BlendBgra::add(fg, loop.dest[i], _srcalpha, _destalpha);
			}
		} while (loop.next());
	}
};

class RtShaded1colRGBACommand : public DrawerRt1colCommand
{
	uint32_t _color;

public:
	RtShaded1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
		_color = LightBgra::shade_pal_index(dc_color, _light, _shade_constants);
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t alpha = _colormap[*loop.source] * 4;
			uint32_t inv_alpha = 256 - alpha;
			*loop.dest = BlendBgra::add(_color, *loop.dest, alpha, inv_alpha);
		} while (loop.next());
	}
};

class RtShaded4colsRGBACommand : public DrawerRt4colsCommand
{
	uint32_t _color;

public:
	RtShaded4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
		_color = LightBgra::shade_pal_index(dc_color, _light, _shade_constants);
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t alpha = _colormap[loop.source[i]] * 4;
				uint32_t inv_alpha = 256 - alpha;
				loop.dest[i] = BlendBgra::add(_color, loop.dest[i], alpha, inv_alpha);
			}
		} while (loop.next());
	}
};

class RtAddClamp1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtAddClamp1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = LightBgra::shade_pal_index(*loop.source, _light, _shade_constants);
			*loop.dest = BlendBgra::add(fg, *loop.dest, _srcalpha, _destalpha);
		} while (loop.next());
	}
};

class RtAddClamp4colsRGBACommand : public DrawerRt4colsCommand
{
public:
	RtAddClamp4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = LightBgra::shade_pal_index(loop.source[i], _light, _shade_constants);
				loop.dest[i] = BlendBgra::add(fg, loop.dest[i], _srcalpha, _destalpha);
			}
		} while (loop.next());
	}
};

class RtSubClamp1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtSubClamp1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = LightBgra::shade_pal_index(*loop.source, _light, _shade_constants);
			*loop.dest = BlendBgra::sub(fg, *loop.dest, _srcalpha, _destalpha);
		} while (loop.next());
	}
};

class RtSubClamp4colsRGBACommand : public DrawerRt4colsCommand
{
public:
	RtSubClamp4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = LightBgra::shade_pal_index(loop.source[i], _light, _shade_constants);
				loop.dest[i] = BlendBgra::sub(fg, loop.dest[i], _srcalpha, _destalpha);
			}
		} while (loop.next());
	}
};

class RtRevSubClamp1colRGBACommand : public DrawerRt1colCommand
{
public:
	RtRevSubClamp1colRGBACommand(int hx, int sx, int yl, int yh) : DrawerRt1colCommand(hx, sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			uint32_t fg = LightBgra::shade_pal_index(*loop.source, _light, _shade_constants);
			*loop.dest = BlendBgra::revsub(fg, *loop.dest, _srcalpha, _destalpha);
		} while (loop.next());
	}
};

class RtRevSubClamp4colsRGBACommand : public DrawerRt4colsCommand
{
public:
	RtRevSubClamp4colsRGBACommand(int sx, int yl, int yh) : DrawerRt4colsCommand(sx, yl, yh)
	{
	}

	void Execute(DrawerThread *thread) override
	{
		LoopIterator loop(this, thread);
		if (!loop) return;
		do
		{
			for (int i = 0; i < 4; i++)
			{
				uint32_t fg = LightBgra::shade_pal_index(loop.source[i], _light, _shade_constants);
				loop.dest[i] = BlendBgra::revsub(fg, loop.dest[i], _srcalpha, _destalpha);
			}
		} while (loop.next());
	}
};

class RtTranslate1colRGBACommand : public DrawerCommand
{
	const BYTE * RESTRICT translation;
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
	const BYTE * RESTRICT translation;
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

class DrawColumnHorizRGBACommand : public DrawerCommand
{
	int _count;
	fixed_t _iscale;
	fixed_t _texturefrac;
	const BYTE * RESTRICT _source;
	int _x;
	int _yl;
	int _yh;

public:
	DrawColumnHorizRGBACommand()
	{
		_count = dc_count;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
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

		const BYTE *source = _source;

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
	DrawerCommandQueue::QueueCommand<RtCopy1colRGBACommand>(hx, sx, yl, yh);
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
	DrawerCommandQueue::QueueCommand<RtMap1colRGBACommand>(hx, sx, yl, yh);
}

// Maps all four spans to the screen starting at sx.
void rt_map4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtMap4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtMap4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

void rt_Translate1col_rgba(const BYTE *translation, int hx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtTranslate1colRGBACommand>(translation, hx, yl, yh);
}

void rt_Translate4cols_rgba(const BYTE *translation, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtTranslate4colsRGBACommand>(translation, yl, yh);
}

// Translates one span at hx to the screen at sx.
void rt_tlate1col_rgba (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_rgba(dc_translation, hx, yl, yh);
	rt_map1col(hx, sx, yl, yh);
}

// Translates all four spans to the screen starting at sx.
void rt_tlate4cols_rgba (int sx, int yl, int yh)
{
	rt_Translate4cols_rgba(dc_translation, yl, yh);
	rt_map4cols(sx, yl, yh);
}

// Adds one span at hx to the screen at sx without clamping.
void rt_add1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtAdd1colRGBACommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx without clamping.
void rt_add4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtAdd4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtAdd4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

// Translates and adds one span at hx to the screen at sx without clamping.
void rt_tlateadd1col_rgba (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_rgba(dc_translation, hx, yl, yh);
	rt_add1col(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx without clamping.
void rt_tlateadd4cols_rgba(int sx, int yl, int yh)
{
	rt_Translate4cols_rgba(dc_translation, yl, yh);
	rt_add4cols(sx, yl, yh);
}

// Shades one span at hx to the screen at sx.
void rt_shaded1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtShaded1colRGBACommand>(hx, sx, yl, yh);
}

// Shades all four spans to the screen starting at sx.
void rt_shaded4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtShaded4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtShaded4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

// Adds one span at hx to the screen at sx with clamping.
void rt_addclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtAddClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Adds all four spans to the screen starting at sx with clamping.
void rt_addclamp4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtAddClamp4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtAddClamp4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

// Translates and adds one span at hx to the screen at sx with clamping.
void rt_tlateaddclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_rgba(dc_translation, hx, yl, yh);
	rt_addclamp1col_rgba(hx, sx, yl, yh);
}

// Translates and adds all four spans to the screen starting at sx with clamping.
void rt_tlateaddclamp4cols_rgba (int sx, int yl, int yh)
{
	rt_Translate4cols_rgba(dc_translation, yl, yh);
	rt_addclamp4cols(sx, yl, yh);
}

// Subtracts one span at hx to the screen at sx with clamping.
void rt_subclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtSubClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Subtracts all four spans to the screen starting at sx with clamping.
void rt_subclamp4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtSubClamp4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtSubClamp4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

// Translates and subtracts one span at hx to the screen at sx with clamping.
void rt_tlatesubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_rgba(dc_translation, hx, yl, yh);
	rt_subclamp1col_rgba(hx, sx, yl, yh);
}

// Translates and subtracts all four spans to the screen starting at sx with clamping.
void rt_tlatesubclamp4cols_rgba (int sx, int yl, int yh)
{
	rt_Translate4cols_rgba(dc_translation, yl, yh);
	rt_subclamp4cols_rgba(sx, yl, yh);
}

// Subtracts one span at hx from the screen at sx with clamping.
void rt_revsubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	DrawerCommandQueue::QueueCommand<RtRevSubClamp1colRGBACommand>(hx, sx, yl, yh);
}

// Subtracts all four spans from the screen starting at sx with clamping.
void rt_revsubclamp4cols_rgba (int sx, int yl, int yh)
{
#ifdef NO_SSE
	DrawerCommandQueue::QueueCommand<RtRevSubClamp4colsRGBACommand>(sx, yl, yh);
#else
	DrawerCommandQueue::QueueCommand<RtRevSubClamp4colsRGBA_SSE_Command>(sx, yl, yh);
#endif
}

// Translates and subtracts one span at hx from the screen at sx with clamping.
void rt_tlaterevsubclamp1col_rgba (int hx, int sx, int yl, int yh)
{
	rt_Translate1col_rgba(dc_translation, hx, yl, yh);
	rt_revsubclamp1col_rgba(hx, sx, yl, yh);
}

// Translates and subtracts all four spans from the screen starting at sx with clamping.
void rt_tlaterevsubclamp4cols_rgba (int sx, int yl, int yh)
{
	rt_Translate4cols_rgba(dc_translation, yl, yh);
	rt_revsubclamp4cols_rgba(sx, yl, yh);
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

	DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand>();
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
