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

class FTexture;
struct FLightNode;
struct seg_t;

namespace swrenderer
{
	struct drawseg_t;
	struct FWallCoords;

	struct WallSampler
	{
		WallSampler() { }
		WallSampler(int y1, double texturemid, float swal, double yrepeat, fixed_t xoffset, double xmagnitude, FTexture *texture);

		uint32_t uv_pos;
		uint32_t uv_step;
		uint32_t uv_max;

		const BYTE *source;
		const BYTE *source2;
		uint32_t texturefracx;
		uint32_t height;
	};

	void R_DrawWallSegment(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, FTexture *rw_pic, int x1, int x2, short *walltop, short *wallbottom, double texturemid, float *swall, fixed_t *lwall, double yscale, double top, double bottom, bool mask, int wallshade, fixed_t xoffset, float light, float lightstep, FLightNode *light_list, bool foggy, FDynamicColormap *basecolormap);
	void R_DrawDrawSeg(sector_t *frontsector, seg_t *curline, const FWallCoords &WallC, FTexture *rw_pic, drawseg_t *ds, int x1, int x2, short *uwal, short *dwal, double texturemid, float *swal, fixed_t *lwal, double yrepeat, int wallshade, fixed_t xoffset, float light, float lightstep, bool foggy, FDynamicColormap *basecolormap);
}
