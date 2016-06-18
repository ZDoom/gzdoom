// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		True color span/column drawing functions.
//
//-----------------------------------------------------------------------------

#include <stddef.h>

#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_plane.h"
#include "r_draw_rgba.h"

#include "gi.h"
#include "stats.h"
#include "x86.h"
#ifndef NO_SSE
#include <emmintrin.h>
#include <immintrin.h>
#endif
#include <vector>

#ifdef _MSC_VER
#pragma warning(disable: 4752) // warning C4752: found Intel(R) Advanced Vector Extensions; consider using /arch:AVX
#endif

extern int vlinebits;
extern int mvlinebits;
extern int tmvlinebits;

extern "C" short spanend[MAXHEIGHT];
extern float rw_light;
extern float rw_lightstep;
extern int wallshade;

CVAR(Bool, r_multithreaded, true, 0)

//#define USE_AVX // Use AVX2 256 bit intrinsics (requires Haswell or newer)

/////////////////////////////////////////////////////////////////////////////

DrawerCommandQueue *DrawerCommandQueue::Instance()
{
	static DrawerCommandQueue queue;
	return &queue;
}

DrawerCommandQueue::~DrawerCommandQueue()
{
	StopThreads();
}

void* DrawerCommandQueue::AllocMemory(size_t size)
{
	// Make sure allocations remain 16-byte aligned
	size = (size + 15) / 16 * 16;

	auto queue = Instance();
	if (queue->memorypool_pos + size > memorypool_size)
		return nullptr;

	void *data = queue->memorypool + queue->memorypool_pos;
	queue->memorypool_pos += size;
	return data;
}

void DrawerCommandQueue::Begin()
{
	auto queue = Instance();
	queue->Finish();
	queue->threaded_render++;
}

void DrawerCommandQueue::End()
{
	auto queue = Instance();
	queue->Finish();
	if (queue->threaded_render > 0)
		queue->threaded_render--;
}

void DrawerCommandQueue::WaitForWorkers()
{
	Instance()->Finish();
}

void DrawerCommandQueue::Finish()
{
	auto queue = Instance();
	if (queue->commands.empty())
		return;

	// Give worker threads something to do:

	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	queue->active_commands.swap(queue->commands);
	queue->run_id++;
	start_lock.unlock();

	queue->StartThreads();
	queue->start_condition.notify_all();

	// Do one thread ourselves:

	DrawerThread thread;
	thread.core = 0;
	thread.num_cores = queue->threads.size() + 1;

	for (int pass = 0; pass < queue->num_passes; pass++)
	{
		thread.pass_start_y = pass * queue->rows_in_pass;
		thread.pass_end_y = (pass + 1) * queue->rows_in_pass;
		if (pass + 1 == queue->num_passes)
			thread.pass_end_y = MAX(thread.pass_end_y, MAXHEIGHT);

		size_t size = queue->active_commands.size();
		for (size_t i = 0; i < size; i++)
		{
			auto &command = queue->active_commands[i];
			command->Execute(&thread);
		}
	}

	// Wait for everyone to finish:

	std::unique_lock<std::mutex> end_lock(queue->end_mutex);
	queue->end_condition.wait(end_lock, [&]() { return queue->finished_threads == queue->threads.size(); });

	// Clean up batch:

	for (auto &command : queue->active_commands)
		command->~DrawerCommand();
	queue->active_commands.clear();
	queue->memorypool_pos = 0;
	queue->finished_threads = 0;
}

void DrawerCommandQueue::StartThreads()
{
	if (!threads.empty())
		return;

	int num_threads = std::thread::hardware_concurrency();
	if (num_threads == 0)
		num_threads = 4;

	threads.resize(num_threads - 1);

	for (int i = 0; i < num_threads - 1; i++)
	{
		DrawerCommandQueue *queue = this;
		DrawerThread *thread = &threads[i];
		thread->core = i + 1;
		thread->num_cores = num_threads;
		thread->thread = std::thread([=]()
		{
			int run_id = 0;
			while (true)
			{
				// Wait until we are signalled to run:
				std::unique_lock<std::mutex> start_lock(queue->start_mutex);
				queue->start_condition.wait(start_lock, [&]() { return queue->run_id != run_id || queue->shutdown_flag; });
				if (queue->shutdown_flag)
					break;
				run_id = queue->run_id;
				start_lock.unlock();

				// Do the work:
				for (int pass = 0; pass < queue->num_passes; pass++)
				{
					thread->pass_start_y = pass * queue->rows_in_pass;
					thread->pass_end_y = (pass + 1) * queue->rows_in_pass;
					if (pass + 1 == queue->num_passes)
						thread->pass_end_y = MAX(thread->pass_end_y, MAXHEIGHT);

					size_t size = queue->active_commands.size();
					for (size_t i = 0; i < size; i++)
					{
						auto &command = queue->active_commands[i];
						command->Execute(thread);
					}
				}

				// Notify main thread that we finished:
				std::unique_lock<std::mutex> end_lock(queue->end_mutex);
				queue->finished_threads++;
				end_lock.unlock();
				queue->end_condition.notify_all();
			}
		});
	}
}

void DrawerCommandQueue::StopThreads()
{
	std::unique_lock<std::mutex> lock(start_mutex);
	shutdown_flag = true;
	lock.unlock();
	start_condition.notify_all();
	for (auto &thread : threads)
		thread.thread.join();
	threads.clear();
	lock.lock();
	shutdown_flag = false;
}

/////////////////////////////////////////////////////////////////////////////

class DrawColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _texturefrac;
	DWORD _iscale;
	fixed_t _light;
	const BYTE * RESTRICT _source;
	int _pitch;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _colormap;

public:
	DrawColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_texturefrac = dc_texturefrac;
		_iscale = dc_iscale;
		_light = dc_light;
		_source = dc_source;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_colormap = dc_colormap;
	}

	void Execute(DrawerThread *thread) override
	{
		int 				count;
		uint32_t*			dest;
		fixed_t 			frac;
		fixed_t 			fracstep;

		count = thread->count_for_thread(_dest_y, _count);

		// Zero length, column does not exceed a pixel.
		if (count <= 0)
			return;

		// Framebuffer destination address.
		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		// Determine scaling,
		//	which is the only mapping to be done.
		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		// [RH] Get local copies of these variables so that the compiler
		//		has a better chance of optimizing this well.
		const BYTE *source = _source;
		int pitch = _pitch * thread->num_cores;
		BYTE *colormap = _colormap;

		do
		{
			*dest = shade_pal_index(colormap[source[frac >> FRACBITS]], light, shade_constants);

			dest += pitch;
			frac += fracstep;

		} while (--count);
	}
};

class FillColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	fixed_t _light;
	int _pitch;
	int _color;

public:
	FillColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_light = dc_light;
		_pitch = dc_pitch;
		_color = dc_color;
	}

	void Execute(DrawerThread *thread) override
	{
		int 				count;
		uint32_t*			dest;

		count = thread->count_for_thread(_dest_y, _count);

		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		uint32_t light = calc_light_multiplier(_light);

		{
			int pitch = _pitch * thread->num_cores;
			uint32_t color = shade_pal_index_simple(_color, light);

			do
			{
				*dest = color;
				dest += pitch;
			} while (--count);
		}
	}
};

class FillAddColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	int _pitch;
	uint32_t _srccolor;

public:
	FillAddColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_pitch = dc_pitch;
		_srccolor = dc_srccolor_bgra;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;

		uint32_t fg = _srccolor;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;
		uint32_t fg_alpha = fg >> 24;
		fg_alpha += fg_alpha >> 7;

		fg_red *= fg_alpha;
		fg_green *= fg_alpha;
		fg_blue *= fg_alpha;

		uint32_t inv_alpha = 256 - fg_alpha;

		do
		{
			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = (fg_red + bg_red * inv_alpha) / 256;
			uint32_t green = (fg_green + bg_green * inv_alpha) / 256;
			uint32_t blue = (fg_blue + bg_blue * inv_alpha) / 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
		} while (--count);
	}
};

class FillAddClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	int _pitch;
	int _color;
	uint32_t _srccolor;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	FillAddClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_pitch = dc_pitch;
		_color = dc_color;
		_srccolor = dc_srccolor_bgra;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;

		uint32_t fg = _srccolor;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;
		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		fg_red *= fg_alpha;
		fg_green *= fg_alpha;
		fg_blue *= fg_alpha;

		do {

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((fg_red + bg_red * bg_alpha) / 256, 0, 255);
			uint32_t green = clamp<uint32_t>((fg_green + bg_green * bg_alpha) / 256, 0, 255);
			uint32_t blue = clamp<uint32_t>((fg_blue + bg_blue * bg_alpha) / 256, 0, 255);

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
		} while (--count);
	}
};

class FillSubClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	int _pitch;
	int _color;
	uint32_t _srccolor;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	FillSubClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_pitch = dc_pitch;
		_color = dc_color;
		_srccolor = dc_srccolor_bgra;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;

		uint32_t fg = _srccolor;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;
		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		fg_red *= fg_alpha;
		fg_green *= fg_alpha;
		fg_blue *= fg_alpha;

		do {
			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((0x10000 - fg_red + bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>((0x10000 - fg_green + bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>((0x10000 - fg_blue + bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
		} while (--count);
	}
};

class FillRevSubClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	int _pitch;
	int _color;
	uint32_t _srccolor;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	FillRevSubClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_pitch = dc_pitch;
		_color = dc_color;
		_srccolor = dc_srccolor_bgra;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;

		uint32_t fg = _srccolor;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;
		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		fg_red *= fg_alpha;
		fg_green *= fg_alpha;
		fg_blue *= fg_alpha;

		do {
			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = clamp<uint32_t>((0x10000 + fg_red - bg_red * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t green = clamp<uint32_t>((0x10000 + fg_green - bg_green * bg_alpha) / 256, 256, 256 + 255) - 256;
			uint32_t blue = clamp<uint32_t>((0x10000 + fg_blue - bg_blue * bg_alpha) / 256, 256, 256 + 255) - 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
		} while (--count);
	}
};

class DrawFuzzColumnRGBACommand : public DrawerCommand
{
	int _x;
	int _yl;
	int _yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	int _fuzzpos;
	int _fuzzviewheight;

public:
	DrawFuzzColumnRGBACommand()
	{
		_x = dc_x;
		_yl = dc_yl;
		_yh = dc_yh;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;

		int yl = MAX(_yl, 1);
		int yh = MIN(_yh, _fuzzviewheight);

		count = thread->count_for_thread(yl, yh - yl + 1);

		// Zero length.
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + _x + (uint32_t*)_destorg);

		int pitch = _pitch * thread->num_cores;
		int fuzzstep = thread->num_cores;
		int fuzz = (_fuzzpos + thread->skipped_by_thread(yl)) % FUZZTABLE;

		yl += thread->skipped_by_thread(yl);

		// Handle the case where we would go out of bounds at the top:
		if (yl < fuzzstep)
		{
			uint32_t bg = dest[fuzzoffset[fuzz] * fuzzstep + pitch];
			uint32_t bg_red = (bg >> 16) & 0xff;
			uint32_t bg_green = (bg >> 8) & 0xff;
			uint32_t bg_blue = (bg) & 0xff;

			uint32_t red = bg_red * 3 / 4;
			uint32_t green = bg_green * 3 / 4;
			uint32_t blue = bg_blue * 3 / 4;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
			fuzz += fuzzstep;
			fuzz %= FUZZTABLE;

			count--;
			if (count == 0)
				return;
		}

		bool lowerbounds = (yl + count * fuzzstep > _fuzzviewheight);
		if (lowerbounds)
			count--;

		// Fuzz where fuzzoffset stays within bounds
		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = MIN(count, next_wrap);
			count -= cnt;
			do
			{
				uint32_t bg = dest[fuzzoffset[fuzz] * fuzzstep];
				uint32_t bg_red = (bg >> 16) & 0xff;
				uint32_t bg_green = (bg >> 8) & 0xff;
				uint32_t bg_blue = (bg) & 0xff;

				uint32_t red = bg_red * 3 / 4;
				uint32_t green = bg_green * 3 / 4;
				uint32_t blue = bg_blue * 3 / 4;

				*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				dest += pitch;
				fuzz += fuzzstep;
			} while (--cnt);

			fuzz %= FUZZTABLE;
		}

		// Handle the case where we would go out of bounds at the bottom
		if (lowerbounds)
		{
			uint32_t bg = dest[fuzzoffset[fuzz] * fuzzstep - pitch];
			uint32_t bg_red = (bg >> 16) & 0xff;
			uint32_t bg_green = (bg >> 8) & 0xff;
			uint32_t bg_blue = (bg) & 0xff;

			uint32_t red = bg_red * 3 / 4;
			uint32_t green = bg_green * 3 / 4;
			uint32_t blue = bg_blue * 3 / 4;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		}
	}
};

class DrawAddColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	BYTE * RESTRICT _colormap;

public:
	DrawAddColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_colormap = dc_colormap;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;

			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;
			BYTE *colormap = _colormap;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(colormap[source[frac >> FRACBITS]], light, shade_constants);

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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawTranslatedColumnRGBACommand : public DrawerCommand
{
	int _count;
	fixed_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	BYTE * RESTRICT _translation;
	const BYTE * RESTRICT _source;
	int _pitch;

public:
	DrawTranslatedColumnRGBACommand()
	{
		_count = dc_count;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_translation = dc_translation;
		_source = dc_source;
		_pitch = dc_pitch;
	}

	void Execute(DrawerThread *thread) override
	{
		int 				count;
		uint32_t*			dest;
		fixed_t 			frac;
		fixed_t 			fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			// [RH] Local copies of global vars to improve compiler optimizations
			BYTE *translation = _translation;
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;

			do
			{
				*dest = shade_pal_index(translation[source[frac >> FRACBITS]], light, shade_constants);
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawTlatedAddColumnRGBACommand : public DrawerCommand
{
	int _count;
	fixed_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	BYTE * RESTRICT _translation;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawTlatedAddColumnRGBACommand()
	{
		_count = dc_count;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_translation = dc_translation;
		_source = dc_source;
		_pitch = dc_pitch;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			BYTE *translation = _translation;
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(translation[source[frac >> FRACBITS]], light, shade_constants);

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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawShadedColumnRGBACommand : public DrawerCommand
{
private:
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	fixed_t _light;
	const BYTE * RESTRICT _source;
	lighttable_t * RESTRICT _colormap;
	int _color;
	int _pitch;

public:
	DrawShadedColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_light = dc_light;
		_source = dc_source;
		_colormap = dc_colormap;
		_color = dc_color;
		_pitch = dc_pitch;
	}

	void Execute(DrawerThread *thread) override
	{
		int  count;
		uint32_t *dest;
		fixed_t frac, fracstep;

		count = thread->count_for_thread(_dest_y, _count);

		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		uint32_t fg = shade_pal_index_simple(_color, calc_light_multiplier(_light));
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		{
			const BYTE *source = _source;
			BYTE *colormap = _colormap;
			int pitch = _pitch * thread->num_cores;

			do
			{
				DWORD alpha = clamp<DWORD>(colormap[source[frac >> FRACBITS]], 0, 64);
				DWORD inv_alpha = 64 - alpha;

				uint32_t bg_red = (*dest >> 16) & 0xff;
				uint32_t bg_green = (*dest >> 8) & 0xff;
				uint32_t bg_blue = (*dest) & 0xff;

				uint32_t red = (fg_red * alpha + bg_red * inv_alpha) / 64;
				uint32_t green = (fg_green * alpha + bg_green * inv_alpha) / 64;
				uint32_t blue = (fg_blue * alpha + bg_blue * inv_alpha) / 64;

				*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawAddClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawAddClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(source[frac >> FRACBITS], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawAddClampTranslatedColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	BYTE * RESTRICT _translation;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawAddClampTranslatedColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_translation = dc_translation;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			BYTE *translation = _translation;
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(translation[source[frac >> FRACBITS]], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawSubClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawSubClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(source[frac >> FRACBITS], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawSubClampTranslatedColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	BYTE * RESTRICT _translation;

public:
	DrawSubClampTranslatedColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_translation = dc_translation;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			BYTE *translation = _translation;
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(translation[source[frac >> FRACBITS]], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawRevSubClampColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawRevSubClampColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			const BYTE *source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;
			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(source[frac >> FRACBITS], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawRevSubClampTranslatedColumnRGBACommand : public DrawerCommand
{
	int _count;
	BYTE * RESTRICT _dest;
	DWORD _iscale;
	DWORD _texturefrac;
	const BYTE * RESTRICT _source;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	BYTE * RESTRICT _translation;

public:
	DrawRevSubClampTranslatedColumnRGBACommand()
	{
		_count = dc_count;
		_dest = dc_dest;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = dc_source;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_translation = dc_translation;
	}

	void Execute(DrawerThread *thread) override
	{
		int count;
		uint32_t *dest;
		fixed_t frac;
		fixed_t fracstep;

		count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);

		fracstep = _iscale * thread->num_cores;
		frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);

		{
			BYTE * RESTRICT translation = _translation;
			const BYTE * RESTRICT source = _source;
			int pitch = _pitch * thread->num_cores;
			uint32_t light = calc_light_multiplier(_light);
			ShadeConstants shade_constants = _shade_constants;

			uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
			uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

			do
			{
				uint32_t fg = shade_pal_index(translation[source[frac >> FRACBITS]], light, shade_constants);
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
				dest += pitch;
				frac += fracstep;
			} while (--count);
		}
	}
};

class DrawSpanRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _xfrac;
	fixed_t _yfrac;
	fixed_t _xstep;
	fixed_t _ystep;
	int _x1;
	int _x2;
	int _y;
	int _xbits;
	int _ybits;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;

public:
	DrawSpanRGBACommand()
	{
		_source = (const uint32_t*)ds_source;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_destorg = dc_destorg;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
#elif defined(USE_AVX)
	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.

			int sse_count = count / 8;
			count -= sse_count * 8;

			if (shade_constants.simple_shade)
			{
				AVX2_SHADE_SIMPLE_INIT(light);

				while (sse_count--)
				{
					uint32_t fg_pixels[8];
					for (int i = 0; i < 8; i++)
					{
						// Current texture index in u,v.
						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						fg_pixels[i] = source[spot];
						xfrac += xstep;
						yfrac += ystep;
					}

					// Lookup pixel from flat texture tile,
					//  re-index using light/colormap.
					__m256i fg = _mm256_loadu_si256((const __m256i*)fg_pixels);
					AVX2_SHADE_SIMPLE(fg);
					_mm256_storeu_si256((__m256i*)dest, fg);

					// Next step in u,v.
					dest += 8;
				}
			}
			else
			{
				AVX2_SHADE_INIT(light, shade_constants);

				while (sse_count--)
				{
					uint32_t fg_pixels[8];
					for (int i = 0; i < 8; i++)
					{
						// Current texture index in u,v.
						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						fg_pixels[i] = source[spot];
						xfrac += xstep;
						yfrac += ystep;
					}

					// Lookup pixel from flat texture tile,
					//  re-index using light/colormap.
					__m256i fg = _mm256_loadu_si256((const __m256i*)fg_pixels);
					AVX2_SHADE(fg, shade_constants);
					_mm256_storeu_si256((__m256i*)dest, fg);

					// Next step in u,v.
					dest += 8;
				}
			}

			if (count == 0)
				return;

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;

			int sse_count = count / 8;
			count -= sse_count * 8;

			if (shade_constants.simple_shade)
			{
				AVX2_SHADE_SIMPLE_INIT(light);

				while (sse_count--)
				{
					uint32_t fg_pixels[8];
					for (int i = 0; i < 8; i++)
					{
						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						fg_pixels[i] = source[spot];
						xfrac += xstep;
						yfrac += ystep;
					}

					// Lookup pixel from flat texture tile
					__m256i fg = _mm256_loadu_si256((const __m256i*)fg_pixels);
					AVX2_SHADE_SIMPLE(fg);
					_mm256_storeu_si256((__m256i*)dest, fg);
					dest += 8;
				}
			}
			else
			{
				AVX2_SHADE_INIT(light, shade_constants);

				while (sse_count--)
				{
					uint32_t fg_pixels[8];
					for (int i = 0; i < 8; i++)
					{
						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						fg_pixels[i] = source[spot];
						xfrac += xstep;
						yfrac += ystep;
					}

					// Lookup pixel from flat texture tile
					__m256i fg = _mm256_loadu_si256((const __m256i*)fg_pixels);
					AVX2_SHADE_SIMPLE(fg);
					_mm256_storeu_si256((__m256i*)dest, fg);
					dest += 4;
				}
			}

			if (count == 0)
				return;

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
#else
	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.

			int sse_count = count / 4;
			count -= sse_count * 4;

			if (shade_constants.simple_shade)
			{
				SSE_SHADE_SIMPLE_INIT(light);

				while (sse_count--)
				{
					// Current texture index in u,v.
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p0 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p1 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p2 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p3 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					// Lookup pixel from flat texture tile,
					//  re-index using light/colormap.
					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					SSE_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);

					// Next step in u,v.
					dest += 4;
				}
			}
			else
			{
				SSE_SHADE_INIT(light, shade_constants);

				while (sse_count--)
				{
					// Current texture index in u,v.
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p0 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p1 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p2 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
					uint32_t p3 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					// Lookup pixel from flat texture tile,
					//  re-index using light/colormap.
					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					SSE_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);

					// Next step in u,v.
					dest += 4;
				}
			}

			if (count == 0)
				return;

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;

			int sse_count = count / 4;
			count -= sse_count * 4;

			if (shade_constants.simple_shade)
			{
				SSE_SHADE_SIMPLE_INIT(light);

				while (sse_count--)
				{
					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p0 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p1 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p2 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p3 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					// Lookup pixel from flat texture tile
					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					SSE_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += 4;
				}
			}
			else
			{
				SSE_SHADE_INIT(light, shade_constants);

				while (sse_count--)
				{
					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p0 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p1 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p2 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
					uint32_t p3 = source[spot];
					xfrac += xstep;
					yfrac += ystep;

					// Lookup pixel from flat texture tile
					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					SSE_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += 4;
				}
			}

			if (count == 0)
				return;

			do
			{
				// Current texture index in u,v.
				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

				// Lookup pixel from flat texture tile
				*dest++ = shade_bgra(source[spot], light, shade_constants);

				// Next step in u,v.
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
#endif
};

class DrawSpanMaskedRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _xfrac;
	fixed_t _yfrac;
	BYTE * RESTRICT _destorg;
	int _x1;
	int _x2;
	int _y1;
	int _y;
	fixed_t _xstep;
	fixed_t _ystep;
	int _xbits;
	int _ybits;

public:
	DrawSpanMaskedRGBACommand()
	{
		_source = (const uint32_t*)ds_source;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_destorg = dc_destorg;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
				texdata = source[spot];
				if (texdata != 0)
				{
					*dest = shade_bgra(texdata, light, shade_constants);
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
				texdata = source[spot];
				if (texdata != 0)
				{
					*dest = shade_bgra(texdata, light, shade_constants);
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
};

class DrawSpanTranslucentRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _xfrac;
	fixed_t _yfrac;
	BYTE * RESTRICT _destorg;
	int _x1;
	int _x2;
	int _y1;
	int _y;
	fixed_t _xstep;
	fixed_t _ystep;
	int _xbits;
	int _ybits;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawSpanTranslucentRGBACommand()
	{
		_source = (const uint32_t *)ds_source;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_destorg = dc_destorg;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				uint32_t fg = shade_bgra(source[spot], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = (fg) & 0xff;

				uint32_t bg_red = (*dest >> 16) & 0xff;
				uint32_t bg_green = (*dest >> 8) & 0xff;
				uint32_t bg_blue = (*dest) & 0xff;

				uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
				uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
				uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

				*dest++ = 0xff000000 | (red << 16) | (green << 8) | blue;

				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;
			do
			{
				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

				uint32_t fg = shade_bgra(source[spot], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = (fg) & 0xff;

				uint32_t bg_red = (*dest >> 16) & 0xff;
				uint32_t bg_green = (*dest >> 8) & 0xff;
				uint32_t bg_blue = (*dest) & 0xff;

				uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
				uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
				uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

				*dest++ = 0xff000000 | (red << 16) | (green << 8) | blue;

				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
};

class DrawSpanMaskedTranslucentRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _xfrac;
	fixed_t _yfrac;
	BYTE * RESTRICT _destorg;
	int _x1;
	int _x2;
	int _y1;
	int _y;
	fixed_t _xstep;
	fixed_t _ystep;
	int _xbits;
	int _ybits;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawSpanMaskedTranslucentRGBACommand()
	{
		_source = (const uint32_t*)ds_source;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_destorg = dc_destorg;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
				texdata = source[spot];
				if (texdata != 0)
				{
					uint32_t fg = shade_bgra(texdata, light, shade_constants);
					uint32_t fg_red = (fg >> 16) & 0xff;
					uint32_t fg_green = (fg >> 8) & 0xff;
					uint32_t fg_blue = (fg) & 0xff;

					uint32_t bg_red = (*dest >> 16) & 0xff;
					uint32_t bg_green = (*dest >> 8) & 0xff;
					uint32_t bg_blue = (*dest) & 0xff;

					uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
					uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
					uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

					*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
				texdata = source[spot];
				if (texdata != 0)
				{
					uint32_t fg = shade_bgra(texdata, light, shade_constants);
					uint32_t fg_red = (fg >> 16) & 0xff;
					uint32_t fg_green = (fg >> 8) & 0xff;
					uint32_t fg_blue = (fg) & 0xff;

					uint32_t bg_red = (*dest >> 16) & 0xff;
					uint32_t bg_green = (*dest >> 8) & 0xff;
					uint32_t bg_blue = (*dest) & 0xff;

					uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
					uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
					uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

					*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
};

class DrawSpanAddClampRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _xfrac;
	fixed_t _yfrac;
	BYTE * RESTRICT _destorg;
	int _x1;
	int _x2;
	int _y1;
	int _y;
	fixed_t _xstep;
	fixed_t _ystep;
	int _xbits;
	int _ybits;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawSpanAddClampRGBACommand()
	{
		_source = (const uint32_t*)ds_source;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_destorg = dc_destorg;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

				uint32_t fg = shade_bgra(source[spot], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = (fg) & 0xff;

				uint32_t bg_red = (*dest >> 16) & 0xff;
				uint32_t bg_green = (*dest >> 8) & 0xff;
				uint32_t bg_blue = (*dest) & 0xff;

				uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
				uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
				uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

				*dest++ = 0xff000000 | (red << 16) | (green << 8) | blue;

				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;
			do
			{
				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

				uint32_t fg = shade_bgra(source[spot], light, shade_constants);
				uint32_t fg_red = (fg >> 16) & 0xff;
				uint32_t fg_green = (fg >> 8) & 0xff;
				uint32_t fg_blue = (fg) & 0xff;

				uint32_t bg_red = (*dest >> 16) & 0xff;
				uint32_t bg_green = (*dest >> 8) & 0xff;
				uint32_t bg_blue = (*dest) & 0xff;

				uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
				uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
				uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

				*dest++ = 0xff000000 | (red << 16) | (green << 8) | blue;

				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
};

class DrawSpanMaskedAddClampRGBACommand : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _xfrac;
	fixed_t _yfrac;
	BYTE * RESTRICT _destorg;
	int _x1;
	int _x2;
	int _y1;
	int _y;
	fixed_t _xstep;
	fixed_t _ystep;
	int _xbits;
	int _ybits;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	DrawSpanMaskedAddClampRGBACommand()
	{
		_source = (const uint32_t*)ds_source;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_destorg = dc_destorg;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		if (_xbits == 6 && _ybits == 6)
		{
			// 64x64 is the most common case by far, so special case it.
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
				texdata = source[spot];
				if (texdata != 0)
				{
					uint32_t fg = shade_bgra(texdata, light, shade_constants);
					uint32_t fg_red = (fg >> 16) & 0xff;
					uint32_t fg_green = (fg >> 8) & 0xff;
					uint32_t fg_blue = (fg) & 0xff;

					uint32_t bg_red = (*dest >> 16) & 0xff;
					uint32_t bg_green = (*dest >> 8) & 0xff;
					uint32_t bg_blue = (*dest) & 0xff;

					uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
					uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
					uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

					*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
		else
		{
			BYTE yshift = 32 - _ybits;
			BYTE xshift = yshift - _xbits;
			int xmask = ((1 << _xbits) - 1) << _ybits;
			do
			{
				uint32_t texdata;

				spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
				texdata = source[spot];
				if (texdata != 0)
				{
					uint32_t fg = shade_bgra(texdata, light, shade_constants);
					uint32_t fg_red = (fg >> 16) & 0xff;
					uint32_t fg_green = (fg >> 8) & 0xff;
					uint32_t fg_blue = (fg) & 0xff;

					uint32_t bg_red = (*dest >> 16) & 0xff;
					uint32_t bg_green = (*dest >> 8) & 0xff;
					uint32_t bg_blue = (*dest) & 0xff;

					uint32_t red = (fg_red * fg_alpha + bg_red * bg_alpha) / 256;
					uint32_t green = (fg_green * fg_alpha + bg_green * bg_alpha) / 256;
					uint32_t blue = (fg_blue * fg_alpha + bg_blue * bg_alpha) / 256;

					*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				}
				dest++;
				xfrac += xstep;
				yfrac += ystep;
			} while (--count);
		}
	}
};

class FillSpanRGBACommand : public DrawerCommand
{
	int _x1;
	int _x2;
	int _y;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	int _color;

public:
	FillSpanRGBACommand()
	{
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_destorg = dc_destorg;
		_light = ds_light;
		_color = ds_color;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t *dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;
		int count = (_x2 - _x1 + 1);
		uint32_t light = calc_light_multiplier(_light);
		uint32_t color = shade_pal_index_simple(_color, light);
		for (int i = 0; i < count; i++)
			dest[i] = color;
	}
};

class Vlinec1RGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int vlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;

public:
	Vlinec1RGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		vlinebits = ::vlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = vlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		do
		{
			*dest = shade_bgra(source[frac >> bits], light, shade_constants);
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Vlinec4RGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	int vlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];

public:
	Vlinec4RGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		vlinebits = ::vlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = vlinebits;
		DWORD place;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			dest[0] = shade_bgra(bufplce[0][(place = local_vplce[0]) >> bits], light0, shade_constants); local_vplce[0] = place + local_vince[0];
			dest[1] = shade_bgra(bufplce[1][(place = local_vplce[1]) >> bits], light1, shade_constants); local_vplce[1] = place + local_vince[1];
			dest[2] = shade_bgra(bufplce[2][(place = local_vplce[2]) >> bits], light2, shade_constants); local_vplce[2] = place + local_vince[2];
			dest[3] = shade_bgra(bufplce[3][(place = local_vplce[3]) >> bits], light3, shade_constants); local_vplce[3] = place + local_vince[3];
			dest += pitch;
		} while (--count);
	}
#elif defined(USE_AVX)
	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = vlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (count & 1)
		{
			DWORD place;
			dest[0] = shade_bgra(bufplce[0][(place = local_vplce[0]) >> bits], light0, shade_constants); local_vplce[0] = place + local_vince[0];
			dest[1] = shade_bgra(bufplce[1][(place = local_vplce[1]) >> bits], light1, shade_constants); local_vplce[1] = place + local_vince[1];
			dest[2] = shade_bgra(bufplce[2][(place = local_vplce[2]) >> bits], light2, shade_constants); local_vplce[2] = place + local_vince[2];
			dest[3] = shade_bgra(bufplce[3][(place = local_vplce[3]) >> bits], light3, shade_constants); local_vplce[3] = place + local_vince[3];
			dest += pitch;
		}
		count /= 2;

		// Assume all columns come from the same texture (which they do):
		const uint32_t *base_addr = MIN(MIN(MIN(bufplce[0], bufplce[1]), bufplce[2]), bufplce[3]);
		__m256i column_offsets = _mm256_set_epi32(
			bufplce[3] - base_addr, bufplce[2] - base_addr, bufplce[1] - base_addr, bufplce[0] - base_addr,
			bufplce[3] - base_addr, bufplce[2] - base_addr, bufplce[1] - base_addr, bufplce[0] - base_addr);

		__m256i place = _mm256_set_epi32(
			local_vplce[3] + local_vince[3], local_vplce[2] + local_vince[2], local_vplce[1] + local_vince[1], local_vplce[0] + local_vince[0],
			local_vplce[3], local_vplce[2], local_vplce[1], local_vplce[0]);

		__m256i step = _mm256_set_epi32(
			local_vince[3], local_vince[2], local_vince[1], local_vince[0],
			local_vince[3], local_vince[2], local_vince[1], local_vince[0]);
		step = _mm256_add_epi32(step, step);

		if (shade_constants.simple_shade)
		{
			AVX2_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
			while (count--)
			{
				__m256i fg = _mm256_i32gather_epi32((const int *)base_addr, _mm256_add_epi32(column_offsets, _mm256_srli_epi32(place, bits)), 4);
				place = _mm256_add_epi32(place, step);
				AVX2_SHADE_SIMPLE(fg);
				_mm256_storeu2_m128i((__m128i*)(dest + pitch), (__m128i*)dest, fg);
				dest += pitch * 2;
			}
		}
		else
		{
			AVX2_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
			while (count--)
			{
				__m256i fg = _mm256_i32gather_epi32((const int *)base_addr, _mm256_add_epi32(column_offsets, _mm256_srai_epi32(place, bits)), 4);
				place = _mm256_add_epi32(place, step);
				AVX2_SHADE(fg, shade_constants);
				_mm256_storeu2_m128i((__m128i*)(dest + pitch), (__m128i*)dest, fg);
				dest += pitch * 2;
			}
		}
	}
#else
	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = vlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			SSE_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
			do
			{
				DWORD place0 = local_vplce[0];
				DWORD place1 = local_vplce[1];
				DWORD place2 = local_vplce[2];
				DWORD place3 = local_vplce[3];

				uint32_t p0 = bufplce[0][place0 >> bits];
				uint32_t p1 = bufplce[1][place1 >> bits];
				uint32_t p2 = bufplce[2][place2 >> bits];
				uint32_t p3 = bufplce[3][place3 >> bits];

				local_vplce[0] = place0 + local_vince[0];
				local_vplce[1] = place1 + local_vince[1];
				local_vplce[2] = place2 + local_vince[2];
				local_vplce[3] = place3 + local_vince[3];

				__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
				SSE_SHADE_SIMPLE(fg);
				_mm_storeu_si128((__m128i*)dest, fg);
				dest += pitch;
			} while (--count);
		}
		else
		{
			SSE_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
			do
			{
				DWORD place0 = local_vplce[0];
				DWORD place1 = local_vplce[1];
				DWORD place2 = local_vplce[2];
				DWORD place3 = local_vplce[3];

				uint32_t p0 = bufplce[0][place0 >> bits];
				uint32_t p1 = bufplce[1][place1 >> bits];
				uint32_t p2 = bufplce[2][place2 >> bits];
				uint32_t p3 = bufplce[3][place3 >> bits];

				local_vplce[0] = place0 + local_vince[0];
				local_vplce[1] = place1 + local_vince[1];
				local_vplce[2] = place2 + local_vince[2];
				local_vplce[3] = place3 + local_vince[3];

				__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
				SSE_SHADE(fg, shade_constants);
				_mm_storeu_si128((__m128i*)dest, fg);
				dest += pitch;
			} while (--count);
		}
	}
#endif
};

class Mvlinec1RGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int mvlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;

public:
	Mvlinec1RGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		mvlinebits = ::mvlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = mvlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		do
		{
			uint32_t pix = source[frac >> bits];
			if (pix != 0)
			{
				*dest = shade_bgra(pix, light, shade_constants);
			}
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Mvlinec4RGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	int mvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];

public:
	Mvlinec4RGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		mvlinebits = ::mvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

#ifdef NO_SSE
	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = mvlinebits;
		DWORD place;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			uint32_t pix;
			pix = bufplce[0][(place = local_vplce[0]) >> bits]; if (pix) dest[0] = shade_bgra(pix, light0, shade_constants); local_vplce[0] = place + local_vince[0];
			pix = bufplce[1][(place = local_vplce[1]) >> bits]; if (pix) dest[1] = shade_bgra(pix, light1, shade_constants); local_vplce[1] = place + local_vince[1];
			pix = bufplce[2][(place = local_vplce[2]) >> bits]; if (pix) dest[2] = shade_bgra(pix, light2, shade_constants); local_vplce[2] = place + local_vince[2];
			pix = bufplce[3][(place = local_vplce[3]) >> bits]; if (pix) dest[3] = shade_bgra(pix, light3, shade_constants); local_vplce[3] = place + local_vince[3];
			dest += pitch;
		} while (--count);
	}
#else
	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = mvlinebits;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			SSE_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
			do
			{
				DWORD place0 = local_vplce[0];
				DWORD place1 = local_vplce[1];
				DWORD place2 = local_vplce[2];
				DWORD place3 = local_vplce[3];

				uint32_t pix0 = bufplce[0][place0 >> bits];
				uint32_t pix1 = bufplce[1][place1 >> bits];
				uint32_t pix2 = bufplce[2][place2 >> bits];
				uint32_t pix3 = bufplce[3][place3 >> bits];

				// movemask = !(pix == 0)
				__m128i movemask = _mm_xor_si128(_mm_cmpeq_epi32(_mm_set_epi32(pix3, pix2, pix1, pix0), _mm_setzero_si128()), _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));

				local_vplce[0] = place0 + local_vince[0];
				local_vplce[1] = place1 + local_vince[1];
				local_vplce[2] = place2 + local_vince[2];
				local_vplce[3] = place3 + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				SSE_SHADE_SIMPLE(fg);
				_mm_maskmoveu_si128(fg, movemask, (char*)dest);
				dest += pitch;
			} while (--count);
		}
		else
		{
			SSE_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
			do
			{
				DWORD place0 = local_vplce[0];
				DWORD place1 = local_vplce[1];
				DWORD place2 = local_vplce[2];
				DWORD place3 = local_vplce[3];

				uint32_t pix0 = bufplce[0][place0 >> bits];
				uint32_t pix1 = bufplce[1][place1 >> bits];
				uint32_t pix2 = bufplce[2][place2 >> bits];
				uint32_t pix3 = bufplce[3][place3 >> bits];

				// movemask = !(pix == 0)
				__m128i movemask = _mm_xor_si128(_mm_cmpeq_epi32(_mm_set_epi32(pix3, pix2, pix1, pix0), _mm_setzero_si128()), _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));

				local_vplce[0] = place0 + local_vince[0];
				local_vplce[1] = place1 + local_vince[1];
				local_vplce[2] = place2 + local_vince[2];
				local_vplce[3] = place3 + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				SSE_SHADE(fg, shade_constants);
				_mm_maskmoveu_si128(fg, movemask, (char*)dest);
				dest += pitch;
			} while (--count);
		}
	}
#endif
};

class Tmvline1AddRGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int tmvlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	Tmvline1AddRGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		tmvlinebits = ::tmvlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = tmvlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		do
		{
			uint32_t pix = source[frac >> bits];
			if (pix != 0)
			{
				uint32_t fg = shade_bgra(pix, light, shade_constants);
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
			}
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Tmvline4AddRGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];

public:
	Tmvline4AddRGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			for (int i = 0; i < 4; ++i)
			{
				uint32_t pix = bufplce[i][local_vplce[i] >> bits];
				if (pix != 0)
				{
					uint32_t fg = shade_bgra(pix, light[i], shade_constants);
					uint32_t fg_red = (fg >> 16) & 0xff;
					uint32_t fg_green = (fg >> 8) & 0xff;
					uint32_t fg_blue = fg & 0xff;

					uint32_t bg_red = (*dest >> 16) & 0xff;
					uint32_t bg_green = (*dest >> 8) & 0xff;
					uint32_t bg_blue = (*dest) & 0xff;

					uint32_t red = clamp<uint32_t>((fg_red * fg_alpha + bg_red * bg_alpha) / 256, 0, 255);
					uint32_t green = clamp<uint32_t>((fg_green * fg_alpha + bg_green * bg_alpha) / 256, 0, 255);
					uint32_t blue = clamp<uint32_t>((fg_blue * fg_alpha + bg_blue * bg_alpha) / 256, 0, 255);

					dest[i] = 0xff000000 | (red << 16) | (green << 8) | blue;
				}
				local_vplce[i] += local_vince[i];
			}
			dest += pitch;
		} while (--count);
	}
};

class Tmvline1AddClampRGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int tmvlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	Tmvline1AddClampRGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		tmvlinebits = ::tmvlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = tmvlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		do
		{
			uint32_t pix = source[frac >> bits];
			if (pix != 0)
			{
				uint32_t fg = shade_bgra(pix, light, shade_constants);
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
			}
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Tmvline4AddClampRGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	Tmvline4AddClampRGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			for (int i = 0; i < 4; ++i)
			{
				uint32_t pix = bufplce[i][local_vplce[i] >> bits];
				if (pix != 0)
				{
					uint32_t fg = shade_bgra(pix, light[i], shade_constants);
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
				local_vplce[i] += local_vince[i];
			}
			dest += pitch;
		} while (--count);
	}
};

class Tmvline1SubClampRGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int tmvlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	Tmvline1SubClampRGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		tmvlinebits = ::tmvlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = tmvlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		do
		{
			uint32_t pix = source[frac >> bits];
			if (pix != 0)
			{
				uint32_t fg = shade_bgra(pix, light, shade_constants);
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
			}
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Tmvline4SubClampRGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	Tmvline4SubClampRGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			for (int i = 0; i < 4; ++i)
			{
				uint32_t pix = bufplce[i][local_vplce[i] >> bits];
				if (pix != 0)
				{
					uint32_t fg = shade_bgra(pix, light[i], shade_constants);
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
				local_vplce[i] += local_vince[i];
			}
			dest += pitch;
		} while (--count);
	}
};

class Tmvline1RevSubClampRGBACommand : public DrawerCommand
{
	DWORD _iscale;
	DWORD _texturefrac;
	int _count;
	const BYTE * RESTRICT _source;
	BYTE * RESTRICT _dest;
	int tmvlinebits;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	Tmvline1RevSubClampRGBACommand()
	{
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_count = dc_count;
		_source = dc_source;
		_dest = dc_dest;
		tmvlinebits = ::tmvlinebits;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		DWORD fracstep = _iscale * thread->num_cores;
		DWORD frac = _texturefrac + _iscale * thread->skipped_by_thread(_dest_y);
		const uint32 *source = (const uint32 *)_source;
		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = tmvlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		do
		{
			uint32_t pix = source[frac >> bits];
			if (pix != 0)
			{
				uint32_t fg = shade_bgra(pix, light, shade_constants);
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
			}
			frac += fracstep;
			dest += pitch;
		} while (--count);
	}
};

class Tmvline4RevSubClampRGBACommand : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	Tmvline4RevSubClampRGBACommand()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		do
		{
			for (int i = 0; i < 4; ++i)
			{
				uint32_t pix = bufplce[i][local_vplce[i] >> bits];
				if (pix != 0)
				{
					uint32_t fg = shade_bgra(pix, light[i], shade_constants);
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
				local_vplce[i] += local_vince[i];
			}
			dest += pitch;
		} while (--count);
	}
};

class DrawFogBoundaryLineRGBACommand : public DrawerCommand
{
	int _y;
	int _x;
	int _x2;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;

public:
	DrawFogBoundaryLineRGBACommand(int y, int x, int x2)
	{
		_y = y;
		_x = x;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x = _x;
		int x2 = _x2;

		uint32_t *dest = ylookup[y] + (uint32_t*)_destorg;

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants constants = _shade_constants;

		do
		{
			uint32_t red = (dest[x] >> 16) & 0xff;
			uint32_t green = (dest[x] >> 8) & 0xff;
			uint32_t blue = dest[x] & 0xff;

			if (constants.simple_shade)
			{
				red = red * light / 256;
				green = green * light / 256;
				blue = blue * light / 256;
			}
			else
			{
				uint32_t inv_light = 256 - light;
				uint32_t inv_desaturate = 256 - constants.desaturate;

				uint32_t intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

				red = (red * inv_desaturate + intensity) / 256;
				green = (green * inv_desaturate + intensity) / 256;
				blue = (blue * inv_desaturate + intensity) / 256;

				red = (constants.fade_red * inv_light + red * light) / 256;
				green = (constants.fade_green * inv_light + green * light) / 256;
				blue = (constants.fade_blue * inv_light + blue * light) / 256;

				red = (red * constants.light_red) / 256;
				green = (green * constants.light_green) / 256;
				blue = (blue * constants.light_blue) / 256;
			}

			dest[x] = 0xff000000 | (red << 16) | (green << 8) | blue;
		} while (++x <= x2);
	}
};

class DrawTiltedSpanRGBACommand : public DrawerCommand
{
	int _y;
	int _x1;
	int _x2;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;
	const BYTE * RESTRICT _source;

public:
	DrawTiltedSpanRGBACommand(int y, int x1, int x2)
	{
		_y = y;
		_x1 = x1;
		_x2 = x2;

		_destorg = dc_destorg;
		_source = ds_source;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x1 = _x1;
		int x2 = _x2;

		// Slopes are broken currently in master.
		// Until R_DrawTiltedPlane is fixed we are just going to fill with a solid color.

		uint32_t *source = (uint32_t*)_source;
		uint32_t *dest = ylookup[y] + x1 + (uint32_t*)_destorg;

		int count = x2 - x1 + 1;
		while (count > 0)
		{
			*(dest++) = source[0];
			count--;
		}
	}
};

class DrawColoredSpanRGBACommand : public DrawerCommand
{
	int _y;
	int _x1;
	int _x2;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	int _color;

public:
	DrawColoredSpanRGBACommand(int y, int x1, int x2)
	{
		_y = y;
		_x1 = x1;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = ds_light;
		_color = ds_color;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x1 = _x1;
		int x2 = _x2;

		uint32_t *dest = ylookup[y] + x1 + (uint32_t*)_destorg;
		int count = (x2 - x1 + 1);
		uint32_t light = calc_light_multiplier(_light);
		uint32_t color = shade_pal_index_simple(_color, light);
		for (int i = 0; i < count; i++)
			dest[i] = color;
	}
};

class FillTransColumnRGBACommand : public DrawerCommand
{
	int _x;
	int _y1;
	int _y2;
	int _color;
	int _a;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;

public:
	FillTransColumnRGBACommand(int x, int y1, int y2, int color, int a)
	{
		_x = x;
		_y1 = y1;
		_y2 = y2;
		_color = color;
		_a = a;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
	}

	void Execute(DrawerThread *thread) override
	{
		int x = _x;
		int y1 = _y1;
		int y2 = _y2;
		int color = _color;
		int a = _a;

		int ycount = thread->count_for_thread(y1, y2 - y1 + 1);
		if (ycount <= 0)
			return;

		uint32_t fg = GPalette.BaseColors[color].d;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t alpha = a + 1;
		uint32_t inv_alpha = 256 - alpha;

		fg_red *= alpha;
		fg_green *= alpha;
		fg_blue *= alpha;

		int spacing = _pitch * thread->num_cores;
		uint32_t *dest = thread->dest_for_thread(y1, _pitch, ylookup[y1] + x + (uint32_t*)_destorg);

		for (int y = 0; y < ycount; y++)
		{
			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = (fg_red + bg_red * inv_alpha) / 256;
			uint32_t green = (fg_green + bg_green * inv_alpha) / 256;
			uint32_t blue = (fg_blue + bg_blue * inv_alpha) / 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += spacing;
		}
	}
};

ApplySpecialColormapRGBACommand::ApplySpecialColormapRGBACommand(FSpecialColormap *colormap, DFrameBuffer *screen)
{
	buffer = screen->GetBuffer();
	pitch = screen->GetPitch();
	width = screen->GetWidth();
	height = screen->GetHeight();

	start_red = (int)(colormap->ColorizeStart[0] * 255);
	start_green = (int)(colormap->ColorizeStart[1] * 255);
	start_blue = (int)(colormap->ColorizeStart[2] * 255);
	end_red = (int)(colormap->ColorizeEnd[0] * 255);
	end_green = (int)(colormap->ColorizeEnd[1] * 255);
	end_blue = (int)(colormap->ColorizeEnd[2] * 255);
}

#ifdef NO_SSE
void ApplySpecialColormapRGBACommand::Execute(DrawerThread *thread)
{
	int y = thread->skipped_by_thread(0);
	int count = thread->count_for_thread(0, height);
	while (count > 0)
	{
		BYTE *pixels = buffer + y * pitch * 4;
		for (int x = 0; x < width; x++)
		{
			int fg_red = pixels[2];
			int fg_green = pixels[1];
			int fg_blue = pixels[0];

			int gray = (fg_red * 77 + fg_green * 143 + fg_blue * 37) >> 8;
			gray += (gray >> 7); // gray*=256/255
			int inv_gray = 256 - gray;

			int red = clamp((start_red * inv_gray + end_red * gray) >> 8, 0, 255);
			int green = clamp((start_green * inv_gray + end_green * gray) >> 8, 0, 255);
			int blue = clamp((start_blue * inv_gray + end_blue * gray) >> 8, 0, 255);

			pixels[0] = (BYTE)blue;
			pixels[1] = (BYTE)green;
			pixels[2] = (BYTE)red;
			pixels[3] = 0xff;

			pixels += 4;
		}
		y += thread->num_cores;
		count--;
	}
}
#else
void ApplySpecialColormapRGBACommand::Execute(DrawerThread *thread)
{
	int y = thread->skipped_by_thread(0);
	int count = thread->count_for_thread(0, height);
	__m128i gray_weight = _mm_set_epi16(256, 77, 143, 37, 256, 77, 143, 37);
	__m128i start_end = _mm_set_epi16(255, start_red, start_green, start_blue, 255, end_red, end_green, end_blue);
	while (count > 0)
	{
		BYTE *pixels = buffer + y * pitch * 4;
		int sse_length = width / 4;
		for (int x = 0; x < sse_length; x++)
		{
			// Unpack to integers:
			__m128i p = _mm_loadu_si128((const __m128i*)pixels);

			__m128i p16_0 = _mm_unpacklo_epi8(p, _mm_setzero_si128());
			__m128i p16_1 = _mm_unpackhi_epi8(p, _mm_setzero_si128());

			// Add gray weighting to colors
			__m128i mullo0 = _mm_mullo_epi16(p16_0, gray_weight);
			__m128i mullo1 = _mm_mullo_epi16(p16_1, gray_weight);
			__m128i p32_0 = _mm_unpacklo_epi16(mullo0, _mm_setzero_si128());
			__m128i p32_1 = _mm_unpackhi_epi16(mullo0, _mm_setzero_si128());
			__m128i p32_2 = _mm_unpacklo_epi16(mullo1, _mm_setzero_si128());
			__m128i p32_3 = _mm_unpackhi_epi16(mullo1, _mm_setzero_si128());

			// Transpose to get color components in individual vectors:
			__m128 tmpx = _mm_castsi128_ps(p32_0);
			__m128 tmpy = _mm_castsi128_ps(p32_1);
			__m128 tmpz = _mm_castsi128_ps(p32_2);
			__m128 tmpw = _mm_castsi128_ps(p32_3);
			_MM_TRANSPOSE4_PS(tmpx, tmpy, tmpz, tmpw);
			__m128i blue = _mm_castps_si128(tmpx);
			__m128i green = _mm_castps_si128(tmpy);
			__m128i red = _mm_castps_si128(tmpz);
			__m128i alpha = _mm_castps_si128(tmpw);

			// Calculate gray and 256-gray values:
			__m128i gray = _mm_srli_epi32(_mm_add_epi32(_mm_add_epi32(red, green), blue), 8);
			__m128i inv_gray = _mm_sub_epi32(_mm_set1_epi32(256), gray);

			// p32 = start * inv_gray + end * gray:
			__m128i gray0 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(0, 0, 0, 0));
			__m128i gray1 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(1, 1, 1, 1));
			__m128i gray2 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(2, 2, 2, 2));
			__m128i gray3 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(3, 3, 3, 3));
			__m128i inv_gray0 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(0, 0, 0, 0));
			__m128i inv_gray1 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(1, 1, 1, 1));
			__m128i inv_gray2 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(2, 2, 2, 2));
			__m128i inv_gray3 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(3, 3, 3, 3));
			__m128i gray16_0 = _mm_packs_epi32(gray0, inv_gray0);
			__m128i gray16_1 = _mm_packs_epi32(gray1, inv_gray1);
			__m128i gray16_2 = _mm_packs_epi32(gray2, inv_gray2);
			__m128i gray16_3 = _mm_packs_epi32(gray3, inv_gray3);
			__m128i gray16_0_mullo = _mm_mullo_epi16(gray16_0, start_end);
			__m128i gray16_1_mullo = _mm_mullo_epi16(gray16_1, start_end);
			__m128i gray16_2_mullo = _mm_mullo_epi16(gray16_2, start_end);
			__m128i gray16_3_mullo = _mm_mullo_epi16(gray16_3, start_end);
			__m128i gray16_0_mulhi = _mm_mulhi_epi16(gray16_0, start_end);
			__m128i gray16_1_mulhi = _mm_mulhi_epi16(gray16_1, start_end);
			__m128i gray16_2_mulhi = _mm_mulhi_epi16(gray16_2, start_end);
			__m128i gray16_3_mulhi = _mm_mulhi_epi16(gray16_3, start_end);
			p32_0 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_0_mullo, gray16_0_mulhi), _mm_unpackhi_epi16(gray16_0_mullo, gray16_0_mulhi)), 8);
			p32_1 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_1_mullo, gray16_1_mulhi), _mm_unpackhi_epi16(gray16_1_mullo, gray16_1_mulhi)), 8);
			p32_2 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_2_mullo, gray16_2_mulhi), _mm_unpackhi_epi16(gray16_2_mullo, gray16_2_mulhi)), 8);
			p32_3 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_3_mullo, gray16_3_mulhi), _mm_unpackhi_epi16(gray16_3_mullo, gray16_3_mulhi)), 8);

			p16_0 = _mm_packs_epi32(p32_0, p32_1);
			p16_1 = _mm_packs_epi32(p32_2, p32_3);
			p = _mm_packus_epi16(p16_0, p16_1);

			_mm_storeu_si128((__m128i*)pixels, p);
			pixels += 16;
		}

		for (int x = sse_length * 4; x < width; x++)
		{
			int fg_red = pixels[2];
			int fg_green = pixels[1];
			int fg_blue = pixels[0];

			int gray = (fg_red * 77 + fg_green * 143 + fg_blue * 37) >> 8;
			gray += (gray >> 7); // gray*=256/255
			int inv_gray = 256 - gray;

			int red = clamp((start_red * inv_gray + end_red * gray) >> 8, 0, 255);
			int green = clamp((start_green * inv_gray + end_green * gray) >> 8, 0, 255);
			int blue = clamp((start_blue * inv_gray + end_blue * gray) >> 8, 0, 255);

			pixels[0] = (BYTE)blue;
			pixels[1] = (BYTE)green;
			pixels[2] = (BYTE)red;
			pixels[3] = 0xff;

			pixels += 4;
		}

		y += thread->num_cores;
		count--;
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////

void R_BeginDrawerCommands()
{
	DrawerCommandQueue::Begin();
}

void R_EndDrawerCommands()
{
	DrawerCommandQueue::End();
}

void R_DrawColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnRGBACommand>();
}

void R_FillColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnRGBACommand>();
}

void R_FillAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillAddColumnRGBACommand>();
}

void R_FillAddClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillAddClampColumnRGBACommand>();
}

void R_FillSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillSubClampColumnRGBACommand>();
}

void R_FillRevSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillRevSubClampColumnRGBACommand>();
}

void R_DrawFuzzColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawFuzzColumnRGBACommand>();

	dc_yl = MAX(dc_yl, 1);
	dc_yh = MIN(dc_yh, fuzzviewheight);
	if (dc_yl <= dc_yh)
		fuzzpos = (fuzzpos + dc_yh - dc_yl + 1) % FUZZTABLE;
}

void R_DrawAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawAddColumnRGBACommand>();
}

void R_DrawTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawTranslatedColumnRGBACommand>();
}

void R_DrawTlatedAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawTlatedAddColumnRGBACommand>();
}

void R_DrawShadedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawShadedColumnRGBACommand>();
}

void R_DrawAddClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawAddClampColumnRGBACommand>();
}

void R_DrawAddClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawAddClampTranslatedColumnRGBACommand>();
}

void R_DrawSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSubClampColumnRGBACommand>();
}

void R_DrawSubClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSubClampTranslatedColumnRGBACommand>();
}

void R_DrawRevSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawRevSubClampColumnRGBACommand>();
}

void R_DrawRevSubClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawRevSubClampTranslatedColumnRGBACommand>();
}

void R_DrawSpan_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanRGBACommand>();
}

void R_DrawSpanMasked_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedRGBACommand>();
}

void R_DrawSpanTranslucent_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanTranslucentRGBACommand>();
}

void R_DrawSpanMaskedTranslucent_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentRGBACommand>();
}

void R_DrawSpanAddClamp_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanAddClampRGBACommand>();
}

void R_DrawSpanMaskedAddClamp_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampRGBACommand>();
}

void R_FillSpan_rgba()
{
	DrawerCommandQueue::QueueCommand<FillSpanRGBACommand>();
}

//extern FTexture *rw_pic; // For the asserts below

DWORD vlinec1_rgba()
{
	/*DWORD fracstep = dc_iscale;
	DWORD frac = dc_texturefrac;
	DWORD height = rw_pic->GetHeight();
	assert((frac >> vlinebits) < height);
	frac += (dc_count-1) * fracstep;
	assert((frac >> vlinebits) <= height);*/

	DrawerCommandQueue::QueueCommand<Vlinec1RGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void vlinec4_rgba()
{
	DrawerCommandQueue::QueueCommand<Vlinec4RGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

DWORD mvlinec1_rgba()
{
	DrawerCommandQueue::QueueCommand<Mvlinec1RGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void mvlinec4_rgba()
{
	DrawerCommandQueue::QueueCommand<Mvlinec4RGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_add_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline1AddRGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_add_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline4AddRGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_addclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline1AddClampRGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_addclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline4AddClampRGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_subclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline1SubClampRGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_subclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline4SubClampRGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_revsubclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline1RevSubClampRGBACommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_revsubclamp_rgba()
{
	DrawerCommandQueue::QueueCommand<Tmvline4RevSubClampRGBACommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

void R_DrawFogBoundarySection_rgba(int y, int y2, int x1)
{
	for (; y < y2; ++y)
	{
		int x2 = spanend[y];
		DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, x1, x2);
	}
}

void R_DrawFogBoundary_rgba(int x1, int x2, short *uclip, short *dclip)
{
	// To do: we do not need to create new spans when using rgba output - instead we should calculate light on a per pixel basis

	// This is essentially the same as R_MapVisPlane but with an extra step
	// to create new horizontal spans whenever the light changes enough that
	// we need to use a new colormap.

	double lightstep = rw_lightstep;
	double light = rw_light + rw_lightstep*(x2 - x1 - 1);
	int x = x2 - 1;
	int t2 = uclip[x];
	int b2 = dclip[x];
	int rcolormap = GETPALOOKUP(light, wallshade);
	int lcolormap;
	BYTE *basecolormapdata = basecolormap->Maps;

	if (b2 > t2)
	{
		clearbufshort(spanend + t2, b2 - t2, x);
	}

	R_SetColorMapLight(basecolormap, (float)light, wallshade);

	BYTE *fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

	for (--x; x >= x1; --x)
	{
		int t1 = uclip[x];
		int b1 = dclip[x];
		const int xr = x + 1;
		int stop;

		light -= rw_lightstep;
		lcolormap = GETPALOOKUP(light, wallshade);
		if (lcolormap != rcolormap)
		{
			if (t2 < b2 && rcolormap != 0)
			{ // Colormap 0 is always the identity map, so rendering it is
			  // just a waste of time.
				R_DrawFogBoundarySection_rgba(t2, b2, xr);
			}
			if (t1 < t2) t2 = t1;
			if (b1 > b2) b2 = b1;
			if (t2 < b2)
			{
				clearbufshort(spanend + t2, b2 - t2, x);
			}
			rcolormap = lcolormap;
			R_SetColorMapLight(basecolormap, (float)light, wallshade);
			fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);
		}
		else
		{
			if (fake_dc_colormap != basecolormapdata)
			{
				stop = MIN(t1, b2);
				while (t2 < stop)
				{
					int y = t2++;
					DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
				}
				stop = MAX(b1, t2);
				while (b2 > stop)
				{
					int y = --b2;
					DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
				}
			}
			else
			{
				t2 = MAX(t2, MIN(t1, b2));
				b2 = MIN(b2, MAX(b1, t2));
			}

			stop = MIN(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = MAX(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}
		}

		t2 = uclip[x];
		b2 = dclip[x];
	}
	if (t2 < b2 && rcolormap != 0)
	{
		R_DrawFogBoundarySection_rgba(t2, b2, x1);
	}
}
