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
	class RenderSkyPlane
	{
	public:
		static void Render(visplane_t *pl);

	private:
		static void DrawSky(visplane_t *pl);
		static void DrawSkyStriped(visplane_t *pl);
		static void DrawCapSky(visplane_t *pl);
		static void DrawSkyColumnStripe(int start_x, int y1, int y2, int columns, double scale, double texturemid, double yrepeat);
		static void DrawSkyColumn(int start_x, int y1, int y2, int columns);

		static const uint8_t *GetOneSkyColumn(FTexture *fronttex, int x);
		static const uint8_t *GetTwoSkyColumns(FTexture *fronttex, int x);

		static FTexture *frontskytex;
		static FTexture *backskytex;
		static angle_t skyflip;
		static int frontpos;
		static int backpos;
		static double frontyScale;
		static fixed_t frontcyl;
		static fixed_t backcyl;
		static double skymid;
		static angle_t skyangle;
		static double frontiScale;

		// Allow for layer skies up to 512 pixels tall. This is overkill,
		// since the most anyone can ever see of the sky is 500 pixels.
		// We need 4 skybufs because R_DrawSkySegment can draw up to 4 columns at a time.
		// Need two versions - one for true color and one for palette
		enum { MAXSKYBUF = 3072 };
		static uint8_t skybuf[4][512];
		static uint32_t skybuf_bgra[MAXSKYBUF][512];
		static uint32_t lastskycol[4];
		static uint32_t lastskycol_bgra[MAXSKYBUF];
		static int skycolplace;
		static int skycolplace_bgra;
	};
}
