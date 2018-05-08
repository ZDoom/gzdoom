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

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_thread.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"
#include <chrono>

#ifdef WIN32
void PeekThreadedErrorPane();
#endif

CVAR(Bool, r_multithreaded, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Int, r_debug_draw, 0, 0);

/////////////////////////////////////////////////////////////////////////////

DrawerThreads *DrawerThreads::Instance()
{
	static DrawerThreads threads;
	return &threads;
}

DrawerThreads::DrawerThreads()
{
}

DrawerThreads::~DrawerThreads()
{
	StopThreads();
}

void DrawerThreads::Execute(DrawerCommandQueuePtr commands)
{
	if (!commands || commands->commands.empty())
		return;
	
	auto queue = Instance();

	// Add to queue and awaken worker threads
	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	std::unique_lock<std::mutex> end_lock(queue->end_mutex);
	queue->StartThreads();
	queue->active_commands.push_back(commands);
	queue->tasks_left += queue->threads.size();
	end_lock.unlock();
	start_lock.unlock();
	queue->start_condition.notify_all();
}

void DrawerThreads::ResetDebugDrawPos()
{
	auto queue = Instance();
	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	bool reached_end = false;
	for (auto &thread : queue->threads)
	{
		if (thread.debug_draw_pos + r_debug_draw * 60 * 2 < queue->debug_draw_end)
			reached_end = true;
		thread.debug_draw_pos = 0;
	}
	if (!reached_end)
		queue->debug_draw_end += r_debug_draw;
	else
		queue->debug_draw_end = 0;
}

void DrawerThreads::WaitForWorkers()
{
	using namespace std::chrono_literals;

	// Wait for workers to finish
	auto queue = Instance();
	std::unique_lock<std::mutex> end_lock(queue->end_mutex);
	if (!queue->end_condition.wait_for(end_lock, 5s, [&]() { return queue->tasks_left == 0; }))
	{
#ifdef WIN32
		PeekThreadedErrorPane();
#endif
		// Invoke the crash reporter so that we can capture the call stack of whatever the hung worker thread is doing
		int *threadCrashed = nullptr;
		*threadCrashed = 0xdeadbeef;
	}
	end_lock.unlock();

	// Clean up
	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	for (auto &thread : queue->threads)
		thread.current_queue = 0;

	for (auto &list : queue->active_commands)
	{
		for (auto &command : list->commands)
			command->~DrawerCommand();
		list->Clear();
	}
	queue->active_commands.clear();
}

void DrawerThreads::WorkerMain(DrawerThread *thread)
{
	while (true)
	{
		// Wait until we are signalled to run:
		std::unique_lock<std::mutex> start_lock(start_mutex);
		start_condition.wait(start_lock, [&]() { return thread->current_queue < active_commands.size() || shutdown_flag; });
		if (shutdown_flag)
			break;

		// Grab the commands
		DrawerCommandQueuePtr list = active_commands[thread->current_queue];
		thread->current_queue++;
		start_lock.unlock();

		// Do the work:
		if (r_debug_draw)
		{
			for (auto& command : list->commands)
			{
				thread->debug_draw_pos++;
				if (thread->debug_draw_pos < debug_draw_end)
					command->Execute(thread);
			}
		}
		else
		{
			for (auto& command : list->commands)
			{
				command->Execute(thread);
			}
		}

		// Notify main thread that we finished:
		std::unique_lock<std::mutex> end_lock(end_mutex);
		tasks_left--;
		bool finishedTasks = tasks_left == 0;
		end_lock.unlock();
		if (finishedTasks)
			end_condition.notify_all();
	}
}

void DrawerThreads::StartThreads()
{
	if (!threads.empty())
		return;

	int num_threads = std::thread::hardware_concurrency();
	if (num_threads == 0)
		num_threads = 4;

	threads.resize(num_threads);

	for (int i = 0; i < num_threads; i++)
	{
		DrawerThreads *queue = this;
		DrawerThread *thread = &threads[i];
		thread->core = i;
		thread->num_cores = num_threads;
		thread->thread = std::thread([=]() { queue->WorkerMain(thread); });
	}
}

void DrawerThreads::StopThreads()
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

#ifndef WIN32

void VectoredTryCatch(void *data, void(*tryBlock)(void *data), void(*catchBlock)(void *data, const char *reason, bool fatal))
{
	tryBlock(data);
}

#endif

DrawerCommandQueue::DrawerCommandQueue(RenderMemory *frameMemory) : FrameMemory(frameMemory)
{
}

void *DrawerCommandQueue::AllocMemory(size_t size)
{
	return FrameMemory->AllocMemory<uint8_t>((int)size);
}
