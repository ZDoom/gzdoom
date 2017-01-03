
#pragma once

#include "r_defs.h"

namespace swrenderer
{
	struct FWallCoords;
	struct FWallTmapVals;

	extern short walltop[MAXWIDTH];
	extern short wallbottom[MAXWIDTH];
	extern short wallupper[MAXWIDTH];
	extern short walllower[MAXWIDTH];
	extern float swall[MAXWIDTH];
	extern fixed_t lwall[MAXWIDTH];
	extern double lwallscale;

	int R_CreateWallSegmentY(short *outbuf, double z1, double z2, const FWallCoords *wallc);
	int R_CreateWallSegmentYSloped(short *outbuf, const secplane_t &plane, const FWallCoords *wallc, seg_t *line, bool xflip);
	int R_CreateWallSegmentY(short *outbuf, double z, const FWallCoords *wallc);

	void PrepWall(float *swall, fixed_t *lwall, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
	void PrepLWall(fixed_t *lwall, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
}
