/*
**  Renderer multithreading framework
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

#include "r_draw.h"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

// Use multiple threads when drawing
EXTERN_CVAR(Bool, r_multithreaded)

// Redirect drawer commands to worker threads
void R_BeginDrawerCommands();

// Wait until all drawers finished executing
void R_EndDrawerCommands();

// Worker data for each thread executing drawer commands
class DrawerThread
{
public:
	DrawerThread()
	{
		dc_temp = dc_temp_buff;
		dc_temp_rgba = dc_temp_rgbabuff_rgba;
	}

	std::thread thread;

	// Thread line index of this thread
	int core = 0;

	// Number of active threads
	int num_cores = 1;

	// Range of rows processed this pass
	int pass_start_y = 0;
	int pass_end_y = MAXHEIGHT;

	// Working buffer used by Rt drawers
	uint8_t dc_temp_buff[MAXHEIGHT * 4];
	uint8_t *dc_temp = nullptr;

	// Working buffer used by Rt drawers, true color edition
	uint32_t dc_temp_rgbabuff_rgba[MAXHEIGHT * 4];
	uint32_t *dc_temp_rgba = nullptr;

	// Working buffer used by the tilted (sloped) span drawer
	const uint8_t *tiltlighting[MAXWIDTH];

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
	template<typename T>
	T *dest_for_thread(int first_line, int pitch, T *dest)
	{
		return dest + skipped_by_thread(first_line) * pitch;
	}

	// The first line in the dc_temp buffer used this thread
	int temp_line_for_thread(int first_line)
	{
		return (first_line + skipped_by_thread(first_line)) / num_cores;
	}
};

// Task to be executed by each worker thread
class DrawerCommand
{
protected:
	int _dest_y;

	void DetectRangeError(uint32_t *&dest, int &dest_y, int &count)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		if (dest_y < 0 || count < 0 || dest_y + count > swrenderer::drawerargs::dc_destheight)
			__debugbreak(); // Buffer overrun detected!
#endif

		if (dest_y < 0)
		{
			count += dest_y;
			dest_y = 0;
			dest = (uint32_t*)swrenderer::drawerargs::dc_destorg;
		}
		else if (dest_y >= swrenderer::drawerargs::dc_destheight)
		{
			dest_y = 0;
			count = 0;
		}

		if (count < 0 || count > MAXHEIGHT) count = 0;
		if (dest_y + count >= swrenderer::drawerargs::dc_destheight)
			count = swrenderer::drawerargs::dc_destheight - dest_y;
	}

public:
	DrawerCommand()
	{
		_dest_y = static_cast<int>((swrenderer::drawerargs::dc_dest - swrenderer::drawerargs::dc_destorg) / (swrenderer::drawerargs::dc_pitch));
	}
	
	virtual ~DrawerCommand() { }

	virtual void Execute(DrawerThread *thread) = 0;
	virtual FString DebugInfo() = 0;
};

void VectoredTryCatch(void *data, void(*tryBlock)(void *data), void(*catchBlock)(void *data, const char *reason, bool fatal));

// Manages queueing up commands and executing them on worker threads
class DrawerCommandQueue
{
	enum { memorypool_size = 16 * 1024 * 1024 };
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
	FString thread_error;
	bool thread_error_fatal = false;

	int threaded_render = 0;
	DrawerThread single_core_thread;
	int num_passes = 1;
	int rows_in_pass = MAXHEIGHT;

	void StartThreads();
	void StopThreads();
	void Finish();

	static DrawerCommandQueue *Instance();
	static void ReportDrawerError(DrawerCommand *command, bool worker_thread, const char *reason, bool fatal);

	DrawerCommandQueue();
	~DrawerCommandQueue();

public:
	// Allocate memory valid for the duration of a command execution
	static void* AllocMemory(size_t size);

	// Queue command to be executed by drawer worker threads
	template<typename T, typename... Types>
	static void QueueCommand(Types &&... args)
	{
		auto queue = Instance();
		if (queue->threaded_render == 0 || !r_multithreaded)
		{
			T command(std::forward<Types>(args)...);
			VectoredTryCatch(&command,
			[](void *data)
			{
				T *c = (T*)data;
				c->Execute(&Instance()->single_core_thread);
			},
			[](void *data, const char *reason, bool fatal)
			{
				T *c = (T*)data;
				ReportDrawerError(c, false, reason, fatal);
			});
		}
		else
		{
			void *ptr = AllocMemory(sizeof(T));
			if (!ptr) // Out of memory - render what we got
			{
				queue->Finish();
				ptr = AllocMemory(sizeof(T));
				if (!ptr)
					return;
			}
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
