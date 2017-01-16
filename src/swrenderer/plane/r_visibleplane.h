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

class ADynamicLight;
struct FLightNode;
struct FDynamicColormap;
struct FSectorPortal;

namespace swrenderer
{
	struct visplane_light
	{
		ADynamicLight *lightsource;
		visplane_light *next;
	};

	struct visplane_t
	{
		visplane_t *next;		// Next visplane in hash chain -- killough

		FDynamicColormap *colormap;		// [RH] Support multiple colormaps
		FSectorPortal *portal;			// [RH] Support sky boxes
		visplane_light *lights;

		FTransform	xform;
		secplane_t	height;
		FTextureID	picnum;
		int			lightlevel;
		int			left, right;
		int			sky;

		// [RH] This set of variables copies information from the time when the
		// visplane is created. They are only used by stacks so that you can
		// have stacked sectors inside a skybox. If the visplane is not for a
		// stack, then they are unused.
		int			extralight;
		double		visibility;
		DVector3	viewpos;
		DAngle		viewangle;
		fixed_t		Alpha;
		bool		Additive;

		// kg3D - keep track of mirror and skybox owner
		int CurrentSkybox;
		int CurrentPortalUniq; // mirror counter, counts all of them
		int MirrorFlags; // this is not related to CurrentMirror

		unsigned short *bottom;			// [RH] bottom and top arrays are dynamically
		unsigned short pad;				//		allocated immediately after the
		unsigned short top[];			//		visplane.

		void AddLights(FLightNode *node);
		void Render(fixed_t alpha, bool additive, bool masked);
	};
}
