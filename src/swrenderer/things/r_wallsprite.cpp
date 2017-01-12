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
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void RenderWallSprite::Project(AActor *thing, const DVector3 &pos, FTextureID picnum, const DVector2 &scale, int renderflags, int spriteshade, bool foggy, FDynamicColormap *basecolormap)
	{
		FWallCoords wallc;
		double x1, x2;
		DVector2 left, right;
		double gzb, gzt, tz;
		FTexture *pic = TexMan(picnum, true);
		DAngle ang = thing->Angles.Yaw + 90;
		double angcos = ang.Cos();
		double angsin = ang.Sin();
		vissprite_t *vis;

		// Determine left and right edges of sprite. The sprite's angle is its normal,
		// so the edges are 90 degrees each side of it.
		x2 = pic->GetScaledWidth();
		x1 = pic->GetScaledLeftOffset();

		x1 *= scale.X;
		x2 *= scale.X;

		left.X = pos.X - x1 * angcos - ViewPos.X;
		left.Y = pos.Y - x1 * angsin - ViewPos.Y;
		right.X = left.X + x2 * angcos;
		right.Y = right.Y + x2 * angsin;

		// Is it off-screen?
		if (wallc.Init(left, right, TOO_CLOSE_Z))
			return;
			
		RenderPortal *renderportal = RenderPortal::Instance();

		if (wallc.sx1 >= renderportal->WindowRight || wallc.sx2 <= renderportal->WindowLeft)
			return;

		// Sprite sorting should probably treat these as walls, not sprites,
		// but right now, I just want to get them drawing.
		tz = (pos.X - ViewPos.X) * ViewTanCos + (pos.Y - ViewPos.Y) * ViewTanSin;

		int scaled_to = pic->GetScaledTopOffset();
		int scaled_bo = scaled_to - pic->GetScaledHeight();
		gzt = pos.Z + scale.Y * scaled_to;
		gzb = pos.Z + scale.Y * scaled_bo;

		vis = VisibleSpriteList::Add();
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
		vis->deltax = float(pos.X - ViewPos.X);
		vis->deltay = float(pos.Y - ViewPos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT) vis->renderflags |= RF_FULLBRIGHT; // kg3D
		vis->Style.RenderStyle = thing->RenderStyle;
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;
		vis->FakeFlatStat = WaterFakeSide::Center;
		vis->Style.Alpha = float(thing->Alpha);
		vis->fakefloor = NULL;
		vis->fakeceiling = NULL;
		vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
		vis->pic = pic;
		vis->bIsVoxel = false;
		vis->bWallSprite = true;
		vis->Style.ColormapNum = GETPALOOKUP(
			r_SpriteVisibility / MAX(tz, MINZ), spriteshade);
		vis->Style.BaseColormap = basecolormap;
		vis->wallc = wallc;
		vis->foggy = foggy;
	}

	void RenderWallSprite::Render(vissprite_t *spr, const short *mfloorclip, const short *mceilingclip)
	{
		int x1, x2;
		double iyscale;
		bool sprflipvert;

		x1 = MAX<int>(spr->x1, spr->wallc.sx1);
		x2 = MIN<int>(spr->x2, spr->wallc.sx2);
		if (x1 >= x2)
			return;
		FWallTmapVals WallT;
		WallT.InitFromWallCoords(&spr->wallc);
		PrepWall(swall, lwall, spr->pic->GetWidth() << FRACBITS, x1, x2, WallT);
		iyscale = 1 / spr->yscale;
		double texturemid = (spr->gzt - ViewPos.Z) * iyscale;
		if (spr->renderflags & RF_XFLIP)
		{
			int right = (spr->pic->GetWidth() << FRACBITS) - 1;

			for (int i = x1; i < x2; i++)
			{
				lwall[i] = right - lwall[i];
			}
		}
		// Prepare lighting
		bool calclighting = false;
		FSWColormap *usecolormap = spr->Style.BaseColormap;
		bool rereadcolormap = true;

		// Decals that are added to the scene must fade to black.
		if (spr->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
		{
			usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
			rereadcolormap = false;
		}

		int shade = LIGHT2SHADE(spr->sector->lightlevel + R_ActualExtraLight(spr->foggy));
		double GlobVis = r_WallVisibility;
		float lightleft = float(GlobVis / spr->wallc.sz1);
		float lightstep = float((GlobVis / spr->wallc.sz2 - lightleft) / (spr->wallc.sx2 - spr->wallc.sx1));
		float light = lightleft + (x1 - spr->wallc.sx1) * lightstep;
		if (fixedlightlev >= 0)
			R_SetColorMapLight(usecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap != NULL)
			R_SetColorMapLight(fixedcolormap, 0, 0);
		else if (!spr->foggy && (spr->renderflags & RF_FULLBRIGHT))
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : usecolormap, 0, 0);
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

		FDynamicColormap *basecolormap = static_cast<FDynamicColormap*>(spr->Style.BaseColormap);

		bool visible = R_SetPatchStyle(spr->Style.RenderStyle, spr->Style.Alpha, spr->Translation, spr->FillColor, basecolormap);

		// R_SetPatchStyle can modify basecolormap.
		if (rereadcolormap)
		{
			usecolormap = spr->Style.BaseColormap;
		}

		if (!visible)
		{
			return;
		}
		else
		{
			while (x < x2)
			{
				if (calclighting)
				{ // calculate lighting
					R_SetColorMapLight(usecolormap, light, shade);
				}
				if (!RenderTranslucentPass::ClipSpriteColumnWithPortals(x, spr))
					DrawColumn(x, WallSpriteTile, texturemid, maskedScaleY, sprflipvert, mfloorclip, mceilingclip);
				light += lightstep;
				x++;
			}
		}
	}

	void RenderWallSprite::DrawColumn(int x, FTexture *WallSpriteTile, double texturemid, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip)
	{
		float iscale = swall[x] * maskedScaleY;
		double spryscale = 1 / iscale;
		double sprtopscreen;
		if (sprflipvert)
			sprtopscreen = CenterY + texturemid * spryscale;
		else
			sprtopscreen = CenterY - texturemid * spryscale;

		R_DrawMaskedColumn(x, FLOAT2FIXED(iscale), WallSpriteTile, lwall[x], spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip);
	}
}
