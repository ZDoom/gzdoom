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
//

#pragma once

#include <stddef.h>
#include "r_defs.h"
#include "r_state.h"
#include "r_memory.h"

struct FDynamicLight;
struct FLightNode;
struct FDynamicColormap;
struct FSectorPortal;

namespace swrenderer
{
	class RenderThread;

	struct VisiblePlaneLight
	{
		FDynamicLight *lightsource;
		VisiblePlaneLight *next;
	};

	struct VisiblePlane
	{
		VisiblePlane(RenderThread *thread);

		void AddLights(RenderThread *thread, FLightNode *node);
		void Render(RenderThread *thread, fixed_t alpha, bool additive, bool masked);

		VisiblePlane *next = nullptr;		// Next visplane in hash chain -- killough

		FDynamicColormap *colormap = nullptr;		// [RH] Support multiple colormaps
		FSectorPortal *portal = nullptr;			// [RH] Support sky boxes
		VisiblePlaneLight *lights = nullptr;

		FTransform	xform;
		secplane_t	height;
		FTextureID	picnum;
		int lightlevel = 0;
		int left = viewwidth;
		int right = 0;
		int sky = 0;

		// [RH] This set of variables copies information from the time when the
		// visplane is created. They are only used by stacks so that you can
		// have stacked sectors inside a skybox. If the visplane is not for a
		// stack, then they are unused.
		int			extralight = 0;
		double		visibility = 0.0;
		DVector3	viewpos = { 0.0, 0.0, 0.0 };
		DAngle		viewangle = nullAngle;
		fixed_t		Alpha = 0;
		bool		Additive = false;

		// kg3D - keep track of mirror and skybox owner
		int CurrentSkybox = 0;
		int CurrentPortalUniq = 0; // mirror counter, counts all of them
		int MirrorFlags = 0; // this is not related to CurrentMirror

		uint16_t *bottom = nullptr;
		uint16_t *top = nullptr;
	};
}
