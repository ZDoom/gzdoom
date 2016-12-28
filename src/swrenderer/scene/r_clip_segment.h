
#pragma once

namespace swrenderer
{
	typedef bool(*VisibleSegmentCallback)(int x1, int x2);

	void R_ClearClipSegs(short left, short right);
	bool R_ClipWallSegment(int x1, int x2, bool solid, VisibleSegmentCallback callback);
	bool R_CheckClipWallSegment(int first, int last);
	bool R_IsWallSegmentVisible(int x1, int x2);
}
