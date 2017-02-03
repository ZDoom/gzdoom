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
#include "swrenderer/viewport/r_skydrawer.h"

namespace swrenderer
{
	class RenderSkyPlane
	{
	public:
		RenderSkyPlane(RenderThread *thread);

		void Render(VisiblePlane *pl);

		RenderThread *Thread = nullptr;

	private:
		void DrawSky(VisiblePlane *pl);
		void DrawSkyColumnStripe(int start_x, int y1, int y2, double scale, double texturemid, double yrepeat);
		void DrawSkyColumn(int start_x, int y1, int y2);

		FTexture *frontskytex = nullptr;
		FTexture *backskytex = nullptr;
		angle_t skyflip = 0;
		int frontpos = 0;
		int backpos = 0;
		fixed_t frontcyl = 0;
		fixed_t backcyl = 0;
		double skymid = 0.0;
		angle_t skyangle = 0;

		SkyDrawerArgs drawerargs;
	};
}
