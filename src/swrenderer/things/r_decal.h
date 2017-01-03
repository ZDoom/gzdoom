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

	void R_RenderDecals(side_t *wall, drawseg_t *draw_segment, int wallshade);
	void R_RenderDecal(side_t *wall, DBaseDecal *first, drawseg_t *clipper, int wallshade, int pass);
}
