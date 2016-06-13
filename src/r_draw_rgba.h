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
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAW_RGBA__
#define __R_DRAW_RGBA__

#include "r_draw.h"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

/////////////////////////////////////////////////////////////////////////////
// Drawer functions:

void rt_initcols_rgba(BYTE *buffer);
void rt_span_coverage_rgba(int x, int start, int stop);

void rt_copy1col_rgba(int hx, int sx, int yl, int yh);
void rt_copy4cols_rgba(int sx, int yl, int yh);
void rt_shaded1col_rgba(int hx, int sx, int yl, int yh);
void rt_shaded4cols_rgba(int sx, int yl, int yh);
void rt_map1col_rgba(int hx, int sx, int yl, int yh);
void rt_add1col_rgba(int hx, int sx, int yl, int yh);
void rt_addclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_subclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_revsubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlate1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlateadd1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlateaddclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlatesubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlaterevsubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_map4cols_rgba(int sx, int yl, int yh);
void rt_add4cols_rgba(int sx, int yl, int yh);
void rt_addclamp4cols_rgba(int sx, int yl, int yh);
void rt_subclamp4cols_rgba(int sx, int yl, int yh);
void rt_revsubclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlate4cols_rgba(int sx, int yl, int yh);
void rt_tlateadd4cols_rgba(int sx, int yl, int yh);
void rt_tlateaddclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlatesubclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlaterevsubclamp4cols_rgba(int sx, int yl, int yh);

void R_DrawColumnHoriz_rgba();
void R_DrawColumn_rgba();
void R_DrawFuzzColumn_rgba();
void R_DrawTranslatedColumn_rgba();
void R_DrawShadedColumn_rgba();

void R_FillColumn_rgba();
void R_FillAddColumn_rgba();
void R_FillAddClampColumn_rgba();
void R_FillSubClampColumn_rgba();
void R_FillRevSubClampColumn_rgba();
void R_DrawAddColumn_rgba();
void R_DrawTlatedAddColumn_rgba();
void R_DrawAddClampColumn_rgba();
void R_DrawAddClampTranslatedColumn_rgba();
void R_DrawSubClampColumn_rgba();
void R_DrawSubClampTranslatedColumn_rgba();
void R_DrawRevSubClampColumn_rgba();
void R_DrawRevSubClampTranslatedColumn_rgba();

void R_DrawSpan_rgba(void);
void R_DrawSpanMasked_rgba(void);
void R_DrawSpanTranslucent_rgba();
void R_DrawSpanMaskedTranslucent_rgba();
void R_DrawSpanAddClamp_rgba();
void R_DrawSpanMaskedAddClamp_rgba();
void R_FillSpan_rgba();

void R_DrawFogBoundary_rgba(int x1, int x2, short *uclip, short *dclip);

DWORD vlinec1_rgba();
void vlinec4_rgba();
DWORD mvlinec1_rgba();
void mvlinec4_rgba();
fixed_t tmvline1_add_rgba();
void tmvline4_add_rgba();
fixed_t tmvline1_addclamp_rgba();
void tmvline4_addclamp_rgba();
fixed_t tmvline1_subclamp_rgba();
void tmvline4_subclamp_rgba();
fixed_t tmvline1_revsubclamp_rgba();
void tmvline4_revsubclamp_rgba();

void R_FillColumnHoriz_rgba();
void R_FillSpan_rgba();

/////////////////////////////////////////////////////////////////////////////
// Multithreaded rendering infrastructure:

// Redirect drawer commands to worker threads
void R_BeginDrawerCommands();

// Wait until all drawers finished executing
void R_EndDrawerCommands();

struct FSpecialColormap;
class DrawerCommandQueue;

// Worker data for each thread executing drawer commands
class DrawerThread
{
public:
	std::thread thread;

	// Thread line index of this thread
	int core = 0;

	// Number of active threads
	int num_cores = 1;

	// Range of rows processed this pass
	int pass_start_y = 0;
	int pass_end_y = MAXHEIGHT;

	uint32_t dc_temp_rgbabuff_rgba[MAXHEIGHT * 4];
	uint32_t *dc_temp_rgba;

	// Checks if a line is rendered by this thread
	bool line_skipped_by_thread(int line)
	{
		return line < pass_start_y || line >= pass_end_y || line % num_cores != core;
	}

	// The number of lines to skip to reach the first line to be rendered by this thread
	int skipped_by_thread(int first_line)
	{
		int pass_skip = MAX(pass_start_y - first_line, 0);
		int core_skip = (num_cores - (first_line + pass_skip - core) % num_cores) % num_cores;
		return pass_skip + core_skip;
	}

	// The number of lines to be rendered by this thread
	int count_for_thread(int first_line, int count)
	{
		int lines_until_pass_end = MAX(pass_end_y - first_line, 0);
		count = MIN(count, lines_until_pass_end);
		int c = (count - skipped_by_thread(first_line) + num_cores - 1) / num_cores;
		return MAX(c, 0);
	}

	// Calculate the dest address for the first line to be rendered by this thread
	uint32_t *dest_for_thread(int first_line, int pitch, uint32_t *dest)
	{
		return dest + skipped_by_thread(first_line) * pitch;
	}
};

// Task to be executed by each worker thread
class DrawerCommand
{
protected:
	int dc_dest_y;

public:
	DrawerCommand()
	{
		dc_dest_y = static_cast<int>((dc_dest - dc_destorg) / (dc_pitch * 4));
	}

	virtual void Execute(DrawerThread *thread) = 0;
};

// Manages queueing up commands and executing them on worker threads
class DrawerCommandQueue
{
	enum { memorypool_size = 4 * 1024 * 1024 };
	char memorypool[memorypool_size];
	size_t memorypool_pos = 0;

	std::vector<DrawerCommand *> commands;

	std::vector<DrawerThread> threads;

	std::mutex start_mutex;
	std::condition_variable start_condition;
	std::vector<DrawerCommand *> active_commands;
	bool shutdown_flag = false;
	int run_id = 0;

	std::mutex end_mutex;
	std::condition_variable end_condition;
	size_t finished_threads = 0;

	int threaded_render = 0;
	DrawerThread single_core_thread;
	int num_passes = 2;
	int rows_in_pass = 540;

	void StartThreads();
	void StopThreads();
	void Finish();

	static DrawerCommandQueue *Instance();

	~DrawerCommandQueue();

public:
	// Allocate memory valid for the duration of a command execution
	static void* AllocMemory(size_t size);

	// Queue command to be executed by drawer worker threads
	template<typename T, typename... Types>
	static void QueueCommand(Types &&... args)
	{
		auto queue = Instance();
		if (queue->threaded_render == 0)
		{
			T command(std::forward<Types>(args)...);
			command.Execute(&queue->single_core_thread);
		}
		else
		{
			void *ptr = AllocMemory(sizeof(T));
			if (!ptr)
				return;
			T *command = new (ptr)T(std::forward<Types>(args)...);
			queue->commands.push_back(command);
		}
	}

	// Redirects all drawing commands to worker threads until End is called
	// Begin/End blocks can be nested.
	static void Begin();

	// End redirection and wait until all worker threads finished executing
	static void End();

	// Waits until all worker threads finished executing
	static void WaitForWorkers();
};

/////////////////////////////////////////////////////////////////////////////
// Drawer commands:

class ApplySpecialColormapRGBACommand : public DrawerCommand
{
	BYTE *buffer;
	int pitch;
	int width;
	int height;
	int start_red;
	int start_green;
	int start_blue;
	int end_red;
	int end_green;
	int end_blue;

public:
	ApplySpecialColormapRGBACommand(FSpecialColormap *colormap, DFrameBuffer *screen);
	void Execute(DrawerThread *thread) override;
};

#endif
