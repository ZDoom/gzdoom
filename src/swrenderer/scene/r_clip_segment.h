
#pragma once

namespace swrenderer
{
	struct cliprange_t
	{
		short first, last;
	};
	
	// newend is one past the last valid seg
	extern cliprange_t *newend;
	extern cliprange_t solidsegs[MAXWIDTH/2+2];
	
	void R_ClearClipSegs(short left, short right);
}
