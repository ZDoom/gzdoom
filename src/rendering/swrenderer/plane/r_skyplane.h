//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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

		double		skytexturemid;
		double		skyscale;
		float		skyiscale;
		fixed_t		sky1cyl, sky2cyl;

		FSoftwareTexture *frontskytex = nullptr;
		FSoftwareTexture *backskytex = nullptr;
		angle_t skyflip = 0;
		int frontpos = 0;
		int backpos = 0;
		fixed_t frontcyl = 0;
		fixed_t backcyl = 0;
		double skymid = 0.0;
		angle_t skyangle = 0;
		int skyoffset = 0;

		SkyDrawerArgs drawerargs;
	};
}
