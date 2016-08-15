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
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/renderer/gl_2ddrawer.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_bloom, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CUSTOM_CVAR(Float, gl_bloom_amount, 1.4f, 0)
{
	if (self < 0.1f) self = 0.1f;
}

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

CVAR(Bool, gl_lens, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Float, gl_lens_k, -0.12f, 0)
CVAR(Float, gl_lens_kcube, 0.1f, 0)
CVAR(Float, gl_lens_chromatic, 1.12f, 0)

EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)


void FGLRenderer::RenderScreenQuad()
{
	mVBO->BindVBO();
	gl_RenderState.ResetVertexBuffer();
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
}

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

	FGLPostProcessState savedState;

	const float blurAmount = gl_bloom_amount;
	int sampleCount = gl_bloom_kernel_size;

	const auto &level0 = mBuffers->BloomLevels[0];

	// Extract blooming pixels from scene texture:
	glBindFramebuffer(GL_FRAMEBUFFER, level0.VFramebuffer);
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mBloomExtractShader->Bind();
	mBloomExtractShader->SceneTexture.Set(0);
	mBloomExtractShader->Exposure.Set(mCameraExposure);
	mBloomExtractShader->Scale.Set(mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height);
	mBloomExtractShader->Offset.Set(mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height);
	RenderScreenQuad();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Blur and downscale:
	for (int i = 0; i < FGLRenderBuffers::NumBloomLevels - 1; i++)
	{
		const auto &level = mBuffers->BloomLevels[i];
		const auto &next = mBuffers->BloomLevels[i + 1];
		mBlurShader->BlurHorizontal(this, blurAmount, sampleCount, level.VTexture, level.HFramebuffer, level.Width, level.Height);
		mBlurShader->BlurVertical(this, blurAmount, sampleCount, level.HTexture, next.VFramebuffer, next.Width, next.Height);
	}

	// Blur and upscale:
	for (int i = FGLRenderBuffers::NumBloomLevels - 1; i > 0; i--)
	{
		const auto &level = mBuffers->BloomLevels[i];
		const auto &next = mBuffers->BloomLevels[i - 1];

		mBlurShader->BlurHorizontal(this, blurAmount, sampleCount, level.VTexture, level.HFramebuffer, level.Width, level.Height);
		mBlurShader->BlurVertical(this, blurAmount, sampleCount, level.HTexture, level.VFramebuffer, level.Width, level.Height);

		// Linear upscale:
		glBindFramebuffer(GL_FRAMEBUFFER, next.VFramebuffer);
		glViewport(0, 0, next.Width, next.Height);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, level.VTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		mBloomCombineShader->Bind();
		mBloomCombineShader->BloomTexture.Set(0);
		RenderScreenQuad();
	}

	mBlurShader->BlurHorizontal(this, blurAmount, sampleCount, level0.VTexture, level0.HFramebuffer, level0.Width, level0.Height);
	mBlurShader->BlurVertical(this, blurAmount, sampleCount, level0.HTexture, level0.VFramebuffer, level0.Width, level0.Height);

	// Add bloom back to scene texture:
	mBuffers->BindCurrentFB();
	glViewport(mSceneViewport.left, mSceneViewport.top, mSceneViewport.width, mSceneViewport.height);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, level0.VTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mBloomCombineShader->Bind();
	mBloomCombineShader->BloomTexture.Set(0);
	RenderScreenQuad();
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
}

//-----------------------------------------------------------------------------
//
// Tonemap scene texture and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::TonemapScene()
{
	if (gl_tonemap == 0 || !FGLRenderBuffers::IsEnabled())
		return;

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mTonemapShader->Bind();
	mTonemapShader->SceneTexture.Set(0);
	mTonemapShader->Exposure.Set(mCameraExposure);
	RenderScreenQuad();
	mBuffers->NextTexture();
}

//-----------------------------------------------------------------------------
//
// Apply lens distortion and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::LensDistortScene()
{
	if (gl_lens == 0 || !FGLRenderBuffers::IsEnabled())
		return;

	float k[4] =
	{
		gl_lens_k,
		gl_lens_k * gl_lens_chromatic,
		gl_lens_k * gl_lens_chromatic * gl_lens_chromatic,
		0.0f
	};
	float kcube[4] =
	{
		gl_lens_kcube,
		gl_lens_kcube * gl_lens_chromatic,
		gl_lens_kcube * gl_lens_chromatic * gl_lens_chromatic,
		0.0f
	};

	float aspect = mSceneViewport.width / mSceneViewport.height;

	// Scale factor to keep sampling within the input texture
	float r2 = aspect * aspect * 0.25 + 0.25f;
	float sqrt_r2 = sqrt(r2);
	float f0 = 1.0f + MAX(r2 * (k[0] + kcube[0] * sqrt_r2), 0.0f);
	float f2 = 1.0f + MAX(r2 * (k[2] + kcube[2] * sqrt_r2), 0.0f);
	float f = MAX(f0, f2);
	float scale = 1.0f / f;

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mLensShader->Bind();
	mLensShader->InputTexture.Set(0);
	mLensShader->AspectRatio.Set(aspect);
	mLensShader->Scale.Set(scale);
	mLensShader->LensDistortionCoefficient.Set(k);
	mLensShader->CubicDistortionValue.Set(kcube);
	RenderScreenQuad();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	mBuffers->NextTexture();
}

//-----------------------------------------------------------------------------
//
// Gamma correct while copying to frame buffer
//
//-----------------------------------------------------------------------------

void FGLRenderer::CopyToBackbuffer(const GL_IRECT *bounds, bool applyGamma)
{
	m2DDrawer->Flush();	// draw all pending 2D stuff before copying the buffer
	if (FGLRenderBuffers::IsEnabled())
	{
		FGLPostProcessState savedState;
		mBuffers->BindOutputFB();

		GL_IRECT box;
		if (bounds)
		{
			box = *bounds;
		}
		else
		{
			ClearBorders();
			box = mOutputLetterbox;
		}

		// Present what was rendered:
		glViewport(box.left, box.top, box.width, box.height);

		mPresentShader->Bind();
		mPresentShader->InputTexture.Set(0);
		if (!applyGamma || framebuffer->IsHWGammaActive())
		{
			mPresentShader->InvGamma.Set(1.0f);
			mPresentShader->Contrast.Set(1.0f);
			mPresentShader->Brightness.Set(0.0f);
		}
		else
		{
			mPresentShader->InvGamma.Set(1.0f / clamp<float>(Gamma, 0.1f, 4.f));
			mPresentShader->Contrast.Set(clamp<float>(vid_contrast, 0.1f, 3.f));
			mPresentShader->Brightness.Set(clamp<float>(vid_brightness, -0.8f, 0.8f));
		}
		mPresentShader->Scale.Set(mScreenViewport.width / (float)mBuffers->GetWidth(), mScreenViewport.height / (float)mBuffers->GetHeight());
		mBuffers->BindCurrentTexture(0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		RenderScreenQuad();
	}
	else if (!bounds)
	{
		FGLPostProcessState savedState;
		ClearBorders();
	}
}

//-----------------------------------------------------------------------------
//
// Fills the black bars around the screen letterbox
//
//-----------------------------------------------------------------------------

void FGLRenderer::ClearBorders()
{
	const auto &box = mOutputLetterbox;

	int clientWidth = framebuffer->GetClientWidth();
	int clientHeight = framebuffer->GetClientHeight();

	glViewport(0, 0, clientWidth, clientHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_SCISSOR_TEST);
	if (box.top > 0)
	{
		glScissor(0, 0, clientWidth, box.top);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (clientHeight - box.top - box.height > 0)
	{
		glScissor(0, box.top + box.height, clientWidth, clientHeight - box.top - box.height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (box.left > 0)
	{
		glScissor(0, box.top, box.left, box.height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (clientWidth - box.left - box.width > 0)
	{
		glScissor(box.left + box.width, box.top, clientWidth - box.left - box.width, box.height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_SCISSOR_TEST);
}
