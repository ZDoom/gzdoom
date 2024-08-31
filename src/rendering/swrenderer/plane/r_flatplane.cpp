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

#include "engineerrors.h"
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
#include "g_levellocals.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "r_flatplane.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "a_dynlight.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/viewport/r_viewport.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	RenderFlatPlane::RenderFlatPlane(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderFlatPlane::Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *colormap, FSoftwareTexture *texture)
	{
		if (alpha <= 0)
		{
			return;
		}

		tex = texture;

		drawerargs.SetSolidColor(3);
		drawerargs.SetTexture(Thread, texture);

		double planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();
		double xstep, ystep, leftxfrac, leftyfrac, rightxfrac, rightyfrac;
		double x;

		if (planeang != 0)
		{
			double cosine = cos(planeang), sine = sin(planeang);
			pviewx = pl->xform.xOffs + Thread->Viewport->viewpoint.Pos.X * cosine - Thread->Viewport->viewpoint.Pos.Y * sine;
			pviewy = pl->xform.yOffs + pl->xform.baseyOffs - Thread->Viewport->viewpoint.Pos.X * sine - Thread->Viewport->viewpoint.Pos.Y * cosine;
		}
		else
		{
			pviewx = pl->xform.xOffs + Thread->Viewport->viewpoint.Pos.X;
			pviewy = pl->xform.yOffs - Thread->Viewport->viewpoint.Pos.Y;
		}

		pviewx = _xscale * pviewx;
		pviewy = _yscale * pviewy;

		// left to right mapping
		planeang += (Thread->Viewport->viewpoint.Angles.Yaw - DAngle::fromDeg(90)).Radians();

		auto viewport = Thread->Viewport.get();

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
		x = pl->right - viewport->CenterX + 0.5;
		rightxfrac = _xscale * (cosine + x * xstep);
		rightyfrac = _yscale * (sine + x * ystep);
		x = pl->left - viewport->CenterX + 0.5;
		leftxfrac = _xscale * (cosine + x * xstep);
		leftyfrac = _yscale * (sine + x * ystep);

		basexfrac = leftxfrac;
		baseyfrac = leftyfrac;
		if (pl->left != pl->right)
		{
			xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left);
			ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left);
		}
		else
		{
			xstepscale = 0;
			ystepscale = 0;
		}

		minx = pl->left;

		planeheight = fabs(pl->height.Zat0() - Thread->Viewport->viewpoint.Pos.Z);

		// [RH] set foggy flag
		auto Level = Thread->Viewport->Level();
		foggy = (Level->fadeto || colormap->Fade || (Level->flags & LEVEL_HASFADETABLE));
		lightlevel = pl->lightlevel;

		CameraLight *cameraLight = CameraLight::Instance();
		plane_shade = cameraLight->FixedLightLevel() < 0 && !cameraLight->FixedColormap();

		drawerargs.SetStyle(masked, additive, alpha, colormap);

		light_list = pl->lights;

		RenderLines(pl);
	}

	void RenderFlatPlane::RenderLine(int y, int x1, int x2)
	{
#ifdef RANGECHECK
		if (x2 < x1 || x1<0 || x2 >= viewwidth || (unsigned)y >= (unsigned)viewheight)
		{
			I_Error("R_MapPlane: %i, %i at %i", x1, x2, y);
		}
#endif

		auto viewport = Thread->Viewport.get();

		double curxfrac = basexfrac + xstepscale * (x1 - minx);
		double curyfrac = baseyfrac + ystepscale * (x1 - minx);

		double distance = viewport->PlaneDepth(y, planeheight);

		float zbufferdepth = (float)(1.0 / fabs(planeheight / Thread->Viewport->ScreenToViewY(y, 1.0)));

		drawerargs.SetTextureUStep(distance * xstepscale / tex->GetWidth());
		drawerargs.SetTextureUPos((distance * curxfrac + pviewx) / tex->GetWidth());

		drawerargs.SetTextureVStep(distance * ystepscale / tex->GetHeight());
		drawerargs.SetTextureVPos((distance * curyfrac + pviewy) / tex->GetHeight());
		
		if (viewport->RenderTarget->IsBgra())
		{
			double distance2 = viewport->PlaneDepth(y + 1, planeheight);
			double xmagnitude = fabs(ystepscale * (distance2 - distance) * viewport->FocalLengthX);
			double ymagnitude = fabs(xstepscale * (distance2 - distance) * viewport->FocalLengthX);
			double magnitude = max(ymagnitude, xmagnitude);
			double min_lod = -1000.0;
			drawerargs.SetTextureLOD(max(log2(magnitude) + r_lod_bias, min_lod));
		}

		if (plane_shade)
		{
			// Determine lighting based on the span's distance from the viewer.
			drawerargs.SetLight((float)Thread->Light->FlatPlaneVis(y, planeheight, foggy, viewport), lightlevel, foggy, viewport);
		}

		if (r_dynlights)
		{
			int tx = x1;
			bool mirror = !!(Thread->Portal->MirrorFlags & RF_XFLIP);
			if (mirror)
				tx = viewwidth - tx - 1;

			// Find row position in view space
			float zspan = (float)(planeheight / (fabs(y + 0.5 - viewport->CenterY) / viewport->InvZtoScale));
			drawerargs.dc_viewpos.X = (float)((tx + 0.5 - viewport->CenterX) / viewport->CenterX * zspan);
			drawerargs.dc_viewpos.Y = zspan;
			drawerargs.dc_viewpos.Z = (float)((viewport->CenterY - y - 0.5) / viewport->InvZtoScale * zspan);
			drawerargs.dc_viewpos_step.X = (float)(zspan / viewport->CenterX);

			if (mirror)
				drawerargs.dc_viewpos_step.X = -drawerargs.dc_viewpos_step.X;

			// Plane normal
			drawerargs.dc_normal.X = 0.0f;
			drawerargs.dc_normal.Y = 0.0f;
			drawerargs.dc_normal.Z = (y >= viewport->CenterY) ? 1.0f : -1.0f;

			// Calculate max lights that can touch the row so we can allocate memory for the list
			int max_lights = 0;
			VisiblePlaneLight *cur_node = light_list;
			while (cur_node)
			{
				if (cur_node->lightsource->IsActive())
					max_lights++;
				cur_node = cur_node->next;
			}

			drawerargs.dc_num_lights = 0;
			drawerargs.dc_lights = Thread->FrameMemory->AllocMemory<DrawerLight>(max_lights);

			// Setup lights for row
			cur_node = light_list;
			while (cur_node)
			{
				double lightX = cur_node->lightsource->X() - Thread->Viewport->viewpoint.Pos.X;
				double lightY = cur_node->lightsource->Y() - Thread->Viewport->viewpoint.Pos.Y;
				double lightZ = cur_node->lightsource->Z() - Thread->Viewport->viewpoint.Pos.Z;

				float lx = (float)(lightX * Thread->Viewport->viewpoint.Sin - lightY * Thread->Viewport->viewpoint.Cos);
				float ly = (float)(lightX * Thread->Viewport->viewpoint.TanCos + lightY * Thread->Viewport->viewpoint.TanSin) - drawerargs.dc_viewpos.Y;
				float lz = (float)lightZ - drawerargs.dc_viewpos.Z;

				// Precalculate the constant part of the dot here so the drawer doesn't have to.
				bool is_point_light = cur_node->lightsource->IsAttenuated();
				float lconstant = ly * ly + lz * lz;
				float nlconstant = is_point_light ? lz * drawerargs.dc_normal.Z : 0.0f;

				// Include light only if it touches this row
				float radius = cur_node->lightsource->GetRadius();
				if (radius * radius >= lconstant && nlconstant >= 0.0f)
				{
					uint32_t red = cur_node->lightsource->GetRed();
					uint32_t green = cur_node->lightsource->GetGreen();
					uint32_t blue = cur_node->lightsource->GetBlue();

					auto &light = drawerargs.dc_lights[drawerargs.dc_num_lights++];
					light.x = lx;
					light.y = lconstant;
					light.z = nlconstant;
					light.radius = 256.0f / radius;
					light.color = (red << 16) | (green << 8) | blue;
				}

				cur_node = cur_node->next;
			}
		}
		else
		{
			drawerargs.dc_num_lights = 0;
		}

		drawerargs.SetDestY(viewport, y);
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
