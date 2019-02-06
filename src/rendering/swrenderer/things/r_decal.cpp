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

#include <stdlib.h>
#include <stddef.h>
#include "templates.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "r_sky.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "r_decal.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/drawers/r_draw.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/viewport/r_spritedrawer.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void RenderDecal::RenderDecals(RenderThread *thread, side_t *sidedef, DrawSegment *draw_segment, seg_t *curline, const ProjectedWallLight &light, const short *walltop, const short *wallbottom, bool drawsegPass)
	{
		for (DBaseDecal *decal = sidedef->AttachedDecals; decal != NULL; decal = decal->WallNext)
		{
			Render(thread, sidedef, decal, draw_segment, curline, light, walltop, wallbottom, drawsegPass);
		}
	}

	void RenderDecal::Render(RenderThread *thread, side_t *wall, DBaseDecal *decal, DrawSegment *clipper, seg_t *curline, const ProjectedWallLight &light, const short *walltop, const short *wallbottom, bool drawsegPass)
	{
		DVector2 decal_left, decal_right, decal_pos;
		int x1, x2;
		double yscale;
		uint8_t flipx;
		double zpos;
		int needrepeat = 0;
		sector_t *back;
		FDynamicColormap *usecolormap;
		const short *mfloorclip;
		const short *mceilingclip;

		if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
			return;

		// Determine actor z
		zpos = decal->Z;
		back = (curline->backsector != NULL) ? curline->backsector : curline->frontsector;

		// for 3d-floor segments use the model sector as reference
		sector_t *front;
		if ((decal->RenderFlags&RF_CLIPMASK) == RF_CLIPMID) front = decal->Sector;
		else front = curline->frontsector;

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

		FTexture *tex = TexMan.GetPalettedTexture(decal->PicNum, true);
		flipx = (uint8_t)(decal->RenderFlags & RF_XFLIP);

		if (tex == NULL || !tex->isValid())
		{
			return;
		}
		FSoftwareTexture *WallSpriteTile = tex->GetSoftwareTexture();

		// Determine left and right edges of sprite. Since this sprite is bound
		// to a wall, we use the wall's angle instead of the decal's. This is
		// pretty much the same as what R_AddLine() does.

		double edge_right = WallSpriteTile->GetWidth();
		double edge_left = WallSpriteTile->GetLeftOffset(0);	// decals should not use renderer-specific offsets.
		edge_right = (edge_right - edge_left) * decal->ScaleX;
		edge_left *= decal->ScaleX;

		double dcx, dcy;
		decal->GetXY(wall, dcx, dcy);
		decal_pos = { dcx, dcy };

		DVector2 angvec = (curline->v2->fPos() - curline->v1->fPos()).Unit();
		float maskedScaleY;

		decal_left = decal_pos - edge_left * angvec - thread->Viewport->viewpoint.Pos;
		decal_right = decal_pos + edge_right * angvec - thread->Viewport->viewpoint.Pos;

		CameraLight *cameraLight;
		double texturemid;

		FWallCoords WallC;
		if (WallC.Init(thread, decal_left, decal_right, TOO_CLOSE_Z))
			return;

		x1 = WallC.sx1;
		x2 = WallC.sx2;

		if (x1 >= clipper->x2 || x2 <= clipper->x1)
			return;

		FWallTmapVals WallT;
		WallT.InitFromWallCoords(thread, &WallC);

		if (drawsegPass)
		{
			uint32_t clipMode = decal->RenderFlags & RF_CLIPMASK;
			if (clipMode != RF_CLIPMID && clipMode != RF_CLIPFULL)
				return;

			// Clip decal to stay within the draw segment wall
			mceilingclip = walltop;
			mfloorclip = wallbottom;

			// Rumor has it that if RT_CLIPMASK is specified then the decal should be clipped according
			// to the full drawsegment visibility, as implemented in the remarked section below.
			//
			// This is problematic because not all 3d floors may have been drawn yet at this point. The
			// code below might work ok for cases where there is only one 3d floor.

			/*if (clipMode == RF_CLIPFULL)
			{
				mceilingclip = clipper->sprtopclip - clipper->x1;
				mfloorclip = clipper->sprbottomclip - clipper->x1;
			}*/
		}
		else
		{
			// Get the top and bottom clipping arrays
			switch (decal->RenderFlags & RF_CLIPMASK)
			{
			default:
				// keep GCC quiet.
				return;

			case RF_CLIPFULL:
				if (curline->backsector == NULL)
				{
					mceilingclip = walltop;
					mfloorclip = wallbottom;
				}
				else
				{
					mceilingclip = walltop;
					mfloorclip = thread->OpaquePass->ceilingclip;
					needrepeat = 1;
				}
				break;

			case RF_CLIPUPPER:
				mceilingclip = walltop;
				mfloorclip = thread->OpaquePass->ceilingclip;
				break;

			case RF_CLIPMID:
				return;

			case RF_CLIPLOWER:
				mceilingclip = thread->OpaquePass->floorclip;
				mfloorclip = wallbottom;
				break;
			}
		}

		yscale = decal->ScaleY;
		texturemid = WallSpriteTile->GetTopOffset(0) + (zpos - thread->Viewport->viewpoint.Pos.Z) / yscale;

		// Clip sprite to drawseg
		x1 = MAX<int>(clipper->x1, x1);
		x2 = MIN<int>(clipper->x2, x2);
		if (x1 >= x2)
		{
			return;
		}

		ProjectedWallTexcoords walltexcoords;
		walltexcoords.Project(thread->Viewport.get(), WallSpriteTile->GetWidth(), x1, x2, WallT);

		if (flipx)
		{
			int i;
			int right = (WallSpriteTile->GetWidth() << FRACBITS) - 1;

			for (i = x1; i < x2; i++)
			{
				walltexcoords.UPos[i] = right - walltexcoords.UPos[i];
			}
		}

		// Prepare lighting
		usecolormap = light.GetBaseColormap();

		// Decals that are added to the scene must fade to black.
		if (decal->RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
		{
			usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
		}

		float lightpos = light.GetLightPos(x1);

		cameraLight = CameraLight::Instance();

		// Draw it
		bool sprflipvert;
		if (decal->RenderFlags & RF_YFLIP)
		{
			sprflipvert = true;
			yscale = -yscale;
			texturemid -= WallSpriteTile->GetHeight();
		}
		else
		{
			sprflipvert = false;
		}

		maskedScaleY = float(1 / yscale);
		do
		{
			int x = x1;

			ColormapLight cmlight;
			cmlight.SetColormap(thread, MINZ, light.GetLightLevel(), light.GetFoggy(), usecolormap, decal->RenderFlags & RF_FULLBRIGHT, false, false, false, false);

			SpriteDrawerArgs drawerargs;
			bool visible = drawerargs.SetStyle(thread->Viewport.get(), decal->RenderStyle, (float)decal->Alpha, decal->Translation, decal->AlphaColor, cmlight);
			bool calclighting = cameraLight->FixedLightLevel() < 0 && !cameraLight->FixedColormap();

			if (visible)
			{
				thread->PrepareTexture(WallSpriteTile, decal->RenderStyle);
				while (x < x2)
				{
					if (calclighting)
					{ // calculate lighting
						drawerargs.SetLight(lightpos, light.GetLightLevel(), light.GetFoggy(), thread->Viewport.get());
					}
					DrawColumn(thread, drawerargs, x, WallSpriteTile, walltexcoords, texturemid, maskedScaleY, sprflipvert, mfloorclip, mceilingclip, decal->RenderStyle);
					lightpos += light.GetLightStep();
					x++;
				}
			}

			// If this sprite is RF_CLIPFULL on a two-sided line, needrepeat will
			// be set 1 if we need to draw on the lower wall. In all other cases,
			// needrepeat will be 0, and the while will fail.
			mceilingclip = thread->OpaquePass->floorclip;
			mfloorclip = wallbottom;
		} while (needrepeat--);
	}

	void RenderDecal::DrawColumn(RenderThread *thread, SpriteDrawerArgs &drawerargs, int x, FSoftwareTexture *WallSpriteTile, const ProjectedWallTexcoords &walltexcoords, double texturemid, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, FRenderStyle style)
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
