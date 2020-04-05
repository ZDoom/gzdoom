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

#pragma once

namespace swrenderer
{
	class RenderThread;
	
	/* portal structure, this is used in r_ code in order to store drawsegs with portals (and mirrors) */
	struct PortalDrawseg
	{
		PortalDrawseg(RenderThread *thread, line_t *linedef, int x1, int x2, const short *topclip, const short *bottomclip);

		line_t* src = nullptr; // source line (the one drawn) this doesn't change over render loops
		line_t* dst = nullptr; // destination line (the one that the portal is linked with, equals 'src' for mirrors)

		int x1 = 0; // drawseg x1
		int x2 = 0; // drawseg x2

		int len = 0;
		short *ceilingclip = nullptr;
		short *floorclip = nullptr;

		bool mirror = false; // true if this is a mirror (src should equal dst)
	};
}
