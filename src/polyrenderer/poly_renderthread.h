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

#pragma once

#include <memory>
#include <thread>
#include "swrenderer/r_memory.h"

class DrawerCommandQueue;
typedef std::shared_ptr<DrawerCommandQueue> DrawerCommandQueuePtr;
class RenderMemory;
class PolyTranslucentObject;
class PolyDrawSectorPortal;
class PolyDrawLinePortal;
class ADynamicLight;

class PolyRenderThread
{
public:
	PolyRenderThread(int threadIndex);
	~PolyRenderThread();

	void FlushDrawQueue();

	int Start = 0;
	int End = 0;
	bool MainThread = false;
	int ThreadIndex = 0;

	std::unique_ptr<RenderMemory> FrameMemory;
	DrawerCommandQueuePtr DrawQueue;

	std::vector<PolyTranslucentObject *> TranslucentObjects;
	std::vector<std::unique_ptr<PolyDrawSectorPortal>> SectorPortals;
	std::vector<std::unique_ptr<PolyDrawLinePortal>> LinePortals;

	TArray<ADynamicLight*> AddedLightsArray;

	// Make sure texture can accessed safely
	void PrepareTexture(FSoftwareTexture *texture, FRenderStyle style);

	// Setup poly object in a threadsafe manner
	void PreparePolyObject(subsector_t *sub);

private:
	std::thread thread;
	std::vector<DrawerCommandQueuePtr> UsedDrawQueues;
	std::vector<DrawerCommandQueuePtr> FreeDrawQueues;

	friend class PolyRenderThreads;
};

class PolyRenderThreads
{
public:
	PolyRenderThreads();
	~PolyRenderThreads();

	void Clear();
	void RenderThreadSlices(int totalcount, std::function<void(PolyRenderThread *)> workerCallback, std::function<void(PolyRenderThread *)> collectCallback);

	PolyRenderThread *MainThread() { return Threads.front().get(); }
	int NumThreads() const { return (int)Threads.size(); }

	std::vector<std::unique_ptr<PolyRenderThread>> Threads;

private:
	void RenderThreadSlice(PolyRenderThread *thread);

	void StartThreads(size_t numThreads);
	void StopThreads();

	std::function<void(PolyRenderThread *)> WorkerCallback;

	std::mutex start_mutex;
	std::condition_variable start_condition;
	bool shutdown_flag = false;
	int run_id = 0;
	std::mutex end_mutex;
	std::condition_variable end_condition;
	size_t finished_threads = 0;
};
