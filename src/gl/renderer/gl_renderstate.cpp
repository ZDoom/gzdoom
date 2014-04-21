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


FRenderState gl_RenderState;
int FStateAttr::ChangeCounter;

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
	mFogColor.d = -1;
	mFogDensity = 0;
	mTextureMode = -1;
	mSrcBlend = GL_SRC_ALPHA;
	mDstBlend = GL_ONE_MINUS_SRC_ALPHA;
	glSrcBlend = glDstBlend = -1;
	mAlphaThreshold = 0.5f;
	mBlendEquation = GL_FUNC_ADD;
	gl_BlendEquation = -1;
	m2D = true;
}


//==========================================================================
//
// Set texture shader info
//
//==========================================================================

int FRenderState::SetupShader(int &shaderindex, int &cm)
{
	bool usecmshader;

	if (shaderindex == 3)
	{
		// Brightmap should not be used.
		if (!mBrightmapEnabled || cm >= CM_FIRSTSPECIALCOLORMAP)
		{
			shaderindex = 0;
		}
	}

	usecmshader = cm > CM_DEFAULT && cm < CM_MAXCOLORMAP && mTextureMode != TM_MASK;

	mEffectState = shaderindex;
	mColormapState = usecmshader? cm : CM_DEFAULT;
	if (usecmshader) cm = CM_DEFAULT;
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
		FShaderContainer *shd = GLRenderer->mShaderManager->Get(mTextureEnabled? mEffectState : 4);

		if (shd != NULL)
		{
			activeShader = shd->Bind(mColormapState);
			assert(activeShader != NULL);
		}
	}

	if (activeShader)
	{
		int fogset = 0;
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

		if (mTextureMode != activeShader->currenttexturemode)
		{
			glUniform1i(activeShader->texturemode_index, (activeShader->currenttexturemode = mTextureMode)); 
		}
		if (activeShader->currentcamerapos.Update(&mCameraPos))
		{
			glUniform3fv(activeShader->camerapos_index, 1, mCameraPos.vec); 
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

			glUniform4f (activeShader->fogcolor_index, mFogColor.r/255.f, mFogColor.g/255.f, 
							mFogColor.b/255.f, 0);
		}
		if (mGlowEnabled)
		{
			glUniform4fv(activeShader->glowtopcolor_index, 1, mGlowTop.vec);
			glUniform4fv(activeShader->glowbottomcolor_index, 1, mGlowBottom.vec);
			glUniform4fv(activeShader->glowtopplane_index, 1, mGlowTopPlane.vec);
			glUniform4fv(activeShader->glowbottomplane_index, 1, mGlowBottomPlane.vec);
			activeShader->currentglowstate = 1;
		}
		else if (activeShader->currentglowstate)
		{
			// if glowing is on, disable it.
			glUniform4f(activeShader->glowtopcolor_index, 0.f, 0.f, 0.f, 0.f);
			glUniform4f(activeShader->glowbottomcolor_index, 0.f, 0.f, 0.f, 0.f);
			activeShader->currentglowstate = 0;
		}
		//if (mLightEnabled)
		{
			glUniform3iv(activeShader->lightrange_index, 1, mNumLights);
			if (mLightData) glUniform4fv(activeShader->lights_index, mNumLights[2], mLightData);
		}

		float newthresh = mAlphaTest ? mAlphaThreshold : -1.f;
		if (newthresh != activeShader->currentalphathreshold)
		{
			glUniform1f(activeShader->alphathreshold_index, newthresh);
			activeShader->currentalphathreshold = newthresh;
		}

		if (mClipPlane != activeShader->currentclipplane)
		{
			glUniform1f(activeShader->clipplane_index, mClipPlane);
			activeShader->currentclipplane = mClipPlane;
		}

		if (glset.lightmode == 8)
		{
			glUniform3fv(activeShader->dlightcolor_index, 1, mDynLight);
			glVertexAttrib1f(VATTR_LIGHTLEVEL, mSoftLightLevel);
		}
		else
		{
			glVertexAttrib1f(VATTR_LIGHTLEVEL, -1.f);
		}

		if (activeShader->mMatrixTick[0] < VSML.getLastUpdate(VSML.MODEL) && activeShader->mModelMatLocation >= 0)
		{
			// update model matrix
			VSML.matrixToGL(activeShader->mModelMatLocation, VSML.MODEL);
			activeShader->mMatrixTick[0] = VSML.getLastUpdate(VSML.MODEL);
		}
		if (activeShader->mMatrixTick[1] < VSML.getLastUpdate(VSML.VIEW) && activeShader->mViewMatLocation >= 0)
		{
			// update view matrix
			VSML.matrixToGL(activeShader->mViewMatLocation, VSML.VIEW);
			activeShader->mMatrixTick[1] = VSML.getLastUpdate(VSML.VIEW);
		}
		if (activeShader->mMatrixTick[2] < VSML.getLastUpdate(VSML.PROJECTION) && activeShader->mProjMatLocation >= 0)
		{
			// update view-projection matrix
			VSML.matrixToGL(activeShader->mProjMatLocation, VSML.PROJECTION);
			activeShader->mMatrixTick[2] = VSML.getLastUpdate(VSML.PROJECTION);
		}
		if (activeShader->mMatrixTick[3] < VSML.getLastUpdate(VSML.AUX0) && activeShader->mTexMatLocation >= 0)
		{
			// update view-projection matrix
			VSML.matrixToGL(activeShader->mTexMatLocation, VSML.AUX0);
			activeShader->mMatrixTick[3] = VSML.getLastUpdate(VSML.AUX0);
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

	ApplyShader();
}

