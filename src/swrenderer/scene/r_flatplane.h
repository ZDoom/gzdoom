
#pragma once

#include "r_visible_plane.h"

namespace swrenderer
{
	void R_SetupPlaneSlope();

	void R_DrawNormalPlane(visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked);
	void R_MapPlane(int y, int x1, int x2);
	void R_StepPlane();

	void R_DrawColoredPlane(visplane_t *pl);
	void R_MapColoredPlane(int y, int x1, int x2);
}
