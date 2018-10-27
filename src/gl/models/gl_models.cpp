// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_models.cpp
**
** OpenGL renderer model handling code
**
**/

#include "gl_load/gl_system.h"
#include "w_wad.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "i_time.h"
#include "cmdlib.h"
#include "hwrenderer/textures/hw_material.h"

#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"

CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)

VSMatrix FGLModelRenderer::GetViewToWorldMatrix()
{
	VSMatrix objectToWorldMatrix;
	di->VPUniforms.mViewMatrix.inverseMatrix(objectToWorldMatrix);
	return objectToWorldMatrix;
}

void FGLModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	glDepthFunc(GL_LEQUAL);
	gl_RenderState.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// [Nash] Don't do back face culling if explicitly specified in MODELDEF
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]) && !(smf->flags & MDL_DONTCULLBACKFACES))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace((mirrored ^ screen->mPortalState->isMirrored()) ? GL_CCW : GL_CW);
	}

	gl_RenderState.mModelMatrix = objectToWorldMatrix;
	gl_RenderState.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
{
	gl_RenderState.EnableModelMatrix(false);

	glDepthFunc(GL_LESS);
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]) && !(smf->flags & MDL_DONTCULLBACKFACES))
		glDisable(GL_CULL_FACE);
}

void FGLModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	glDepthFunc(GL_LEQUAL);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace((mirrored ^ screen->mPortalState->isMirrored()) ? GL_CW : GL_CCW);
	}

	gl_RenderState.mModelMatrix = objectToWorldMatrix;
	gl_RenderState.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawHUDModel(AActor *actor)
{
	gl_RenderState.EnableModelMatrix(false);

	glDepthFunc(GL_LESS);
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]))
		glDisable(GL_CULL_FACE);
}

IModelVertexBuffer *FGLModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new FModelVertexBuffer(needindex, singleframe);
}

void FGLModelRenderer::SetVertexBuffer(IModelVertexBuffer *buffer)
{
	static_cast<FModelVertexBuffer*>(buffer)->Bind(gl_RenderState);
}

void FGLModelRenderer::ResetVertexBuffer()
{
	GLRenderer->mVBO->Bind(gl_RenderState);
}

void FGLModelRenderer::SetInterpolation(double inter)
{
	gl_RenderState.SetInterpolationFactor((float)inter);
}

void FGLModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
{
	FMaterial * tex = FMaterial::ValidateTexture(skin, false);
	gl_RenderState.ApplyMaterial(tex, clampNoFilter ? CLAMP_NOFILTER : CLAMP_NONE, translation, -1);
	/*if (modellightindex != -1)*/ gl_RenderState.SetLightIndex(modellightindex);
	gl_RenderState.Apply();
}

void FGLModelRenderer::DrawArrays(int start, int count)
{
	glDrawArrays(GL_TRIANGLES, start, count);
}

void FGLModelRenderer::DrawElements(int numIndices, size_t offset)
{
	glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, (void*)(intptr_t)offset);
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::FModelVertexBuffer(bool needindex, bool singleframe)
{
	mVertexBuffer = screen->CreateVertexBuffer();
	mIndexBuffer = needindex ? screen->CreateIndexBuffer() : nullptr;

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, myoffsetof(FModelVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, myoffsetof(FModelVertex, u) },
		{ 0, VATTR_NORMAL, VFmt_Packed_A2R10G10B10, myoffsetof(FModelVertex, packedNormal) },
		{ 0, VATTR_VERTEX2, VFmt_Float3, myoffsetof(FModelVertex, x) }
	};
	mVertexBuffer->SetFormat(2, 4, sizeof(FModelVertex), format);
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::Bind(FRenderState &state)
{
	state.SetVertexBuffer(mVertexBuffer, mIndexFrame[0], mIndexFrame[1]);
	if (mIndexBuffer) state.SetIndexBuffer(mIndexBuffer);
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::~FModelVertexBuffer()
{
	if (mIndexBuffer) delete mIndexBuffer;
	delete mVertexBuffer;
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertex *FModelVertexBuffer::LockVertexBuffer(unsigned int size)
{
	return static_cast<FModelVertex*>(mVertexBuffer->Lock(size * sizeof(FModelVertex)));
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockVertexBuffer()
{
	mVertexBuffer->Unlock();
}

//===========================================================================
//
//
//
//===========================================================================

unsigned int *FModelVertexBuffer::LockIndexBuffer(unsigned int size)
{
	if (mIndexBuffer) return static_cast<unsigned int*>(mIndexBuffer->Lock(size * sizeof(unsigned int)));
	else return nullptr;
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockIndexBuffer()
{
	if (mIndexBuffer) mIndexBuffer->Unlock();
}


//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size)
{
	mIndexFrame[0] = frame1;
	mIndexFrame[1] = frame2;
}
