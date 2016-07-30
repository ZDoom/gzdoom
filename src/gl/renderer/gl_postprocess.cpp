/*
** gl_postprocess.cpp
** Post processing effects in the render pipeline
**
**---------------------------------------------------------------------------
** Copyright 2016 Magnus Norddahl
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
#include "gi.h"
#include "m_png.h"
#include "m_random.h"
#include "st_stuff.h"
#include "dobject.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_data/r_interpolate.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_effect.h"
#include "sbar.h"
#include "po_man.h"
#include "r_utility.h"
#include "a_hexenglobal.h"
#include "p_local.h"
#include "gl/gl_functions.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_presentshader.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_bloom, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Float, gl_bloom_amount, 1.4f, 0)
CVAR(Float, gl_exposure, 0.0f, 0)

CUSTOM_CVAR(Int, gl_tonemap, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 4)
		self = 0;
}

CUSTOM_CVAR(Int, gl_bloom_kernel_size, 7, 0)
{
	if (self < 3 || self > 15 || self % 2 == 0)
		self = 7;
}

EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)

//-----------------------------------------------------------------------------
//
// Adds bloom contribution to scene texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::BloomScene()
{
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || !FGLRenderBuffers::IsEnabled() || gl_fixedcolormap != CM_DEFAULT)
		return;

	const float blurAmount = gl_bloom_amount;
	int sampleCount = gl_bloom_kernel_size;

	// TBD: Maybe need a better way to share state with other parts of the pipeline
	GLint activeTex, textureBinding, samplerBinding;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
		glBindSampler(0, 0);
	}
	GLboolean blendEnabled, scissorEnabled;
	GLint currentProgram, blendEquationRgb, blendEquationAlpha, blendSrcRgb, blendSrcAlpha, blendDestRgb, blendDestAlpha;
	glGetBooleanv(GL_BLEND, &blendEnabled);
	glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled);
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
	glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
	glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRgb);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);

	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	const auto &level0 = mBuffers->BloomLevels[0];

	// Extract blooming pixels from scene texture:
	glBindFramebuffer(GL_FRAMEBUFFER, level0.VFramebuffer);
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindSceneTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mBloomExtractShader->Bind();
	mBloomExtractShader->SceneTexture.Set(0);
	mBloomExtractShader->Exposure.Set(mCameraExposure);
	{
		FFlatVertex *ptr = mVBO->GetBuffer();
		ptr->Set(-1.0f, -1.0f, 0, 0.0f, 0.0f); ptr++;
		ptr->Set(-1.0f, 1.0f, 0, 0.0f, 1.0f); ptr++;
		ptr->Set(1.0f, -1.0f, 0, 1.0f, 0.0f); ptr++;
		ptr->Set(1.0f, 1.0f, 0, 1.0f, 1.0f); ptr++;
		mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Blur and downscale:
	for (int i = 0; i < FGLRenderBuffers::NumBloomLevels - 1; i++)
	{
		const auto &level = mBuffers->BloomLevels[i];
		const auto &next = mBuffers->BloomLevels[i + 1];
		mBlurShader->BlurHorizontal(mVBO, blurAmount, sampleCount, level.VTexture, level.HFramebuffer, level.Width, level.Height);
		mBlurShader->BlurVertical(mVBO, blurAmount, sampleCount, level.HTexture, next.VFramebuffer, next.Width, next.Height);
	}

	// Blur and upscale:
	for (int i = FGLRenderBuffers::NumBloomLevels - 1; i > 0; i--)
	{
		const auto &level = mBuffers->BloomLevels[i];
		const auto &next = mBuffers->BloomLevels[i - 1];

		mBlurShader->BlurHorizontal(mVBO, blurAmount, sampleCount, level.VTexture, level.HFramebuffer, level.Width, level.Height);
		mBlurShader->BlurVertical(mVBO, blurAmount, sampleCount, level.HTexture, level.VFramebuffer, level.Width, level.Height);

		// Linear upscale:
		glBindFramebuffer(GL_FRAMEBUFFER, next.VFramebuffer);
		glViewport(0, 0, next.Width, next.Height);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, level.VTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		mBloomCombineShader->Bind();
		mBloomCombineShader->BloomTexture.Set(0);
		{
			FFlatVertex *ptr = mVBO->GetBuffer();
			ptr->Set(-1.0f, -1.0f, 0, 0.0f, 0.0f); ptr++;
			ptr->Set(-1.0f, 1.0f, 0, 0.0f, 1.0f); ptr++;
			ptr->Set(1.0f, -1.0f, 0, 1.0f, 0.0f); ptr++;
			ptr->Set(1.0f, 1.0f, 0, 1.0f, 1.0f); ptr++;
			mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
		}
	}

	mBlurShader->BlurHorizontal(mVBO, blurAmount, sampleCount, level0.VTexture, level0.HFramebuffer, level0.Width, level0.Height);
	mBlurShader->BlurVertical(mVBO, blurAmount, sampleCount, level0.HTexture, level0.VFramebuffer, level0.Width, level0.Height);

	// Add bloom back to scene texture:
	mBuffers->BindSceneTextureFB();
	glViewport(0, 0, mOutputViewport.width, mOutputViewport.height);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, level0.VTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mBloomCombineShader->Bind();
	mBloomCombineShader->BloomTexture.Set(0);
	{
		FFlatVertex *ptr = mVBO->GetBuffer();
		ptr->Set(-1.0f, -1.0f, 0, 0.0f, 0.0f); ptr++;
		ptr->Set(-1.0f, 1.0f, 0, 0.0f, 1.0f); ptr++;
		ptr->Set(1.0f, -1.0f, 0, 1.0f, 0.0f); ptr++;
		ptr->Set(1.0f, 1.0f, 0, 1.0f, 1.0f); ptr++;
		mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
	}

	if (blendEnabled)
		glEnable(GL_BLEND);
	if (scissorEnabled)
		glEnable(GL_SCISSOR_TEST);
	glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
	glBlendFuncSeparate(blendSrcRgb, blendDestRgb, blendSrcAlpha, blendDestAlpha);
	glUseProgram(currentProgram);
	glBindTexture(GL_TEXTURE_2D, textureBinding);
	if (gl.flags & RFL_SAMPLER_OBJECTS)
		glBindSampler(0, samplerBinding);
	glActiveTexture(activeTex);
}

//-----------------------------------------------------------------------------
//
// Tonemap scene texture and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::TonemapScene()
{
	if (gl_tonemap == 0)
		return;

	GLint activeTex, textureBinding, samplerBinding;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
		glBindSampler(0, 0);
	}

	GLboolean blendEnabled, scissorEnabled;
	glGetBooleanv(GL_BLEND, &blendEnabled);
	glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled);

	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	mBuffers->BindHudFB();
	mBuffers->BindSceneTexture(0);
	mTonemapShader->Bind();
	mTonemapShader->SceneTexture.Set(0);
	mTonemapShader->Exposure.Set(mCameraExposure);
	
	FFlatVertex *ptr = mVBO->GetBuffer();
	ptr->Set(-1.0f, -1.0f, 0, 0.0f, 0.0f); ptr++;
	ptr->Set(-1.0f, 1.0f, 0, 0.0f, 1.0f); ptr++;
	ptr->Set(1.0f, -1.0f, 0, 1.0f, 0.0f); ptr++;
	ptr->Set(1.0f, 1.0f, 0, 1.0f, 1.0f); ptr++;
	mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

	if (blendEnabled)
		glEnable(GL_BLEND);
	if (scissorEnabled)
		glEnable(GL_SCISSOR_TEST);
	glBindTexture(GL_TEXTURE_2D, textureBinding);
	if (gl.flags & RFL_SAMPLER_OBJECTS)
		glBindSampler(0, samplerBinding);
	glActiveTexture(activeTex);
}

//-----------------------------------------------------------------------------
//
// Gamma correct while copying to frame buffer
//
//-----------------------------------------------------------------------------

void FGLRenderer::Flush()
{
	if (FGLRenderBuffers::IsEnabled())
	{
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);

		GLboolean blendEnabled;
		GLint currentProgram;
		GLint activeTex, textureBinding, samplerBinding;
		glGetBooleanv(GL_BLEND, &blendEnabled);
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
		if (gl.flags & RFL_SAMPLER_OBJECTS)
		{
			glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
			glBindSampler(0, 0);
		}

		mBuffers->BindOutputFB();

		// Calculate letterbox
		int clientWidth = framebuffer->GetClientWidth();
		int clientHeight = framebuffer->GetClientHeight();
		float scaleX = clientWidth / (float)mOutputViewport.width;
		float scaleY = clientHeight / (float)mOutputViewport.height;
		float scale = MIN(scaleX, scaleY);
		int width = (int)round(mOutputViewport.width * scale);
		int height = (int)round(mOutputViewport.height * scale);
		int x = (clientWidth - width) / 2;
		int y = (clientHeight - height) / 2;

		// Black bars around the box:
		glViewport(0, 0, clientWidth, clientHeight);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glEnable(GL_SCISSOR_TEST);
		if (y > 0)
		{
			glScissor(0, 0, clientWidth, y);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		if (clientHeight - y - height > 0)
		{
			glScissor(0, y + height, clientWidth, clientHeight - y - height);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		if (x > 0)
		{
			glScissor(0, y, x, height);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		if (clientWidth - x - width > 0)
		{
			glScissor(x + width, y, clientWidth - x - width, height);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		glDisable(GL_SCISSOR_TEST);

		// Present what was rendered:
		glViewport(x, y, width, height);
		glDisable(GL_BLEND);

		mPresentShader->Bind();
		mPresentShader->InputTexture.Set(0);
		if (framebuffer->IsHWGammaActive())
		{
			mPresentShader->Gamma.Set(1.0f);
			mPresentShader->Contrast.Set(1.0f);
			mPresentShader->Brightness.Set(0.0f);
		}
		else
		{
			mPresentShader->Gamma.Set(clamp<float>(Gamma, 0.1f, 4.f));
			mPresentShader->Contrast.Set(clamp<float>(vid_contrast, 0.1f, 3.f));
			mPresentShader->Brightness.Set(clamp<float>(vid_brightness, -0.8f, 0.8f));
		}
		mBuffers->BindHudTexture(0);

		FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(-1.0f, -1.0f, 0, 0.0f, 0.0f); ptr++;
		ptr->Set(-1.0f, 1.0f, 0, 0.0f, 1.0f); ptr++;
		ptr->Set(1.0f, -1.0f, 0, 1.0f, 0.0f); ptr++;
		ptr->Set(1.0f, 1.0f, 0, 1.0f, 1.0f); ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

		if (blendEnabled)
			glEnable(GL_BLEND);
		glUseProgram(currentProgram);
		glBindTexture(GL_TEXTURE_2D, textureBinding);
		if (gl.flags & RFL_SAMPLER_OBJECTS)
			glBindSampler(0, samplerBinding);
		glActiveTexture(activeTex);
	}
}
