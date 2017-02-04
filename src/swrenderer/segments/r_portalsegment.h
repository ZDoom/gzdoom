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
