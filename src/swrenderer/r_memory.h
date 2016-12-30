
#pragma once

namespace swrenderer
{
	extern short *openings;

	ptrdiff_t R_NewOpening(ptrdiff_t len);
	void R_FreeOpenings();
}
