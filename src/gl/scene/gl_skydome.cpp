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

#include "gl_load/gl_system.h"
#include "doomtype.h"
#include "g_level.h"
#include "w_wad.h"
#include "r_state.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "textures/skyboxtexture.h"

#include "gl_load/gl_interface.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyVertexBuffer::FSkyVertexBuffer()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBufferData(GL_ARRAY_BUFFER, mVertices.Size() * sizeof(FSkyVertex), &mVertices[0], GL_STATIC_DRAW);
}

void FSkyVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->x);
	glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->u);
	glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSkyVertex), &VSO->color);
	glEnableVertexAttribArray(VATTR_VERTEX);
	glEnableVertexAttribArray(VATTR_TEXCOORD);
	glEnableVertexAttribArray(VATTR_COLOR);
	glDisableVertexAttribArray(VATTR_VERTEX2);
	glDisableVertexAttribArray(VATTR_NORMAL);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

inline void FSkyVertexBuffer::RenderRow(int prim, int row)
{
	glDrawArrays(prim, mPrimStart[row], mPrimStart[row + 1] - mPrimStart[row]);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::RenderDome(FMaterial *tex, int mode)
{
	int rc = mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == SKYMODE_MAINLAYER && tex != NULL)
	{
		PalEntry pe = tex->tex->GetSkyCapColor(false);
		gl_RenderState.SetObjectColor(pe);
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply();
		RenderRow(GL_TRIANGLE_FAN, 0);

		pe = tex->tex->GetSkyCapColor(true);
		gl_RenderState.SetObjectColor(pe);
		gl_RenderState.Apply();
		RenderRow(GL_TRIANGLE_FAN, rc);
		gl_RenderState.EnableTexture(true);
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.Apply();
	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(GL_TRIANGLE_STRIP, i);
		RenderRow(GL_TRIANGLE_STRIP, rc + i);
	}
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void RenderDome(FMaterial * tex, float x_offset, float y_offset, bool mirror, int mode)
{
	if (tex)
	{
		gl_RenderState.SetMaterial(tex, CLAMP_NONE, 0, -1, false);
		gl_RenderState.EnableModelMatrix(true);
		gl_RenderState.EnableTextureMatrix(true);

		GLRenderer->mSkyVBO->SetupMatrices(tex, x_offset, y_offset, mirror, mode, gl_RenderState.mModelMatrix, gl_RenderState.mTextureMatrix);
	}

	GLRenderer->mSkyVBO->RenderDome(tex, mode);
	gl_RenderState.EnableTextureMatrix(false);
	gl_RenderState.EnableModelMatrix(false);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderBox(FTextureID texno, FMaterial * gltex, float x_offset, bool sky2)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FMaterial * tex;

	gl_RenderState.EnableModelMatrix(true);
	gl_RenderState.mModelMatrix.loadIdentity();

	if (!sky2)
		gl_RenderState.mModelMatrix.rotate(-180.0f+x_offset, level.info->skyrotatevector.X, level.info->skyrotatevector.Z, level.info->skyrotatevector.Y);
	else
		gl_RenderState.mModelMatrix.rotate(-180.0f+x_offset, level.info->skyrotatevector2.X, level.info->skyrotatevector2.Z, level.info->skyrotatevector2.Y);

	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(0), 4);

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(1), 4);

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(2), 4);

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(3), 4);
	}
	else 
	{
		faces=1;
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(-1), 10);
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(sb->fliptop? 6:5), 4);

	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(4), 4);

	gl_RenderState.EnableModelMatrix(false);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLSkyPortal::DrawContents(HWDrawInfo *di)
{
	bool drawBoth = false;
	auto &vp = di->Viewpoint;

	// We have no use for Doom lighting special handling here, so disable it for this function.
	int oldlightmode = ::level.lightmode;
	if (::level.lightmode == 8)
	{
		::level.lightmode = 2;
		gl_RenderState.SetSoftLightLevel(-1);
	}


	gl_RenderState.ResetColor();
	gl_RenderState.EnableFog(false);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bool oldClamp = gl_RenderState.SetDepthClamp(true);

	di->SetupView(0, 0, 0, !!(mState->MirrorFlag & 1), !!(mState->PlaneMirrorFlag & 1));

	gl_RenderState.SetVertexBuffer(GLRenderer->mSkyVBO);
	if (origin->texture[0] && origin->texture[0]->tex->bSkybox)
	{
		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->sky2);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			gl_RenderState.SetTextureMode(TM_OPAQUE);
			RenderDome(origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER);
			gl_RenderState.SetTextureMode(TM_MODULATE);
		}
		
		gl_RenderState.AlphaFunc(GL_GREATER, 0.f);
		
		if (origin->doublesky && origin->texture[1])
		{
			RenderDome(origin->texture[1], origin->x_offset[1], origin->y_offset, false, FSkyVertexBuffer::SKYMODE_SECONDLAYER);
		}

		if (::level.skyfog>0 && !di->isFullbrightScene()  && (origin->fadecolor & 0xffffff) != 0)
		{
			PalEntry FadeColor = origin->fadecolor;
			FadeColor.a = clamp<int>(::level.skyfog, 0, 255);

			gl_RenderState.EnableTexture(false);
			gl_RenderState.SetObjectColor(FadeColor);
			gl_RenderState.Apply();
			glDrawArrays(GL_TRIANGLES, 0, 12);
			gl_RenderState.EnableTexture(true);
			gl_RenderState.SetObjectColor(0xffffffff);
		}
	}
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	di->ApplyVPUniforms();
	::level.lightmode = oldlightmode;
	gl_RenderState.SetDepthClamp(oldClamp);
}

