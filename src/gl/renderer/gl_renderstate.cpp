/*
** gl_renderstate.cpp
** Render state maintenance
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "gl/data/vsMathLib.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_colormap.h"
#include "gl/utility/gl_clock.h"


FRenderState gl_RenderState;

CVAR(Bool, gl_direct_state_change, true, 0)


//==========================================================================
//
//
//
//==========================================================================

void FRenderState::Reset()
{
	mTextureEnabled = true;
	mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
	mSpecialEffect = EFF_NONE;
	mObjectColor.d = -1;
	mFogColor.d = -1;
	mFogDensity = 0;
	mTextureMode = -1;
	mColorControl = 2;
	mSrcBlend = GL_SRC_ALPHA;
	mDstBlend = GL_ONE_MINUS_SRC_ALPHA;
	glSrcBlend = glDstBlend = -1;
	mAlphaThreshold = 0.5f;
	mBlendEquation = GL_FUNC_ADD;
	gl_BlendEquation = -1;
	mSpecialMode = 0;
	mLightIndex = -1;
	mTexMatrixTick = -1;
	mColor = 0;
	mLightAttr = 0;
}


//==========================================================================
//
// Set texture shader info
//
//==========================================================================

int FRenderState::SetupShader(int &shaderindex)
{
	mEffectState = shaderindex;
	return 0;
}


//==========================================================================
//
// Apply shader settings
//
//==========================================================================

bool FRenderState::ApplyShader()
{
	bool useshaders = false;
	FShader *activeShader = NULL;

	if (mSpecialEffect >= 0)
	{
		activeShader = GLRenderer->mShaderManager->BindEffect(mSpecialEffect);
	}
	else
	{
		FShader *shd = GLRenderer->mShaderManager->Get(mTextureEnabled? mEffectState : 4);

		if (shd != NULL)
		{
			activeShader = shd;
			shd->Bind();
		}
	}
	gl_FrameState.ApplyToShader(&activeShader->mFrameStateIndices);

	if (activeShader)
	{
		//
		// set all uniforms that got changed since the last use of this shader
		//
		if (mColorControl != activeShader->currentColorControl)
		{
			glUniform1i(activeShader->colorcontrol_index, (activeShader->currentColorControl = mColorControl));
		}
		if (mTextureMode != activeShader->currenttexturemode)
		{
			glUniform1i(activeShader->texturemode_index, (activeShader->currenttexturemode = mTextureMode));
		}
		if (mObjectColor != activeShader->currentobjectcolor)
		{
			activeShader->currentobjectcolor = mObjectColor;
			glUniform4f(activeShader->objectcolor_index, mObjectColor.r / 255.f, mObjectColor.g / 255.f, mObjectColor.b / 255.f, 1.0f);
		}
		if (activeShader->dyncolortick != mDynTick)
		{
			activeShader->dyncolortick = mDynTick;
			glUniform4f(activeShader->dlightcolor_index, mDynLight[0], mDynLight[1], mDynLight[2], 0.f);
		}
		float newthresh = mAlphaTest ? mAlphaThreshold : -1.f;
		if (newthresh != activeShader->currentalphathreshold)
		{
			glUniform1f(activeShader->alphathreshold_index, newthresh);
			activeShader->currentalphathreshold = newthresh;
		}
		if (mSpecialMode != activeShader->currentSpecialMode)
		{
			glUniform1i(activeShader->specialmode_index, (activeShader->currentSpecialMode = mSpecialMode));
		}
		if (activeShader->mMatrixTick < VSML.getLastUpdate(VSML.MODEL) && activeShader->mModelMatLocation >= 0)
		{
			// model matrix is a regular uniform which is part of the render state.
			VSML.matrixToGL(activeShader->mModelMatLocation, VSML.MODEL);
			activeShader->mMatrixTick = VSML.getLastUpdate(VSML.MODEL);
		}

		//
		// end of uniforms. Now for the buffer parameters.
		//

		AttribBufferElement *aptr;
		unsigned int aindex = GLRenderer->mAttribBuffer->Reserve(1, &aptr);

		glVertexAttribI1i(VATTR_ATTRIBINDEX, aindex);
		if (mGlowEnabled)
		{
			ParameterBufferElement *pptr;
			int glowindex = GLRenderer->mParmBuffer->Reserve(4, &pptr);
			memcpy(pptr, mGlowParms, 16 * sizeof(float));
			activeShader->currentglowstate = 1;
			aptr->mGlowIndex = glowindex;
		}
		else
		{
			aptr->mGlowIndex = -1;
		}
		aptr->mLightIndex = mLightIndex;

		if (VSML.stackSize(VSML.AUX0) > 0)	// if there's nothing on the stack we don't need a texture matrix.
		{
			if (mTexMatrixTick < VSML.getLastUpdate(VSML.AUX0))
			{
				// update texture matrix only if it is different from last time.
				ParameterBufferElement *pptr;
				mTexMatrixIndex = GLRenderer->mParmBuffer->Reserve(4, &pptr);
				VSML.copy(pptr->vec, VSML.AUX0);
				mTexMatrixTick = VSML.getLastUpdate(VSML.AUX0);
			}
			aptr->mMatIndex = mTexMatrixIndex;
		}
		else
		{
			aptr->mMatIndex = -1;
		}

		aptr->mColor = mColor;
		aptr->mFogColor = mFogColor;
		aptr->mFogColor.a = mFogEnabled;
		aptr->mLightAttr = mLightAttr;
		aptr->mFogDensity = mFogDensity * (-1.442692f /*1/log(2)*/ / 64000.f);
		return true;
	}
	return false;
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
		if (mSrcBlend != glSrcBlend || mDstBlend != glDstBlend)
		{
			glSrcBlend = mSrcBlend;
			glDstBlend = mDstBlend;
			glBlendFunc(mSrcBlend, mDstBlend);
		}
		if (mBlendEquation != gl_BlendEquation)
		{
			gl_BlendEquation = mBlendEquation;
			::glBlendEquation(mBlendEquation);
		}
	}

	if (mVertexArray != mLastVertexArray)
	{
		mLastVertexArray = mVertexArray;
		glBindVertexArray(mVertexArray);
	}
	ApplyShader();
}
