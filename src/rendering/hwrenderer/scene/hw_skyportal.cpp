// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2003-2016 Christoph Oelckers
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

#include "doomtype.h"
#include "g_level.h"
#include "filesystem.h"
#include "r_state.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "hw_skydome.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_renderstate.h"
#include "skyboxtexture.h"

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void HWSkyPortal::DrawContents(HWDrawInfo *di, FRenderState &state)
{
	bool drawBoth = false;
	auto &vp = di->Viewpoint;

	// We have no use for Doom lighting special handling here, so disable it for this function.
	auto oldlightmode = di->lightmode;
	if (di->isSoftwareLighting())
	{
		di->SetFallbackLightMode();
		state.SetNoSoftLightLevel();
	}


	state.ResetColor();
	state.EnableFog(false);
	state.AlphaFunc(Alpha_GEqual, 0.f);
	state.SetRenderStyle(STYLE_Translucent);
	bool oldClamp = state.SetDepthClamp(true);

	di->SetupView(state, 0, 0, 0, !!(mState->MirrorFlag & 1), !!(mState->PlaneMirrorFlag & 1));

	state.SetVertexBuffer(vertexBuffer);
	auto skybox = origin->texture[0] ? dynamic_cast<FSkyBox*>(origin->texture[0]->GetTexture()) : nullptr;
	if (skybox)
	{
		vertexBuffer->RenderBox(state, skybox, origin->x_offset[0], origin->sky2, di->Level->info->pixelstretch, di->Level->info->skyrotatevector, di->Level->info->skyrotatevector2);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			state.SetTextureMode(TM_OPAQUE);
			vertexBuffer->RenderDome(state, origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER, !!(di->Level->flags & LEVEL_FORCETILEDSKY));
			state.SetTextureMode(TM_NORMAL);
		}
		
		state.AlphaFunc(Alpha_Greater, 0.f);
		
		if (origin->doublesky && origin->texture[1])
		{
			vertexBuffer->RenderDome(state, origin->texture[1], origin->x_offset[1], origin->y_offset, false, FSkyVertexBuffer::SKYMODE_SECONDLAYER, !!(di->Level->flags & LEVEL_FORCETILEDSKY));
		}

		if (di->Level->skyfog>0 && !di->isFullbrightScene()  && (origin->fadecolor & 0xffffff) != 0)
		{
			PalEntry FadeColor = origin->fadecolor;
			FadeColor.a = clamp<int>(di->Level->skyfog, 0, 255);

			state.EnableTexture(false);
			state.SetObjectColor(FadeColor);
			state.Draw(DT_Triangles, 0, 12);
			state.EnableTexture(true);
			state.SetObjectColor(0xffffffff);
		}
	}
	di->lightmode = oldlightmode;
	state.SetDepthClamp(oldClamp);
}

const char *HWSkyPortal::GetName() { return "Sky"; }
