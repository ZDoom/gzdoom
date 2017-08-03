// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Magnus Norddahl
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
** gl_postprocess.cpp
** Post processing effects in the render pipeline
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
#include "p_local.h"
#include "colormatcher.h"
#include "gl/gl_functions.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/shaders/gl_ambientshader.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_colormapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_fxaashader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/shaders/gl_postprocessshader.h"
#include "gl/renderer/gl_2ddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "r_videoscale.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_bloom, false, CVAR_ARCHIVE);
CUSTOM_CVAR(Float, gl_bloom_amount, 1.4f, CVAR_ARCHIVE)
{
	if (self < 0.1f) self = 0.1f;
}

CVAR(Float, gl_exposure_scale, 1.3f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_min, 0.35f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_base, 0.35f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_speed, 0.05f, CVAR_ARCHIVE)

CUSTOM_CVAR(Int, gl_tonemap, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 5)
		self = 0;
}

CUSTOM_CVAR(Int, gl_bloom_kernel_size, 7, CVAR_ARCHIVE)
{
	if (self < 3 || self > 15 || self % 2 == 0)
		self = 7;
}

CVAR(Bool, gl_lens, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Float, gl_lens_k, -0.12f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_lens_kcube, 0.1f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_lens_chromatic, 1.12f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, gl_fxaa, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self >= FFXAAShader::Count)
	{
		self = 0;
	}
}

CUSTOM_CVAR(Int, gl_ssao, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 3)
		self = 0;
}

CUSTOM_CVAR(Int, gl_ssao_portals, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
}

CVAR(Float, gl_ssao_strength, 0.7, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, gl_ssao_debug, 0, 0)
CVAR(Float, gl_ssao_bias, 0.2f, 0)
CVAR(Float, gl_ssao_radius, 80.0f, 0)
CUSTOM_CVAR(Float, gl_ssao_blur, 16.0f, 0)
{
	if (self < 0.1f) self = 0.1f;
}

CUSTOM_CVAR(Float, gl_ssao_exponent, 1.8f, 0)
{
	if (self < 0.1f) self = 0.1f;
}

CUSTOM_CVAR(Float, gl_paltonemap_powtable, 2.0f, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (GLRenderer)
		GLRenderer->ClearTonemapPalette();
}

CUSTOM_CVAR(Bool, gl_paltonemap_reverselookup, true, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (GLRenderer)
		GLRenderer->ClearTonemapPalette();
}

CVAR(Float, gl_menu_blur, -1.0f, CVAR_ARCHIVE)

EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)
EXTERN_CVAR(Float, vid_saturation)
EXTERN_CVAR(Int, gl_satformula)

void FGLRenderer::RenderScreenQuad()
{
	mVBO->BindVBO();
	gl_RenderState.ResetVertexBuffer();
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, FFlatVertexBuffer::PRESENT_INDEX, 4);
}

void FGLRenderer::PostProcessScene(int fixedcm)
{
	mBuffers->BlitSceneToTexture();
	UpdateCameraExposure();
	mCustomPostProcessShaders->Run("beforebloom");
	BloomScene(fixedcm);
	TonemapScene();
	ColormapScene(fixedcm);
	LensDistortScene();
	ApplyFXAA();
	mCustomPostProcessShaders->Run("scene");
}

//-----------------------------------------------------------------------------
//
// Adds ambient occlusion to the scene
//
//-----------------------------------------------------------------------------

void FGLRenderer::AmbientOccludeScene()
{
	FGLDebug::PushGroup("AmbientOccludeScene");

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(3);

	float bias = gl_ssao_bias;
	float aoRadius = gl_ssao_radius;
	const float blurAmount = gl_ssao_blur;
	float aoStrength = gl_ssao_strength;

	//float tanHalfFovy = tan(fovy * (M_PI / 360.0f));
	float tanHalfFovy = 1.0f / gl_RenderState.mProjectionMatrix.get()[5];
	float invFocalLenX = tanHalfFovy * (mBuffers->GetSceneWidth() / (float)mBuffers->GetSceneHeight());
	float invFocalLenY = tanHalfFovy;
	float nDotVBias = clamp(bias, 0.0f, 1.0f);
	float r2 = aoRadius * aoRadius;

	float blurSharpness = 1.0f / blurAmount;

	float sceneScaleX = mSceneViewport.width / (float)mScreenViewport.width;
	float sceneScaleY = mSceneViewport.height / (float)mScreenViewport.height;
	float sceneOffsetX = mSceneViewport.left / (float)mScreenViewport.width;
	float sceneOffsetY = mSceneViewport.top / (float)mScreenViewport.height;

	int randomTexture = clamp(gl_ssao - 1, 0, FGLRenderBuffers::NumAmbientRandomTextures - 1);

	// Calculate linear depth values
	glBindFramebuffer(GL_FRAMEBUFFER, mBuffers->LinearDepthFB);
	glViewport(0, 0, mBuffers->AmbientWidth, mBuffers->AmbientHeight);
	mBuffers->BindSceneDepthTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	mBuffers->BindSceneColorTexture(1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glActiveTexture(GL_TEXTURE0);
	mLinearDepthShader->Bind();
	mLinearDepthShader->DepthTexture.Set(0);
	mLinearDepthShader->ColorTexture.Set(1);
	if (gl_multisample > 1) mLinearDepthShader->SampleIndex.Set(0);
	mLinearDepthShader->LinearizeDepthA.Set(1.0f / GetZFar() - 1.0f / GetZNear());
	mLinearDepthShader->LinearizeDepthB.Set(MAX(1.0f / GetZNear(), 1.e-8f));
	mLinearDepthShader->InverseDepthRangeA.Set(1.0f);
	mLinearDepthShader->InverseDepthRangeB.Set(0.0f);
	mLinearDepthShader->Scale.Set(sceneScaleX, sceneScaleY);
	mLinearDepthShader->Offset.Set(sceneOffsetX, sceneOffsetY);
	RenderScreenQuad();

	// Apply ambient occlusion
	glBindFramebuffer(GL_FRAMEBUFFER, mBuffers->AmbientFB1);
	glBindTexture(GL_TEXTURE_2D, mBuffers->LinearDepthTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mBuffers->AmbientRandomTexture[randomTexture]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	mBuffers->BindSceneNormalTexture(2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glActiveTexture(GL_TEXTURE0);
	mSSAOShader->Bind();
	mSSAOShader->DepthTexture.Set(0);
	mSSAOShader->RandomTexture.Set(1);
	mSSAOShader->NormalTexture.Set(2);
	if (gl_multisample > 1) mSSAOShader->SampleIndex.Set(0);
	mSSAOShader->UVToViewA.Set(2.0f * invFocalLenX, 2.0f * invFocalLenY);
	mSSAOShader->UVToViewB.Set(-invFocalLenX, -invFocalLenY);
	mSSAOShader->InvFullResolution.Set(1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight);
	mSSAOShader->NDotVBias.Set(nDotVBias);
	mSSAOShader->NegInvR2.Set(-1.0f / r2);
	mSSAOShader->RadiusToScreen.Set(aoRadius * 0.5 / tanHalfFovy * mBuffers->AmbientHeight);
	mSSAOShader->AOMultiplier.Set(1.0f / (1.0f - nDotVBias));
	mSSAOShader->AOStrength.Set(aoStrength);
	mSSAOShader->Scale.Set(sceneScaleX, sceneScaleY);
	mSSAOShader->Offset.Set(sceneOffsetX, sceneOffsetY);
	RenderScreenQuad();

	// Blur SSAO texture
	if (gl_ssao_debug < 2)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mBuffers->AmbientFB0);
		glBindTexture(GL_TEXTURE_2D, mBuffers->AmbientTexture1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		mDepthBlurShader->Bind(false);
		mDepthBlurShader->BlurSharpness[false].Set(blurSharpness);
		mDepthBlurShader->InvFullResolution[false].Set(1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight);
		RenderScreenQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, mBuffers->AmbientFB1);
		glBindTexture(GL_TEXTURE_2D, mBuffers->AmbientTexture0);
		mDepthBlurShader->Bind(true);
		mDepthBlurShader->BlurSharpness[true].Set(blurSharpness);
		mDepthBlurShader->InvFullResolution[true].Set(1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight);
		mDepthBlurShader->PowExponent[true].Set(gl_ssao_exponent);
		RenderScreenQuad();
	}

	// Add SSAO back to scene texture:
	mBuffers->BindSceneFB(false);
	glViewport(mSceneViewport.left, mSceneViewport.top, mSceneViewport.width, mSceneViewport.height);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (gl_ssao_debug != 0)
	{
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mBuffers->AmbientTexture1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mBuffers->BindSceneFogTexture(1);
	mSSAOCombineShader->Bind();
	mSSAOCombineShader->AODepthTexture.Set(0);
	mSSAOCombineShader->SceneFogTexture.Set(1);
	if (gl_multisample > 1) mSSAOCombineShader->SampleCount.Set(gl_multisample);
	mSSAOCombineShader->Scale.Set(sceneScaleX, sceneScaleY);
	mSSAOCombineShader->Offset.Set(sceneOffsetX, sceneOffsetY);
	RenderScreenQuad();

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Extracts light average from the scene and updates the camera exposure texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::UpdateCameraExposure()
{
	if (!gl_bloom && gl_tonemap == 0)
		return;

	FGLDebug::PushGroup("UpdateCameraExposure");

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	// Extract light level from scene texture:
	const auto &level0 = mBuffers->ExposureLevels[0];
	glBindFramebuffer(GL_FRAMEBUFFER, level0.Framebuffer);
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mExposureExtractShader->Bind();
	mExposureExtractShader->SceneTexture.Set(0);
	mExposureExtractShader->Scale.Set(mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height);
	mExposureExtractShader->Offset.Set(mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height);
	RenderScreenQuad();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Find the average value:
	for (unsigned int i = 0; i + 1 < mBuffers->ExposureLevels.Size(); i++)
	{
		const auto &level = mBuffers->ExposureLevels[i];
		const auto &next = mBuffers->ExposureLevels[i + 1];

		glBindFramebuffer(GL_FRAMEBUFFER, next.Framebuffer);
		glViewport(0, 0, next.Width, next.Height);
		glBindTexture(GL_TEXTURE_2D, level.Texture);
		mExposureAverageShader->Bind();
		mExposureAverageShader->ExposureTexture.Set(0);
		RenderScreenQuad();
	}

	// Combine average value with current camera exposure:
	glBindFramebuffer(GL_FRAMEBUFFER, mBuffers->ExposureFB);
	glViewport(0, 0, 1, 1);
	if (!mBuffers->FirstExposureFrame)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		mBuffers->FirstExposureFrame = false;
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mBuffers->ExposureLevels.Last().Texture);
	mExposureCombineShader->Bind();
	mExposureCombineShader->ExposureTexture.Set(0);
	mExposureCombineShader->ExposureBase.Set(gl_exposure_base);
	mExposureCombineShader->ExposureMin.Set(gl_exposure_min);
	mExposureCombineShader->ExposureScale.Set(gl_exposure_scale);
	mExposureCombineShader->ExposureSpeed.Set(gl_exposure_speed);
	RenderScreenQuad();
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Adds bloom contribution to scene texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::BloomScene(int fixedcm)
{
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || fixedcm != CM_DEFAULT || gl_ssao_debug)
		return;

	FGLDebug::PushGroup("BloomScene");

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	const float blurAmount = gl_bloom_amount;
	int sampleCount = gl_bloom_kernel_size;

	const auto &level0 = mBuffers->BloomLevels[0];

	// Extract blooming pixels from scene texture:
	glBindFramebuffer(GL_FRAMEBUFFER, level0.VFramebuffer);
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mBuffers->ExposureTexture);
	glActiveTexture(GL_TEXTURE0);
	mBloomExtractShader->Bind();
	mBloomExtractShader->SceneTexture.Set(0);
	mBloomExtractShader->ExposureTexture.Set(1);
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

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Blur the scene
//
//-----------------------------------------------------------------------------

void FGLRenderer::BlurScene(float gameinfobluramount)
{
	// first, respect the CVar
	float blurAmount = gl_menu_blur;

	// if CVar is negative, use the gameinfo entry
	if (gl_menu_blur < 0)
		blurAmount = gameinfobluramount;

	// if blurAmount == 0 or somehow still returns negative, exit to prevent a crash, clearly we don't want this
	if ((blurAmount <= 0.0) || !FGLRenderBuffers::IsEnabled())
		return;

	FGLDebug::PushGroup("BlurScene");

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	int sampleCount = 9;
	int numLevels = 3; // Must be 4 or less (since FGLRenderBuffers::NumBloomLevels is 4 and we are using its buffers).
	assert(numLevels <= FGLRenderBuffers::NumBloomLevels);

	const auto &viewport = mScreenViewport; // The area we want to blur. Could also be mSceneViewport if only the scene area is to be blured

	const auto &level0 = mBuffers->BloomLevels[0];

	// Grab the area we want to bloom:
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mBuffers->GetCurrentFB());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, level0.VFramebuffer);
	glBlitFramebuffer(viewport.left, viewport.top, viewport.width, viewport.height, 0, 0, level0.Width, level0.Height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	// Blur and downscale:
	for (int i = 0; i < numLevels - 1; i++)
	{
		const auto &level = mBuffers->BloomLevels[i];
		const auto &next = mBuffers->BloomLevels[i + 1];
		mBlurShader->BlurHorizontal(this, blurAmount, sampleCount, level.VTexture, level.HFramebuffer, level.Width, level.Height);
		mBlurShader->BlurVertical(this, blurAmount, sampleCount, level.HTexture, next.VFramebuffer, next.Width, next.Height);
	}

	// Blur and upscale:
	for (int i = numLevels - 1; i > 0; i--)
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

	// Copy blur back to scene texture:
	glBindFramebuffer(GL_READ_FRAMEBUFFER, level0.VFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mBuffers->GetCurrentFB());
	glBlitFramebuffer(0, 0, level0.Width, level0.Height, viewport.left, viewport.top, viewport.width, viewport.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);

	FGLDebug::PopGroup();
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

	FGLDebug::PushGroup("TonemapScene");

	CreateTonemapPalette();

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mTonemapShader->Bind();
	mTonemapShader->SceneTexture.Set(0);

	if (mTonemapShader->IsPaletteMode())
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, mTonemapPalette->GetTextureHandle(0));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glActiveTexture(GL_TEXTURE0);

		mTonemapShader->PaletteLUT.Set(1);
	}
	else
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, mBuffers->ExposureTexture);
		glActiveTexture(GL_TEXTURE0);

		mTonemapShader->ExposureTexture.Set(1);
	}

	RenderScreenQuad();
	mBuffers->NextTexture();

	FGLDebug::PopGroup();
}

void FGLRenderer::CreateTonemapPalette()
{
	if (!mTonemapPalette)
	{
		TArray<unsigned char> lut;
		lut.Resize(512 * 512 * 4);
		for (int r = 0; r < 64; r++)
		{
			for (int g = 0; g < 64; g++)
			{
				for (int b = 0; b < 64; b++)
				{
					PalEntry color = GPalette.BaseColors[(uint8_t)PTM_BestColor((uint32_t *)GPalette.BaseColors, (r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4), 0, 256)];
					int index = ((r * 64 + g) * 64 + b) * 4;
					lut[index] = color.b;
					lut[index + 1] = color.g;
					lut[index + 2] = color.r;
					lut[index + 3] = 255;
				}
			}
		}

		mTonemapPalette = new FHardwareTexture(512, 512, true);
		mTonemapPalette->CreateTexture(&lut[0], 512, 512, 0, false, 0, "mTonemapPalette");
	}
}

void FGLRenderer::ClearTonemapPalette()
{
	if (mTonemapPalette)
	{
		delete mTonemapPalette;
		mTonemapPalette = nullptr;
	}
}

//-----------------------------------------------------------------------------
//
// Colormap scene texture and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::ColormapScene(int fixedcm)
{
	if (fixedcm < CM_FIRSTSPECIALCOLORMAP || fixedcm >= CM_MAXCOLORMAP)
		return;

	FGLDebug::PushGroup("ColormapScene");

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mColormapShader->Bind();
	
	FSpecialColormap *scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];
	float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
		scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

	mColormapShader->MapStart.Set(scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], 0.f);
	mColormapShader->MapRange.Set(m);

	RenderScreenQuad();
	mBuffers->NextTexture();

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Apply lens distortion and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::LensDistortScene()
{
	if (gl_lens == 0)
		return;

	FGLDebug::PushGroup("LensDistortScene");

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

	float aspect = mSceneViewport.width / (float)mSceneViewport.height;

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

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Apply FXAA and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::ApplyFXAA()
{
	if (0 == gl_fxaa)
	{
		return;
	}

	FGLDebug::PushGroup("ApplyFXAA");

	const GLfloat rpcRes[2] =
	{
		1.0f / mBuffers->GetWidth(),
		1.0f / mBuffers->GetHeight()
	};

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mFXAALumaShader->Bind();
	mFXAALumaShader->InputTexture.Set(0);
	RenderScreenQuad();
	mBuffers->NextTexture();

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mFXAAShader->Bind();
	mFXAAShader->InputTexture.Set(0);
	mFXAAShader->ReciprocalResolution.Set(rpcRes);
	RenderScreenQuad();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	mBuffers->NextTexture();

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Copies the rendered screen to its final destination
//
//-----------------------------------------------------------------------------

void FGLRenderer::Flush()
{
	const s3d::Stereo3DMode& stereo3dMode = s3d::Stereo3DMode::getCurrentMode();

	if (stereo3dMode.IsMono() || !FGLRenderBuffers::IsEnabled())
	{
		CopyToBackbuffer(nullptr, true);
	}
	else
	{
		// Render 2D to eye textures
		for (int eye_ix = 0; eye_ix < stereo3dMode.eye_count(); ++eye_ix)
		{
			FGLDebug::PushGroup("Eye2D");
			mBuffers->BindEyeFB(eye_ix);
			glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
			glScissor(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
			m2DDrawer->Draw();
			FGLDebug::PopGroup();
		}
		m2DDrawer->Clear();

		FGLPostProcessState savedState;
		FGLDebug::PushGroup("PresentEyes");
		stereo3dMode.Present();
		FGLDebug::PopGroup();
	}
}

//-----------------------------------------------------------------------------
//
// Gamma correct while copying to frame buffer
//
//-----------------------------------------------------------------------------

void FGLRenderer::CopyToBackbuffer(const GL_IRECT *bounds, bool applyGamma)
{
	m2DDrawer->Draw();	// draw all pending 2D stuff before copying the buffer
	m2DDrawer->Clear();

	mCustomPostProcessShaders->Run("screen");

	FGLDebug::PushGroup("CopyToBackbuffer");
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

		mBuffers->BindCurrentTexture(0);
		DrawPresentTexture(box, applyGamma);
	}
	else if (!bounds)
	{
		FGLPostProcessState savedState;
		ClearBorders();
	}
	FGLDebug::PopGroup();
}

void FGLRenderer::DrawPresentTexture(const GL_IRECT &box, bool applyGamma)
{
	glViewport(box.left, box.top, box.width, box.height);

	glActiveTexture(GL_TEXTURE0);
	if (ViewportLinearScale())
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	mPresentShader->Bind();
	mPresentShader->InputTexture.Set(0);
	if (!applyGamma || framebuffer->IsHWGammaActive())
	{
		mPresentShader->InvGamma.Set(1.0f);
		mPresentShader->Contrast.Set(1.0f);
		mPresentShader->Brightness.Set(0.0f);
		mPresentShader->Saturation.Set(1.0f);
	}
	else
	{
		mPresentShader->InvGamma.Set(1.0f / clamp<float>(Gamma, 0.1f, 4.f));
		mPresentShader->Contrast.Set(clamp<float>(vid_contrast, 0.1f, 3.f));
		mPresentShader->Brightness.Set(clamp<float>(vid_brightness, -0.8f, 0.8f));
		mPresentShader->Saturation.Set(clamp<float>(vid_saturation, -15.0f, 15.f));
		mPresentShader->GrayFormula.Set(static_cast<int>(gl_satformula));
	}
	mPresentShader->Scale.Set(mScreenViewport.width / (float)mBuffers->GetWidth(), mScreenViewport.height / (float)mBuffers->GetHeight());
	RenderScreenQuad();
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
	if (clientWidth == 0 || clientHeight == 0)
		return;

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


// [SP] Re-implemented BestColor for more precision rather than speed. This function is only ever called once until the game palette is changed.

int FGLRenderer::PTM_BestColor (const uint32_t *pal_in, int r, int g, int b, int first, int num)
{
	const PalEntry *pal = (const PalEntry *)pal_in;
	static double powtable[256];
	static bool firstTime = true;
	static float trackpowtable = 0.;

	double fbestdist, fdist;
	int bestcolor = 0;

	if (firstTime || trackpowtable != gl_paltonemap_powtable)
	{
		trackpowtable = gl_paltonemap_powtable;
		firstTime = false;
		for (int x = 0; x < 256; x++) powtable[x] = pow((double)x/255, (double)gl_paltonemap_powtable);
	}

	for (int color = first; color < num; color++)
	{
		double x = powtable[abs(r-pal[color].r)];
		double y = powtable[abs(g-pal[color].g)];
		double z = powtable[abs(b-pal[color].b)];
		fdist = x + y + z;
		if (color == first || ((gl_paltonemap_reverselookup)?(fdist <= fbestdist):(fdist < fbestdist)))
		{
			if (fdist == 0 && !gl_paltonemap_reverselookup)
				return color;

			fbestdist = fdist;
			bestcolor = color;
		}
	}
	return bestcolor;
}

