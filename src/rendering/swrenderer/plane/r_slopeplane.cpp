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
#include "g_levellocals.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "r_slopeplane.h"
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
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

namespace swrenderer
{
	RenderSlopePlane::RenderSlopePlane(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderSlopePlane::Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *colormap, FSoftwareTexture *texture)
	{
		static const float ifloatpow2[16] =
		{
			// ifloatpow2[i] = 1 / (1 << i)
			64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f,
			0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f,
			0.00390625f, 0.001953125f
			/*, 0.0009765625f, 0.00048828125f, 0.000244140625f,
			1.220703125e-4f, 6.103515625e-5, 3.0517578125e-5*/
		};
		double lxscale, lyscale;
		double xscale, yscale;
		FVector3 p, m, n;
		double ang, planeang, cosine, sine;
		double zeroheight;

		if (alpha <= 0)
		{
			return;
		}

		auto viewport = Thread->Viewport.get();

		// Stupid way of doing it, but at least it works
		DVector3 worldP0(viewport->viewpoint.Pos.XY(), pl->height.ZatPoint(viewport->viewpoint.Pos));
		DVector3 worldP1 = worldP0 + pl->height.Normal();
		DVector3 viewP0 = viewport->PointWorldToView(worldP0);
		DVector3 viewP1 = viewport->PointWorldToView(worldP1);
		planeNormal = viewP1 - viewP0;
		planeD = -(viewP0 | planeNormal);

		if (Thread->Portal->MirrorFlags & RF_XFLIP)
			planeNormal.X = -planeNormal.X;

		light_list = pl->lights;

		drawerargs.SetSolidColor(3);
		drawerargs.SetTexture(Thread, texture);

		_xscale /= texture->GetPhysicalScale();
		_yscale /= texture->GetPhysicalScale();

		lxscale = _xscale * ifloatpow2[drawerargs.TextureWidthBits()];
		lyscale = _yscale * ifloatpow2[drawerargs.TextureHeightBits()];
		xscale = 64.f / lxscale;
		yscale = 64.f / lyscale;
		zeroheight = pl->height.ZatPoint(Thread->Viewport->viewpoint.Pos);

		pviewx = xs_ToFixed(32 - drawerargs.TextureWidthBits(), pl->xform.xOffs * pl->xform.xScale);
		pviewy = xs_ToFixed(32 - drawerargs.TextureHeightBits(), pl->xform.yOffs * pl->xform.yScale);
		planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();

		// p is the texture origin in view space
		// Don't add in the offsets at this stage, because doing so can result in
		// errors if the flat is rotated.
		ang = M_PI * 3 / 2 - Thread->Viewport->viewpoint.Angles.Yaw.Radians();
		cosine = cos(ang), sine = sin(ang);
		p[0] = Thread->Viewport->viewpoint.Pos.X * cosine - Thread->Viewport->viewpoint.Pos.Y * sine;
		p[2] = Thread->Viewport->viewpoint.Pos.X * sine + Thread->Viewport->viewpoint.Pos.Y * cosine;
		p[1] = pl->height.ZatPoint(0.0, 0.0) - Thread->Viewport->viewpoint.Pos.Z;

		// m is the v direction vector in view space
		ang = ang - M_PI / 2 - planeang;
		cosine = cos(ang), sine = sin(ang);
		m[0] = yscale * cosine;
		m[2] = yscale * sine;
		//	m[1] = pl->height.ZatPointF (0, iyscale) - pl->height.ZatPointF (0,0));
		//	VectorScale2 (m, 64.f/VectorLength(m));

		// n is the u direction vector in view space
#if 0
		//let's use the sin/cosine we already know instead of computing new ones
		ang += M_PI / 2
			n[0] = -xscale * cos(ang);
		n[2] = -xscale * sin(ang);
#else
		n[0] = xscale * sine;
		n[2] = -xscale * cosine;
#endif
		//	n[1] = pl->height.ZatPointF (ixscale, 0) - pl->height.ZatPointF (0,0));
		//	VectorScale2 (n, 64.f/VectorLength(n));

		// This code keeps the texture coordinates constant across the x,y plane no matter
		// how much you slope the surface. Use the commented-out code above instead to keep
		// the textures a constant size across the surface's plane instead.
		cosine = cos(planeang), sine = sin(planeang);
		m[1] = pl->height.ZatPoint(Thread->Viewport->viewpoint.Pos.X + yscale * sine, Thread->Viewport->viewpoint.Pos.Y + yscale * cosine) - zeroheight;
		n[1] = -(pl->height.ZatPoint(Thread->Viewport->viewpoint.Pos.X - xscale * cosine, Thread->Viewport->viewpoint.Pos.Y + xscale * sine) - zeroheight);

		plane_su = p ^ m;
		plane_sv = p ^ n;
		plane_sz = m ^ n;

		plane_su.Z *= viewport->FocalLengthX;
		plane_sv.Z *= viewport->FocalLengthX;
		plane_sz.Z *= viewport->FocalLengthX;

		plane_su.Y *= viewport->IYaspectMul;
		plane_sv.Y *= viewport->IYaspectMul;
		plane_sz.Y *= viewport->IYaspectMul;

		// Premultiply the texture vectors with the scale factors
		plane_su *= 4294967296.f;
		plane_sv *= 4294967296.f;

		RenderPortal *renderportal = Thread->Portal.get();
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			plane_su[0] = -plane_su[0];
			plane_sv[0] = -plane_sv[0];
			plane_sz[0] = -plane_sz[0];
		}

		// [RH] set foggy flag
		basecolormap = colormap;
		auto Level = viewport->Level();
		foggy = Level->fadeto || basecolormap->Fade || (Level->flags & LEVEL_HASFADETABLE);

		planelightfloat = (Thread->Light->SlopePlaneGlobVis(foggy) * lxscale * lyscale) / (fabs(pl->height.ZatPoint(Thread->Viewport->viewpoint.Pos) - Thread->Viewport->viewpoint.Pos.Z)) / 65536.f;

		if (pl->height.fC() > 0)
			planelightfloat = -planelightfloat;

		drawerargs.SetStyle(false, false, OPAQUE, basecolormap);

		CameraLight *cameraLight = CameraLight::Instance();
		plane_shade = cameraLight->FixedLightLevel() < 0 && !cameraLight->FixedColormap();

		lightlevel = pl->lightlevel;

		// Hack in support for 1 x Z and Z x 1 texture sizes
		if (drawerargs.TextureHeightBits() == 0)
		{
			plane_sv[2] = plane_sv[1] = plane_sv[0] = 0;
		}
		if (drawerargs.TextureWidthBits() == 0)
		{
			plane_su[2] = plane_su[1] = plane_su[0] = 0;
		}

		RenderLines(pl);
	}

	void RenderSlopePlane::RenderLine(int y, int x1, int x2)
	{
		if (r_dynlights)
		{
			// Find row position in view space
			DVector3 viewposX1 = Thread->Viewport->ScreenToViewPos(x1, y, planeNormal, planeD);
			DVector3 viewposX2 = Thread->Viewport->ScreenToViewPos(x2, y, planeNormal, planeD);

			// Convert to screen space
			viewposX1.Z = 1.0 / viewposX1.Z;
			viewposX1.X *= viewposX1.Z;
			viewposX1.Y *= viewposX1.Z;
			viewposX2.Z = 1.0 / viewposX2.Z;
			viewposX2.X *= viewposX2.Z;
			viewposX2.Y *= viewposX2.Z;

			drawerargs.dc_viewpos.X = viewposX1.X;
			drawerargs.dc_viewpos.Y = viewposX1.Y;
			drawerargs.dc_viewpos.Z = viewposX1.Z;
			drawerargs.dc_viewpos_step.X = (viewposX2.X - viewposX1.X) / (x2 - x1);
			drawerargs.dc_viewpos_step.Y = (viewposX2.Y - viewposX1.Y) / (x2 - x1);
			drawerargs.dc_viewpos_step.Z = (viewposX2.Z - viewposX1.Z) / (x2 - x1);

			// Plane normal
			drawerargs.dc_normal.X = planeNormal.X;
			drawerargs.dc_normal.Y = planeNormal.Y;
			drawerargs.dc_normal.Z = planeNormal.Z;

			// Calculate max lights that can touch the row so we can allocate memory for the list
			int max_lights = 0;
			VisiblePlaneLight* cur_node = light_list;
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
				if (cur_node->lightsource->IsActive())
				{
					DVector3 lightPos = Thread->Viewport->PointWorldToView(cur_node->lightsource->Pos);

					uint32_t red = cur_node->lightsource->GetRed();
					uint32_t green = cur_node->lightsource->GetGreen();
					uint32_t blue = cur_node->lightsource->GetBlue();

					auto& light = drawerargs.dc_lights[drawerargs.dc_num_lights++];
					light.x = lightPos.X;
					light.y = lightPos.Y;
					light.z = lightPos.Z;
					light.radius = 256.0f / cur_node->lightsource->GetRadius();
					light.color = (red << 16) | (green << 8) | blue;

					bool is_point_light = cur_node->lightsource->IsAttenuated();
					if (is_point_light)
						light.radius = -light.radius;
				}

				cur_node = cur_node->next;
			}
		}
		else
		{
			drawerargs.dc_num_lights = 0;
		}

		drawerargs.DrawTiltedSpan(Thread, y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, lightlevel, foggy, planelightfloat, pviewx, pviewy, basecolormap);
	}
}
