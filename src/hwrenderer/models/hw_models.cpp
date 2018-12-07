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
** hardware renderer model handling code
**
**/

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
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_models.h"

CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)

VSMatrix FGLModelRenderer::GetViewToWorldMatrix()
{
	VSMatrix objectToWorldMatrix;
	di->VPUniforms.mViewMatrix.inverseMatrix(objectToWorldMatrix);
	return objectToWorldMatrix;
}

void FGLModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	state.SetDepthFunc(DF_LEqual);
	state.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// [Nash] Don't do back face culling if explicitly specified in MODELDEF
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == DefaultRenderStyle()) && !(smf->flags & MDL_DONTCULLBACKFACES))
	{
		state.SetCulling((mirrored ^ screen->mPortalState->isMirrored()) ? Cull_CCW : Cull_CW);
	}

	state.mModelMatrix = objectToWorldMatrix;
	state.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
{
	state.EnableModelMatrix(false);
	state.SetDepthFunc(DF_Less);
	if (!(actor->RenderStyle == DefaultRenderStyle()) && !(smf->flags & MDL_DONTCULLBACKFACES))
		state.SetCulling(Cull_None);
}

void FGLModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	state.SetDepthFunc(DF_LEqual);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == DefaultRenderStyle()))
	{
		state.SetCulling((mirrored ^ screen->mPortalState->isMirrored()) ? Cull_CW : Cull_CCW);
	}

	state.mModelMatrix = objectToWorldMatrix;
	state.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawHUDModel(AActor *actor)
{
	state.EnableModelMatrix(false);

	state.SetDepthFunc(DF_Less);
	if (!(actor->RenderStyle == DefaultRenderStyle()))
		state.SetCulling(Cull_None);
}

IModelVertexBuffer *FGLModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new FModelVertexBuffer(needindex, singleframe);
}

void FGLModelRenderer::SetInterpolation(double inter)
{
	state.SetInterpolationFactor((float)inter);
}

void FGLModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
{
	FMaterial * tex = FMaterial::ValidateTexture(skin, false);
	state.SetMaterial(tex, clampNoFilter ? CLAMP_NOFILTER : CLAMP_NONE, translation, -1);
	state.SetLightIndex(modellightindex);
}

void FGLModelRenderer::DrawArrays(int start, int count)
{
	state.Draw(DT_Triangles, start, count);
}

void FGLModelRenderer::DrawElements(int numIndices, size_t offset)
{
	state.DrawIndexed(DT_Triangles, int(offset / sizeof(unsigned int)), numIndices);
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
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FModelVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FModelVertex, u) },
		{ 0, VATTR_NORMAL, VFmt_Packed_A2R10G10B10, (int)myoffsetof(FModelVertex, packedNormal) },
		{ 1, VATTR_VERTEX2, VFmt_Float3, (int)myoffsetof(FModelVertex, x) },
		{ 1, VATTR_NORMAL2, VFmt_Packed_A2R10G10B10, (int)myoffsetof(FModelVertex, packedNormal) }
	};
	mVertexBuffer->SetFormat(2, 5, sizeof(FModelVertex), format);
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
	auto &state = static_cast<FGLModelRenderer*>(renderer)->state;
	state.SetVertexBuffer(mVertexBuffer, frame1, frame2);
	if (mIndexBuffer) state.SetIndexBuffer(mIndexBuffer);
}
