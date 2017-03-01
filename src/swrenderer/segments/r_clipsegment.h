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

	class VisibleSegmentRenderer
	{
	public:
		virtual ~VisibleSegmentRenderer() { }
		virtual bool RenderWallSegment(int x1, int x2) { return true; }
	};

	class RenderClipSegment
	{
	public:
		void Clear(short left, short right);
		bool Clip(int x1, int x2, bool solid, VisibleSegmentRenderer *visitor);
		bool Check(int first, int last);
		bool IsVisible(int x1, int x2);
		
	private:
		struct cliprange_t
		{
			short first, last;
		};

		cliprange_t *newend; // newend is one past the last valid seg
		cliprange_t solidsegs[MAXWIDTH / 2 + 2];
	};
}
