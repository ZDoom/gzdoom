
#pragma once

#include "r_visibleplane.h"

namespace swrenderer
{
	void R_DrawSkyPlane(visplane_t *pl);

	void R_DrawSky(visplane_t *pl);
	void R_DrawSkyStriped(visplane_t *pl);
	void R_DrawCapSky(visplane_t *pl);
	void R_DrawSkyColumnStripe(int start_x, int y1, int y2, int columns, double scale, double texturemid, double yrepeat);
	void R_DrawSkyColumn(int start_x, int y1, int y2, int columns);

	const uint8_t *R_GetOneSkyColumn(FTexture *fronttex, int x);
	const uint8_t *R_GetTwoSkyColumns(FTexture *fronttex, int x);
}
