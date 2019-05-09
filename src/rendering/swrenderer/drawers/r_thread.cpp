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

CVAR(Int, r_multithreaded, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
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

	queue->StartThreads();

	// Add to queue and awaken worker threads
	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	std::unique_lock<std::mutex> end_lock(queue->end_mutex);
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
		thread->numa_start_y = thread->numa_node * screen->GetHeight() / thread->num_numa_nodes;
		thread->numa_end_y = (thread->numa_node + 1) * screen->GetHeight() / thread->num_numa_nodes;
		if (thread->poly)
		{
			thread->poly->numa_start_y = thread->numa_start_y;
			thread->poly->numa_end_y = thread->numa_end_y;
		}
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
	std::unique_lock<std::mutex> lock(threads_mutex);

	int num_numathreads = 0;
	for (int i = 0; i < I_GetNumaNodeCount(); i++)
		num_numathreads += I_GetNumaNodeThreadCount(i);

	int num_threads = num_numathreads;
	if (num_threads == 0)
	{
		static bool firstCall = true;
		if (firstCall)
		{
			firstCall = false;
			if (r_multithreaded == 1)
				Printf("Warning: Unable to determine number of CPU cores/threads for this computer. To improve performance, please type 'r_multithreaded x' in the console, where x is the number of threads to use.\n");
		}

		num_threads = 1;
	}

	if (r_multithreaded == 0)
		num_threads = 1;
	else if (r_multithreaded != 1)
		num_threads = r_multithreaded;

	if (num_threads != (int)threads.size())
	{
		StopThreads();

		threads.resize(num_threads);

		if (num_threads == num_numathreads)
		{
			int curThread = 0;
			for (int numaNode = 0; numaNode < I_GetNumaNodeCount(); numaNode++)
			{
				for (int i = 0; i < I_GetNumaNodeThreadCount(numaNode); i++)
				{
					DrawerThreads *queue = this;
					DrawerThread *thread = &threads[curThread++];
					thread->core = i;
					thread->num_cores = I_GetNumaNodeThreadCount(numaNode);
					thread->numa_node = numaNode;
					thread->num_numa_nodes = I_GetNumaNodeCount();
					thread->thread = std::thread([=]() { queue->WorkerMain(thread); });
					I_SetThreadNumaNode(thread->thread, numaNode);
				}
			}
		}
		else
		{
			for (int i = 0; i < num_threads; i++)
			{
				DrawerThreads *queue = this;
				DrawerThread *thread = &threads[i];
				thread->core = i;
				thread->num_cores = num_threads;
				thread->numa_node = 0;
				thread->num_numa_nodes = 1;
				thread->thread = std::thread([=]() { queue->WorkerMain(thread); });
				I_SetThreadNumaNode(thread->thread, 0);
			}
		}
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

/////////////////////////////////////////////////////////////////////////////

DrawerCommandQueue::DrawerCommandQueue(RenderMemory *frameMemory) : FrameMemory(frameMemory)
{
}

void *DrawerCommandQueue::AllocMemory(size_t size)
{
	return FrameMemory->AllocMemory<uint8_t>((int)size);
}

/////////////////////////////////////////////////////////////////////////////

void GroupMemoryBarrierCommand::Execute(DrawerThread *thread)
{
	std::unique_lock<std::mutex> lock(mutex);
	count++;
	condition.notify_all();
	condition.wait(lock, [&]() { return count >= (size_t)thread->num_cores; });
}

/////////////////////////////////////////////////////////////////////////////

MemcpyCommand::MemcpyCommand(void *dest, int destpitch, const void *src, int width, int height, int srcpitch, int pixelsize)
	: dest(dest), src(src), destpitch(destpitch), width(width), height(height), srcpitch(srcpitch), pixelsize(pixelsize)
{
}

void MemcpyCommand::Execute(DrawerThread *thread)
{
	int start = thread->skipped_by_thread(0);
	int count = thread->count_for_thread(0, height);
	int sstep = thread->num_cores * srcpitch * pixelsize;
	int dstep = thread->num_cores * destpitch * pixelsize;
	int size = width * pixelsize;
	uint8_t *d = (uint8_t*)dest + start * destpitch * pixelsize;
	const uint8_t *s = (const uint8_t*)src + start * srcpitch * pixelsize;
	for (int i = 0; i < count; i++)
	{
		memcpy(d, s, size);
		d += dstep;
		s += sstep;
	}
}
