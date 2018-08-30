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
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/data/gl_modelbuffer.h"

extern uint32_t r_renderercaps;

void gl_SetRenderStyle(FRenderStyle style)
{
	int sb, db, be;

	gl_GetRenderStyle(style, &sb, &db, &be);
	gl_RenderState.BlendEquation(be);
	gl_RenderState.BlendFunc(sb, db);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::DrawSprite(GLSprite *sprite, int pass)
{
	if (pass == GLPASS_DECALS) return;

	auto RenderStyle = sprite->RenderStyle;

	bool additivefog = false;
	bool foglayer = false;
	int rel = sprite->fullbright? 0 : getExtraLight();
    auto &vp = Viewpoint;

	if (sprite->translucentpass)
	{
		// The translucent pass requires special setup for the various modes.

		// for special render styles brightmaps would not look good - especially for subtractive.
		if (RenderStyle.BlendOp != STYLEOP_Add)
		{
			gl_RenderState.EnableBrightmap(false);
		}
		gl_SetRenderStyle(RenderStyle);
	}
	else if (sprite->modelframe == nullptr)
	{
		sprite->polyoffset = true;
	}

	if (sprite->gltexture) gl_RenderState.SetMaterial(sprite->gltexture, CLAMP_XY, sprite->translation, sprite->OverrideShader);
	else if (!sprite->modelframe) gl_RenderState.EnableTexture(false);

	{
		GLRenderer->mModelMatrix->Bind(sprite->modelindex);
		if (!sprite->modelframe)
		{
			gl_RenderState.SetNormal(0, 0, 0);
			gl_RenderState.Apply(sprite->attrindex, sprite->alphateston);
            
			if (sprite->polyoffset)
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(-1.0f, -128.0f);
			}
			
			glDrawArrays(GL_TRIANGLE_STRIP, sprite->vertexindex, 4);

			if (foglayer)
			{
				// If we get here we know that we have colored fog and no fixed colormap.
				gl_RenderState.BlendEquation(GL_FUNC_ADD);
				gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				gl_RenderState.Apply(sprite->attrindex+1, sprite->alphateston);
				glDrawArrays(GL_TRIANGLE_STRIP, sprite->vertexindex, 4);
			}
		}
		else
		{
            FGLModelRenderer renderer(this);
            renderer.RenderModel(sprite->modelframe, sprite->actor, sprite->modelmirrored);
		}
	}

	if (pass==GLPASS_TRANSLUCENT)
	{
		gl_RenderState.EnableBrightmap(true);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
	}
	if (sprite->polyoffset)
	{
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
	gl_RenderState.EnableTexture(true);
}


//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::AddSprite(GLSprite *sprite, bool translucent)
{
	int list = translucent? GLDL_TRANSLUCENT : GLDL_MODELS;
	auto newsprt = drawlists[list].NewSprite();
	*newsprt = *sprite;
}

