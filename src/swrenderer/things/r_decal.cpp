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
#include <stddef.h>
#include "templates.h"
#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "r_sky.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "swrenderer/scene/r_bsp.h"
#include "r_decal.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/drawers/r_draw.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "r_wallsprite.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void R_RenderDecals(side_t *sidedef, drawseg_t *draw_segment, int wallshade, float lightleft, float lightstep, seg_t *curline, const FWallCoords &wallC)
	{
		for (DBaseDecal *decal = sidedef->AttachedDecals; decal != NULL; decal = decal->WallNext)
		{
			R_RenderDecal(sidedef, decal, draw_segment, wallshade, lightleft, lightstep, curline, wallC, 0);
		}
	}

	// pass = 0: when seg is first drawn
	//		= 1: drawing masked textures (including sprites)
	// Currently, only pass = 0 is done or used

	void R_RenderDecal(side_t *wall, DBaseDecal *decal, drawseg_t *clipper, int wallshade, float lightleft, float lightstep, seg_t *curline, FWallCoords WallC, int pass)
	{
		DVector2 decal_left, decal_right, decal_pos;
		int x1, x2;
		double yscale;
		BYTE flipx;
		double zpos;
		int needrepeat = 0;
		sector_t *front, *back;
		bool calclighting;
		bool rereadcolormap;
		FDynamicColormap *usecolormap;
		float light = 0;

		if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
			return;

		// Determine actor z
		zpos = decal->Z;
		front = curline->frontsector;
		back = (curline->backsector != NULL) ? curline->backsector : curline->frontsector;
		switch (decal->RenderFlags & RF_RELMASK)
		{
		default:
			zpos = decal->Z;
			break;
		case RF_RELUPPER:
			if (curline->linedef->flags & ML_DONTPEGTOP)
			{
				zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
			}
			else
			{
				zpos = decal->Z + back->GetPlaneTexZ(sector_t::ceiling);
			}
			break;
		case RF_RELLOWER:
			if (curline->linedef->flags & ML_DONTPEGBOTTOM)
			{
				zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
			}
			else
			{
				zpos = decal->Z + back->GetPlaneTexZ(sector_t::floor);
			}
			break;
		case RF_RELMID:
			if (curline->linedef->flags & ML_DONTPEGBOTTOM)
			{
				zpos = decal->Z + front->GetPlaneTexZ(sector_t::floor);
			}
			else
			{
				zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
			}
		}

		WallSpriteTile = TexMan(decal->PicNum, true);
		flipx = (BYTE)(decal->RenderFlags & RF_XFLIP);

		if (WallSpriteTile == NULL || WallSpriteTile->UseType == FTexture::TEX_Null)
		{
			return;
		}

		// Determine left and right edges of sprite. Since this sprite is bound
		// to a wall, we use the wall's angle instead of the decal's. This is
		// pretty much the same as what R_AddLine() does.

		FWallCoords savecoord = WallC;

		double edge_right = WallSpriteTile->GetWidth();
		double edge_left = WallSpriteTile->LeftOffset;
		edge_right = (edge_right - edge_left) * decal->ScaleX;
		edge_left *= decal->ScaleX;

		double dcx, dcy;
		decal->GetXY(wall, dcx, dcy);
		decal_pos = { dcx, dcy };

		DVector2 angvec = (curline->v2->fPos() - curline->v1->fPos()).Unit();
		float maskedScaleY;

		decal_left = decal_pos - edge_left * angvec - ViewPos;
		decal_right = decal_pos + edge_right * angvec - ViewPos;

		if (WallC.Init(decal_left, decal_right, TOO_CLOSE_Z))
			goto done;

		x1 = WallC.sx1;
		x2 = WallC.sx2;

		if (x1 >= clipper->x2 || x2 <= clipper->x1)
			goto done;

		FWallTmapVals WallT;
		WallT.InitFromWallCoords(&WallC);

		// Get the top and bottom clipping arrays
		switch (decal->RenderFlags & RF_CLIPMASK)
		{
		case RF_CLIPFULL:
			if (curline->backsector == NULL)
			{
				if (pass != 0)
				{
					goto done;
				}
				mceilingclip = walltop;
				mfloorclip = wallbottom;
			}
			else if (pass == 0)
			{
				mceilingclip = walltop;
				mfloorclip = RenderBSP::Instance()->ceilingclip;
				needrepeat = 1;
			}
			else
			{
				mceilingclip = openings + clipper->sprtopclip - clipper->x1;
				mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
			}
			break;

		case RF_CLIPUPPER:
			if (pass != 0)
			{
				goto done;
			}
			mceilingclip = walltop;
			mfloorclip = RenderBSP::Instance()->ceilingclip;
			break;

		case RF_CLIPMID:
			if (curline->backsector != NULL && pass != 2)
			{
				goto done;
			}
			mceilingclip = openings + clipper->sprtopclip - clipper->x1;
			mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
			break;

		case RF_CLIPLOWER:
			if (pass != 0)
			{
				goto done;
			}
			mceilingclip = RenderBSP::Instance()->floorclip;
			mfloorclip = wallbottom;
			break;
		}

		yscale = decal->ScaleY;
		dc_texturemid = WallSpriteTile->TopOffset + (zpos - ViewPos.Z) / yscale;

		// Clip sprite to drawseg
		x1 = MAX<int>(clipper->x1, x1);
		x2 = MIN<int>(clipper->x2, x2);
		if (x1 >= x2)
		{
			goto done;
		}

		PrepWall(swall, lwall, WallSpriteTile->GetWidth(), x1, x2, WallT);

		if (flipx)
		{
			int i;
			int right = (WallSpriteTile->GetWidth() << FRACBITS) - 1;

			for (i = x1; i < x2; i++)
			{
				lwall[i] = right - lwall[i];
			}
		}

		// Prepare lighting
		calclighting = false;
		usecolormap = basecolormap;
		rereadcolormap = true;

		// Decals that are added to the scene must fade to black.
		if (decal->RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
		{
			usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
			rereadcolormap = false;
		}

		light = lightleft + (x1 - savecoord.sx1) * lightstep;
		if (fixedlightlev >= 0)
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : usecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap != NULL)
			R_SetColorMapLight(fixedcolormap, 0, 0);
		else if (!foggy && (decal->RenderFlags & RF_FULLBRIGHT))
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : usecolormap, 0, 0);
		else
			calclighting = true;

		// Draw it
		if (decal->RenderFlags & RF_YFLIP)
		{
			sprflipvert = true;
			yscale = -yscale;
			dc_texturemid -= WallSpriteTile->GetHeight();
		}
		else
		{
			sprflipvert = false;
		}

		maskedScaleY = float(1 / yscale);
		do
		{
			int x = x1;

			bool visible = R_SetPatchStyle(decal->RenderStyle, (float)decal->Alpha, decal->Translation, decal->AlphaColor);

			// R_SetPatchStyle can modify basecolormap.
			if (rereadcolormap)
			{
				usecolormap = basecolormap;
			}

			if (visible)
			{
				while (x < x2)
				{
					if (calclighting)
					{ // calculate lighting
						R_SetColorMapLight(usecolormap, light, wallshade);
					}
					R_DecalColumn(x, maskedScaleY);
					light += lightstep;
					x++;
				}
			}

			// If this sprite is RF_CLIPFULL on a two-sided line, needrepeat will
			// be set 1 if we need to draw on the lower wall. In all other cases,
			// needrepeat will be 0, and the while will fail.
			mceilingclip = RenderBSP::Instance()->floorclip;
			mfloorclip = wallbottom;
			R_FinishSetPatchStyle();
		} while (needrepeat--);

		colfunc = basecolfunc;

		R_FinishSetPatchStyle();
	done:
		WallC = savecoord;
	}

	void R_DecalColumn(int x, float maskedScaleY)
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
	}
}
