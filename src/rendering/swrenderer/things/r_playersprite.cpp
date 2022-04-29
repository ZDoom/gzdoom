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

#include "doomdef.h"
#include "m_swap.h"

#include "filesystem.h"
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
#include "voxels.h"
#include "p_local.h"
#include "p_maputl.h"
#include "r_voxel.h"
#include "texturemanager.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/viewport/r_viewport.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "g_levellocals.h"
#include "v_draw.h"

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

CVAR(Bool, r_noaccel, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

namespace swrenderer
{
	RenderPlayerSprites::RenderPlayerSprites(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderPlayerSprites::Render()
	{
		int 		i;
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
		
		renderHUDModel = r_modelscene && IsHUDModelForPlayerAvailable(players[consoleplayer].camera->player);

		FDynamicColormap *basecolormap;
		CameraLight *cameraLight = CameraLight::Instance();
		auto nc = !!(Thread->Viewport->Level()->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);

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
							basecolormap = GetSpriteColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], nc);
						else
							basecolormap = GetSpriteColorTable(Thread->Viewport->viewpoint.sector->e->XFloor.lightlist[i].extra_colormap, sec->SpecialColors[sector_t::sprites], nc);
					}
					break;
				}
			}
			if (!sec)
			{
				sec = Thread->Viewport->viewpoint.sector;
				basecolormap = GetSpriteColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], nc);
			}
			floorlight = ceilinglight = sec->lightlevel;
		}
		else
		{	// This used to use camera->Sector but due to interpolation that can be incorrect
			// when the interpolated viewpoint is in a different sector than the camera.
			sec = Thread->OpaquePass->FakeFlat(Thread->Viewport->viewpoint.sector, &tempsec, &floorlight, &ceilinglight, nullptr, 0, 0, 0, 0);

			// [RH] set basecolormap
			basecolormap = GetSpriteColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], nc);
		}

		// [RH] set foggy flag
		auto Level = Thread->Viewport->Level();
		bool foggy = (Level->fadeto || basecolormap->Fade || (Level->flags & LEVEL_HASFADETABLE));

		// get light level
		int lightlevel = (floorlight + ceilinglight) >> 1;

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
					RenderSprite(psp, viewport->viewpoint.camera, bobx, boby, wx, wy, viewport->viewpoint.TicFrac, lightlevel, basecolormap, foggy);
				}

				psp = psp->GetNext();
			}

			viewport->CenterY = centerhack;
		}
	}

	void RenderPlayerSprites::RenderSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int lightlevel, FDynamicColormap *basecolormap, bool foggy)
	{
		double 				tx;
		int 				x1;
		int 				x2;
		double				sx, sy;
		spritedef_t*		sprdef;
		spriteframe_t*		sprframe;
		FTextureID			picnum;
		uint16_t				flip;
		FGameTexture*			tex;
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
		tex = TexMan.GetGameTexture(picnum);

		if (!tex->isValid())
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
			sx += (pspr->Flags & PSPF_MIRROR) ? -bobx : bobx;
			sy += boby;
		}

		if (pspr->Flags & PSPF_ADDWEAPON && pspr->GetID() != PSP_WEAPON)
		{
			sx += wx;
			sy += wy;
		}
		
		if (renderHUDModel)
		{
			RenderHUDModel(Thread, pspr, (float)sx, (float)sy);
			return;
		}

		auto viewport = Thread->Viewport.get();

		double pspritexscale = viewport->viewwindow.centerxwide / 160.0;
		double pspriteyscale = pspritexscale * pspr->baseScale.Y * ((double)SCREENHEIGHT / SCREENWIDTH) * r_viewwindow.WidescreenRatio;
		pspritexscale *= pspr->baseScale.X; // [XA] don't accidentally apply this to the Y calculation above; math be weird
		double pspritexiscale = 1 / pspritexscale;

		int tleft = tex->GetDisplayLeftOffset(0);
		int twidth = tex->GetDisplayWidth();

		// calculate edges of the shape
		tx = (pspr->Flags & PSPF_MIRROR) ? ((BASEXCENTER - twidth) - (sx - tleft)) : ((sx - BASEXCENTER) - tleft);
		x1 = xs_RoundToInt(viewport->CenterX + tx * pspritexscale);

		// off the right side
		if (x1 > viewwidth)
			return;

		tx += twidth;
		x2 = xs_RoundToInt(viewport->CenterX + tx * pspritexscale);

		// off the left side
		if (x2 <= 0)
			return;

		// store information in a vissprite
		NoAccelPlayerSprite vis;

		vis.renderflags = owner->renderflags;

		FSoftwareTexture* stex = GetSoftwareTexture(tex);
		double textureadj = (120.0f / pspr->baseScale.Y) - BASEYCENTER; // [XA] scale relative to weapon baseline
		vis.texturemid = (BASEYCENTER - sy - textureadj) * stex->GetScale().Y + stex->GetTopOffset(0);

		// Force it to use software rendering when drawing to a canvas texture.
		bool renderToCanvas = viewport->RenderingToCanvas;

		if (Thread->Viewport->viewpoint.camera->player && (renderToCanvas ||
			viewheight == viewport->RenderTarget->GetHeight() ||
			(viewport->RenderTarget->GetWidth() > (BASEXCENTER * 2))))
		{	// Adjust PSprite for fullscreen views
			vis.texturemid -= pspr->GetYAdjust(renderToCanvas || viewheight == viewport->RenderTarget->GetHeight());
		}
		if (pspr->GetID() < PSP_TARGETCENTER)
		{ // Move the weapon down for 1280x1024.
			vis.texturemid -= AspectPspriteOffset(viewport->viewwindow.WidescreenRatio);
		}
		vis.x1 = x1 < 0 ? 0 : x1;
		vis.x2 = x2 >= viewwidth ? viewwidth : x2;
		vis.xscale = FLOAT2FIXED(pspritexscale / stex->GetScale().X);
		vis.yscale = float(pspriteyscale / stex->GetScale().Y);
		vis.pic = stex;

		uint32_t trans = pspr->GetTranslation() != 0 ? pspr->GetTranslation() : 0;
		if ((pspr->Flags & PSPF_PLAYERTRANSLATED)) trans = owner->Translation;
		vis.Translation = trans;

		// If flip is used, provided that it's not already flipped (that would just invert itself)
		// (It's an XOR...)
		if (!(flip) != !(pspr->Flags & PSPF_FLIP))
		{
			vis.xiscale = -FLOAT2FIXED(pspritexiscale * stex->GetScale().X);
			vis.startfrac = (stex->GetWidth() << FRACBITS) - 1;
		}
		else
		{
			vis.xiscale = FLOAT2FIXED(pspritexiscale * stex->GetScale().X);
			vis.startfrac = 0;
		}

		if (vis.x1 > x1)
			vis.startfrac += vis.xiscale*(vis.x1 - x1);

		noaccel = r_noaccel;
		FDynamicColormap *colormap_to_use = nullptr;
		if (pspr->GetID() < PSP_TARGETCENTER)
		{
			// [MC] Set the render style 
			auto rs = pspr->GetRenderStyle(owner->RenderStyle, owner->Alpha);
			vis.RenderStyle = rs.first;
			vis.Alpha = rs.second;

			if (!vis.RenderStyle.IsVisible(vis.Alpha))
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

			const FState* const psprState = pspr->GetState();
			bool fullbright = !foggy && (psprState == nullptr ? false : psprState->GetFullbright());
			bool fadeToBlack = (vis.RenderStyle.Flags & STYLEF_FadeToBlack) != 0;

			vis.Light.SetColormap(Thread, MINZ, lightlevel, foggy, basecolormap, fullbright, invertcolormap, fadeToBlack, true, false);

			colormap_to_use = (FDynamicColormap*)vis.Light.BaseColormap;

			if (Thread->Viewport->viewpoint.camera->Inventory != nullptr)
			{
				visstyle_t visstyle;
				visstyle.Alpha = vis.Alpha;
				visstyle.RenderStyle = STYLE_Count;
				visstyle.Invert = false;
				
				Thread->Viewport->viewpoint.camera->Inventory->AlterWeaponSprite(&visstyle);
				
				if (!(pspr->Flags & PSPF_FORCEALPHA)) vis.Alpha = visstyle.Alpha;

				if (visstyle.RenderStyle != STYLE_Count && !(pspr->Flags & PSPF_FORCESTYLE))
				{
					vis.RenderStyle = visstyle.RenderStyle;
				}

				if (visstyle.Invert)
				{
					vis.Light.BaseColormap = &SpecialSWColormaps[REALINVERSECOLORMAP];
					vis.Light.ColormapNum = 0;
					noaccel = true;
				}
			}

			// If drawing with a BOOM colormap, disable acceleration.
			if (vis.Light.BaseColormap == &NormalLight && NormalLight.Maps != realcolormaps.Maps)
			{
				noaccel = true;
			}
#if 0
			// If the main colormap has fixed lights, and this sprite is being drawn with that
			// colormap, disable acceleration so that the lights can remain fixed.
			CameraLight *cameraLight = CameraLight::Instance();
			if (!noaccel && cameraLight->ShaderColormap() == nullptr &&
				NormalLightHasFixedLights && vis.Light.BaseColormap == &NormalLight &&
				vis.pic->UseBasePalette())
			{
				noaccel = true;
			}
#endif
		}
		else
		{
			colormap_to_use = basecolormap;

			vis.Light.BaseColormap = basecolormap;
			vis.Light.ColormapNum = 0;
		}

		// Check for hardware-assisted 2D. If it's available, and this sprite is not
		// fuzzy, don't draw it until after the switch to 2D mode.
		if (!noaccel && !renderToCanvas)
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

				accelSprite.Translation = trans;

				if (vis.Light.BaseColormap >= &SpecialSWColormaps[0] &&
					vis.Light.BaseColormap < &SpecialSWColormaps[SpecialColormaps.Size()])
				{
					accelSprite.special = &SpecialColormaps[vis.Light.BaseColormap - &SpecialSWColormaps[0]];
				}
				else if (CameraLight::Instance()->ShaderColormap())
				{
					accelSprite.special = CameraLight::Instance()->ShaderColormap();
				}
				else 
				{
					accelSprite.overlay = colormap_to_use->Fade;
					accelSprite.overlay.a = uint8_t(vis.Light.ColormapNum * 255 / NUMCOLORMAPS);
					accelSprite.LightColor = colormap_to_use->Color;
					accelSprite.Desaturate = (uint8_t)clamp(colormap_to_use->Desaturate, 0, 255);
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
			DrawTexture(twod, sprite.pic->GetTexture(),
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
				DTA_Color, sprite.LightColor | 0xff000000,	// the color here does not have a valid alpha component.
				DTA_Desaturate, sprite.Desaturate,
				TAG_DONE);
		}

		AcceleratedSprites.Clear();
	}

	/////////////////////////////////////////////////////////////////////////

	void NoAccelPlayerSprite::Render(RenderThread *thread)
	{
		SpriteDrawerArgs drawerargs;
		bool visible = drawerargs.SetStyle(thread->Viewport.get(), RenderStyle, Alpha, Translation, FillColor, Light);
		if (!visible)
			return;

		double centerY = viewheight / 2;
		double y1, y2;
		if (renderflags & RF_YFLIP)
		{
			y1 = centerY + (texturemid - pic->GetHeight()) * (-yscale);
			y2 = y1 + pic->GetHeight() * (-yscale);
		}
		else
		{
			y1 = centerY - texturemid * yscale;
			y2 = y1 + pic->GetHeight() * yscale;
		}
		drawerargs.DrawMasked2D(thread, x1, x2, y1, y2, pic, RenderStyle);
	}
}
