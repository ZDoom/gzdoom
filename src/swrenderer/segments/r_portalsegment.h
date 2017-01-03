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
	/* portal structure, this is used in r_ code in order to store drawsegs with portals (and mirrors) */
	struct PortalDrawseg
	{
		line_t* src; // source line (the one drawn) this doesn't change over render loops
		line_t* dst; // destination line (the one that the portal is linked with, equals 'src' for mirrors)

		int x1; // drawseg x1
		int x2; // drawseg x2

		int len;
		TArray<short> ceilingclip;
		TArray<short> floorclip;

		bool mirror; // true if this is a mirror (src should equal dst)
	};
	
	extern TArray<PortalDrawseg> WallPortals;
}
