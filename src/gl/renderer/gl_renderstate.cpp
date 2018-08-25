// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2009-2016 Christoph Oelckers
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
** gl_renderstate.cpp
** Render state maintenance
**
*/

#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "gl/data/gl_vertexbuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/dynlights//gl_lightbuffer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/data/gl_dynamicuniformbuffer.h"

FRenderState gl_RenderState;

CVAR(Bool, gl_direct_state_change, true, 0)


static VSMatrix identityMatrix(1);

//==========================================================================
//
//
//
//==========================================================================

void FRenderState::Reset()
{
	mAttributes.SetDefaults();
	mTextureEnabled = true;
	mSplitEnabled = mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
	mSrcBlend = GL_SRC_ALPHA;
	mDstBlend = GL_ONE_MINUS_SRC_ALPHA;
	mBlendEquation = GL_FUNC_ADD;
	mVertexBuffer = mCurrentVertexBuffer = NULL;
	mSpecialEffect = EFF_NONE;
	ClearClipSplit();

	mLastDepthClamp = true;

	mEffectState = 0;
	activeShader = nullptr;
	mPassType = NORMAL_PASS;
}

//==========================================================================
//
// Apply shader settings
//
//==========================================================================

bool FRenderState::ApplyShader()
{
	static uint64_t firstFrame = 0;
	// if firstFrame is not yet initialized, initialize it to current time
	// if we're going to overflow a float (after ~4.6 hours, or 24 bits), re-init to regain precision
	if ((firstFrame == 0) || (screen->FrameTime - firstFrame >= 1<<24) || level.ShaderStartTime >= firstFrame)
		firstFrame = screen->FrameTime;

	static const float nulvec[] = { 0.f, 0.f, 0.f, 0.f };
	if (mSpecialEffect > EFF_NONE)
	{
		activeShader = GLRenderer->mShaderManager->BindEffect(mSpecialEffect, mPassType);
	}
	else
	{
		activeShader = GLRenderer->mShaderManager->Get(mTextureEnabled ? mEffectState : SHADER_NoTexture, mAttributes.uAlphaThreshold >= 0.f, mPassType);
		activeShader->Bind();
	}

	int fogset = 0;

	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			fogset = -3;	// 2D rendering with 'foggy' overlay.
		}
		else if (mAttributes.uFogColor.XYZ().isZero())
		{
			fogset = gl_fogmode;
		}
		else
		{
			fogset = -gl_fogmode;
		}
	}
	mAttributes.uFogEnabled = fogset;
	if (!mGlowEnabled) mAttributes.uGlowTopColor.W = mAttributes.uGlowBottomColor.W = 0.f;

	if (GLRenderer->mLights->GetBufferType() == GL_UNIFORM_BUFFER)
	{
		GLRenderer->mLights->BindUBO(mAttributes.uLightIndex);
		GLRenderer->mTextureMatrices->Bind(mAttributes.uTexMatrixIndex);
	}
	auto index = GLRenderer->mAttributes->Upload(&mAttributes);
 	GLRenderer->mAttributes->Bind(index);

	glVertexAttrib4fv(VATTR_NORMAL, &mNormal.X);


	return true;
}


//==========================================================================
//
// Apply State
//
//==========================================================================

void FRenderState::Apply()
{
	if (mVertexBuffer != mCurrentVertexBuffer)
	{
		if (mVertexBuffer == NULL) glBindBuffer(GL_ARRAY_BUFFER, 0);
		else mVertexBuffer->BindVBO();
		mCurrentVertexBuffer = mVertexBuffer;
	}
	ApplyShader();
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

void FRenderState::SetMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader, bool alphatexture)
{
	if (mat->tex->bHasCanvas)
	{
		mTempTM = TM_OPAQUE;
	}
	else
	{
		mTempTM = TM_MODULATE;
	}
	mEffectState = overrideshader >= 0 ? overrideshader : mat->mShaderIndex;
	mAttributes.timer = mat->tex->shaderspeed;
	
	auto tex = mat->tex;
	if (tex->UseType == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
	if (tex->bHasCanvas) clampmode = CLAMP_CAMTEX;
	else if ((tex->bWarped || tex->shaderindex >= FIRST_USER_SHADER) && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;
	
	// avoid rebinding the same texture multiple times.
	if (mat == lastMaterial && lastClamp == clampmode && translation == lastTranslation) return;
	lastMaterial = mat;
	lastClamp = clampmode;
	lastTranslation = translation;

	int usebright = false;
	int maxbound = 0;

	// Textures that are already scaled in the texture lump will not get replaced by hires textures.
	int flags = mat->isExpanded() ? CTF_Expand : (gl_texture_usehires && tex->Scale.X == 1 && tex->Scale.Y == 1 && clampmode <= CLAMP_XY) ? CTF_CheckHires : 0;
	int numLayers = mat->GetLayers();
	auto base = static_cast<FHardwareTexture*>(mat->GetLayer(0));

	if (base->BindOrCreate(tex, 0, clampmode, translation, flags))
	{
		for (int i = 1; i<numLayers; i++)
		{
			FTexture *layer;
			auto systex = static_cast<FHardwareTexture*>(mat->GetLayer(i, &layer));
			systex->BindOrCreate(layer, i, clampmode, 0, mat->isExpanded() ? CTF_Expand : 0);
			maxbound = i;
		}
	}
	// unbind everything from the last texture that's still active
	for (int i = maxbound + 1; i <= maxBoundMaterial; i++)
	{
		FHardwareTexture::Unbind(i);
		maxBoundMaterial = maxbound;
	}
}


