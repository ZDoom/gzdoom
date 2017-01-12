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

#include "r_visiblesprite.h"
#include "swrenderer/scene/r_opaque_pass.h"

namespace swrenderer
{
	class RenderParticle
	{
	public:
		static void Project(particle_t *, const sector_t *sector, int shade, WaterFakeSide fakeside, bool foggy);
		static void Render(vissprite_t *);

	private:
		static void DrawMaskedSegsBehindParticle(const vissprite_t *vis);
	};
}
