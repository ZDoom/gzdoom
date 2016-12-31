
#pragma once

#include "r_visibleplane.h"

namespace swrenderer
{
	void R_DrawTiltedPlane(visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked);
	void R_MapTiltedPlane(int y, int x1, int x2);
}
