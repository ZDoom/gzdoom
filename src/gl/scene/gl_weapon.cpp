// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_weapon.cpp
** Weapon sprite drawing
**
*/

#include "gl/system/gl_system.h"
#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"

#include "gl/system/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/dynlights/gl_lightbuffer.h"

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR (Bool, r_deathcamera)

//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void GLSceneDrawer::DrawPSprite (player_t * player,DPSprite *psp, float sx, float sy, int OverrideShader, bool alphatexture)
{
	WeaponRect rc;
	
	if (!GetWeaponRect(psp, sx, sy, player, rc)) return;
	gl_RenderState.SetMaterial(rc.tex, CLAMP_XY_NOMIP, 0, OverrideShader, alphatexture);
	if (rc.tex->tex->GetTranslucency() || OverrideShader != -1)
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	}
	gl_RenderState.Apply();
	FQuadDrawer qd;
	qd.Set(0, rc.x1, rc.y1, 0, rc.u1, rc.v1);
	qd.Set(1, rc.x1, rc.y2, 0, rc.u1, rc.v2);
	qd.Set(2, rc.x2, rc.y1, 0, rc.u2, rc.v1);
	qd.Set(3, rc.x2, rc.y2, 0, rc.u2, rc.v2);
	qd.Render(GL_TRIANGLE_STRIP);
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
}

//==========================================================================
//
//
//
//==========================================================================

void GLSceneDrawer::SetupWeaponLight()
{
	weapondynlightindex.Clear();

	AActor *camera = r_viewpoint.camera;
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	// this is the same as in DrawPlayerSprites below (i.e. no weapon being drawn.)
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	// Check if lighting can be used on this item.
	if (camera->RenderStyle.BlendOp == STYLEOP_Shadow || !gl_lights || !gl_light_sprites || !GLRenderer->mLightCount || FixedColormap != CM_DEFAULT || gl.legacyMode)
		return;

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr)
		{
			FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false) : nullptr;
			if (smf)
			{
				hw_GetDynModelLight(playermo, lightdata);
				weapondynlightindex[psp] = GLRenderer->mLights->UploadLights(lightdata);
			}
		}
	}
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawPlayerSprites(sector_t * viewsector, bool hudModelStep)
{
	bool brightflash = false;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	s3d::Stereo3DMode::getCurrentMode().AdjustPlayerSprites();

	AActor *camera = r_viewpoint.camera;

	// this is the same as the software renderer
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) || 
		(r_deathcamera && camera->health <= 0))
		return;

	WeaponPosition weap = GetWeaponPosition(camera->player);
	WeaponLighting light = GetWeaponLighting(viewsector, r_viewpoint.Pos, FixedColormap, in_area, camera->Pos());

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	int oldlightmode = level.lightmode;
	if (level.lightmode == 8) level.lightmode = 2;

	for(DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (!psp->GetState()) continue;
		WeaponRenderStyle rs = GetWeaponRenderStyle(psp, camera);
		if (rs.RenderStyle.BlendOp == STYLEOP_None) continue;

		gl_SetRenderStyle(rs.RenderStyle, false, false);

		PalEntry ThingColor = (camera->RenderStyle.Flags & STYLEF_ColorIsFixed) ? camera->fillcolor : 0xffffff;
		ThingColor.a = 255;

		// now draw the different layers of the weapon.
		// For stencil render styles brightmaps need to be disabled.
		gl_RenderState.EnableBrightmap(!(rs.RenderStyle.Flags & STYLEF_ColorIsFixed));

		const bool bright = isBright(psp);
		const PalEntry finalcol = bright? ThingColor : ThingColor.Modulate(viewsector->SpecialColors[sector_t::sprites]);
		gl_RenderState.SetObjectColor(finalcol);

		auto ll = light;
		if (bright) ll.SetBright();

		// set the lighting parameters
		if (rs.RenderStyle.BlendOp == STYLEOP_Shadow)
		{
			gl_RenderState.SetColor(0.2f, 0.2f, 0.2f, 0.33f, ll.cm.Desaturation);
		}
		else
		{
			if (gl_lights && GLRenderer->mLightCount && FixedColormap == CM_DEFAULT && gl_light_sprites)
			{
				FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false) : nullptr;
				if (!smf || gl.legacyMode)	// For models with per-pixel lighting this was done in a previous pass.
				{
					float out[3];
					gl_drawinfo->GetDynSpriteLight(playermo, nullptr, out);
					gl_RenderState.SetDynLight(out[0], out[1], out[2]);
				}
			}
			SetColor(ll.lightlevel, 0, ll.cm, rs.alpha, true);
		}

		FVector2 spos = BobWeapon(weap, psp);

		// [BB] In the HUD model step we just render the model and break out. 
		if (hudModelStep)
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderHUDModel(psp, spos.X, spos.Y, weapondynlightindex[psp]);
		}
		else
		{
			DrawPSprite(player, psp, spos.X, spos.Y, rs.OverrideShader, !!(rs.RenderStyle.Flags & STYLEF_RedIsAlpha));
		}
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);
	level.lightmode = oldlightmode;
}


//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawTargeterSprites()
{
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		GLRenderer->mViewActor!=playermo) return;

	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
	gl_RenderState.BlendEquation(GL_FUNC_ADD);
	gl_RenderState.ResetColor();
	gl_RenderState.SetTextureMode(TM_MODULATE);

	// The Targeter's sprites are always drawn normally.
	for (DPSprite *psp = player->FindPSprite(PSP_TARGETCENTER); psp != nullptr; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr) DrawPSprite(player, psp, psp->x, psp->y, 0, false);
	}
}
