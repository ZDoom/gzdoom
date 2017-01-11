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
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Bool, r_drawvoxels)

namespace swrenderer
{
	void R_ProjectSprite(AActor *thing, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade)
	{
		double 			tr_x;
		double 			tr_y;

		double				gzt;				// killough 3/27/98
		double				gzb;				// [RH] use bottom of sprite, not actor
		double	 			tx;// , tx2;
		double 				tz;

		double 				xscale = 1, yscale = 1;

		int 				x1;
		int 				x2;

		FTextureID			picnum;
		FTexture			*tex;
		FVoxelDef			*voxel;

		vissprite_t*		vis;

		fixed_t 			iscale;

		sector_t*			heightsec;			// killough 3/27/98

												// Don't waste time projecting sprites that are definitely not visible.
		if (thing == nullptr ||
			(thing->renderflags & RF_INVISIBLE) ||
			!thing->RenderStyle.IsVisible(thing->Alpha) ||
			!thing->IsVisibleToPlayer() ||
			!thing->IsInsideVisibleAngles())
		{
			return;
		}

		RenderPortal *renderportal = RenderPortal::Instance();

		// [ZZ] Or less definitely not visible (hue)
		// [ZZ] 10.01.2016: don't try to clip stuff inside a skybox against the current portal.
		if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && !!P_PointOnLineSidePrecise(thing->Pos(), renderportal->CurrentPortal->dst))
			return;

		// [RH] Interpolate the sprite's position to make it look smooth
		DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
		pos.Z += thing->GetBobOffset(r_TicFracF);

		tex = nullptr;
		voxel = nullptr;

		int spritenum = thing->sprite;
		DVector2 spriteScale = thing->Scale;
		int renderflags = thing->renderflags;
		if (spriteScale.Y < 0)
		{
			spriteScale.Y = -spriteScale.Y;
			renderflags ^= RF_YFLIP;
		}
		if (thing->player != nullptr)
		{
			P_CheckPlayerSprite(thing, spritenum, spriteScale);
		}

		if (thing->picnum.isValid())
		{
			picnum = thing->picnum;

			tex = TexMan(picnum);
			if (tex->UseType == FTexture::TEX_Null)
			{
				return;
			}

			if (tex->Rotations != 0xFFFF)
			{
				// choose a different rotation based on player view
				spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
				DAngle ang = (pos - ViewPos).Angle();
				angle_t rot;
				if (sprframe->Texture[0] == sprframe->Texture[1])
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + 45.0 / 2 * 9).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + 45.0 / 2 * 9).BAMs() >> 28;
				}
				else
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
				}
				picnum = sprframe->Texture[rot];
				if (sprframe->Flip & (1 << rot))
				{
					renderflags ^= RF_XFLIP;
				}
				tex = TexMan[picnum];	// Do not animate the rotation
			}
		}
		else
		{
			// decide which texture to use for the sprite
			if ((unsigned)spritenum >= sprites.Size())
			{
				DPrintf(DMSG_ERROR, "R_ProjectSprite: invalid sprite number %u\n", spritenum);
				return;
			}
			spritedef_t *sprdef = &sprites[spritenum];
			if (thing->frame >= sprdef->numframes)
			{
				// If there are no frames at all for this sprite, don't draw it.
				return;
			}
			else
			{
				//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
				// choose a different rotation based on player view
				spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];
				DAngle ang = (pos - ViewPos).Angle();
				angle_t rot;
				if (sprframe->Texture[0] == sprframe->Texture[1])
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + 45.0 / 2 * 9).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + 45.0 / 2 * 9).BAMs() >> 28;
				}
				else
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
				}
				picnum = sprframe->Texture[rot];
				if (sprframe->Flip & (1 << rot))
				{
					renderflags ^= RF_XFLIP;
				}
				tex = TexMan[picnum];	// Do not animate the rotation
				if (r_drawvoxels)
				{
					voxel = sprframe->Voxel;
				}
			}
		}
		if (spriteScale.X < 0)
		{
			spriteScale.X = -spriteScale.X;
			renderflags ^= RF_XFLIP;
		}
		if (voxel == nullptr && (tex == nullptr || tex->UseType == FTexture::TEX_Null))
		{
			return;
		}

		if ((renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			R_ProjectWallSprite(thing, pos, picnum, spriteScale, renderflags, spriteshade);
			return;
		}

		// transform the origin point
		tr_x = pos.X - ViewPos.X;
		tr_y = pos.Y - ViewPos.Y;

		tz = tr_x * ViewTanCos + tr_y * ViewTanSin;

		// thing is behind view plane?
		if (voxel == nullptr && tz < MINZ)
			return;

		tx = tr_x * ViewSin - tr_y * ViewCos;

		// [RH] Flip for mirrors
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			tx = -tx;
		}
		//tx2 = tx >> 4;

		// too far off the side?
		// if it's a voxel, it can be further off the side
		if ((voxel == nullptr && (fabs(tx / 64) > fabs(tz))) ||
			(voxel != nullptr && (fabs(tx / 128) > fabs(tz))))
		{
			return;
		}

		if (voxel == nullptr)
		{
			// [RH] Added scaling
			int scaled_to = tex->GetScaledTopOffset();
			int scaled_bo = scaled_to - tex->GetScaledHeight();
			gzt = pos.Z + spriteScale.Y * scaled_to;
			gzb = pos.Z + spriteScale.Y * scaled_bo;
		}
		else
		{
			xscale = spriteScale.X * voxel->Scale;
			yscale = spriteScale.Y * voxel->Scale;
			double piv = voxel->Voxel->Mips[0].Pivot.Z;
			gzt = pos.Z + yscale * piv - thing->Floorclip;
			gzb = pos.Z + yscale * (piv - voxel->Voxel->Mips[0].SizeZ);
			if (gzt <= gzb)
				return;
		}

		// killough 3/27/98: exclude things totally separated
		// from the viewer, by either water or fake ceilings
		// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

		heightsec = thing->Sector->GetHeightSec();

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

		if (voxel == nullptr)
		{
			xscale = CenterX / tz;

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
			x1 = centerx + xs_RoundToInt(dtx1);

			// off the right side?
			if (x1 >= renderportal->WindowRight)
				return;

			tx += tex->GetWidth() * thingxscalemul;
			x2 = centerx + xs_RoundToInt(tx * xscale);

			// off the left side or too small?
			if ((x2 < renderportal->WindowLeft || x2 <= x1))
				return;

			xscale = spriteScale.X * xscale / tex->Scale.X;
			iscale = (fixed_t)(FRACUNIT / xscale); // Round towards zero to avoid wrapping in edge cases

			double yscale = spriteScale.Y / tex->Scale.Y;

			// store information in a vissprite
			vis = R_NewVisSprite();

			vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
			vis->xscale = FLOAT2FIXED(xscale);
			vis->yscale = float(InvZtoScale * yscale / tz);
			vis->idepth = float(1 / tz);
			vis->floorclip = thing->Floorclip / yscale;
			vis->texturemid = tex->TopOffset - (ViewPos.Z - pos.Z + thing->Floorclip) / yscale;
			vis->x1 = x1 < renderportal->WindowLeft ? renderportal->WindowLeft : x1;
			vis->x2 = x2 > renderportal->WindowRight ? renderportal->WindowRight : x2;
			vis->Angle = thing->Angles.Yaw;

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
		}
		else
		{
			vis = R_NewVisSprite();

			vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
			vis->xscale = FLOAT2FIXED(xscale);
			vis->yscale = (float)yscale;
			vis->x1 = renderportal->WindowLeft;
			vis->x2 = renderportal->WindowRight;
			vis->idepth = 1 / MINZ;
			vis->floorclip = thing->Floorclip;

			pos.Z -= thing->Floorclip;

			vis->Angle = thing->Angles.Yaw + voxel->AngleOffset;

			int voxelspin = (thing->flags & MF_DROPPED) ? voxel->DroppedSpin : voxel->PlacedSpin;
			if (voxelspin != 0)
			{
				DAngle ang = double(I_FPSTime()) * voxelspin / 1000;
				vis->Angle -= ang;
			}

			vis->pa.vpos = { (float)ViewPos.X, (float)ViewPos.Y, (float)ViewPos.Z };
			vis->pa.vang = FAngle((float)ViewAngle.Degrees);
		}

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
		vis->Style.RenderStyle = thing->RenderStyle;
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;		// [RH] thing translation table
		vis->FakeFlatStat = fakeside;
		vis->Style.Alpha = float(thing->Alpha);
		vis->fakefloor = fakefloor;
		vis->fakeceiling = fakeceiling;
		vis->Style.ColormapNum = 0;
		vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
		vis->bSplitSprite = false;

		if (voxel != nullptr)
		{
			vis->voxel = voxel->Voxel;
			vis->bIsVoxel = true;
			vis->bWallSprite = false;
			DrewAVoxel = true;
		}
		else
		{
			vis->pic = tex;
			vis->bIsVoxel = false;
			vis->bWallSprite = false;
		}

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		INTBOOL invertcolormap = (vis->Style.RenderStyle.Flags & STYLEF_InvertOverlay);

		if (vis->Style.RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		FDynamicColormap *mybasecolormap = basecolormap;
		if (current_sector->sectornum != thing->Sector->sectornum)	// compare sectornums to account for R_FakeFlat copies.
		{
			// Todo: The actor is from a different sector so we have to retrieve the proper basecolormap for that sector.
		}

		// Sprites that are added to the scene must fade to black.
		if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
		}

		if (vis->Style.RenderStyle.Flags & STYLEF_FadeToBlack)
		{
			if (invertcolormap)
			{ // Fade to white
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255, 255, 255), mybasecolormap->Desaturate);
				invertcolormap = false;
			}
			else
			{ // Fade to black
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0, 0, 0), mybasecolormap->Desaturate);
			}
		}

		// get light level
		if (fixedcolormap != nullptr)
		{ // fixed map
			vis->Style.BaseColormap = fixedcolormap;
			vis->Style.ColormapNum = 0;
		}
		else
		{
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (fixedlightlev >= 0)
			{
				vis->Style.BaseColormap = mybasecolormap;
				vis->Style.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
			}
			else if (!foggy && ((renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
			{ // full bright
				vis->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
				vis->Style.ColormapNum = 0;
			}
			else
			{ // diminished light
				vis->Style.ColormapNum = GETPALOOKUP(
					r_SpriteVisibility / MAX(tz, MINZ), spriteshade);
				vis->Style.BaseColormap = mybasecolormap;
			}
		}
	}

	void R_DrawVisSprite(vissprite_t *vis, const short *mfloorclip, const short *mceilingclip)
	{
		fixed_t 		frac;
		FTexture		*tex;
		int				x2;
		fixed_t			xiscale;
		bool			ispsprite = (!vis->sector && vis->gpos != FVector3(0, 0, 0));

		double spryscale, sprtopscreen;
		bool sprflipvert;

		if (vis->xscale == 0 || fabs(vis->yscale) < (1.0f / 32000.0f))
		{ // scaled to 0; can't see
			return;
		}

		fixed_t centeryfrac = FLOAT2FIXED(CenterY);
		R_SetColorMapLight(vis->Style.BaseColormap, 0, vis->Style.ColormapNum << FRACBITS);

		bool visible = R_SetPatchStyle(vis->Style.RenderStyle, vis->Style.Alpha, vis->Translation, vis->FillColor);

		if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Shaded])
		{ // For shaded sprites, R_SetPatchStyle sets a dc_colormap to an alpha table, but
		  // it is the brightest one. We need to get back to the proper light level for
		  // this sprite.
			R_SetColorMapLight(drawerargs::dc_fcolormap, 0, vis->Style.ColormapNum << FRACBITS);
		}

		if (visible)
		{
			tex = vis->pic;
			spryscale = vis->yscale;
			sprflipvert = false;
			fixed_t iscale = FLOAT2FIXED(1 / vis->yscale);
			frac = vis->startfrac;
			xiscale = vis->xiscale;
			dc_texturemid = vis->texturemid;

			if (vis->renderflags & RF_YFLIP)
			{
				sprflipvert = true;
				spryscale = -spryscale;
				iscale = -iscale;
				dc_texturemid -= vis->pic->GetHeight();
				sprtopscreen = CenterY + dc_texturemid * spryscale;
			}
			else
			{
				sprflipvert = false;
				sprtopscreen = CenterY - dc_texturemid * spryscale;
			}

			int x = vis->x1;
			x2 = vis->x2;

			if (x < x2)
			{
				while (x < x2)
				{
					if (ispsprite || !R_ClipSpriteColumnWithPortals(x, vis))
						R_DrawMaskedColumn(x, iscale, tex, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, false);
					x++;
					frac += xiscale;
				}
			}
		}

		R_FinishSetPatchStyle();

		NetUpdate();
	}
}
