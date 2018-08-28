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
#include "gl/data/gl_modelbuffer.h"
#include "gl/data/gl_dynamicuniformbuffer.h"
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

void FSkyVertexBuffer::RenderDome(FMaterial *tex, int mode, int atindex)
{
	int rc = mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == SKYMODE_MAINLAYER && tex != NULL)
	{
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply(atindex, false);
		RenderRow(GL_TRIANGLE_FAN, 0);

		gl_RenderState.Apply(atindex+1, false);
		RenderRow(GL_TRIANGLE_FAN, rc);
		gl_RenderState.EnableTexture(true);
		atindex += 2;
	}
	gl_RenderState.Apply(atindex, mode == SKYMODE_SECONDLAYER);
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

void RenderDome(FMaterial * tex, float x_offset, float y_offset, bool mirror, int mode, int atindex)
{
	int mmindex = 0;
	gl_RenderState.SetMaterial(tex, CLAMP_NONE, 0, -1, false);
	GLRenderer->mSkyVBO->RenderDome(tex, mode, atindex);
	gl_RenderState.SetTexMatrixIndex(0);
	GLRenderer->mModelMatrix->Bind(0);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderBox(FTextureID texno, FMaterial * gltex, float x_offset, bool sky2, int atindex)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FMaterial * tex;

	if (sb->faces[5])
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply(atindex, false);
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(0), 4);

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply(atindex, false);
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(1), 4);

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply(atindex, false);
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(2), 4);

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply(atindex, false);
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(3), 4);
	}
	else 
	{
		faces=1;
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply(atindex, false);
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(-1), 10);
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply(atindex, false);
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(sb->fliptop? 6:5), 4);

	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply(atindex, false);
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(4), 4);

	GLRenderer->mModelMatrix->Bind(0);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLSkyPortal::DrawContents(HWDrawInfo *di)
{
	GLRenderer->mTextureMatrices->Map();
	GLRenderer->mModelMatrix->Map();
	GLRenderer->mAttributes->Map();
	FSkyBufferInfo indices = GLRenderer->mSkyVBO->PrepareContents(di, origin);
	GLRenderer->mTextureMatrices->Unmap();
	GLRenderer->mModelMatrix->Unmap();
	GLRenderer->mAttributes->Unmap();
	
	bool drawBoth = false;
	auto &vp = di->Viewpoint;

	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bool oldClamp = gl_RenderState.SetDepthClamp(true);

	di->SetupView(0, 0, 0, !!(mState->MirrorFlag & 1), !!(mState->PlaneMirrorFlag & 1));

	gl_RenderState.SetVertexBuffer(GLRenderer->mSkyVBO);
	GLRenderer->mModelMatrix->Bind(indices.mModelMatrixIndex);
	if (indices.mFlags == 0)
	{
		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->sky2, indices.mAttributeIndex);
	}
	else
	{
		RenderDome(origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER, indices.mAttributeIndex);
		indices.mAttributeIndex+=3;

		if (indices.mFlags & FSkyVertexBuffer::SKYMODE_SECONDLAYER)
		{
			GLRenderer->mModelMatrix->Bind(indices.mModelMatrixIndex+1);
			RenderDome(origin->texture[1], origin->x_offset[1], origin->y_offset, false, FSkyVertexBuffer::SKYMODE_SECONDLAYER, indices.mAttributeIndex);
			indices.mAttributeIndex++;
		}
		if (indices.mFlags & FSkyVertexBuffer::SKYMODE_FOGLAYER)
		{
			GLRenderer->mModelMatrix->Bind(0);

			gl_RenderState.EnableTexture(false);
			gl_RenderState.Apply(indices.mAttributeIndex, false);
			glDrawArrays(GL_TRIANGLES, 0, 12);
			gl_RenderState.EnableTexture(true);
			gl_RenderState.SetObjectColor(0xffffffff);
		}
	}
	gl_RenderState.SetTexMatrixIndex(0);
	GLRenderer->mModelMatrix->Bind(0);

	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	gl_RenderState.SetDepthClamp(oldClamp);
}

