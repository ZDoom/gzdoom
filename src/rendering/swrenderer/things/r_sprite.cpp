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

#include "filesystem.h"
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
#include "voxels.h"
#include "p_local.h"
#include "r_voxel.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/viewport/r_viewport.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "a_dynlight.h"
#include "r_data/r_vanillatrans.h"

EXTERN_CVAR(Bool, gl_light_sprites)
EXTERN_CVAR(Int, gl_texture_hqresizemult)
EXTERN_CVAR(Int, gl_texture_hqresizemode)
EXTERN_CVAR(Int, gl_texture_hqresize_targets)

namespace swrenderer
{
	void RenderSprite::Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FSoftwareTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int lightlevel, bool foggy, FDynamicColormap *basecolormap)
	{
		auto viewport = thread->Viewport.get();

		const double thingxscalemul = spriteScale.X / tex->GetScale().X;

		// Calculate billboard line for the sprite
		double SpriteOffX = (thing) ? thing->SpriteOffset.X : 0.;
		DVector2 dir = { viewport->viewpoint.Sin, -viewport->viewpoint.Cos };
		DVector2 trs = pos.XY() - viewport->viewpoint.Pos.XY();
		trs = { trs.X + SpriteOffX * dir.X, trs.Y + SpriteOffX * dir.Y };
		DVector2 pt1 = trs - dir * (((renderflags & RF_XFLIP) ? (tex->GetWidth() - tex->GetLeftOffsetSW() - 1) : tex->GetLeftOffsetSW()) * thingxscalemul);
		DVector2 pt2 = pt1 + dir * (tex->GetWidth() * thingxscalemul);

		FWallCoords wallc;
		if (wallc.Init(thread, pt1, pt2))
			return;

		// [RH] Added scaling
		double scaled_to = tex->GetScaledTopOffsetSW();
		double scaled_bo = scaled_to - tex->GetScaledHeight();
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
				if (!(heightsec->MoreFlags & SECMF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
		}
		

		// [RH] Flip for mirrors
		auto renderportal = thread->Portal.get();
		renderflags ^= renderportal->MirrorFlags & RF_XFLIP;

		// [SP] SpriteFlip
		if (thing->renderflags & RF_SPRITEFLIP)
			renderflags ^= RF_XFLIP;

		double yscale, origyscale;
		int resizeMult = gl_texture_hqresizemult;

		origyscale = spriteScale.Y / tex->GetScale().Y;
		if (gl_texture_hqresizemode == 0 || gl_texture_hqresizemult < 1 || !(gl_texture_hqresize_targets & 2))
			resizeMult = 1;
		yscale = origyscale / resizeMult;

		// store information in a vissprite
		RenderSprite *vis = thread->FrameMemory->NewObject<RenderSprite>();

		vis->wallc = wallc;
		vis->SpriteScale = yscale;
		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->yscale = float(viewport->InvZtoScale * yscale / wallc.sz1);
		vis->idepth = float(1 / wallc.sz1);
		vis->floorclip = thing->Floorclip / origyscale;
		vis->texturemid = tex->GetTopOffsetSW() - (viewport->viewpoint.Pos.Z - pos.Z + thing->Floorclip / resizeMult) / yscale;
		vis->x1 = wallc.sx1 < renderportal->WindowLeft ? renderportal->WindowLeft : wallc.sx1;
		vis->x2 = wallc.sx2 > renderportal->WindowRight ? renderportal->WindowRight : wallc.sx2;
		vis->heightsec = heightsec;
		vis->sector = thing->Sector;
		vis->section = thing->section;
		vis->depth = (float)wallc.sz1;
		vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
		vis->gzb = (float)gzb;
		vis->gzt = (float)gzt;
		vis->deltax = float(pos.X - viewport->viewpoint.Pos.X);
		vis->deltay = float(pos.Y - viewport->viewpoint.Pos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT)
			vis->renderflags |= RF_FULLBRIGHT;
		vis->RenderStyle = thing->RenderStyle;
		if (r_UseVanillaTransparency)
		{
			if (thing->renderflags & RF_ZDOOMTRANS)
				vis->RenderStyle = LegacyRenderStyles[STYLE_Normal];
		}
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;		// [RH] thing translation table
		vis->FakeFlatStat = fakeside;
		vis->Alpha = float(thing->Alpha);
		vis->fakefloor = fakefloor;
		vis->fakeceiling = fakeceiling;
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
		
		if (r_dynlights && gl_light_sprites)
		{
			float lit_red = 0;
			float lit_green = 0;
			float lit_blue = 0;
			auto node = vis->section->lighthead;
			while (node != nullptr)
			{
				FDynamicLight *light = node->lightsource;
				if (light->ShouldLightActor(thing))
				{
					float lx = (float)(light->X() - thing->X());
					float ly = (float)(light->Y() - thing->Y());
					float lz = (float)(light->Z() - thing->Center());
					float LdotL = lx * lx + ly * ly + lz * lz;
					float radius = node->lightsource->GetRadius();
					if (radius * radius >= LdotL)
					{
						float distance = sqrt(LdotL);
						float attenuation = 1.0f - distance / radius;
						if (attenuation > 0.0f)
						{						
							float red = light->GetRed() * (1.0f / 255.0f);
							float green = light->GetGreen() * (1.0f / 255.0f);
							float blue = light->GetBlue() * (1.0f / 255.0f);
							/*if (light->IsSubtractive())
							{
								float bright = FVector3(lr, lg, lb).Length();
								FVector3 lightColor(lr, lg, lb);
								red = (bright - lr) * -1;
								green = (bright - lg) * -1;
								blue = (bright - lb) * -1;
							}*/
						
							lit_red += red * attenuation;
							lit_green += green * attenuation;
							lit_blue += blue * attenuation;
						}
					}
				}
				node = node->nextLight;
			}
			lit_red = clamp(lit_red * 255.0f, 0.0f, 255.0f);
			lit_green = clamp(lit_green * 255.0f, 0.0f, 255.0f);
			lit_blue = clamp(lit_blue * 255.0f, 0.0f, 255.0f);
			vis->dynlightcolor = (((uint32_t)lit_red) << 16) | (((uint32_t)lit_green) << 8) | ((uint32_t)lit_blue);
		}
		else
		{
			vis->dynlightcolor = 0;
		}

		vis->Light.SetColormap(thread, wallc.sz1, lightlevel, foggy, basecolormap, fullbright, invertcolormap, fadeToBlack, false, false);

		thread->SpriteList->Push(vis);
	}

	void RenderSprite::Render(RenderThread *thread, short *mfloorclip, short *mceilingclip, int, int, Fake3DTranslucent)
	{
		SpriteDrawerArgs drawerargs;
		drawerargs.SetDynamicLight(dynlightcolor);
		bool visible = drawerargs.SetStyle(thread->Viewport.get(), RenderStyle, Alpha, Translation, FillColor, Light);
		if (visible)
		{
			RenderTranslucentPass *translucentPass = thread->TranslucentPass.get();
			short portalfloorclip[MAXWIDTH];
			for (int x = x1; x < x2; x++)
			{
				if (translucentPass->ClipSpriteColumnWithPortals(x, this))
					portalfloorclip[x] = mceilingclip[x];
				else
					portalfloorclip[x] = mfloorclip[x];
			}

			ProjectedWallLight mlight;
			mlight.SetSpriteLight();

			drawerargs.DrawMasked(thread, gzt - floorclip, SpriteScale, renderflags & RF_XFLIP, renderflags & RF_YFLIP, wallc, x1, x2, mlight, pic, portalfloorclip, mceilingclip, RenderStyle);
		}
	}
}
