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
