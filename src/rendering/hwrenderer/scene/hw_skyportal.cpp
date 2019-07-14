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
#include "w_wad.h"
#include "r_state.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "textures/skyboxtexture.h"



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

void HWSkyPortal::RenderDome(HWDrawInfo *di, FRenderState &state, FMaterial * tex, float x_offset, float y_offset, bool mirror, int mode)
{
	if (tex)
	{
		state.SetMaterial(tex, CLAMP_NONE, 0, -1);
		state.EnableModelMatrix(true);
		state.EnableTextureMatrix(true);

		vertexBuffer->SetupMatrices(di, tex, x_offset, y_offset, mirror, mode, state.mModelMatrix, state.mTextureMatrix);
	}

	int rc = vertexBuffer->mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == FSkyVertexBuffer::SKYMODE_MAINLAYER && tex != NULL)
	{
		PalEntry pe = tex->tex->GetSkyCapColor(false);
		state.SetObjectColor(pe);
		state.EnableTexture(false);
		RenderRow(di, state, DT_TriangleFan, 0);

		pe = tex->tex->GetSkyCapColor(true);
		state.SetObjectColor(pe);
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

void HWSkyPortal::RenderBox(HWDrawInfo *di, FRenderState &state, FTextureID texno, FMaterial * gltex, float x_offset, bool sky2)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FMaterial * tex;

	state.EnableModelMatrix(true);
	state.mModelMatrix.loadIdentity();

	if (!sky2)
        state.mModelMatrix.rotate(-180.0f+x_offset, di->Level->info->skyrotatevector.X, di->Level->info->skyrotatevector.Z, di->Level->info->skyrotatevector.Y);
	else
        state.mModelMatrix.rotate(-180.0f+x_offset, di->Level->info->skyrotatevector2.X, di->Level->info->skyrotatevector2.Z, di->Level->info->skyrotatevector2.Y);

	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		state.SetMaterial(tex, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(0), 4);

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1], false);
		state.SetMaterial(tex, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(1), 4);

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2], false);
		state.SetMaterial(tex, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(2), 4);

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3], false);
		state.SetMaterial(tex, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(3), 4);
	}
	else 
	{
		faces=1;
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		state.SetMaterial(tex, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(-1), 10);
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces], false);
	state.SetMaterial(tex, CLAMP_XY, 0, -1);
	state.Draw(DT_TriangleStrip, vertexBuffer->FaceStart(sb->fliptop ? 6 : 5), 4);

	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1], false);
	state.SetMaterial(tex, CLAMP_XY, 0, -1);
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
	if (origin->texture[0] && origin->texture[0]->tex->isSkybox())
	{
		RenderBox(di, state, origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->sky2);
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
