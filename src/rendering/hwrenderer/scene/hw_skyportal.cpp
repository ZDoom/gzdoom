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
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_renderstate.h"
#include "skyboxtexture.h"



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void HWSkyPortal::RenderRow(HWDrawInfo *di, FRenderState &state, EDrawType prim, int row, bool apply)
{
	state.Draw(prim, vertexBuffer->mPrimStart[row], vertexBuffer->mPrimStart[row + 1] - vertexBuffer->mPrimStart[row]);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void HWSkyPortal::RenderDome(HWDrawInfo *di, FRenderState &state, FGameTexture * tex, float x_offset, float y_offset, bool mirror, int mode)
{
	if (tex)
	{
		state.SetMaterial(tex, UF_Texture, 0, CLAMP_NONE, 0, -1);
		state.EnableModelMatrix(true);
		state.EnableTextureMatrix(true);

		vertexBuffer->SetupMatrices(di, tex, x_offset, y_offset, mirror, mode, state.mModelMatrix, state.mTextureMatrix);
	}

	int rc = vertexBuffer->mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == FSkyVertexBuffer::SKYMODE_MAINLAYER && tex != NULL)
	{
		auto &col = R_GetSkyCapColor(tex);
		state.SetObjectColor(col.first);
		state.EnableTexture(false);
		RenderRow(di, state, DT_TriangleFan, 0);

		state.SetObjectColor(col.second);
		RenderRow(di, state, DT_TriangleFan, rc);
		state.EnableTexture(true);
	}
	state.SetObjectColor(0xffffffff);
	for (int i = 1; i <= vertexBuffer->mRows; i++)
	{
		RenderRow(di, state, DT_TriangleStrip, i, i == 1);
		RenderRow(di, state, DT_TriangleStrip, rc + i, false);
	}

	state.EnableTextureMatrix(false);
	state.EnableModelMatrix(false);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void HWSkyPortal::RenderBox(HWDrawInfo *di, FRenderState &state, FTextureID texno, FSkyBox * tex, float x_offset, bool sky2)
{
	int faces;

	state.EnableModelMatrix(true);
	state.mModelMatrix.loadIdentity();
	state.mModelMatrix.scale(1, 1 / di->Level->info->pixelstretch, 1); // Undo the map's vertical scaling as skyboxes are true cubes.

	if (!sky2)
        state.mModelMatrix.rotate(-180.0f+x_offset, di->Level->info->skyrotatevector.X, di->Level->info->skyrotatevector.Z, di->Level->info->skyrotatevector.Y);
	else
        state.mModelMatrix.rotate(-180.0f+x_offset, di->Level->info->skyrotatevector2.X, di->Level->info->skyrotatevector2.Z, di->Level->info->skyrotatevector2.Y);

	if (tex->GetSkyFace(5))
	{
		faces=4;

		// north
		state.SetMaterial(tex->GetSkyFace(0), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(0), 4);

		// east
		state.SetMaterial(tex->GetSkyFace(1), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(1), 4);

		// south
		state.SetMaterial(tex->GetSkyFace(2), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(2), 4);

		// west
		state.SetMaterial(tex->GetSkyFace(3), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(3), 4);
	}
	else 
	{
		faces=1;
		state.SetMaterial(tex->GetSkyFace(0), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(-1), 10);
	}

	// top
	state.SetMaterial(tex->GetSkyFace(faces), UF_Texture, 0, CLAMP_XY, 0, -1);
	state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(tex->GetSkyFlip() ? 6 : 5), 4);

	// bottom
	state.SetMaterial(tex->GetSkyFace(faces+1), UF_Texture, 0, CLAMP_XY, 0, -1);
	state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(4), 4);

	state.EnableModelMatrix(false);
}

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
		RenderBox(di, state, origin->skytexno1, skybox, origin->x_offset[0], origin->sky2);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			state.SetTextureMode(TM_OPAQUE);
			RenderDome(di, state, origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER);
			state.SetTextureMode(TM_NORMAL);
		}
		
		state.AlphaFunc(Alpha_Greater, 0.f);
		
		if (origin->doublesky && origin->texture[1])
		{
			RenderDome(di, state, origin->texture[1], origin->x_offset[1], origin->y_offset, false, FSkyVertexBuffer::SKYMODE_SECONDLAYER);
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
