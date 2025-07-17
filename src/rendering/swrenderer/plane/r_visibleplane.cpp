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
//

#include <stdlib.h>
#include <float.h>



#include "filesystem.h"
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
#include "a_dynlight.h"
#include "texturemanager.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"
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
	VisiblePlane::VisiblePlane(RenderThread *thread)
	{
		picnum.SetNull();
		height.set(0.0, 0.0, 1.0, 0.0);

		bottom = thread->FrameMemory->AllocMemory<uint16_t>(viewwidth);
		top = thread->FrameMemory->AllocMemory<uint16_t>(viewwidth);

		fillshort(bottom, viewwidth, 0);
		fillshort(top, viewwidth, 0x7fff);
	}

	void VisiblePlane::AddLights(RenderThread *thread, FLightNode *node)
	{
		if (!r_dynlights)
			return;

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap() != NULL || cameraLight->FixedLightLevel() >= 0)
			return; // [SP] no dynlights if invul or lightamp

		while (node)
		{
			if (node->lightsource->IsActive() && (height.PointOnSide(node->lightsource->Pos) > 0))
			{
				bool found = false;
				VisiblePlaneLight *light_node = lights;
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
					VisiblePlaneLight *newlight = thread->FrameMemory->NewObject<VisiblePlaneLight>();
					newlight->next = lights;
					newlight->lightsource = node->lightsource;
					lights = newlight;
				}
			}
			node = node->nextLight;
		}
	}

	void VisiblePlane::Render(RenderThread *thread, fixed_t alpha, bool additive, bool masked)
	{
		if (left >= right)
			return;

		if (picnum == skyflatnum) // sky flat
		{
			RenderSkyPlane renderer(thread);
			renderer.Render(this);
		}
		else // regular flat
		{
			auto tex = GetPalettedSWTexture(picnum, true);
			if (tex == nullptr)
				return;
			if (!masked && !additive)
			{ // If we're not supposed to see through this plane, draw it opaque.
				alpha = OPAQUE;
			}
			else if (!tex->isMasked())
			{ // Don't waste time on a masked texture if it isn't really masked.
				masked = false;
			}
			double xscale = xform.xScale * tex->GetScale().X;
			double yscale = xform.yScale * tex->GetScale().Y;

			if (!height.isSlope() && !tilt)
			{
				RenderFlatPlane renderer(thread);
				renderer.Render(this, xscale, yscale, alpha, additive, masked, colormap, tex);
			}
			else
			{
				RenderSlopePlane renderer(thread);
				renderer.Render(this, xscale, yscale, alpha, additive, masked, colormap, tex);
			}
		}
	}
}
