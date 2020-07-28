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
#include <stddef.h>
#include <cmath>

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"

#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_walldraw.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "a_dynlight.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/r_renderthread.h"
#include "r_memory.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	RenderWallPart::RenderWallPart(RenderThread* thread)
	{
		Thread = thread;
	}

	void RenderWallPart::Render(const sector_t* lightsector, seg_t* curline, const FWallCoords& WallC, FSoftwareTexture* pic, int x1, int x2, const short* walltop, const short* wallbottom, const ProjectedWallTexcoords& texcoords, bool mask, bool additive, fixed_t alpha)
	{
		if (pic == nullptr)
			return;

		this->x1 = x1;
		this->x2 = x2;
		this->lightsector = lightsector;
		this->curline = curline;
		this->WallC = WallC;
		this->pic = pic;
		this->mask = mask;
		this->additive = additive;
		this->alpha = alpha;

		light_list = GetLightList();

		mLight.SetColormap(lightsector, curline);
		mLight.SetLightLeft(Thread, WallC);

		Thread->PrepareTexture(pic, DefaultRenderStyle()); // Get correct render style? Shaded won't get here.

		CameraLight* cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap() || cameraLight->FixedLightLevel() >= 0 || !(lightsector->e && lightsector->e->XFloor.lightlist.Size()))
		{
			ProcessNormalWall(walltop, wallbottom, texcoords);
		}
		else
		{
			ProcessStripedWall(walltop, wallbottom, texcoords);
		}
	}

	void RenderWallPart::ProcessStripedWall(const short* uwal, const short* dwal, const ProjectedWallTexcoords& texcoords)
	{
		RenderPortal* renderportal = Thread->Portal.get();

		ProjectedWallLine most1, most2, most3;
		const short* up = uwal;
		short* down = most1.ScreenY;

		for (unsigned int i = 0; i < lightsector->e->XFloor.lightlist.Size(); i++)
		{
			ProjectedWallCull j = most3.Project(Thread->Viewport.get(), lightsector->e->XFloor.lightlist[i].plane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			if (j != ProjectedWallCull::OutsideAbove)
			{
				for (int j = x1; j < x2; ++j)
				{
					down[j] = clamp(most3.ScreenY[j], up[j], dwal[j]);
				}
				ProcessNormalWall(up, down, texcoords);
				up = down;
				down = (down == most1.ScreenY) ? most2.ScreenY : most1.ScreenY;
			}

			mLight.SetColormap(lightsector, curline, &lightsector->e->XFloor.lightlist[i]);
		}

		ProcessNormalWall(up, dwal, texcoords);
	}

	void RenderWallPart::ProcessNormalWall(const short* uwal, const short* dwal, const ProjectedWallTexcoords& texcoords)
	{
		CameraLight* cameraLight = CameraLight::Instance();
		RenderViewport* viewport = Thread->Viewport.get();

		WallDrawerArgs drawerargs;

		// Textures that aren't masked can use the faster opaque drawer
		if (!pic->isMasked() && mask && alpha >= OPAQUE && !additive)
		{
			drawerargs.SetStyle(true, false, OPAQUE, light_list);
		}
		else
		{
			drawerargs.SetStyle(mask, additive, alpha, light_list);
		}

		if (cameraLight->FixedLightLevel() >= 0)
		{
			drawerargs.SetBaseColormap((r_fullbrightignoresectorcolor) ? &FullNormalLight : mLight.GetBaseColormap());
			drawerargs.SetLight(0.0f, cameraLight->FixedLightLevelShade());
			drawerargs.fixedlight = true;
		}
		else if (cameraLight->FixedColormap())
		{
			drawerargs.SetBaseColormap(cameraLight->FixedColormap());
			drawerargs.SetLight(0.0f, 0);
			drawerargs.fixedlight = true;
		}
		else
		{
			drawerargs.SetBaseColormap(mLight.GetBaseColormap());
			drawerargs.SetLight(0.0f, mLight.GetLightLevel(), mLight.GetFoggy(), viewport);
			drawerargs.fixedlight = false;
		}

		int count = x2 - x1;
		short* data = Thread->FrameMemory->AllocMemory<short>(count << 1);

		drawerargs.x1 = x1;
		drawerargs.x2 = x2;
		drawerargs.uwal = data - x1;
		drawerargs.dwal = data + count - x1; // to avoid calling AllocMemory twice
		memcpy(drawerargs.uwal + x1, uwal + x1, sizeof(short) * count);
		memcpy(drawerargs.dwal + x1, dwal + x1, sizeof(short) * count);
		drawerargs.WallC = WallC;
		drawerargs.texcoords = texcoords;

		drawerargs.lightpos = mLight.GetLightPos(x1);
		drawerargs.lightstep = mLight.GetLightStep();

		drawerargs.lightlist = light_list;

		drawerargs.texwidth = pic->GetPhysicalWidth();
		drawerargs.texheight = pic->GetPhysicalHeight();
		if (viewport->RenderTarget->IsBgra())
		{
			drawerargs.texpixels = pic->GetPixelsBgra();
			drawerargs.fracbits = 32;
			drawerargs.mipmapped = r_mipmap && pic->Mipmapped();
		}
		else
		{
			drawerargs.texpixels = pic->GetPixels(DefaultRenderStyle());
			int fracbits = 32 - pic->GetHeightBits();
			if (fracbits == 32) fracbits = 0; // One pixel tall textures
			drawerargs.fracbits = fracbits;
			drawerargs.mipmapped = false;
		}

		// This data really should be its own command as it rarely changes
		drawerargs.SetDest(viewport);
		drawerargs.CenterX = Thread->Viewport->CenterX;
		drawerargs.CenterY = Thread->Viewport->CenterY;
		drawerargs.FocalTangent = Thread->Viewport->viewwindow.FocalTangent;
		drawerargs.InvZtoScale = Thread->Viewport->InvZtoScale;
		drawerargs.ViewpointPos = { (float)Thread->Viewport->viewpoint.Pos.X, (float)Thread->Viewport->viewpoint.Pos.Y, (float)Thread->Viewport->viewpoint.Pos.Z };
		drawerargs.Sin = Thread->Viewport->viewpoint.Sin;
		drawerargs.Cos = Thread->Viewport->viewpoint.Cos;
		drawerargs.TanCos = Thread->Viewport->viewpoint.TanCos;
		drawerargs.TanSin = Thread->Viewport->viewpoint.TanSin;
		drawerargs.PortalMirrorFlags = Thread->Portal->MirrorFlags;

		drawerargs.DrawWall(Thread);
	}

	FLightNode* RenderWallPart::GetLightList()
	{
		CameraLight* cameraLight = CameraLight::Instance();
		if ((cameraLight->FixedLightLevel() >= 0) || cameraLight->FixedColormap())
			return nullptr; // [SP] Don't draw dynlights if invul/lightamp active
		else if (curline && curline->sidedef)
			return curline->sidedef->lighthead;
		else
			return nullptr;
	}
}
