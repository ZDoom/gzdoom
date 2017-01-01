
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
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
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_segs.h"
#include "swrenderer/scene/r_3dfloors.h"
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
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void R_ProjectWallSprite(AActor *thing, const DVector3 &pos, FTextureID picnum, const DVector2 &scale, int renderflags)
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

		if (wallc.sx1 >= WindowRight || wallc.sx2 <= WindowLeft)
			return;

		// Sprite sorting should probably treat these as walls, not sprites,
		// but right now, I just want to get them drawing.
		tz = (pos.X - ViewPos.X) * ViewTanCos + (pos.Y - ViewPos.Y) * ViewTanSin;

		int scaled_to = pic->GetScaledTopOffset();
		int scaled_bo = scaled_to - pic->GetScaledHeight();
		gzt = pos.Z + scale.Y * scaled_to;
		gzb = pos.Z + scale.Y * scaled_bo;

		vis = R_NewVisSprite();
		vis->CurrentPortalUniq = CurrentPortalUniq;
		vis->x1 = wallc.sx1 < WindowLeft ? WindowLeft : wallc.sx1;
		vis->x2 = wallc.sx2 >= WindowRight ? WindowRight : wallc.sx2;
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
		vis->FakeFlatStat = 0;
		vis->Style.Alpha = float(thing->Alpha);
		vis->fakefloor = NULL;
		vis->fakeceiling = NULL;
		vis->bInMirror = MirrorFlags & RF_XFLIP;
		vis->pic = pic;
		vis->bIsVoxel = false;
		vis->bWallSprite = true;
		vis->Style.ColormapNum = GETPALOOKUP(
			r_SpriteVisibility / MAX(tz, MINZ), spriteshade);
		vis->Style.BaseColormap = basecolormap;
		vis->wallc = wallc;
	}

	void R_DrawWallSprite(vissprite_t *spr)
	{
		int x1, x2;
		double iyscale;

		x1 = MAX<int>(spr->x1, spr->wallc.sx1);
		x2 = MIN<int>(spr->x2, spr->wallc.sx2);
		if (x1 >= x2)
			return;
		WallT.InitFromWallCoords(&spr->wallc);
		PrepWall(swall, lwall, spr->pic->GetWidth() << FRACBITS, x1, x2);
		iyscale = 1 / spr->yscale;
		dc_texturemid = (spr->gzt - ViewPos.Z) * iyscale;
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
		FDynamicColormap *usecolormap = basecolormap;
		bool rereadcolormap = true;

		// Decals that are added to the scene must fade to black.
		if (spr->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
		{
			usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
			rereadcolormap = false;
		}

		int shade = LIGHT2SHADE(spr->sector->lightlevel + r_actualextralight);
		GlobVis = r_WallVisibility;
		rw_lightleft = float(GlobVis / spr->wallc.sz1);
		rw_lightstep = float((GlobVis / spr->wallc.sz2 - rw_lightleft) / (spr->wallc.sx2 - spr->wallc.sx1));
		rw_light = rw_lightleft + (x1 - spr->wallc.sx1) * rw_lightstep;
		if (fixedlightlev >= 0)
			R_SetColorMapLight(usecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap != NULL)
			R_SetColorMapLight(fixedcolormap, 0, 0);
		else if (!foggy && (spr->renderflags & RF_FULLBRIGHT))
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : usecolormap, 0, 0);
		else
			calclighting = true;

		// Draw it
		WallSpriteTile = spr->pic;
		if (spr->renderflags & RF_YFLIP)
		{
			sprflipvert = true;
			iyscale = -iyscale;
			dc_texturemid -= spr->pic->GetHeight();
		}
		else
		{
			sprflipvert = false;
		}

		float maskedScaleY = (float)iyscale;

		int x = x1;

		bool visible = R_SetPatchStyle(spr->Style.RenderStyle, spr->Style.Alpha, spr->Translation, spr->FillColor);

		// R_SetPatchStyle can modify basecolormap.
		if (rereadcolormap)
		{
			usecolormap = basecolormap;
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
					R_SetColorMapLight(usecolormap, rw_light, shade);
				}
				if (!R_ClipSpriteColumnWithPortals(spr))
					R_WallSpriteColumn(x, maskedScaleY);
				x++;
			}
		}
		R_FinishSetPatchStyle();
	}

	void R_WallSpriteColumn(int x, float maskedScaleY)
	{
		using namespace drawerargs;

		dc_x = x;

		float iscale = swall[dc_x] * maskedScaleY;
		dc_iscale = FLOAT2FIXED(iscale);
		spryscale = 1 / iscale;
		if (sprflipvert)
			sprtopscreen = CenterY + dc_texturemid * spryscale;
		else
			sprtopscreen = CenterY - dc_texturemid * spryscale;

		dc_texturefrac = 0;
		R_DrawMaskedColumn(WallSpriteTile, lwall[dc_x]);
		rw_light += rw_lightstep;
	}
}
