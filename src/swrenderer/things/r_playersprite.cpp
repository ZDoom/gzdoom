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
#include "swrenderer/things/r_playersprite.h"
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
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/r_memory.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, st_scale)
EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	TArray<RenderPlayerSprite::vispsp_t> RenderPlayerSprite::vispsprites;
	unsigned int RenderPlayerSprite::vispspindex;

	double RenderPlayerSprite::pspritexscale;
	double RenderPlayerSprite::pspritexiscale;
	double RenderPlayerSprite::pspriteyscale;

	TArray<vissprite_t> RenderPlayerSprite::avis;

	void RenderPlayerSprite::SetupSpriteScale()
	{
		pspritexscale = centerxwide / 160.0;
		pspriteyscale = pspritexscale * YaspectMul;
		pspritexiscale = 1 / pspritexscale;
	}

	void RenderPlayerSprite::RenderPlayerSprites()
	{
		int 		i;
		int 		lightnum;
		DPSprite*	psp;
		DPSprite*	weapon;
		sector_t*	sec = NULL;
		static sector_t tempsec;
		int			floorlight, ceilinglight;
		F3DFloor *rover;

		if (!r_drawplayersprites ||
			!camera ||
			!camera->player ||
			(players[consoleplayer].cheats & CF_CHASECAM) ||
			(r_deathcamera && camera->health <= 0))
			return;

		if (fixedlightlev < 0 && viewsector->e && viewsector->e->XFloor.lightlist.Size())
		{
			for (i = viewsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			{
				if (ViewPos.Z <= viewsector->e->XFloor.lightlist[i].plane.Zat0())
				{
					rover = viewsector->e->XFloor.lightlist[i].caster;
					if (rover)
					{
						if (rover->flags & FF_DOUBLESHADOW && ViewPos.Z <= rover->bottom.plane->Zat0())
							break;
						sec = rover->model;
						if (rover->flags & FF_FADEWALLS)
							basecolormap = sec->ColorMap;
						else
							basecolormap = viewsector->e->XFloor.lightlist[i].extra_colormap;
					}
					break;
				}
			}
			if (!sec)
			{
				sec = viewsector;
				basecolormap = sec->ColorMap;
			}
			floorlight = ceilinglight = sec->lightlevel;
		}
		else
		{	// This used to use camera->Sector but due to interpolation that can be incorrect
			// when the interpolated viewpoint is in a different sector than the camera.
			sec = RenderOpaquePass::Instance()->FakeFlat(viewsector, &tempsec, &floorlight, &ceilinglight, nullptr, 0, 0, 0, 0);

			// [RH] set basecolormap
			basecolormap = sec->ColorMap;
		}

		// [RH] set foggy flag
		foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE));

		// get light level
		lightnum = ((floorlight + ceilinglight) >> 1) + R_ActualExtraLight(foggy);
		int spriteshade = LIGHT2SHADE(lightnum) - 24 * FRACUNIT;

		if (camera->player != NULL)
		{
			double centerhack = CenterY;
			double wx, wy;
			float bobx, boby;

			CenterY = viewheight / 2;

			P_BobWeapon(camera->player, &bobx, &boby, r_TicFracF);

			// Interpolate the main weapon layer once so as to be able to add it to other layers.
			if ((weapon = camera->player->FindPSprite(PSP_WEAPON)) != nullptr)
			{
				if (weapon->firstTic)
				{
					wx = weapon->x;
					wy = weapon->y;
				}
				else
				{
					wx = weapon->oldx + (weapon->x - weapon->oldx) * r_TicFracF;
					wy = weapon->oldy + (weapon->y - weapon->oldy) * r_TicFracF;
				}
			}
			else
			{
				wx = 0;
				wy = 0;
			}

			// add all active psprites
			psp = camera->player->psprites;
			while (psp)
			{
				// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
				// It's possible this psprite's caller is now null but the layer itself hasn't been destroyed
				// because it didn't tick yet (if we typed 'take all' while in the console for example).
				// In this case let's simply not draw it to avoid crashing.

				if ((psp->GetID() != PSP_TARGETCENTER || CrosshairImage == nullptr) && psp->GetCaller() != nullptr)
				{
					Render(psp, camera, bobx, boby, wx, wy, r_TicFracF, spriteshade);
				}

				psp = psp->GetNext();
			}

			CenterY = centerhack;
		}
	}

	void RenderPlayerSprite::Render(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int spriteshade)
	{
		double 				tx;
		int 				x1;
		int 				x2;
		double				sx, sy;
		spritedef_t*		sprdef;
		spriteframe_t*		sprframe;
		FTextureID			picnum;
		WORD				flip;
		FTexture*			tex;
		vissprite_t*		vis;
		bool				noaccel;
		double				alpha = owner->Alpha;

		if (avis.Size() < vispspindex + 1)
			avis.Reserve(avis.Size() - vispspindex + 1);

		// decide which patch to use
		if ((unsigned)pspr->GetSprite() >= (unsigned)sprites.Size())
		{
			DPrintf(DMSG_ERROR, "R_DrawPSprite: invalid sprite number %i\n", pspr->GetSprite());
			return;
		}
		sprdef = &sprites[pspr->GetSprite()];
		if (pspr->GetFrame() >= sprdef->numframes)
		{
			DPrintf(DMSG_ERROR, "R_DrawPSprite: invalid sprite frame %i : %i\n", pspr->GetSprite(), pspr->GetFrame());
			return;
		}
		sprframe = &SpriteFrames[sprdef->spriteframes + pspr->GetFrame()];

		picnum = sprframe->Texture[0];
		flip = sprframe->Flip & 1;
		tex = TexMan(picnum);

		if (tex->UseType == FTexture::TEX_Null || pspr->RenderStyle == STYLE_None)
			return;

		if (pspr->firstTic)
		{ // Can't interpolate the first tic.
			pspr->firstTic = false;
			pspr->oldx = pspr->x;
			pspr->oldy = pspr->y;
		}

		sx = pspr->oldx + (pspr->x - pspr->oldx) * ticfrac;
		sy = pspr->oldy + (pspr->y - pspr->oldy) * ticfrac + WEAPON_FUDGE_Y;

		if (pspr->Flags & PSPF_ADDBOB)
		{
			sx += bobx;
			sy += boby;
		}

		if (pspr->Flags & PSPF_ADDWEAPON && pspr->GetID() != PSP_WEAPON)
		{
			sx += wx;
			sy += wy;
		}

		// calculate edges of the shape
		tx = sx - BASEXCENTER;

		tx -= tex->GetScaledLeftOffset();
		x1 = xs_RoundToInt(CenterX + tx * pspritexscale);

		// off the right side
		if (x1 > viewwidth)
			return;

		tx += tex->GetScaledWidth();
		x2 = xs_RoundToInt(CenterX + tx * pspritexscale);

		// off the left side
		if (x2 <= 0)
			return;

		// store information in a vissprite
		vis = &avis[vispspindex];
		vis->renderflags = owner->renderflags;
		vis->floorclip = 0;

		vis->texturemid = (BASEYCENTER - sy) * tex->Scale.Y + tex->TopOffset;

		if (camera->player && (RenderTarget != screen ||
			viewheight == RenderTarget->GetHeight() ||
			(RenderTarget->GetWidth() > (BASEXCENTER * 2) && !st_scale)))
		{	// Adjust PSprite for fullscreen views
			AWeapon *weapon = dyn_cast<AWeapon>(pspr->GetCaller());
			if (weapon != nullptr && weapon->YAdjust != 0)
			{
				if (RenderTarget != screen || viewheight == RenderTarget->GetHeight())
				{
					vis->texturemid -= weapon->YAdjust;
				}
				else
				{
					vis->texturemid -= StatusBar->GetDisplacement() * weapon->YAdjust;
				}
			}
		}
		if (pspr->GetID() < PSP_TARGETCENTER)
		{ // Move the weapon down for 1280x1024.
			vis->texturemid -= AspectPspriteOffset(WidescreenRatio);
		}
		vis->x1 = x1 < 0 ? 0 : x1;
		vis->x2 = x2 >= viewwidth ? viewwidth : x2;
		vis->xscale = FLOAT2FIXED(pspritexscale / tex->Scale.X);
		vis->yscale = float(pspriteyscale / tex->Scale.Y);
		vis->Translation = 0;		// [RH] Use default colors
		vis->pic = tex;
		vis->Style.ColormapNum = 0;

		// If flip is used, provided that it's not already flipped (that would just invert itself)
		// (It's an XOR...)
		if (!(flip) != !(pspr->Flags & PSPF_FLIP))
		{
			vis->xiscale = -FLOAT2FIXED(pspritexiscale * tex->Scale.X);
			vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
		}
		else
		{
			vis->xiscale = FLOAT2FIXED(pspritexiscale * tex->Scale.X);
			vis->startfrac = 0;
		}

		if (vis->x1 > x1)
			vis->startfrac += vis->xiscale*(vis->x1 - x1);

		noaccel = false;
		FDynamicColormap *colormap_to_use = nullptr;
		if (pspr->GetID() < PSP_TARGETCENTER)
		{
			// [MC] Set the render style 

			if (pspr->Flags & PSPF_RENDERSTYLE)
			{
				const int rs = clamp<int>(pspr->RenderStyle, 0, STYLE_Count);

				if (pspr->Flags & PSPF_FORCESTYLE)
				{
					vis->Style.RenderStyle = LegacyRenderStyles[rs];
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_Fuzzy])
				{
					vis->Style.RenderStyle = LegacyRenderStyles[STYLE_Fuzzy];
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy])
				{
					vis->Style.RenderStyle = LegacyRenderStyles[STYLE_OptFuzzy];
					vis->Style.RenderStyle.CheckFuzz();
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_Subtract])
				{
					vis->Style.RenderStyle = LegacyRenderStyles[STYLE_Subtract];
				}
				else
				{
					vis->Style.RenderStyle = LegacyRenderStyles[rs];
				}
			}
			else
			{
				vis->Style.RenderStyle = owner->RenderStyle;
			}

			// Set the alpha based on if using the overlay's own or not. Also adjust
			// and override the alpha if not forced.
			if (pspr->Flags & PSPF_ALPHA)
			{
				if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Fuzzy])
				{
					alpha = owner->Alpha;
				}
				else if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy])
				{
					FRenderStyle style = vis->Style.RenderStyle;
					style.CheckFuzz();
					switch (style.BlendOp)
					{
					default:
						alpha = pspr->alpha * owner->Alpha;
						break;
					case STYLEOP_Fuzz:
					case STYLEOP_Sub:
						alpha = owner->Alpha;
						break;
					}

				}
				else if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Subtract])
				{
					alpha = owner->Alpha;
				}
				else if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Translucent] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_TranslucentStencil] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_AddStencil] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_AddShaded])
				{
					alpha = owner->Alpha * pspr->alpha;
				}
				else
				{
					alpha = owner->Alpha;
				}
			}

			// Should normal renderstyle come out on top at the end and we desire alpha,
			// switch it to translucent. Normal never applies any sort of alpha.
			if ((pspr->Flags & PSPF_ALPHA) &&
				vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Normal] &&
				vis->Style.Alpha < 1.0)
			{
				vis->Style.RenderStyle = LegacyRenderStyles[STYLE_Translucent];
				alpha = owner->Alpha * pspr->alpha;
			}

			// ALWAYS take priority if asked for, except fuzz. Fuzz does absolutely nothing
			// no matter what way it's changed.
			if (pspr->Flags & PSPF_FORCEALPHA)
			{
				//Due to lack of != operators...
				if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Fuzzy] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_SoulTrans] ||
					vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Stencil])
				{
				}
				else
				{
					alpha = pspr->alpha;
					vis->Style.RenderStyle.Flags |= STYLEF_ForceAlpha;
				}
			}
			vis->Style.Alpha = clamp<float>(float(alpha), 0.f, 1.f);

			// Due to how some of the effects are handled, going to 0 or less causes some
			// weirdness to display. There's no point rendering it anyway if it's 0.
			if (vis->Style.Alpha <= 0.)
				return;

			//-----------------------------------------------------------------------------

			// The software renderer cannot invert the source without inverting the overlay
			// too. That means if the source is inverted, we need to do the reverse of what
			// the invert overlay flag says to do.
			INTBOOL invertcolormap = (vis->Style.RenderStyle.Flags & STYLEF_InvertOverlay);

			if (vis->Style.RenderStyle.Flags & STYLEF_InvertSource)
			{
				invertcolormap = !invertcolormap;
			}

			FDynamicColormap *mybasecolormap = basecolormap;

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

			if (realfixedcolormap != nullptr && (!r_swtruecolor || (r_shadercolormaps && screen->Accel2D)))
			{ // fixed color
				vis->Style.BaseColormap = realfixedcolormap;
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
					vis->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
					vis->Style.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
				}
				else if (!foggy && pspr->GetState()->GetFullbright())
				{ // full bright
					vis->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;	// [RH] use basecolormap
					vis->Style.ColormapNum = 0;
				}
				else
				{ // local light
					vis->Style.BaseColormap = mybasecolormap;
					vis->Style.ColormapNum = GETPALOOKUP(0, spriteshade);
				}
			}
			if (camera->Inventory != nullptr)
			{
				BYTE oldcolormapnum = vis->Style.ColormapNum;
				FSWColormap *oldcolormap = vis->Style.BaseColormap;
				camera->Inventory->AlterWeaponSprite(&vis->Style);
				if (vis->Style.BaseColormap != oldcolormap || vis->Style.ColormapNum != oldcolormapnum)
				{
					// The colormap has changed. Is it one we can easily identify?
					// If not, then don't bother trying to identify it for
					// hardware accelerated drawing.
					if (vis->Style.BaseColormap < &SpecialColormaps[0] ||
						vis->Style.BaseColormap > &SpecialColormaps.Last())
					{
						noaccel = true;
					}
					// Has the basecolormap changed? If so, we can't hardware accelerate it,
					// since we don't know what it is anymore.
					else if (vis->Style.BaseColormap != mybasecolormap)
					{
						noaccel = true;
					}
				}
			}
			// If we're drawing with a special colormap, but shaders for them are disabled, do
			// not accelerate.
			if (!r_shadercolormaps && (vis->Style.BaseColormap >= &SpecialColormaps[0] &&
				vis->Style.BaseColormap <= &SpecialColormaps.Last()))
			{
				noaccel = true;
			}
			// If drawing with a BOOM colormap, disable acceleration.
			if (mybasecolormap == &NormalLight && NormalLight.Maps != realcolormaps.Maps)
			{
				noaccel = true;
			}
			// If the main colormap has fixed lights, and this sprite is being drawn with that
			// colormap, disable acceleration so that the lights can remain fixed.
			if (!noaccel && realfixedcolormap == nullptr &&
				NormalLightHasFixedLights && mybasecolormap == &NormalLight &&
				vis->pic->UseBasePalette())
			{
				noaccel = true;
			}
			// [SP] If emulating GZDoom fullbright, disable acceleration
			if (r_fullbrightignoresectorcolor && fixedlightlev >= 0)
				mybasecolormap = &FullNormalLight;
			if (r_fullbrightignoresectorcolor && !foggy && pspr->GetState()->GetFullbright())
				mybasecolormap = &FullNormalLight;
			colormap_to_use = mybasecolormap;
		}
		else
		{
			colormap_to_use = basecolormap;

			vis->Style.BaseColormap = basecolormap;
			vis->Style.ColormapNum = 0;
		}

		// Check for hardware-assisted 2D. If it's available, and this sprite is not
		// fuzzy, don't draw it until after the switch to 2D mode.
		if (!noaccel && RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
		{
			FRenderStyle style = vis->Style.RenderStyle;
			style.CheckFuzz();
			if (style.BlendOp != STYLEOP_Fuzz)
			{
				if (vispsprites.Size() < vispspindex + 1)
					vispsprites.Reserve(vispsprites.Size() - vispspindex + 1);

				vispsprites[vispspindex].vis = vis;
				vispsprites[vispspindex].basecolormap = colormap_to_use;
				vispsprites[vispspindex].x1 = x1;
				vispspindex++;
				return;
			}
		}

		// clip to screen bounds
		short *mfloorclip = screenheightarray;
		short *mceilingclip = zeroarray;

		RenderSprite::Render(vis, mfloorclip, mceilingclip);
	}

	void RenderPlayerSprite::RenderRemainingPlayerSprites()
	{
		for (unsigned int i = 0; i < vispspindex; i++)
		{
			vissprite_t *vis;

			vis = vispsprites[i].vis;
			FDynamicColormap *colormap = vispsprites[i].basecolormap;
			bool flip = vis->xiscale < 0;
			FSpecialColormap *special = NULL;
			PalEntry overlay = 0;
			FColormapStyle colormapstyle;
			bool usecolormapstyle = false;

			if (vis->Style.BaseColormap >= &SpecialColormaps[0] &&
				vis->Style.BaseColormap < &SpecialColormaps[SpecialColormaps.Size()])
			{
				special = static_cast<FSpecialColormap*>(vis->Style.BaseColormap);
			}
			else if (colormap->Color == PalEntry(255, 255, 255) &&
				colormap->Desaturate == 0)
			{
				overlay = colormap->Fade;
				overlay.a = BYTE(vis->Style.ColormapNum * 255 / NUMCOLORMAPS);
			}
			else
			{
				usecolormapstyle = true;
				colormapstyle.Color = colormap->Color;
				colormapstyle.Fade = colormap->Fade;
				colormapstyle.Desaturate = colormap->Desaturate;
				colormapstyle.FadeLevel = vis->Style.ColormapNum / float(NUMCOLORMAPS);
			}
			screen->DrawTexture(vis->pic,
				viewwindowx + vispsprites[i].x1,
				viewwindowy + viewheight / 2 - vis->texturemid * vis->yscale - 0.5,
				DTA_DestWidthF, FIXED2DBL(vis->pic->GetWidth() * vis->xscale),
				DTA_DestHeightF, vis->pic->GetHeight() * vis->yscale,
				DTA_Translation, TranslationToTable(vis->Translation),
				DTA_FlipX, flip,
				DTA_TopOffset, 0,
				DTA_LeftOffset, 0,
				DTA_ClipLeft, viewwindowx,
				DTA_ClipTop, viewwindowy,
				DTA_ClipRight, viewwindowx + viewwidth,
				DTA_ClipBottom, viewwindowy + viewheight,
				DTA_AlphaF, vis->Style.Alpha,
				DTA_RenderStyle, vis->Style.RenderStyle,
				DTA_FillColor, vis->FillColor,
				DTA_SpecialColormap, special,
				DTA_ColorOverlay, overlay.d,
				DTA_ColormapStyle, usecolormapstyle ? &colormapstyle : NULL,
				TAG_DONE);
		}

		vispspindex = 0;
	}
}
