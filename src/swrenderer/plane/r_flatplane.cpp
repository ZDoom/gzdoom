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
#include "swrenderer/scene/r_opaque_pass.h"
#include "r_flatplane.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "gl/dynlights/gl_dynlight.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	RenderFlatPlane::RenderFlatPlane(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderFlatPlane::Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *colormap, FTexture *texture)
	{
		if (alpha <= 0)
		{
			return;
		}

		drawerargs.SetSolidColor(3);
		drawerargs.SetTexture(texture);

		double planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();
		double xstep, ystep, leftxfrac, leftyfrac, rightxfrac, rightyfrac;
		double x;

		if (planeang != 0)
		{
			double cosine = cos(planeang), sine = sin(planeang);
			pviewx = pl->xform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine;
			pviewy = pl->xform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine;
		}
		else
		{
			pviewx = pl->xform.xOffs + ViewPos.X;
			pviewy = pl->xform.yOffs - ViewPos.Y;
		}

		pviewx = _xscale * pviewx;
		pviewy = _yscale * pviewy;

		// left to right mapping
		planeang += (ViewAngle - 90).Radians();

		auto viewport = RenderViewport::Instance();

		// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
		xstep = cos(planeang) / viewport->FocalLengthX;
		ystep = -sin(planeang) / viewport->FocalLengthX;

		// [RH] flip for mirrors
		RenderPortal *renderportal = Thread->Portal.get();
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			xstep = -xstep;
			ystep = -ystep;
		}

		planeang += M_PI / 2;
		double cosine = cos(planeang), sine = -sin(planeang);
		x = pl->right - viewport->CenterX - 0.5;
		rightxfrac = _xscale * (cosine + x * xstep);
		rightyfrac = _yscale * (sine + x * ystep);
		x = pl->left - viewport->CenterX + 0.5;
		leftxfrac = _xscale * (cosine + x * xstep);
		leftyfrac = _yscale * (sine + x * ystep);

		basexfrac = leftxfrac;
		baseyfrac = leftyfrac;
		xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left + 1);
		ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left + 1);

		minx = pl->left;

		planeheight = fabs(pl->height.Zat0() - ViewPos.Z);

		basecolormap = colormap;
		GlobVis = LightVisibility::Instance()->FlatPlaneGlobVis() / planeheight;

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
		{
			drawerargs.SetLight(basecolormap, 0, FIXEDLIGHT2SHADE(cameraLight->FixedLightLevel()));
			plane_shade = false;
		}
		else if (cameraLight->FixedColormap())
		{
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
			plane_shade = false;
		}
		else
		{
			plane_shade = true;
			planeshade = LIGHT2SHADE(pl->lightlevel);
		}

		drawerargs.SetStyle(masked, additive, alpha);

		light_list = pl->lights;

		RenderLines(pl);
	}

	void RenderFlatPlane::RenderLine(int y, int x1, int x2)
	{
#ifdef RANGECHECK
		if (x2 < x1 || x1<0 || x2 >= viewwidth || (unsigned)y >= (unsigned)viewheight)
		{
			I_FatalError("R_MapPlane: %i, %i at %i", x1, x2, y);
		}
#endif

		auto viewport = RenderViewport::Instance();

		double curxfrac = basexfrac + xstepscale * (x1 + 0.5 - minx);
		double curyfrac = baseyfrac + ystepscale * (x1 + 0.5 - minx);

		double distance = viewport->PlaneDepth(y, planeheight);

		if (drawerargs.TextureWidthBits() != 0)
		{
			drawerargs.SetTextureUStep(xs_ToFixed(32 - drawerargs.TextureWidthBits(), distance * xstepscale));
			drawerargs.SetTextureUPos(xs_ToFixed(32 - drawerargs.TextureWidthBits(), distance * curxfrac + pviewx));
		}
		else
		{
			drawerargs.SetTextureUStep(0);
			drawerargs.SetTextureUPos(0);
		}

		if (drawerargs.TextureHeightBits() != 0)
		{
			drawerargs.SetTextureVStep(xs_ToFixed(32 - drawerargs.TextureHeightBits(), distance * ystepscale));
			drawerargs.SetTextureVPos(xs_ToFixed(32 - drawerargs.TextureHeightBits(), distance * curyfrac + pviewy));
		}
		else
		{
			drawerargs.SetTextureVStep(0);
			drawerargs.SetTextureVPos(0);
		}
		
		if (viewport->RenderTarget->IsBgra())
		{
			double distance2 = viewport->PlaneDepth(y + 1, planeheight);
			double xmagnitude = fabs(ystepscale * (distance2 - distance) * viewport->FocalLengthX);
			double ymagnitude = fabs(xstepscale * (distance2 - distance) * viewport->FocalLengthX);
			double magnitude = MAX(ymagnitude, xmagnitude);
			double min_lod = -1000.0;
			drawerargs.SetTextureLOD(MAX(log2(magnitude) + r_lod_bias, min_lod));
		}

		if (plane_shade)
		{
			// Determine lighting based on the span's distance from the viewer.
			drawerargs.SetLight(basecolormap, (float)(GlobVis * fabs(viewport->CenterY - y)), planeshade);
		}

		if (r_dynlights)
		{
			// Find row position in view space
			float zspan = (float)(planeheight / (fabs(y + 0.5 - viewport->CenterY) / viewport->InvZtoScale));
			drawerargs.dc_viewpos.X = (float)((x1 + 0.5 - viewport->CenterX) / viewport->CenterX * zspan);
			drawerargs.dc_viewpos.Y = zspan;
			drawerargs.dc_viewpos.Z = (float)((viewport->CenterY - y - 0.5) / viewport->InvZtoScale * zspan);
			drawerargs.dc_viewpos_step.X = (float)(zspan / viewport->CenterX);

			static DrawerLight lightbuffer[64 * 1024];
			static int nextlightindex = 0;
			
			// Plane normal
			drawerargs.dc_normal.X = 0.0f;
			drawerargs.dc_normal.Y = 0.0f;
			drawerargs.dc_normal.Z = (y >= viewport->CenterY) ? 1.0f : -1.0f;

			// Setup lights for row
			drawerargs.dc_num_lights = 0;
			drawerargs.dc_lights = lightbuffer + nextlightindex;
			VisiblePlaneLight *cur_node = light_list;
			while (cur_node && nextlightindex < 64 * 1024)
			{
				double lightX = cur_node->lightsource->X() - ViewPos.X;
				double lightY = cur_node->lightsource->Y() - ViewPos.Y;
				double lightZ = cur_node->lightsource->Z() - ViewPos.Z;

				float lx = (float)(lightX * ViewSin - lightY * ViewCos);
				float ly = (float)(lightX * ViewTanCos + lightY * ViewTanSin) - drawerargs.dc_viewpos.Y;
				float lz = (float)lightZ - drawerargs.dc_viewpos.Z;

				// Precalculate the constant part of the dot here so the drawer doesn't have to.
				bool is_point_light = (cur_node->lightsource->flags4 & MF4_ATTENUATE) != 0;
				float lconstant = ly * ly + lz * lz;
				float nlconstant = is_point_light ? lz * drawerargs.dc_normal.Z : 0.0f;

				// Include light only if it touches this row
				float radius = cur_node->lightsource->GetRadius();
				if (radius * radius >= lconstant && nlconstant >= 0.0f)
				{
					uint32_t red = cur_node->lightsource->GetRed();
					uint32_t green = cur_node->lightsource->GetGreen();
					uint32_t blue = cur_node->lightsource->GetBlue();

					nextlightindex++;
					auto &light = drawerargs.dc_lights[drawerargs.dc_num_lights++];
					light.x = lx;
					light.y = lconstant;
					light.z = nlconstant;
					light.radius = 256.0f / radius;
					light.color = (red << 16) | (green << 8) | blue;
				}

				cur_node = cur_node->next;
			}

			if (nextlightindex == 64 * 1024)
				nextlightindex = 0;
		}
		else
		{
			drawerargs.dc_num_lights = 0;
		}

		drawerargs.SetDestY(y);
		drawerargs.SetDestX1(x1);
		drawerargs.SetDestX2(x2);

		drawerargs.DrawSpan(Thread);
	}

	/////////////////////////////////////////////////////////////////////////
	
	RenderColoredPlane::RenderColoredPlane(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderColoredPlane::Render(VisiblePlane *pl)
	{
		RenderLines(pl);
	}

	void RenderColoredPlane::RenderLine(int y, int x1, int x2)
	{
		drawerargs.DrawColoredSpan(Thread, y, x1, x2);
	}
}
