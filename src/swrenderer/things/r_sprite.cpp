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
#include "swrenderer/scene/r_light.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "a_dynlight.h"
#include "r_data/r_vanillatrans.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)
EXTERN_CVAR(Bool, gl_light_sprites)

namespace swrenderer
{
	void RenderSprite::Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FTexture *ttex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int lightlevel, bool foggy, FDynamicColormap *basecolormap)
	{
		FSoftwareTexture *tex = ttex->GetSoftwareTexture();
		// transform the origin point
		double tr_x = pos.X - thread->Viewport->viewpoint.Pos.X;
		double tr_y = pos.Y - thread->Viewport->viewpoint.Pos.Y;

		double tz = tr_x * thread->Viewport->viewpoint.TanCos + tr_y * thread->Viewport->viewpoint.TanSin;

		// thing is behind view plane?
		if (tz < MINZ)
			return;

		double tx = tr_x * thread->Viewport->viewpoint.Sin - tr_y * thread->Viewport->viewpoint.Cos;

		// [RH] Flip for mirrors
		RenderPortal *renderportal = thread->Portal.get();
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
		int scaled_to = tex->GetScaledTopOffsetSW();
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
				if (!(heightsec->MoreFlags & SECMF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
		}
		
		auto viewport = thread->Viewport.get();

		double xscale = viewport->CenterX / tz;

		// [RH] Reject sprites that are off the top or bottom of the screen
		if (viewport->globaluclip * tz > viewport->viewpoint.Pos.Z - gzb || viewport->globaldclip * tz < viewport->viewpoint.Pos.Z - gzt)
		{
			return;
		}

		// [RH] Flip for mirrors
		renderflags ^= renderportal->MirrorFlags & RF_XFLIP;

		// [SP] SpriteFlip
		if (thing->renderflags & RF_SPRITEFLIP)
			renderflags ^= RF_XFLIP;

		// calculate edges of the shape
		const double thingxscalemul = spriteScale.X / tex->GetScale().X;

		tx -= ((renderflags & RF_XFLIP) ? (tex->GetWidth() - tex->GetLeftOffsetSW() - 1) : tex->GetLeftOffsetSW()) * thingxscalemul;
		double dtx1 = tx * xscale;
		int x1 = viewport->viewwindow.centerx + xs_RoundToInt(dtx1);

		// off the right side?
		if (x1 >= renderportal->WindowRight)
			return;

		tx += tex->GetWidth() * thingxscalemul;
		int x2 = viewport->viewwindow.centerx + xs_RoundToInt(tx * xscale);

		// off the left side or too small?
		if ((x2 < renderportal->WindowLeft || x2 <= x1))
			return;

		xscale = spriteScale.X * xscale / tex->GetScale().X;
		fixed_t iscale = (fixed_t)(FRACUNIT / xscale); // Round towards zero to avoid wrapping in edge cases

		double yscale = spriteScale.Y / tex->GetScale().Y;

		// store information in a vissprite
		RenderSprite *vis = thread->FrameMemory->NewObject<RenderSprite>();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = float(viewport->InvZtoScale * yscale / tz);
		vis->idepth = float(1 / tz);
		vis->floorclip = thing->Floorclip / yscale;
		vis->texturemid = tex->GetTopOffsetSW() - (viewport->viewpoint.Pos.Z - pos.Z + thing->Floorclip) / yscale;
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

		vis->startfrac += (fixed_t)(vis->xiscale * (vis->x1 - viewport->viewwindow.centerx + 0.5 - dtx1));

		// killough 3/27/98: save sector for special clipping later
		vis->heightsec = heightsec;
		vis->sector = thing->Sector;
		vis->section = thing->section;

		vis->depth = (float)tz;
		vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
		vis->gzb = (float)gzb;		// [RH] use gzb, not thing->z
		vis->gzt = (float)gzt;		// killough 3/27/98
		vis->deltax = float(pos.X - viewport->viewpoint.Pos.X);
		vis->deltay = float(pos.Y - viewport->viewpoint.Pos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT)
			vis->renderflags |= RF_FULLBRIGHT; // kg3D
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
		
		if (r_dynlights && gl_light_sprites)
		{
			float lit_red = 0;
			float lit_green = 0;
			float lit_blue = 0;
			auto node = vis->section->lighthead;
			while (node != nullptr)
			{
				ADynamicLight *light = node->lightsource;
				if (light->visibletoplayer && !(light->flags2&MF2_DORMANT) && (!(light->lightflags&LF_DONTLIGHTSELF) || light->target != thing) && !(light->lightflags&LF_DONTLIGHTACTORS))
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

		vis->Light.SetColormap(thread, tz, lightlevel, foggy, basecolormap, fullbright, invertcolormap, fadeToBlack, false, false);

		thread->SpriteList->Push(vis);
	}

	void RenderSprite::Render(RenderThread *thread, short *mfloorclip, short *mceilingclip, int, int, Fake3DTranslucent)
	{
		auto vis = this;

		fixed_t 		frac;
		FSoftwareTexture		*tex;
		int				x2;
		fixed_t			xiscale;

		double spryscale, sprtopscreen;
		bool sprflipvert;

		if (vis->xscale == 0 || fabs(vis->yscale) < (1.0f / 32000.0f))
		{ // scaled to 0; can't see
			return;
		}

		SpriteDrawerArgs drawerargs;
		drawerargs.SetDynamicLight(dynlightcolor);
		bool visible = drawerargs.SetStyle(thread->Viewport.get(), vis->RenderStyle, vis->Alpha, vis->Translation, vis->FillColor, vis->Light);

		if (visible)
		{
			tex = vis->pic;
			spryscale = vis->yscale;
			sprflipvert = false;
			fixed_t iscale = FLOAT2FIXED(1 / vis->yscale);
			frac = vis->startfrac;
			xiscale = vis->xiscale;
			double texturemid = vis->texturemid;

			auto viewport = thread->Viewport.get();

			if (vis->renderflags & RF_YFLIP)
			{
				sprflipvert = true;
				spryscale = -spryscale;
				iscale = -iscale;
				texturemid -= vis->pic->GetHeight();
				sprtopscreen = viewport->CenterY + texturemid * spryscale;
			}
			else
			{
				sprflipvert = false;
				sprtopscreen = viewport->CenterY - texturemid * spryscale;
			}

			int x = vis->x1;
			x2 = vis->x2;

			if (x < x2)
			{
				RenderTranslucentPass *translucentPass = thread->TranslucentPass.get();

				thread->PrepareTexture(tex, vis->RenderStyle);
				while (x < x2)
				{
					if (!translucentPass->ClipSpriteColumnWithPortals(x, vis))
						drawerargs.DrawMaskedColumn(thread, x, iscale, tex, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, vis->RenderStyle, false);
					x++;
					frac += xiscale;
				}
			}
		}

		if (thread->MainThread)
			NetUpdate();
	}
}
