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

namespace swrenderer
{
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

		xscale = xs_ToFixed(32 - drawerargs.TextureWidthBits(), _xscale);
		yscale = xs_ToFixed(32 - drawerargs.TextureHeightBits(), _yscale);
		if (planeang != 0)
		{
			double cosine = cos(planeang), sine = sin(planeang);
			pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine);
			pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine);
		}
		else
		{
			pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X);
			pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.Y);
		}

		pviewx = FixedMul(xscale, pviewx);
		pviewy = FixedMul(yscale, pviewy);

		// left to right mapping
		planeang += (ViewAngle - 90).Radians();

		auto viewport = RenderViewport::Instance();

		// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
		xstep = cos(planeang) / viewport->FocalLengthX;
		ystep = -sin(planeang) / viewport->FocalLengthX;

		// [RH] flip for mirrors
		RenderPortal *renderportal = RenderPortal::Instance();
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			xstep = -xstep;
			ystep = -ystep;
		}

		planeang += M_PI / 2;
		double cosine = cos(planeang), sine = -sin(planeang);
		x = pl->right - centerx - 0.5;
		rightxfrac = _xscale * (cosine + x * xstep);
		rightyfrac = _yscale * (sine + x * ystep);
		x = pl->left - centerx - 0.5;
		leftxfrac = _xscale * (cosine + x * xstep);
		leftyfrac = _yscale * (sine + x * ystep);

		basexfrac = rightxfrac;
		baseyfrac = rightyfrac;
		xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left);
		ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left);

		planeheight = fabs(pl->height.Zat0() - ViewPos.Z);

		basecolormap = colormap;
		GlobVis = LightVisibility::Instance()->FlatPlaneGlobVis() / planeheight;

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->fixedlightlev >= 0)
		{
			drawerargs.SetLight(basecolormap, 0, FIXEDLIGHT2SHADE(cameraLight->fixedlightlev));
			plane_shade = false;
		}
		else if (cameraLight->fixedcolormap)
		{
			drawerargs.SetLight(cameraLight->fixedcolormap, 0, 0);
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
		double distance;

#ifdef RANGECHECK
		if (x2 < x1 || x1<0 || x2 >= viewwidth || (unsigned)y >= (unsigned)viewheight)
		{
			I_FatalError("R_MapPlane: %i, %i at %i", x1, x2, y);
		}
#endif

		// [RH] Notice that I dumped the caching scheme used by Doom.
		// It did not offer any appreciable speedup.

		distance = planeheight * yslope[y];

		if (drawerargs.TextureWidthBits() != 0)
		{
			drawerargs.SetTextureUStep(xs_ToFixed(32 - drawerargs.TextureWidthBits(), distance * xstepscale));
			drawerargs.SetTextureUPos(xs_ToFixed(32 - drawerargs.TextureWidthBits(), distance * basexfrac) + pviewx);
		}
		else
		{
			drawerargs.SetTextureUStep(0);
			drawerargs.SetTextureUPos(0);
		}

		if (drawerargs.TextureHeightBits() != 0)
		{
			drawerargs.SetTextureVStep(xs_ToFixed(32 - drawerargs.TextureHeightBits(), distance * ystepscale));
			drawerargs.SetTextureVPos(xs_ToFixed(32 - drawerargs.TextureHeightBits(), distance * baseyfrac) + pviewy);
		}
		else
		{
			drawerargs.SetTextureVStep(0);
			drawerargs.SetTextureVPos(0);
		}
		
		auto viewport = RenderViewport::Instance();

		if (viewport->RenderTarget->IsBgra())
		{
			double distance2 = planeheight * yslope[(y + 1 < viewheight) ? y + 1 : y - 1];
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

			static TriLight lightbuffer[64 * 1024];
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

		drawerargs.DrawSpan();
	}

	void RenderFlatPlane::StepColumn()
	{
		basexfrac -= xstepscale;
		baseyfrac -= ystepscale;
	}

	void RenderFlatPlane::SetupSlope()
	{
		auto viewport = RenderViewport::Instance();

		int i = 0;
		int e = viewheight;
		float focus = float(viewport->FocalLengthY);
		float den;
		float cy = float(viewport->CenterY);
		if (i < centery)
		{
			den = cy - i - 0.5f;
			if (e <= centery)
			{
				do {
					yslope[i] = focus / den;
					den -= 1;
				} while (++i < e);
			}
			else
			{
				do {
					yslope[i] = focus / den;
					den -= 1;
				} while (++i < centery);
				den = i - cy + 0.5f;
				do {
					yslope[i] = focus / den;
					den += 1;
				} while (++i < e);
			}
		}
		else
		{
			den = i - cy + 0.5f;
			do {
				yslope[i] = focus / den;
				den += 1;
			} while (++i < e);
		}
	}

	float RenderFlatPlane::yslope[MAXHEIGHT];

	/////////////////////////////////////////////////////////////////////////

	void RenderColoredPlane::Render(VisiblePlane *pl)
	{
		RenderLines(pl);
	}

	void RenderColoredPlane::RenderLine(int y, int x1, int x2)
	{
		drawerargs.DrawColoredSpan(y, x1, x2);
	}
}
