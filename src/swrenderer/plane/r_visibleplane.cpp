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

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "gl/dynlights/gl_dynlight.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_flatplane.h"
#include "swrenderer/plane/r_slopeplane.h"
#include "swrenderer/plane/r_skyplane.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/drawers/r_draw.h"

CVAR(Bool, tilt, false, 0);

namespace swrenderer
{
	void visplane_t::AddLights(FLightNode *node)
	{
		if (!r_dynlights)
			return;

		while (node)
		{
			if (!(node->lightsource->flags2&MF2_DORMANT))
			{
				bool found = false;
				visplane_light *light_node = lights;
				while (light_node)
				{
					if (light_node->lightsource == node->lightsource)
					{
						found = true;
						break;
					}
					light_node = light_node->next;
				}
				if (!found)
				{
					visplane_light *newlight = R_NewPlaneLight();
					if (!newlight)
						return;

					newlight->next = lights;
					newlight->lightsource = node->lightsource;
					lights = newlight;
				}
			}
			node = node->nextLight;
		}
	}

	void visplane_t::Render(fixed_t alpha, bool additive, bool masked)
	{
		if (left >= right)
			return;

		if (picnum == skyflatnum) // sky flat
		{
			RenderSkyPlane::Render(this);
		}
		else // regular flat
		{
			FTexture *tex = TexMan(picnum, true);

			if (tex->UseType == FTexture::TEX_Null)
			{
				return;
			}

			if (!masked && !additive)
			{ // If we're not supposed to see through this plane, draw it opaque.
				alpha = OPAQUE;
			}
			else if (!tex->bMasked)
			{ // Don't waste time on a masked texture if it isn't really masked.
				masked = false;
			}
			R_SetSpanTexture(tex);
			double xscale = xform.xScale * tex->Scale.X;
			double yscale = xform.yScale * tex->Scale.Y;

			basecolormap = colormap;

			if (!height.isSlope() && !tilt)
			{
				RenderFlatPlane renderer;
				renderer.Render(this, xscale, yscale, alpha, additive, masked);
			}
			else
			{
				RenderSlopePlane renderer;
				renderer.Render(this, xscale, yscale, alpha, additive, masked);
			}
		}
		NetUpdate();
	}
}
