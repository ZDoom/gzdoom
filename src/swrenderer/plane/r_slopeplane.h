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

#include "r_visibleplane.h"

namespace swrenderer
{
	void R_DrawTiltedPlane(visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked);
	void R_MapTiltedPlane(int y, int x1, int x2);
}
