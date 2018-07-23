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

void gl_SetTextureMode(int type);

FRenderState gl_RenderState;

CVAR(Bool, gl_direct_state_change, true, 0)


static VSMatrix identityMatrix(1);
TArray<VSMatrix> gl_MatrixStack;

static void matrixToGL(const VSMatrix &mat, int loc)
{
	glUniformMatrix4fv(loc, 1, false, (float*)&mat);
}

//==========================================================================
//
//
//
//==========================================================================

void FRenderState::Reset()
{
	mTextureEnabled = true;
	mSplitEnabled = mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
	mColorMask[0] = mColorMask[1] = mColorMask[2] = mColorMask[3] = true;
	currentColorMask[0] = currentColorMask[1] = currentColorMask[2] = currentColorMask[3] = true;
	mFogColor.d = -1;
	mTextureMode = -1;
	mDesaturation = 0;
	mSrcBlend = GL_SRC_ALPHA;
	mDstBlend = GL_ONE_MINUS_SRC_ALPHA;
	mAlphaThreshold = 0.5f;
	mBlendEquation = GL_FUNC_ADD;
	mModelMatrixEnabled = false;
	mTextureMatrixEnabled = false;
	mObjectColor = 0xffffffff;
	mObjectColor2 = 0;
	mVertexBuffer = mCurrentVertexBuffer = NULL;
	mSoftLight = 0;
	mLightParms[0] = mLightParms[1] = mLightParms[2] = 0.0f;
	mLightParms[3] = -1.f;
	mSpecialEffect = EFF_NONE;
	mGlossiness = 0.0f;
	mSpecularLevel = 0.0f;
	mShaderTimer = 0.0f;
	ClearClipSplit();

	stSrcBlend = stDstBlend = -1;
	stBlendEquation = -1;
	stAlphaThreshold = -1.f;
	stAlphaTest = 0;
	mLastDepthClamp = true;
	mInterpolationFactor = 0.0f;

	mColor.Set(1.0f, 1.0f, 1.0f, 1.0f);
	mGlowTop.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mGlowBottom.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mGlowTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mGlowBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mSplitTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mSplitBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mDynColor.Set(0.0f, 0.0f, 0.0f, 0.0f);
	mEffectState = 0;
	activeShader = nullptr;
	mModelMatrix.loadIdentity();
	mTextureMatrix.loadIdentity();
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
		activeShader = GLRenderer->mShaderManager->Get(mTextureEnabled ? mEffectState : SHADER_NoTexture, mAlphaThreshold >= 0.f, mPassType);
		activeShader->Bind();
	}

	int fogset = 0;

	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			fogset = -3;	// 2D rendering with 'foggy' overlay.
		}
		else if ((mFogColor & 0xffffff) == 0)
		{
			fogset = gl_fogmode;
		}
		else
		{
			fogset = -gl_fogmode;
		}
	}

	glVertexAttrib4fv(VATTR_COLOR, mColor.vec);
	glVertexAttrib4fv(VATTR_NORMAL, mNormal.vec);

	activeShader->muDesaturation.Set(mDesaturation / 255.f);
	activeShader->muFogEnabled.Set(fogset);
	activeShader->muTextureMode.Set(mTextureMode == TM_MODULATE && mTempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode);
	activeShader->muLightParms.Set(mLightParms);
	activeShader->muFogColor.Set(mFogColor);
	activeShader->muObjectColor.Set(mObjectColor);
	activeShader->muObjectColor2.Set(mObjectColor2);
	activeShader->muDynLightColor.Set(mDynColor.vec);
	activeShader->muInterpolationFactor.Set(mInterpolationFactor);
	activeShader->muTimer.Set((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);
	activeShader->muAlphaThreshold.Set(mAlphaThreshold);
	activeShader->muLightIndex.Set(-1);
	activeShader->muClipSplit.Set(mClipSplit);
	activeShader->muSpecularMaterial.Set(mGlossiness, mSpecularLevel);

	if (mGlowEnabled)
	{
		activeShader->muGlowTopColor.Set(mGlowTop.vec);
		activeShader->muGlowBottomColor.Set(mGlowBottom.vec);
		activeShader->currentglowstate = 1;
	}
	else if (activeShader->currentglowstate)
	{
		// if glowing is on, disable it.
		activeShader->muGlowTopColor.Set(nulvec);
		activeShader->muGlowBottomColor.Set(nulvec);
		activeShader->currentglowstate = 0;
	}
	if (mGlowEnabled || mObjectColor2.a != 0)
	{
		activeShader->muGlowTopPlane.Set(mGlowTopPlane.vec);
		activeShader->muGlowBottomPlane.Set(mGlowBottomPlane.vec);
	}

	if (mSplitEnabled)
	{
		activeShader->muSplitTopPlane.Set(mSplitTopPlane.vec);
		activeShader->muSplitBottomPlane.Set(mSplitBottomPlane.vec);
		activeShader->currentsplitstate = 1;
	}
	else if (activeShader->currentsplitstate)
	{
		activeShader->muSplitTopPlane.Set(nulvec);
		activeShader->muSplitBottomPlane.Set(nulvec);
		activeShader->currentsplitstate = 0;
	}

	if (mTextureMatrixEnabled)
	{
		matrixToGL(mTextureMatrix, activeShader->texturematrix_index);
		activeShader->currentTextureMatrixState = true;
	}
	else if (activeShader->currentTextureMatrixState)
	{
		activeShader->currentTextureMatrixState = false;
		matrixToGL(identityMatrix, activeShader->texturematrix_index);
	}

	if (mModelMatrixEnabled)
	{
		matrixToGL(mModelMatrix, activeShader->modelmatrix_index);
		VSMatrix norm;
		norm.computeNormalMatrix(mModelMatrix);
		matrixToGL(norm, activeShader->normalmodelmatrix_index);
		activeShader->currentModelMatrixState = true;
	}
	else if (activeShader->currentModelMatrixState)
	{
		activeShader->currentModelMatrixState = false;
		matrixToGL(identityMatrix, activeShader->modelmatrix_index);
		matrixToGL(identityMatrix, activeShader->normalmodelmatrix_index);
	}
	return true;
}


//==========================================================================
//
// Apply State
//
//==========================================================================

void FRenderState::Apply()
{
	if (!gl_direct_state_change)
	{
		if (mSrcBlend != stSrcBlend || mDstBlend != stDstBlend)
		{
			stSrcBlend = mSrcBlend;
			stDstBlend = mDstBlend;
			glBlendFunc(mSrcBlend, mDstBlend);
		}
		if (mBlendEquation != stBlendEquation)
		{
			stBlendEquation = mBlendEquation;
			glBlendEquation(mBlendEquation);
		}
	}

	//ApplyColorMask(); I don't think this is needed.

	if (mVertexBuffer != mCurrentVertexBuffer)
	{
		if (mVertexBuffer == NULL) glBindBuffer(GL_ARRAY_BUFFER, 0);
		else mVertexBuffer->BindVBO();
		mCurrentVertexBuffer = mVertexBuffer;
	}
	ApplyShader();
}



void FRenderState::ApplyColorMask()
{
	if ((mColorMask[0] != currentColorMask[0]) ||
		(mColorMask[1] != currentColorMask[1]) ||
		(mColorMask[2] != currentColorMask[2]) ||
		(mColorMask[3] != currentColorMask[3]))
	{
		glColorMask(mColorMask[0], mColorMask[1], mColorMask[2], mColorMask[3]);
		currentColorMask[0] = mColorMask[0];
		currentColorMask[1] = mColorMask[1];
		currentColorMask[2] = mColorMask[2];
		currentColorMask[3] = mColorMask[3];
	}
}

void FRenderState::ApplyLightIndex(int index)
{
	if (index > -1 && GLRenderer->mLights->GetBufferType() == GL_UNIFORM_BUFFER)
	{
		index = GLRenderer->mLights->BindUBO(index);
	}
	activeShader->muLightIndex.Set(index);
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
	mShaderTimer = mat->tex->shaderspeed;
	SetSpecular(mat->tex->Glossiness, mat->tex->SpecularLevel);

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


