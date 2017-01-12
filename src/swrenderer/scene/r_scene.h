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
#include "r_defs.h"
#include "d_player.h"

extern cycle_t FrameCycles;

namespace swrenderer
{
	extern cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

	extern bool r_dontmaplines;

	void R_RenderActorView(AActor *actor, bool dontmaplines = false);
	void R_RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);

	void R_InitRenderer();
	void R_MultiresInit();
	void R_ShutdownRenderer();
}
