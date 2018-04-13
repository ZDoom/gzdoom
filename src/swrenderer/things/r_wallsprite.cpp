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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "swrenderer/things/r_wallsprite.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "p_effect.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_draw_pal.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "p_local.h"
#include "p_maputl.h"
#include "r_voxel.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void RenderWallSprite::Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FTexture *pic, const DVector2 &scale, int renderflags, int spriteshade, bool foggy, FDynamicColormap *basecolormap)
	{
		FWallCoords wallc;
		double x1, x2;
		DVector2 left, right;
		double gzb, gzt, tz;
		DAngle ang = thing->Angles.Yaw + 90;
		double angcos = ang.Cos();
		double angsin = ang.Sin();

		// Determine left and right edges of sprite. The sprite's angle is its normal,
		// so the edges are 90 degrees each side of it.
		x2 = pic->GetScaledWidth();
		x1 = pic->GetScaledLeftOffsetSW();

		x1 *= scale.X;
		x2 *= scale.X;

		left.X = pos.X - x1 * angcos - thread->Viewport->viewpoint.Pos.X;
		left.Y = pos.Y - x1 * angsin - thread->Viewport->viewpoint.Pos.Y;
		right.X = left.X + x2 * angcos;
		right.Y = left.Y + x2 * angsin;

		// Is it off-screen?
		if (wallc.Init(thread, left, right, TOO_CLOSE_Z))
			return;
			
		RenderPortal *renderportal = thread->Portal.get();

		if (wallc.sx1 >= renderportal->WindowRight || wallc.sx2 <= renderportal->WindowLeft)
			return;

		// Sprite sorting should probably treat these as walls, not sprites,
		// but right now, I just want to get them drawing.
		tz = (pos.X - thread->Viewport->viewpoint.Pos.X) * thread->Viewport->viewpoint.TanCos + (pos.Y - thread->Viewport->viewpoint.Pos.Y) * thread->Viewport->viewpoint.TanSin;

		int scaled_to = pic->GetScaledTopOffsetSW();
		int scaled_bo = scaled_to - pic->GetScaledHeight();
		gzt = pos.Z + scale.Y * scaled_to;
		gzb = pos.Z + scale.Y * scaled_bo;

		RenderWallSprite *vis = thread->FrameMemory->NewObject<RenderWallSprite>();
		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->x1 = wallc.sx1 < renderportal->WindowLeft ? renderportal->WindowLeft : wallc.sx1;
		vis->x2 = wallc.sx2 >= renderportal->WindowRight ? renderportal->WindowRight : wallc.sx2;
		vis->yscale = (float)scale.Y;
		vis->idepth = float(1 / tz);
		vis->depth = (float)tz;
		vis->sector = thing->Sector;
		vis->heightsec = NULL;
		vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
		vis->gzb = (float)gzb;
		vis->gzt = (float)gzt;
		vis->deltax = float(pos.X - thread->Viewport->viewpoint.Pos.X);
		vis->deltay = float(pos.Y - thread->Viewport->viewpoint.Pos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT) vis->renderflags |= RF_FULLBRIGHT; // kg3D
		vis->RenderStyle = thing->RenderStyle;
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;
		vis->FakeFlatStat = WaterFakeSide::Center;
		vis->Alpha = float(thing->Alpha);
		vis->fakefloor = NULL;
		vis->fakeceiling = NULL;
		//vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
		vis->pic = pic;
		vis->wallc = wallc;
		vis->foggy = foggy;

		vis->Light.SetColormap(thread->Light->SpriteGlobVis(foggy) / MAX(tz, MINZ), spriteshade, basecolormap, false, false, false);

		thread->SpriteList->Push(vis);
	}

	void RenderWallSprite::Render(RenderThread *thread, short *mfloorclip, short *mceilingclip, int, int, Fake3DTranslucent)
	{
		auto spr = this;

		int x1, x2;
		double iyscale;
		bool sprflipvert;

		x1 = MAX<int>(spr->x1, spr->wallc.sx1);
		x2 = MIN<int>(spr->x2, spr->wallc.sx2);
		if (x1 >= x2)
			return;

		FWallTmapVals WallT;
		WallT.InitFromWallCoords(thread, &spr->wallc);

		ProjectedWallTexcoords walltexcoords;
		walltexcoords.Project(thread->Viewport.get(), spr->pic->GetWidth() << FRACBITS, x1, x2, WallT);

		iyscale = 1 / spr->yscale;
		double texturemid = (spr->gzt - thread->Viewport->viewpoint.Pos.Z) * iyscale;
		if (spr->renderflags & RF_XFLIP)
		{
			int right = (spr->pic->GetWidth() << FRACBITS) - 1;

			for (int i = x1; i < x2; i++)
			{
				walltexcoords.UPos[i] = right - walltexcoords.UPos[i];
			}
		}
		// Prepare lighting
		bool calclighting = false;
		FSWColormap *usecolormap = spr->Light.BaseColormap;
		bool rereadcolormap = true;

		// Decals that are added to the scene must fade to black.
		if (spr->RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
		{
			usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
			rereadcolormap = false;
		}

		SpriteDrawerArgs drawerargs;

		int shade = LightVisibility::LightLevelToShade(spr->sector->lightlevel + LightVisibility::ActualExtraLight(spr->foggy, thread->Viewport.get()), spr->foggy);
		double GlobVis = thread->Light->WallGlobVis(foggy);
		float lightleft = float(GlobVis / spr->wallc.sz1);
		float lightstep = float((GlobVis / spr->wallc.sz2 - lightleft) / (spr->wallc.sx2 - spr->wallc.sx1));
		float light = lightleft + (x1 - spr->wallc.sx1) * lightstep;
		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
			drawerargs.SetLight(usecolormap, 0, cameraLight->FixedLightLevelShade());
		else if (cameraLight->FixedColormap() != NULL)
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
		else if (!spr->foggy && (spr->renderflags & RF_FULLBRIGHT))
			drawerargs.SetLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : usecolormap, 0, 0);
		else
			calclighting = true;

		// Draw it
		FTexture *WallSpriteTile = spr->pic;
		if (spr->renderflags & RF_YFLIP)
		{
			sprflipvert = true;
			iyscale = -iyscale;
			texturemid -= spr->pic->GetHeight();
		}
		else
		{
			sprflipvert = false;
		}

		float maskedScaleY = (float)iyscale;

		int x = x1;

		FDynamicColormap *basecolormap = static_cast<FDynamicColormap*>(spr->Light.BaseColormap);

		bool visible = drawerargs.SetStyle(thread->Viewport.get(), spr->RenderStyle, spr->Alpha, spr->Translation, spr->FillColor, basecolormap);

		// R_SetPatchStyle can modify basecolormap.
		if (rereadcolormap)
		{
			usecolormap = spr->Light.BaseColormap;
		}

		if (!visible)
		{
			return;
		}
		else
		{
			RenderTranslucentPass *translucentPass = thread->TranslucentPass.get();

			thread->PrepareTexture(WallSpriteTile, spr->RenderStyle);
			while (x < x2)
			{
				if (calclighting)
				{ // calculate lighting
					drawerargs.SetLight(usecolormap, light, shade);
				}
				if (!translucentPass->ClipSpriteColumnWithPortals(x, spr))
					DrawColumn(thread, drawerargs, x, WallSpriteTile, walltexcoords, texturemid, maskedScaleY, sprflipvert, mfloorclip, mceilingclip, spr->RenderStyle);
				light += lightstep;
				x++;
			}
		}
	}

	void RenderWallSprite::DrawColumn(RenderThread *thread, SpriteDrawerArgs &drawerargs, int x, FTexture *WallSpriteTile, const ProjectedWallTexcoords &walltexcoords, double texturemid, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, FRenderStyle style)
	{
		auto viewport = thread->Viewport.get();
		
		float iscale = walltexcoords.VStep[x] * maskedScaleY;
		double spryscale = 1 / iscale;
		double sprtopscreen;
		if (sprflipvert)
			sprtopscreen = viewport->CenterY + texturemid * spryscale;
		else
			sprtopscreen = viewport->CenterY - texturemid * spryscale;

		drawerargs.DrawMaskedColumn(thread, x, FLOAT2FIXED(iscale), WallSpriteTile, walltexcoords.UPos[x], spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, style);
	}
}
