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

#include <stdlib.h>
#include <float.h>



#include "filesystem.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "a_dynlight.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_planerenderer.h"

namespace swrenderer
{
	void PlaneRenderer::RenderLines(VisiblePlane *pl)
	{
		// t1/b1 are at x
		// t2/b2 are at x+1
		// spanend[y] is at the right edge

		int x = pl->right - 1;
		int t2 = pl->top[x];
		int b2 = pl->bottom[x];

		if (b2 > t2)
		{
			fillshort(spanend + t2, b2 - t2, x);
		}

		for (--x; x >= pl->left; --x)
		{
			int t1 = pl->top[x];
			int b1 = pl->bottom[x];
			const int xr = x + 1;
			int stop;

			// Draw any spans that have just closed
			stop = min(t1, b2);
			while (t2 < stop)
			{
				int y = t2++;
				int x2 = spanend[y];
				RenderLine(y, xr, x2);
			}
			stop = max(b1, t2);
			while (b2 > stop)
			{
				int y = --b2;
				int x2 = spanend[y];
				RenderLine(y, xr, x2);
			}

			// Mark any spans that have just opened
			stop = min(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = max(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}

			t2 = pl->top[x];
			b2 = pl->bottom[x];
		}
		// Draw any spans that are still open
		while (t2 < b2)
		{
			int y = --b2;
			int x2 = spanend[y];
			RenderLine(y, pl->left, x2);
		}
	}
}
