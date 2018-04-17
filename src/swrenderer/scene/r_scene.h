//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

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
	extern cycle_t WallCycles, PlaneCycles, MaskedCycles, DrawerWaitCycles;

	class RenderThread;
	
	class RenderScene
	{
	public:
		RenderScene();
		~RenderScene();

		void Deinit();	

		void SetClearColor(int color);
		
		void RenderView(player_t *player, DCanvas *target);
		void RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);
	
		bool DontMapLines() const { return dontmaplines; }

		RenderThread *MainThread() { return Threads.front().get(); }

	private:
		void RenderActorView(AActor *actor, bool dontmaplines = false);
		void RenderThreadSlices();
		void RenderThreadSlice(RenderThread *thread);
		void RenderPSprites();

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
