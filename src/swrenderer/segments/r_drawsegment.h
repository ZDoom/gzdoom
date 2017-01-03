
#pragma once

#include "swrenderer/line/r_line.h"

namespace swrenderer
{
	struct drawseg_t
	{
		seg_t *curline;
		float light, lightstep;
		float iscale, iscalestep;
		short x1, x2; // Same as sx1 and sx2, but clipped to the drawseg
		short sx1, sx2; // left, right of parent seg on screen
		float sz1, sz2; // z for left, right of parent seg on screen
		float siz1, siz2; // 1/z for left, right of parent seg on screen
		float cx, cy, cdx, cdy;
		float yscale;
		uint8_t silhouette; // 0=none, 1=bottom, 2=top, 3=both
		uint8_t bFogBoundary;
		uint8_t bFakeBoundary; // for fake walls
		int shade;

		// Pointers to lists for sprite clipping, all three adjusted so [x1] is first value.
		ptrdiff_t sprtopclip; // type short
		ptrdiff_t sprbottomclip; // type short
		ptrdiff_t maskedtexturecol; // type short
		ptrdiff_t swall; // type float
		ptrdiff_t bkup; // sprtopclip backup, for mid and fake textures
		
		FWallTmapVals tmapvals;
		
		int fake; // ident fake drawseg, don't draw and clip sprites backups
		int CurrentPortalUniq; // [ZZ] to identify the portal that this drawseg is in. used for sprite clipping.
	};
	
	extern drawseg_t *firstdrawseg;
	extern drawseg_t *ds_p;
	extern drawseg_t *drawsegs;

	extern TArray<size_t> InterestingDrawsegs; // drawsegs that have something drawn on them
	extern size_t FirstInterestingDrawseg;
	
	void R_ClearDrawSegs();
	void R_FreeDrawSegs();

	drawseg_t *R_AddDrawSegment();
	void ClipMidtex(int x1, int x2);
	void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2);
	void R_RenderFakeWall(drawseg_t *ds, int x1, int x2, F3DFloor *rover, int wallshade);
	void R_RenderFakeWallRange(drawseg_t *ds, int x1, int x2, int wallshade);
}
