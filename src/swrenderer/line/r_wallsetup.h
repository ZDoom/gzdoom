
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

		ProjectedWallCull Project(double z1, double z2, const FWallCoords *wallc);
		ProjectedWallCull Project(const secplane_t &plane, const FWallCoords *wallc, seg_t *line, bool xflip);
		ProjectedWallCull Project(double z, const FWallCoords *wallc);
	};

	class ProjectedWallTexcoords
	{
	public:
		float VStep[MAXWIDTH]; // swall
		fixed_t UPos[MAXWIDTH]; // lwall

		void Project(double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
		void ProjectPos(double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
	};
}
