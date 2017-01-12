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

struct side_t;
class DBaseDecal;

namespace swrenderer
{
	struct drawseg_t;

	class RenderDecal
	{
	public:
		static void RenderDecals(side_t *wall, drawseg_t *draw_segment, int wallshade, float lightleft, float lightstep, seg_t *curline, const FWallCoords &wallC, bool foggy, FDynamicColormap *basecolormap);

	private:
		static void Render(side_t *wall, DBaseDecal *first, drawseg_t *clipper, int wallshade, float lightleft, float lightstep, seg_t *curline, FWallCoords wallC, bool foggy, FDynamicColormap *basecolormap, int pass);
		static void DrawColumn(int x, FTexture *WallSpriteTile, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip);
	};
}
