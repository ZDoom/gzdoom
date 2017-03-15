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
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, st_scale)
EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	RenderPlayerSprites::RenderPlayerSprites(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderPlayerSprites::Render()
	{
		int 		i;
		int 		lightnum;
		DPSprite*	psp;
		DPSprite*	weapon;
		sector_t*	sec = NULL;
		int			floorlight, ceilinglight;
		F3DFloor *rover;

		if (!r_drawplayersprites ||
			!Thread->Viewport->viewpoint.camera ||
			!Thread->Viewport->viewpoint.camera->player ||
			(players[consoleplayer].cheats & CF_CHASECAM) ||
			(r_deathcamera && Thread->Viewport->viewpoint.camera->health <= 0))
			return;

		FDynamicColormap *basecolormap;
		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() < 0 && Thread->Viewport->viewpoint.sector->e && Thread->Viewport->viewpoint.sector->e->XFloor.lightlist.Size())
		{
			for (i = Thread->Viewport->viewpoint.sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			{
				if (Thread->Viewport->viewpoint.Pos.Z <= Thread->Viewport->viewpoint.sector->e->XFloor.lightlist[i].plane.Zat0())
				{
					rover = Thread->Viewport->viewpoint.sector->e->XFloor.lightlist[i].caster;
					if (rover)
					{
						if (rover->flags & FF_DOUBLESHADOW && Thread->Viewport->viewpoint.Pos.Z <= rover->bottom.plane->Zat0())
							break;
						sec = rover->model;
						if (rover->flags & FF_FADEWALLS)
							basecolormap = sec->ColorMap;
						else
							basecolormap = Thread->Viewport->viewpoint.sector->e->XFloor.lightlist[i].extra_colormap;
					}
					break;
				}
			}
			if (!sec)
			{
				sec = Thread->Viewport->viewpoint.sector;
				basecolormap = sec->ColorMap;
			}
			floorlight = ceilinglight = sec->lightlevel;
		}
		else
		{	// This used to use camera->Sector but due to interpolation that can be incorrect
			// when the interpolated viewpoint is in a different sector than the camera.
			sec = Thread->OpaquePass->FakeFlat(Thread->Viewport->viewpoint.sector, &tempsec, &floorlight, &ceilinglight, nullptr, 0, 0, 0, 0);

			// [RH] set basecolormap
			basecolormap = sec->ColorMap;
		}

		// [RH] set foggy flag
		bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE));

		// get light level
		lightnum = ((floorlight + ceilinglight) >> 1) + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get());
		int spriteshade = LightVisibility::LightLevelToShade(lightnum, foggy) - 24 * FRACUNIT;

		if (Thread->Viewport->viewpoint.camera->player != NULL)
		{
			auto viewport = Thread->Viewport.get();
			
			double centerhack = viewport->CenterY;
			double wx, wy;
			float bobx, boby;

			viewport->CenterY = viewheight / 2;

			P_BobWeapon(viewport->viewpoint.camera->player, &bobx, &boby, viewport->viewpoint.TicFrac);

			// Interpolate the main weapon layer once so as to be able to add it to other layers.
			if ((weapon = viewport->viewpoint.camera->player->FindPSprite(PSP_WEAPON)) != nullptr)
			{
				if (weapon->firstTic)
				{
					wx = weapon->x;
					wy = weapon->y;
				}
				else
				{
					wx = weapon->oldx + (weapon->x - weapon->oldx) * viewport->viewpoint.TicFrac;
					wy = weapon->oldy + (weapon->y - weapon->oldy) * viewport->viewpoint.TicFrac;
				}
			}
			else
			{
				wx = 0;
				wy = 0;
			}

			// add all active psprites
			psp = viewport->viewpoint.camera->player->psprites;
			while (psp)
			{
				// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
				// It's possible this psprite's caller is now null but the layer itself hasn't been destroyed
				// because it didn't tick yet (if we typed 'take all' while in the console for example).
				// In this case let's simply not draw it to avoid crashing.

				if ((psp->GetID() != PSP_TARGETCENTER || CrosshairImage == nullptr) && psp->GetCaller() != nullptr)
				{
					RenderSprite(psp, viewport->viewpoint.camera, bobx, boby, wx, wy, viewport->viewpoint.TicFrac, spriteshade, basecolormap, foggy);
				}

				psp = psp->GetNext();
			}

			viewport->CenterY = centerhack;
		}
	}

	void RenderPlayerSprites::RenderSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int spriteshade, FDynamicColormap *basecolormap, bool foggy)
	{
		double 				tx;
		int 				x1;
		int 				x2;
		double				sx, sy;
		spritedef_t*		sprdef;
		spriteframe_t*		sprframe;
		FTextureID			picnum;
		uint16_t				flip;
		FTexture*			tex;
		bool				noaccel;
		double				alpha = owner->Alpha;

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
		
		auto viewport = Thread->Viewport.get();

		double pspritexscale = viewport->viewwindow.centerxwide / 160.0;
		double pspriteyscale = pspritexscale * viewport->YaspectMul;
		double pspritexiscale = 1 / pspritexscale;

		// calculate edges of the shape
		tx = sx - BASEXCENTER;

		tx -= tex->GetScaledLeftOffset();
		x1 = xs_RoundToInt(viewport->CenterX + tx * pspritexscale);

		// off the right side
		if (x1 > viewwidth)
			return;

		tx += tex->GetScaledWidth();
		x2 = xs_RoundToInt(viewport->CenterX + tx * pspritexscale);

		// off the left side
		if (x2 <= 0)
			return;

		// store information in a vissprite
		NoAccelPlayerSprite vis;

		vis.renderflags = owner->renderflags;

		vis.texturemid = (BASEYCENTER - sy) * tex->Scale.Y + tex->TopOffset;

		if (Thread->Viewport->viewpoint.camera->player && (viewport->RenderTarget != screen ||
			viewheight == viewport->RenderTarget->GetHeight() ||
			(viewport->RenderTarget->GetWidth() > (BASEXCENTER * 2) && !st_scale)))
		{	// Adjust PSprite for fullscreen views
			AWeapon *weapon = dyn_cast<AWeapon>(pspr->GetCaller());
			if (weapon != nullptr && weapon->YAdjust != 0)
			{
				if (viewport->RenderTarget != screen || viewheight == viewport->RenderTarget->GetHeight())
				{
					vis.texturemid -= weapon->YAdjust;
				}
				else
				{
					vis.texturemid -= StatusBar->GetDisplacement() * weapon->YAdjust;
				}
			}
		}
		if (pspr->GetID() < PSP_TARGETCENTER)
		{ // Move the weapon down for 1280x1024.
			vis.texturemid -= AspectPspriteOffset(viewport->viewwindow.WidescreenRatio);
		}
		vis.x1 = x1 < 0 ? 0 : x1;
		vis.x2 = x2 >= viewwidth ? viewwidth : x2;
		vis.xscale = FLOAT2FIXED(pspritexscale / tex->Scale.X);
		vis.yscale = float(pspriteyscale / tex->Scale.Y);
		vis.pic = tex;

		// If flip is used, provided that it's not already flipped (that would just invert itself)
		// (It's an XOR...)
		if (!(flip) != !(pspr->Flags & PSPF_FLIP))
		{
			vis.xiscale = -FLOAT2FIXED(pspritexiscale * tex->Scale.X);
			vis.startfrac = (tex->GetWidth() << FRACBITS) - 1;
		}
		else
		{
			vis.xiscale = FLOAT2FIXED(pspritexiscale * tex->Scale.X);
			vis.startfrac = 0;
		}

		if (vis.x1 > x1)
			vis.startfrac += vis.xiscale*(vis.x1 - x1);

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
					vis.RenderStyle = LegacyRenderStyles[rs];
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_Fuzzy])
				{
					vis.RenderStyle = LegacyRenderStyles[STYLE_Fuzzy];
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy])
				{
					vis.RenderStyle = LegacyRenderStyles[STYLE_OptFuzzy];
					vis.RenderStyle.CheckFuzz();
				}
				else if (owner->RenderStyle == LegacyRenderStyles[STYLE_Subtract])
				{
					vis.RenderStyle = LegacyRenderStyles[STYLE_Subtract];
				}
				else
				{
					vis.RenderStyle = LegacyRenderStyles[rs];
				}
			}
			else
			{
				vis.RenderStyle = owner->RenderStyle;
			}

			// Set the alpha based on if using the overlay's own or not. Also adjust
			// and override the alpha if not forced.
			if (pspr->Flags & PSPF_ALPHA)
			{
				if (vis.RenderStyle == LegacyRenderStyles[STYLE_Fuzzy])
				{
					alpha = owner->Alpha;
				}
				else if (vis.RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy])
				{
					FRenderStyle style = vis.RenderStyle;
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
				else if (vis.RenderStyle == LegacyRenderStyles[STYLE_Subtract])
				{
					alpha = owner->Alpha;
				}
				else if (vis.RenderStyle == LegacyRenderStyles[STYLE_Add] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_Translucent] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_TranslucentStencil] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_AddStencil] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_AddShaded])
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
				vis.RenderStyle == LegacyRenderStyles[STYLE_Normal] &&
				vis.Alpha < 1.0)
			{
				vis.RenderStyle = LegacyRenderStyles[STYLE_Translucent];
				alpha = owner->Alpha * pspr->alpha;
			}

			// ALWAYS take priority if asked for, except fuzz. Fuzz does absolutely nothing
			// no matter what way it's changed.
			if (pspr->Flags & PSPF_FORCEALPHA)
			{
				//Due to lack of != operators...
				if (vis.RenderStyle == LegacyRenderStyles[STYLE_Fuzzy] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_SoulTrans] ||
					vis.RenderStyle == LegacyRenderStyles[STYLE_Stencil])
				{
				}
				else
				{
					alpha = pspr->alpha;
					vis.RenderStyle.Flags |= STYLEF_ForceAlpha;
				}
			}
			vis.Alpha = clamp<float>(float(alpha), 0.f, 1.f);

			// Due to how some of the effects are handled, going to 0 or less causes some
			// weirdness to display. There's no point rendering it anyway if it's 0.
			if (vis.Alpha <= 0.)
				return;

			//-----------------------------------------------------------------------------

			// The software renderer cannot invert the source without inverting the overlay
			// too. That means if the source is inverted, we need to do the reverse of what
			// the invert overlay flag says to do.
			bool invertcolormap = (vis.RenderStyle.Flags & STYLEF_InvertOverlay) != 0;

			if (vis.RenderStyle.Flags & STYLEF_InvertSource)
			{
				invertcolormap = !invertcolormap;
			}

			bool fullbright = !foggy && pspr->GetState()->GetFullbright();
			bool fadeToBlack = (vis.RenderStyle.Flags & STYLEF_FadeToBlack) != 0;

			vis.Light.SetColormap(0, spriteshade, basecolormap, fullbright, invertcolormap, fadeToBlack);

			colormap_to_use = (FDynamicColormap*)vis.Light.BaseColormap;

			if (Thread->Viewport->viewpoint.camera->Inventory != nullptr)
			{
				visstyle_t visstyle;
				visstyle.Alpha = vis.Alpha;
				visstyle.RenderStyle = STYLE_Count;
				visstyle.Invert = false;
				
				Thread->Viewport->viewpoint.camera->Inventory->AlterWeaponSprite(&visstyle);
				
				vis.Alpha = visstyle.Alpha;

				if (visstyle.RenderStyle != STYLE_Count)
				{
					vis.RenderStyle = visstyle.RenderStyle;
				}

				if (visstyle.Invert)
				{
					vis.Light.BaseColormap = &SpecialColormaps[INVERSECOLORMAP];
					vis.Light.ColormapNum = 0;
					noaccel = true;
				}
			}
			// If we're drawing with a special colormap, but shaders for them are disabled, do
			// not accelerate.
			if (!r_shadercolormaps && (vis.Light.BaseColormap >= &SpecialColormaps[0] &&
				vis.Light.BaseColormap <= &SpecialColormaps.Last()))
			{
				noaccel = true;
			}
			// If drawing with a BOOM colormap, disable acceleration.
			if (vis.Light.BaseColormap == &NormalLight && NormalLight.Maps != realcolormaps.Maps)
			{
				noaccel = true;
			}
			// If the main colormap has fixed lights, and this sprite is being drawn with that
			// colormap, disable acceleration so that the lights can remain fixed.
			CameraLight *cameraLight = CameraLight::Instance();
			if (!noaccel && cameraLight->ShaderColormap() == nullptr &&
				NormalLightHasFixedLights && vis.Light.BaseColormap == &NormalLight &&
				vis.pic->UseBasePalette())
			{
				noaccel = true;
			}
		}
		else
		{
			colormap_to_use = basecolormap;

			vis.Light.BaseColormap = basecolormap;
			vis.Light.ColormapNum = 0;
		}

		// Check for hardware-assisted 2D. If it's available, and this sprite is not
		// fuzzy, don't draw it until after the switch to 2D mode.
		if (!noaccel && viewport->RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
		{
			FRenderStyle style = vis.RenderStyle;
			style.CheckFuzz();
			if (style.BlendOp != STYLEOP_Fuzz)
			{
				HWAccelPlayerSprite accelSprite;

				accelSprite.pic = vis.pic;
				accelSprite.texturemid = vis.texturemid;
				accelSprite.yscale = vis.yscale;
				accelSprite.xscale = vis.xscale;

				accelSprite.Alpha = vis.Alpha;
				accelSprite.RenderStyle = vis.RenderStyle;
				accelSprite.Translation = vis.Translation;
				accelSprite.FillColor = vis.FillColor;

				accelSprite.basecolormap = colormap_to_use;
				accelSprite.x1 = x1;
				accelSprite.flip = vis.xiscale < 0;

				if (vis.Light.BaseColormap >= &SpecialColormaps[0] &&
					vis.Light.BaseColormap < &SpecialColormaps[SpecialColormaps.Size()])
				{
					accelSprite.special = static_cast<FSpecialColormap*>(vis.Light.BaseColormap);
				}
				else if (CameraLight::Instance()->ShaderColormap())
				{
					accelSprite.special = CameraLight::Instance()->ShaderColormap();
				}
				else if (colormap_to_use->Color == PalEntry(255, 255, 255) &&
					colormap_to_use->Desaturate == 0)
				{
					accelSprite.overlay = colormap_to_use->Fade;
					accelSprite.overlay.a = uint8_t(vis.Light.ColormapNum * 255 / NUMCOLORMAPS);
				}
				else
				{
					accelSprite.usecolormapstyle = true;
					accelSprite.colormapstyle.Color = colormap_to_use->Color;
					accelSprite.colormapstyle.Fade = colormap_to_use->Fade;
					accelSprite.colormapstyle.Desaturate = colormap_to_use->Desaturate;
					accelSprite.colormapstyle.FadeLevel = vis.Light.ColormapNum / float(NUMCOLORMAPS);
				}

				AcceleratedSprites.Push(accelSprite);
				return;
			}
		}

		vis.Render(Thread);
	}

	void RenderPlayerSprites::RenderRemaining()
	{
		for (const HWAccelPlayerSprite &sprite : AcceleratedSprites)
		{
			screen->DrawTexture(sprite.pic,
				viewwindowx + sprite.x1,
				viewwindowy + viewheight / 2 - sprite.texturemid * sprite.yscale - 0.5,
				DTA_DestWidthF, FIXED2DBL(sprite.pic->GetWidth() * sprite.xscale),
				DTA_DestHeightF, sprite.pic->GetHeight() * sprite.yscale,
				DTA_TranslationIndex, sprite.Translation,
				DTA_FlipX, sprite.flip,
				DTA_TopOffset, 0,
				DTA_LeftOffset, 0,
				DTA_ClipLeft, viewwindowx,
				DTA_ClipTop, viewwindowy,
				DTA_ClipRight, viewwindowx + viewwidth,
				DTA_ClipBottom, viewwindowy + viewheight,
				DTA_Alpha, sprite.Alpha,
				DTA_RenderStyle, sprite.RenderStyle,
				DTA_FillColor, sprite.FillColor,
				DTA_SpecialColormap, sprite.special,
				DTA_ColorOverlay, sprite.overlay.d,
				DTA_ColormapStyle, sprite.usecolormapstyle ? &sprite.colormapstyle : nullptr,
				TAG_DONE);
		}

		AcceleratedSprites.Clear();
	}

	/////////////////////////////////////////////////////////////////////////

	void NoAccelPlayerSprite::Render(RenderThread *thread)
	{
		if (xscale == 0 || fabs(yscale) < (1.0f / 32000.0f))
		{ // scaled to 0; can't see
			return;
		}

		SpriteDrawerArgs drawerargs;
		drawerargs.SetLight(Light.BaseColormap, 0, Light.ColormapNum << FRACBITS);

		FDynamicColormap *basecolormap = static_cast<FDynamicColormap*>(Light.BaseColormap);

		bool visible = drawerargs.SetStyle(thread->Viewport.get(), RenderStyle, Alpha, Translation, FillColor, basecolormap, Light.ColormapNum << FRACBITS);
		if (!visible)
			return;

		double spryscale = yscale;
		bool sprflipvert = false;
		fixed_t iscale = FLOAT2FIXED(1 / yscale);
		
		auto viewport = thread->Viewport.get();

		double sprtopscreen;
		if (renderflags & RF_YFLIP)
		{
			sprflipvert = true;
			spryscale = -spryscale;
			iscale = -iscale;
			sprtopscreen = viewport->CenterY + (texturemid - pic->GetHeight()) * spryscale;
		}
		else
		{
			sprflipvert = false;
			sprtopscreen = viewport->CenterY - texturemid * spryscale;
		}

		// clip to screen bounds
		short *mfloorclip = screenheightarray;
		short *mceilingclip = zeroarray;

		fixed_t frac = startfrac;
		thread->PrepareTexture(pic);
		for (int x = x1; x < x2; x++)
		{
			drawerargs.DrawMaskedColumn(thread, x, iscale, pic, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, false);
			frac += xiscale;
		}

		if (thread->MainThread)
			NetUpdate();
	}
}
