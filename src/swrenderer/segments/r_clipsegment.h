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
	typedef bool(*VisibleSegmentCallback)(int x1, int x2);

	void R_ClearClipSegs(short left, short right);
	bool R_ClipWallSegment(int x1, int x2, bool solid, VisibleSegmentCallback callback);
	bool R_CheckClipWallSegment(int first, int last);
	bool R_IsWallSegmentVisible(int x1, int x2);
}
