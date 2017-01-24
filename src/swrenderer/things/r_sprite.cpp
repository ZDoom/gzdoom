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
#include "r_voxel.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	void RenderSprite::Project(AActor *thing, const DVector3 &pos, FTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade, bool foggy, FDynamicColormap *basecolormap)
	{
		// transform the origin point
		double tr_x = pos.X - ViewPos.X;
		double tr_y = pos.Y - ViewPos.Y;

		double tz = tr_x * ViewTanCos + tr_y * ViewTanSin;

		// thing is behind view plane?
		if (tz < MINZ)
			return;

		double tx = tr_x * ViewSin - tr_y * ViewCos;

		// [RH] Flip for mirrors
		RenderPortal *renderportal = RenderPortal::Instance();
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			tx = -tx;
		}
		//tx2 = tx >> 4;

		// too far off the side?
		if (fabs(tx / 64) > fabs(tz))
		{
			return;
		}

		// [RH] Added scaling
		int scaled_to = tex->GetScaledTopOffset();
		int scaled_bo = scaled_to - tex->GetScaledHeight();
		double gzt = pos.Z + spriteScale.Y * scaled_to;
		double gzb = pos.Z + spriteScale.Y * scaled_bo;

		// killough 3/27/98: exclude things totally separated
		// from the viewer, by either water or fake ceilings
		// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

		sector_t *heightsec = thing->Sector->GetHeightSec();

		if (heightsec != nullptr)	// only clip things which are in special sectors
		{
			if (fakeside == WaterFakeSide::AboveCeiling)
			{
				if (gzt < heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
			else if (fakeside == WaterFakeSide::BelowFloor)
			{
				if (gzb >= heightsec->floorplane.ZatPoint(pos))
					return;
			}
			else
			{
				if (gzt < heightsec->floorplane.ZatPoint(pos))
					return;
				if (!(heightsec->MoreFlags & SECF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
		}

		double xscale = CenterX / tz;

		// [RH] Reject sprites that are off the top or bottom of the screen
		if (globaluclip * tz > ViewPos.Z - gzb || globaldclip * tz < ViewPos.Z - gzt)
		{
			return;
		}

		// [RH] Flip for mirrors
		renderflags ^= renderportal->MirrorFlags & RF_XFLIP;

		// calculate edges of the shape
		const double thingxscalemul = spriteScale.X / tex->Scale.X;

		tx -= ((renderflags & RF_XFLIP) ? (tex->GetWidth() - tex->LeftOffset - 1) : tex->LeftOffset) * thingxscalemul;
		double dtx1 = tx * xscale;
		int x1 = centerx + xs_RoundToInt(dtx1);

		// off the right side?
		if (x1 >= renderportal->WindowRight)
			return;

		tx += tex->GetWidth() * thingxscalemul;
		int x2 = centerx + xs_RoundToInt(tx * xscale);

		// off the left side or too small?
		if ((x2 < renderportal->WindowLeft || x2 <= x1))
			return;

		xscale = spriteScale.X * xscale / tex->Scale.X;
		fixed_t iscale = (fixed_t)(FRACUNIT / xscale); // Round towards zero to avoid wrapping in edge cases

		double yscale = spriteScale.Y / tex->Scale.Y;

		// store information in a vissprite
		RenderSprite *vis = RenderMemory::NewObject<RenderSprite>();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = float(InvZtoScale * yscale / tz);
		vis->idepth = float(1 / tz);
		vis->floorclip = thing->Floorclip / yscale;
		vis->texturemid = tex->TopOffset - (ViewPos.Z - pos.Z + thing->Floorclip) / yscale;
		vis->x1 = x1 < renderportal->WindowLeft ? renderportal->WindowLeft : x1;
		vis->x2 = x2 > renderportal->WindowRight ? renderportal->WindowRight : x2;
		//vis->Angle = thing->Angles.Yaw;

		if (renderflags & RF_XFLIP)
		{
			vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
			vis->xiscale = -iscale;
		}
		else
		{
			vis->startfrac = 0;
			vis->xiscale = iscale;
		}

		vis->startfrac += (fixed_t)(vis->xiscale * (vis->x1 - centerx + 0.5 - dtx1));

		// killough 3/27/98: save sector for special clipping later
		vis->heightsec = heightsec;
		vis->sector = thing->Sector;

		vis->depth = (float)tz;
		vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
		vis->gzb = (float)gzb;		// [RH] use gzb, not thing->z
		vis->gzt = (float)gzt;		// killough 3/27/98
		vis->deltax = float(pos.X - ViewPos.X);
		vis->deltay = float(pos.Y - ViewPos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT)
			vis->renderflags |= RF_FULLBRIGHT; // kg3D
		vis->RenderStyle = thing->RenderStyle;
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;		// [RH] thing translation table
		vis->FakeFlatStat = fakeside;
		vis->Alpha = float(thing->Alpha);
		vis->fakefloor = fakefloor;
		vis->fakeceiling = fakeceiling;
		//vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
		//vis->bSplitSprite = false;

		vis->pic = tex;

		vis->foggy = foggy;

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		bool invertcolormap = (vis->RenderStyle.Flags & STYLEF_InvertOverlay) != 0;
		if (vis->RenderStyle.Flags & STYLEF_InvertSource)
			invertcolormap = !invertcolormap;

		if (vis->RenderStyle == LegacyRenderStyles[STYLE_Add] && basecolormap->Fade != 0)
		{
			basecolormap = GetSpecialLights(basecolormap->Color, 0, basecolormap->Desaturate);
		}

		bool fullbright = !vis->foggy && ((renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
		bool fadeToBlack = (vis->RenderStyle.Flags & STYLEF_FadeToBlack) != 0;

		vis->Light.SetColormap(r_SpriteVisibility / MAX(tz, MINZ), spriteshade, basecolormap, fullbright, invertcolormap, fadeToBlack);

		VisibleSpriteList::Instance()->Push(vis);
	}

	void RenderSprite::Render(short *mfloorclip, short *mceilingclip, int, int)
	{
		auto vis = this;

		fixed_t 		frac;
		FTexture		*tex;
		int				x2;
		fixed_t			xiscale;

		double spryscale, sprtopscreen;
		bool sprflipvert;

		if (vis->xscale == 0 || fabs(vis->yscale) < (1.0f / 32000.0f))
		{ // scaled to 0; can't see
			return;
		}

		fixed_t centeryfrac = FLOAT2FIXED(CenterY);
		R_SetColorMapLight(vis->Light.BaseColormap, 0, vis->Light.ColormapNum << FRACBITS);

		FDynamicColormap *basecolormap = static_cast<FDynamicColormap*>(vis->Light.BaseColormap);

		bool visible = R_SetPatchStyle(vis->RenderStyle, vis->Alpha, vis->Translation, vis->FillColor, basecolormap);

		if (vis->RenderStyle == LegacyRenderStyles[STYLE_Shaded])
		{ // For shaded sprites, R_SetPatchStyle sets a dc_colormap to an alpha table, but
		  // it is the brightest one. We need to get back to the proper light level for
		  // this sprite.
			R_SetColorMapLight(drawerargs::dc_fcolormap, 0, vis->Light.ColormapNum << FRACBITS);
		}

		if (visible)
		{
			tex = vis->pic;
			spryscale = vis->yscale;
			sprflipvert = false;
			fixed_t iscale = FLOAT2FIXED(1 / vis->yscale);
			frac = vis->startfrac;
			xiscale = vis->xiscale;
			double texturemid = vis->texturemid;

			if (vis->renderflags & RF_YFLIP)
			{
				sprflipvert = true;
				spryscale = -spryscale;
				iscale = -iscale;
				texturemid -= vis->pic->GetHeight();
				sprtopscreen = CenterY + texturemid * spryscale;
			}
			else
			{
				sprflipvert = false;
				sprtopscreen = CenterY - texturemid * spryscale;
			}

			int x = vis->x1;
			x2 = vis->x2;

			if (x < x2)
			{
				while (x < x2)
				{
					if (!RenderTranslucentPass::ClipSpriteColumnWithPortals(x, vis))
						R_DrawMaskedColumn(x, iscale, tex, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, false);
					x++;
					frac += xiscale;
				}
			}
		}

		NetUpdate();
	}
}
