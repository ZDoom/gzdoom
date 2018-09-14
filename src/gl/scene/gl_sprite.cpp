// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
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
** gl_sprite.cpp
** Sprite/Particle rendering
**
*/

#include "gl_load/gl_system.h"
#include "p_local.h"
#include "p_effect.h"
#include "g_level.h"
#include "doomstat.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "a_pickups.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "events.h"
#include "actorinlines.h"
#include "r_data/r_vanillatrans.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/dynlights/gl_lightbuffer.h"

extern uint32_t r_renderercaps;

void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending)
{
	int tm, sb, db, be;

	gl_GetRenderStyle(style, drawopaque, allowcolorblending, &tm, &sb, &db, &be);
	gl_RenderState.BlendEquation(be);
	gl_RenderState.BlendFunc(sb, db);
	gl_RenderState.SetTextureMode(tm);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::DrawSprite(GLSprite *sprite, int pass)
{
	if (pass == GLPASS_DECALS || pass == GLPASS_LIGHTSONLY) return;

	auto RenderStyle = sprite->RenderStyle;

	bool additivefog = false;
	bool foglayer = false;
	int rel = sprite->fullbright? 0 : getExtraLight();
    auto &vp = Viewpoint;

	if (pass==GLPASS_TRANSLUCENT)
	{
		// The translucent pass requires special setup for the various modes.

		// for special render styles brightmaps would not look good - especially for subtractive.
		if (RenderStyle.BlendOp != STYLEOP_Add)
		{
			gl_RenderState.EnableBrightmap(false);
		}

		gl_SetRenderStyle(RenderStyle, false, 
			// The rest of the needed checks are done inside gl_SetRenderStyle
			sprite->trans > 1.f - FLT_EPSILON && gl_usecolorblending && !isFullbrightScene() && sprite->actor &&
			sprite->fullbright && sprite->gltexture && !sprite->gltexture->tex->GetTranslucency());

		if (sprite->hw_styleflags == STYLEHW_NoAlphaTest)
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
		}
		else
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
		}

		if (RenderStyle.BlendOp == STYLEOP_Shadow)
		{
			float fuzzalpha=0.44f;
			float minalpha=0.1f;

			// fog + fuzz don't work well without some fiddling with the alpha value!
			if (!sprite->Colormap.FadeColor.isBlack())
			{
				float dist=Dist2(vp.Pos.X, vp.Pos.Y, sprite->x, sprite->y);
				int fogd = hw_GetFogDensity(sprite->lightlevel, sprite->Colormap.FadeColor, sprite->Colormap.FogDensity, sprite->Colormap.BlendFactor);

				// this value was determined by trial and error and is scale dependent!
				float factor = 0.05f + exp(-fogd*dist / 62500.f);
				fuzzalpha*=factor;
				minalpha*=factor;
			}

			gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
			gl_RenderState.SetColor(0.2f,0.2f,0.2f,fuzzalpha, sprite->Colormap.Desaturation);
			additivefog = true;
			sprite->lightlist = nullptr;	// the fuzz effect does not use the sector's light level so splitting is not needed.
		}
		else if (RenderStyle.BlendOp == STYLEOP_Add && RenderStyle.DestAlpha == STYLEALPHA_One)
		{
			additivefog = true;
		}
	}
	else if (sprite->modelframe == nullptr)
	{
		int tm, sb, db, be;

		// This still needs to set the texture mode. As blend mode it will always use GL_ONE/GL_ZERO
		gl_GetRenderStyle(RenderStyle, false, false, &tm, &sb, &db, &be);
		gl_RenderState.SetTextureMode(tm);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
	}
	if (RenderStyle.BlendOp != STYLEOP_Shadow)
	{
		if (level.HasDynamicLights && !isFullbrightScene() && !sprite->fullbright)
		{
			if ( sprite->dynlightindex == -1)	// only set if we got no light buffer index. This covers all cases where sprite lighting is used.
			{
				float out[3];
				GetDynSpriteLight(gl_light_sprites ? sprite->actor : nullptr, gl_light_particles ? sprite->particle : nullptr, out);
				gl_RenderState.SetDynLight(out[0], out[1], out[2]);
			}
		}
		sector_t *cursec = sprite->actor ? sprite->actor->Sector : sprite->particle ? sprite->particle->subsector->sector : nullptr;
		if (cursec != nullptr)
		{
			const PalEntry finalcol = sprite->fullbright
				? sprite->ThingColor
				: sprite->ThingColor.Modulate(cursec->SpecialColors[sector_t::sprites]);

			gl_RenderState.SetObjectColor(finalcol);
		}
		SetColor(sprite->lightlevel, rel, sprite->Colormap, sprite->trans);
	}


	if (sprite->Colormap.FadeColor.isBlack()) sprite->foglevel = sprite->lightlevel;

	if (RenderStyle.Flags & STYLEF_FadeToBlack) 
	{
		sprite->Colormap.FadeColor=0;
		additivefog = true;
	}

	if (RenderStyle.BlendOp == STYLEOP_RevSub || RenderStyle.BlendOp == STYLEOP_Sub)
	{
		if (!sprite->modelframe)
		{
			// non-black fog with subtractive style needs special treatment
			if (!sprite->Colormap.FadeColor.isBlack())
			{
				foglayer = true;
				// Due to the two-layer approach we need to force an alpha test that lets everything pass
				gl_RenderState.AlphaFunc(GL_GREATER, 0);
			}
		}
		else RenderStyle.BlendOp = STYLEOP_Fuzz;	// subtractive with models is not going to work.
	}

	if (!foglayer) SetFog(sprite->foglevel, rel, &sprite->Colormap, additivefog);
	else
	{
		gl_RenderState.EnableFog(false);
		gl_RenderState.SetFog(0, 0);
	}

	if (sprite->gltexture) gl_RenderState.SetMaterial(sprite->gltexture, CLAMP_XY, sprite->translation, sprite->OverrideShader, !!(RenderStyle.Flags & STYLEF_RedIsAlpha));
	else if (!sprite->modelframe) gl_RenderState.EnableTexture(false);

		//SetColor(lightlevel, rel, Colormap, trans);

	unsigned int iter = sprite->lightlist? sprite->lightlist->Size() : 1;
	bool clipping = false;
	auto lightlist = sprite->lightlist;
	if (lightlist || sprite->topclip != LARGE_VALUE || sprite->bottomclip != -LARGE_VALUE)
	{
		clipping = true;
		gl_RenderState.EnableSplit(true);
	}

	secplane_t bottomp = { { 0, 0, -1. }, sprite->bottomclip };
	secplane_t topp = { { 0, 0, -1. }, sprite->topclip };
	for (unsigned i = 0; i < iter; i++)
	{
		if (lightlist)
		{
			// set up the light slice
			secplane_t *topplane = i == 0 ? &topp : &(*lightlist)[i].plane;
			secplane_t *lowplane = i == (*lightlist).Size() - 1 ? &bottomp : &(*lightlist)[i + 1].plane;

			int thislight = (*lightlist)[i].caster != nullptr ? hw_ClampLight(*(*lightlist)[i].p_lightlevel) : sprite->lightlevel;
			int thisll = sprite->actor == nullptr? thislight : (uint8_t)sprite->actor->Sector->CheckSpriteGlow(thislight, sprite->actor->InterpolatedPosition(vp.TicFrac));

			FColormap thiscm;
			thiscm.CopyFog(sprite->Colormap);
			thiscm.CopyFrom3DLight(&(*lightlist)[i]);
			if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING)
			{
				thiscm.Decolorize();
			}

			SetColor(thisll, rel, thiscm, sprite->trans);
			if (!foglayer)
			{
				SetFog(thislight, rel, &thiscm, additivefog);
			}
			gl_RenderState.SetSplitPlanes(*topplane, *lowplane);
		}
		else if (clipping)
		{
			gl_RenderState.SetSplitPlanes(topp, bottomp);
		}

		if (!sprite->modelframe)
		{
			gl_RenderState.Apply();

			FVector3 v[4];
			gl_RenderState.SetNormal(0, 0, 0);
            
			if (sprite->CalculateVertices(this, v, &vp.Pos))
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(-1.0f, -128.0f);
			}
			
			FQuadDrawer qd;
			qd.Set(0, v[0][0], v[0][1], v[0][2], sprite->ul, sprite->vt);
			qd.Set(1, v[1][0], v[1][1], v[1][2], sprite->ur, sprite->vt);
			qd.Set(2, v[2][0], v[2][1], v[2][2], sprite->ul, sprite->vb);
			qd.Set(3, v[3][0], v[3][1], v[3][2], sprite->ur, sprite->vb);
			qd.Render(GL_TRIANGLE_STRIP);

			if (foglayer)
			{
				// If we get here we know that we have colored fog and no fixed colormap.
				SetFog(sprite->foglevel, rel, &sprite->Colormap, additivefog);
				gl_RenderState.SetTextureMode(TM_FOGLAYER);
				gl_RenderState.BlendEquation(GL_FUNC_ADD);
				gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				gl_RenderState.Apply();
				qd.Render(GL_TRIANGLE_STRIP);
				gl_RenderState.SetTextureMode(TM_MODULATE);
			}
		}
		else
		{
            FGLModelRenderer renderer(this, sprite->dynlightindex);
            renderer.RenderModel(sprite->x, sprite->y, sprite->z, sprite->modelframe, sprite->actor, vp.TicFrac);
		}
	}

	if (clipping)
	{
		gl_RenderState.EnableSplit(false);
	}

	if (pass==GLPASS_TRANSLUCENT)
	{
		gl_RenderState.EnableBrightmap(true);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
		gl_RenderState.SetTextureMode(TM_MODULATE);
		if (sprite->actor != nullptr && (sprite->actor->renderflags & RF_SPRITETYPEMASK) == RF_FLATSPRITE)
		{
			glPolygonOffset(0.0f, 0.0f);
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
	}
	else if (sprite->modelframe == nullptr)
	{
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetDynLight(0,0,0);
}


//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::AddSprite(GLSprite *sprite, bool translucent)
{
	int list;
	// [BB] Allow models to be drawn in the GLDL_TRANSLUCENT pass.
	if (translucent || sprite->actor == nullptr || (!sprite->modelframe && (sprite->actor->renderflags & RF_SPRITETYPEMASK) != RF_WALLSPRITE))
	{
		list = GLDL_TRANSLUCENT;
	}
	else
	{
		list = GLDL_MODELS;
	}
	
	auto newsprt = drawlists[list].NewSprite();
	*newsprt = *sprite;
}

