// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2018 Christoph Oelckers
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

#include "gl_load/gl_system.h"
#include "r_utility.h"
#include "v_video.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/dynlights/gl_lightbuffer.h"

//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void FDrawInfo::DrawPSprite (HUDSprite *huds)
{
	gl_SetRenderStyle(huds->RenderStyle);
	gl_RenderState.EnableBrightmap(!(huds->RenderStyle.Flags & STYLEF_ColorIsFixed));

	if (huds->mframe)
	{
		gl_RenderState.Apply(huds->attrindex, false);
        FGLModelRenderer renderer(this);
        renderer.RenderHUDModel(huds->mframe, huds->weapon, huds->modelmirrored);
	}
	else
	{
		gl_RenderState.SetMaterial(huds->tex, CLAMP_XY_NOMIP, 0, huds->OverrideShader);
		gl_RenderState.Apply(huds->attrindex, true);
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, huds->mx, 4);
	}
	gl_RenderState.EnableBrightmap(false);
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void FDrawInfo::DrawPlayerSprites(bool hudModelStep)
{
	int oldlightmode = level.lightmode;
	if (!hudModelStep && level.lightmode == 8) level.lightmode = 2;	// Software lighting cannot handle 2D content so revert to lightmode 2 for that.
	for(auto &hudsprite : hudsprites)
	{
		if ((!!hudsprite.mframe) == hudModelStep)
			DrawPSprite(&hudsprite);
	}
	level.lightmode = oldlightmode;
}

void FDrawInfo::AddHUDSprite(HUDSprite *huds)
{
	hudsprites.Push(*huds);
}
