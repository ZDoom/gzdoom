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

CVAR(Bool, r_multithreaded, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

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

void DrawerThreads::Execute(const std::vector<DrawerCommandQueuePtr> &queues)
{
	bool hasWork = false;
	for (const auto &queue : queues)
		hasWork = hasWork || !queue->commands.empty();
	if (!hasWork)
		return;
	
	auto queue = Instance();

	// Give worker threads something to do:

	std::unique_lock<std::mutex> start_lock(queue->start_mutex);
	queue->active_commands = queues;
	queue->run_id++;
	start_lock.unlock();

	queue->StartThreads();
	queue->start_condition.notify_all();

	// Do one thread ourselves:

	static DrawerThread thread;
	thread.core = 0;
	thread.num_cores = (int)(queue->threads.size() + 1);

	struct TryCatchData
	{
		DrawerThreads *queue;
		DrawerThread *thread;
		size_t list_index;
		size_t command_index;
	} data;

	data.queue = queue;
	data.thread = &thread;
	data.list_index = 0;
	data.command_index = 0;
	VectoredTryCatch(&data,
	[](void *data)
	{
		TryCatchData *d = (TryCatchData*)data;

		for (int pass = 0; pass < d->queue->num_passes; pass++)
		{
			d->thread->pass_start_y = pass * d->queue->rows_in_pass;
			d->thread->pass_end_y = (pass + 1) * d->queue->rows_in_pass;
			if (pass + 1 == d->queue->num_passes)
				d->thread->pass_end_y = MAX(d->thread->pass_end_y, MAXHEIGHT);

			for (auto &list : d->queue->active_commands)
			{
				size_t size = list->commands.size();
				for (d->command_index = 0; d->command_index < size; d->command_index++)
				{
					auto &command = list->commands[d->command_index];
					command->Execute(d->thread);
				}
				d->list_index++;
			}
		}
	},
	[](void *data, const char *reason, bool fatal)
	{
		TryCatchData *d = (TryCatchData*)data;
		ReportDrawerError(d->queue->active_commands[d->list_index]->commands[d->command_index], true, reason, fatal);
	});

	// Wait for everyone to finish:

	std::unique_lock<std::mutex> end_lock(queue->end_mutex);
	queue->end_condition.wait(end_lock, [&]() { return queue->finished_threads == queue->threads.size(); });

	if (!queue->thread_error.IsEmpty())
	{
		static bool first = true;
		if (queue->thread_error_fatal)
			I_FatalError("%s", queue->thread_error.GetChars());
		else if (first)
			Printf("%s\n", queue->thread_error.GetChars());
		first = false;
	}

	// Clean up batch:

	for (auto &list : queue->active_commands)
	{
		for (auto &command : list->commands)
			command->~DrawerCommand();
		list->Clear();
	}
	queue->active_commands.clear();
	queue->finished_threads = 0;
}

void DrawerThreads::StartThreads()
{
	if (!threads.empty())
		return;

	int num_threads = std::thread::hardware_concurrency();
	if (num_threads == 0)
		num_threads = 4;

	threads.resize(num_threads - 1);

	for (int i = 0; i < num_threads - 1; i++)
	{
		DrawerThreads *queue = this;
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

				struct TryCatchData
				{
					DrawerThreads *queue;
					DrawerThread *thread;
					size_t list_index;
					size_t command_index;
				} data;

				data.queue = queue;
				data.thread = thread;
				data.list_index = 0;
				data.command_index = 0;
				VectoredTryCatch(&data,
				[](void *data)
				{
					TryCatchData *d = (TryCatchData*)data;

					for (int pass = 0; pass < d->queue->num_passes; pass++)
					{
						d->thread->pass_start_y = pass * d->queue->rows_in_pass;
						d->thread->pass_end_y = (pass + 1) * d->queue->rows_in_pass;
						if (pass + 1 == d->queue->num_passes)
							d->thread->pass_end_y = MAX(d->thread->pass_end_y, MAXHEIGHT);

						for (auto &list : d->queue->active_commands)
						{
							size_t size = list->commands.size();
							for (d->command_index = 0; d->command_index < size; d->command_index++)
							{
								auto &command = list->commands[d->command_index];
								command->Execute(d->thread);
							}
							d->list_index++;
						}
					}
				},
				[](void *data, const char *reason, bool fatal)
				{
					TryCatchData *d = (TryCatchData*)data;
					ReportDrawerError(d->queue->active_commands[d->list_index]->commands[d->command_index], true, reason, fatal);
				});

				// Notify main thread that we finished:
				std::unique_lock<std::mutex> end_lock(queue->end_mutex);
				queue->finished_threads++;
				end_lock.unlock();
				queue->end_condition.notify_all();
			}
		});
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

void DrawerThreads::ReportDrawerError(DrawerCommand *command, bool worker_thread, const char *reason, bool fatal)
{
	if (worker_thread)
	{
		std::unique_lock<std::mutex> end_lock(Instance()->end_mutex);
		if (Instance()->thread_error.IsEmpty() || (!Instance()->thread_error_fatal && fatal))
		{
			Instance()->thread_error = reason + (FString)": " + command->DebugInfo();
			Instance()->thread_error_fatal = fatal;
		}
	}
	else
	{
		static bool first = true;
		if (fatal)
			I_FatalError("%s: %s", reason, command->DebugInfo().GetChars());
		else if (first)
			Printf("%s: %s\n", reason, command->DebugInfo().GetChars());
		first = false;
	}
}

#ifndef WIN32

void VectoredTryCatch(void *data, void(*tryBlock)(void *data), void(*catchBlock)(void *data, const char *reason, bool fatal))
{
	tryBlock(data);
}

#endif

DrawerCommandQueue::DrawerCommandQueue(swrenderer::RenderThread *renderthread) : renderthread(renderthread)
{
}

void *DrawerCommandQueue::AllocMemory(size_t size)
{
	return renderthread->FrameMemory->AllocMemory<uint8_t>((int)size);
}
