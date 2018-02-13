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
#include "swrenderer/r_memory.h"
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

	void RenderSlopePlane::Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *colormap, FTexture *texture)
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

		drawerargs.SetSolidColor(3);
		drawerargs.SetTexture(Thread, texture);

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
		bool foggy = level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE);;

		planelightfloat = (Thread->Light->SlopePlaneGlobVis(foggy) * lxscale * lyscale) / (fabs(pl->height.ZatPoint(Thread->Viewport->viewpoint.Pos) - Thread->Viewport->viewpoint.Pos.Z)) / 65536.f;

		if (pl->height.fC() > 0)
			planelightfloat = -planelightfloat;


		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
		{
			drawerargs.SetLight(basecolormap, 0, cameraLight->FixedLightLevelShade());
			plane_shade = false;
		}
		else if (cameraLight->FixedColormap())
		{
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
			plane_shade = false;
		}
		else
		{
			drawerargs.SetLight(basecolormap, 0, 0);
			plane_shade = true;
			planeshade = LightVisibility::LightLevelToShade(pl->lightlevel, foggy);
		}

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
		/* To do: project (x1,y) and (x2,y) on the plane to calculate the depth
		double distance = Thread->Viewport->PlaneDepth(y, planeheight);
		float zbufferdepth = 1.0f / (distance * Thread->Viewport->viewwindow.FocalTangent);
		drawerargs.SetDestX1(x1);
		drawerargs.SetDestX2(x2);
		drawerargs.SetDestY(Thread->Viewport.get(), y);
		drawerargs.DrawDepthSpan(Thread, zbufferdepth);
		*/

		drawerargs.DrawTiltedSpan(Thread, y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
	}
}
