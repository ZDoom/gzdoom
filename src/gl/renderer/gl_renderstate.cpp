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
			gl_FrameState.ApplyToShader(&shd->mFrameStateIndices);
		}
	}

	if (activeShader)
	{
		// set all uniforms that got changed since the last use of this shader
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
		if (activeShader->mMatrixTick[0] < VSML.getLastUpdate(VSML.MODEL) && activeShader->mModelMatLocation >= 0)
		{
			// model matrix is a regular uniform which is part of the render state.
			VSML.matrixToGL(activeShader->mModelMatLocation, VSML.MODEL);
			activeShader->mMatrixTick[0] = VSML.getLastUpdate(VSML.MODEL);
		}

		glUniform1i(100, -1);	// kill the old dynamic light list (must be removed once buffers are working!)
		int fogset = 0;
		PalEntry pe(
			xs_CRoundToInt(mColor[3] * 255.f + 0.1f),
			xs_CRoundToInt(mColor[0] * 255.f + 0.1f),
			xs_CRoundToInt(mColor[1] * 255.f + 0.1f),
			xs_CRoundToInt(mColor[2] * 255.f + 0.1f));
		glUniform1i(activeShader->buffercolor_index, pe.d);
		// fixme: Reimplement desaturation.


		// now the buffer stuff
		if (mFogEnabled)
		{
			if ((mFogColor & 0xffffff) == 0)
			{
				fogset = gl_fogmode;
			}
			else
			{
				fogset = -gl_fogmode;
			}
		}

		if (fogset != activeShader->currentfogenabled)
		{
			glUniform1i(activeShader->fogenabled_index, (activeShader->currentfogenabled = fogset)); 
		}

		/*if (mLightParms[0] != activeShader->currentlightfactor || 
			mLightParms[1] != activeShader->currentlightdist ||
			mFogDensity != activeShader->currentfogdensity)*/
		{
			const float LOG2E = 1.442692f;	// = 1/log(2)
			//activeShader->currentlightdist = mLightParms[1];
			//activeShader->currentlightfactor = mLightParms[0];
			//activeShader->currentfogdensity = mFogDensity;
			// premultiply the density with as much as possible here to reduce shader
			// execution time.
			glVertexAttrib4f(VATTR_FOGPARAMS, mLightParms[0], mLightParms[1], mFogDensity * (-LOG2E / 64000.f), 0);
		}
		if (mFogColor != activeShader->currentfogcolor)
		{
			activeShader->currentfogcolor = mFogColor;

			glUniform4f (activeShader->fogcolor_index, mFogColor.r/255.f, mFogColor.g/255.f, mFogColor.b/255.f, 0);
		}
		if (mGlowEnabled)
		{
			ParameterBufferElement *pptr;
			int glowindex = GLRenderer->mParmBuffer->Reserve(4, &pptr);
			memcpy(pptr, mGlowParms, 16 * sizeof(float));
			activeShader->currentglowstate = 1;
			glUniform1i(activeShader->glowindex_index, glowindex);
		}
		else if (activeShader->currentglowstate)
		{
			// if glowing is on, disable it.
			glUniform1i(activeShader->glowindex_index, -1);
		}

		if (glset.lightmode == 8)
		{
			glVertexAttrib1f(VATTR_LIGHTLEVEL, mSoftLightLevel);
		}
		else
		{
			glVertexAttrib1f(VATTR_LIGHTLEVEL, -1.f);
		}

		if (activeShader->mMatrixTick[3] < VSML.getLastUpdate(VSML.AUX0))
		{
			int index;
			if (VSML.stackSize(VSML.AUX0))
			{
				// update texture matrix
				ParameterBufferElement *pptr;
				index = GLRenderer->mParmBuffer->Reserve(4, &pptr);
				VSML.copy(pptr->vec, VSML.AUX0);
				activeShader->mMatrixTick[3] = VSML.getLastUpdate(VSML.AUX0);
			}
			else
			{
				index = -1;
			}
			glUniform1i(101, index);
		}

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
