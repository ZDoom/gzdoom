
#pragma once

#include "r_defs.h"

namespace swrenderer
{
	struct FWallCoords;
	struct FWallTmapVals;

	enum class ProjectedWallCull
	{
		Visible,
		OutsideAbove,
		OutsideBelow
	};

	class ProjectedWallLine
	{
	public:
		short ScreenY[MAXWIDTH];

		ProjectedWallCull Project(RenderViewport *viewport, double z1, double z2, const FWallCoords *wallc);
		ProjectedWallCull Project(RenderViewport *viewport, const secplane_t &plane, const FWallCoords *wallc, seg_t *line, bool xflip);
		ProjectedWallCull Project(RenderViewport *viewport, double z, const FWallCoords *wallc);
	};

	class ProjectedWallTexcoords
	{
	public:
		float VStep[MAXWIDTH]; // swall
		fixed_t UPos[MAXWIDTH]; // lwall

		void Project(RenderViewport *viewport, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
		void ProjectPos(RenderViewport *viewport, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
	};
}
