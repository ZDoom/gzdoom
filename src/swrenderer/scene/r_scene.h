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

#pragma once

#include <stddef.h>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "r_defs.h"
#include "d_player.h"

extern cycle_t FrameCycles;

namespace swrenderer
{
	extern cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

	class RenderThread;
	
	class RenderScene
	{
	public:
		RenderScene();
		~RenderScene();

		void Init();
		void ScreenResized();
		void Deinit();	

		void SetClearColor(int color);
		
		void RenderView(player_t *player);
		void RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);
	
		bool DontMapLines() const { return dontmaplines; }

		RenderThread *MainThread() { return Threads.front().get(); }

	private:
		void RenderActorView(AActor *actor, bool dontmaplines = false);
		void RenderDrawQueues();
		void RenderThreadSlices();
		void RenderThreadSlice(RenderThread *thread);

		void StartThreads(size_t numThreads);
		void StopThreads();
		
		bool dontmaplines = false;
		int clearcolor = 0;

		std::vector<std::unique_ptr<RenderThread>> Threads;
		std::mutex start_mutex;
		std::condition_variable start_condition;
		bool shutdown_flag = false;
		int run_id = 0;
		std::mutex end_mutex;
		std::condition_variable end_condition;
		size_t finished_threads = 0;
	};
}
