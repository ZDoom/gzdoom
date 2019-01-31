/*
**  Polygon Doom software renderer
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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"

#include "p_lnspec.h"
#include "p_setup.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "poly_renderthread.h"
#include "poly_renderer.h"
#include <mutex>

#ifdef WIN32
void PeekThreadedErrorPane();
#endif

EXTERN_CVAR(Int, r_scene_multithreaded);

PolyRenderThread::PolyRenderThread(int threadIndex) : MainThread(threadIndex == 0), ThreadIndex(threadIndex)
{
	FrameMemory.reset(new RenderMemory());
	DrawQueue = std::make_shared<DrawerCommandQueue>(FrameMemory.get());
}

PolyRenderThread::~PolyRenderThread()
{
}

void PolyRenderThread::FlushDrawQueue()
{
	DrawerThreads::Execute(DrawQueue);

	UsedDrawQueues.push_back(DrawQueue);
	DrawQueue.reset();

	if (!FreeDrawQueues.empty())
	{
		DrawQueue = FreeDrawQueues.back();
		FreeDrawQueues.pop_back();
	}
	else
	{
		DrawQueue = std::make_shared<DrawerCommandQueue>(FrameMemory.get());
	}
}

static std::mutex loadmutex;
void PolyRenderThread::PrepareTexture(FSoftwareTexture *texture, FRenderStyle style)
{
	if (texture == nullptr)
		return;

	// Textures may not have loaded/refreshed yet. The shared code doing
	// this is not thread safe. By calling GetPixels in a mutex lock we
	// make sure that only one thread is loading a texture at any given
	// time.
	//
	// It is critical that this function is called before any direct
	// calls to GetPixels for this to work.

	std::unique_lock<std::mutex> lock(loadmutex);

	const FSoftwareTextureSpan *spans;
	if (PolyRenderer::Instance()->RenderTarget->IsBgra())
	{
		texture->GetPixelsBgra();
		texture->GetColumnBgra(0, &spans);
	}
	else
	{
		bool alpha = !!(style.Flags & STYLEF_RedIsAlpha);
		texture->GetPixels(alpha);
		texture->GetColumn(alpha, 0, &spans);
	}
}

static std::mutex polyobjmutex;
void PolyRenderThread::PreparePolyObject(subsector_t *sub)
{
	std::unique_lock<std::mutex> lock(polyobjmutex);

	if (sub->BSP == nullptr || sub->BSP->bDirty)
	{
		sub->BuildPolyBSP();
	}
}

/////////////////////////////////////////////////////////////////////////////

PolyRenderThreads::PolyRenderThreads()
{
	std::unique_ptr<PolyRenderThread> thread(new PolyRenderThread(0));
	Threads.push_back(std::move(thread));
}

PolyRenderThreads::~PolyRenderThreads()
{
	StopThreads();
}

void PolyRenderThreads::Clear()
{
	for (auto &thread : Threads)
	{
		thread->FrameMemory->Clear();
		thread->DrawQueue->Clear();
		
		while (!thread->UsedDrawQueues.empty())
		{
			auto queue = thread->UsedDrawQueues.back();
			thread->UsedDrawQueues.pop_back();
			queue->Clear();
			thread->FreeDrawQueues.push_back(queue);
		}
	}
}

void PolyRenderThreads::RenderThreadSlices(int totalcount, std::function<void(PolyRenderThread *)> workerCallback, std::function<void(PolyRenderThread *)> collectCallback)
{
	WorkerCallback = workerCallback;

	int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0)
		numThreads = 4;

	if (r_scene_multithreaded == 0 || r_multithreaded == 0)
		numThreads = 1;
	else if (r_scene_multithreaded != 1)
		numThreads = r_scene_multithreaded;

	if (numThreads != (int)Threads.size())
	{
		StopThreads();
		StartThreads(numThreads);
	}

	// Setup threads:
	std::unique_lock<std::mutex> start_lock(start_mutex);
	for (int i = 0; i < numThreads; i++)
	{
		Threads[i]->Start = totalcount * i / numThreads;
		Threads[i]->End = totalcount * (i + 1) / numThreads;
	}
	run_id++;
	start_lock.unlock();

	// Notify threads to run
	if (Threads.size() > 1)
	{
		start_condition.notify_all();
	}

	// Do the main thread ourselves:
	RenderThreadSlice(MainThread());

	// Wait for everyone to finish:
	if (Threads.size() > 1)
	{
		using namespace std::chrono_literals;
		std::unique_lock<std::mutex> end_lock(end_mutex);
		finished_threads++;
		if (!end_condition.wait_for(end_lock, 5s, [&]() { return finished_threads == Threads.size(); }))
		{
#ifdef WIN32
			PeekThreadedErrorPane();
#endif
			// Invoke the crash reporter so that we can capture the call stack of whatever the hung worker thread is doing
			int *threadCrashed = nullptr;
			*threadCrashed = 0xdeadbeef;
		}
		finished_threads = 0;
	}

	for (int i = 0; i < numThreads; i++)
	{
		Threads[i]->FlushDrawQueue();
	}

	WorkerCallback = {};

	for (int i = 1; i < numThreads; i++)
	{
		collectCallback(Threads[i].get());
	}
}

void PolyRenderThreads::RenderThreadSlice(PolyRenderThread *thread)
{
	WorkerCallback(thread);
}

void PolyRenderThreads::StartThreads(size_t numThreads)
{
	while (Threads.size() < (size_t)numThreads)
	{
		std::unique_ptr<PolyRenderThread> thread(new PolyRenderThread((int)Threads.size()));
		auto renderthread = thread.get();
		int start_run_id = run_id;
		thread->thread = std::thread([=]()
		{
			int last_run_id = start_run_id;
			while (true)
			{
				// Wait until we are signalled to run:
				std::unique_lock<std::mutex> start_lock(start_mutex);
				start_condition.wait(start_lock, [&]() { return run_id != last_run_id || shutdown_flag; });
				if (shutdown_flag)
					break;
				last_run_id = run_id;
				start_lock.unlock();

				RenderThreadSlice(renderthread);

				// Notify main thread that we finished:
				std::unique_lock<std::mutex> end_lock(end_mutex);
				finished_threads++;
				end_lock.unlock();
				end_condition.notify_all();
			}
		});
		Threads.push_back(std::move(thread));
	}
}

void PolyRenderThreads::StopThreads()
{
	std::unique_lock<std::mutex> lock(start_mutex);
	shutdown_flag = true;
	lock.unlock();
	start_condition.notify_all();
	while (Threads.size() > 1)
	{
		Threads.back()->thread.join();
		Threads.pop_back();
	}
	lock.lock();
	shutdown_flag = false;
}
