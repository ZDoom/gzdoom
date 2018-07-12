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

#include "gl_load/gl_system.h"
#include "gi.h"
#include "m_png.h"
#include "r_utility.h"
#include "d_player.h"
#include "gl/system/gl_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "hwrenderer/postprocessing/hw_ambientshader.h"
#include "hwrenderer/postprocessing/hw_bloomshader.h"
#include "hwrenderer/postprocessing/hw_blurshader.h"
#include "hwrenderer/postprocessing/hw_tonemapshader.h"
#include "hwrenderer/postprocessing/hw_colormapshader.h"
#include "hwrenderer/postprocessing/hw_lensshader.h"
#include "hwrenderer/postprocessing/hw_fxaashader.h"
#include "hwrenderer/postprocessing/hw_presentshader.h"
#include "gl/shaders/gl_postprocessshaderinstance.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/textures/gl_hwtexture.h"
#include "r_videoscale.h"

void FGLRenderer::RenderScreenQuad()
{
	mVBO->BindVBO();
	gl_RenderState.ResetVertexBuffer();
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, FFlatVertexBuffer::PRESENT_INDEX, 4);
}

void FGLRenderer::PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	mBuffers->BlitSceneToTexture();
	UpdateCameraExposure();
	mCustomPostProcessShaders->Run("beforebloom");
	BloomScene(fixedcm);
	afterBloomDrawEndScene2D();
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

	const auto &mSceneViewport = screen->mSceneViewport;
	const auto &mScreenViewport = screen->mScreenViewport;

	float sceneScaleX = mSceneViewport.width / (float)mScreenViewport.width;
	float sceneScaleY = mSceneViewport.height / (float)mScreenViewport.height;
	float sceneOffsetX = mSceneViewport.left / (float)mScreenViewport.width;
	float sceneOffsetY = mSceneViewport.top / (float)mScreenViewport.height;

	int randomTexture = clamp(gl_ssao - 1, 0, FGLRenderBuffers::NumAmbientRandomTextures - 1);

	// Calculate linear depth values
	mBuffers->LinearDepthFB.Bind();
	glViewport(0, 0, mBuffers->AmbientWidth, mBuffers->AmbientHeight);
	mBuffers->BindSceneDepthTexture(0);
	mBuffers->BindSceneColorTexture(1);
	mLinearDepthShader->Bind(NOQUEUE);
	if (gl_multisample > 1) mLinearDepthShader->Uniforms->SampleIndex = 0;
	mLinearDepthShader->Uniforms->LinearizeDepthA = 1.0f / GetZFar() - 1.0f / GetZNear();
	mLinearDepthShader->Uniforms->LinearizeDepthB = MAX(1.0f / GetZNear(), 1.e-8f);
	mLinearDepthShader->Uniforms->InverseDepthRangeA = 1.0f;
	mLinearDepthShader->Uniforms->InverseDepthRangeB = 0.0f;
	mLinearDepthShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mLinearDepthShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mLinearDepthShader->Uniforms.Set();
	RenderScreenQuad();

	// Apply ambient occlusion
	mBuffers->AmbientFB1.Bind();
	mBuffers->LinearDepthTexture.Bind(0);
	mBuffers->AmbientRandomTexture[randomTexture].Bind(2, GL_NEAREST, GL_REPEAT);
	mBuffers->BindSceneNormalTexture(1);
	mSSAOShader->Bind(NOQUEUE);
	if (gl_multisample > 1) mSSAOShader->Uniforms->SampleIndex = 0;
	mSSAOShader->Uniforms->UVToViewA = { 2.0f * invFocalLenX, 2.0f * invFocalLenY };
	mSSAOShader->Uniforms->UVToViewB = { -invFocalLenX, -invFocalLenY };
	mSSAOShader->Uniforms->InvFullResolution = { 1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight };
	mSSAOShader->Uniforms->NDotVBias = nDotVBias;
	mSSAOShader->Uniforms->NegInvR2 = -1.0f / r2;
	mSSAOShader->Uniforms->RadiusToScreen = aoRadius * 0.5 / tanHalfFovy * mBuffers->AmbientHeight;
	mSSAOShader->Uniforms->AOMultiplier = 1.0f / (1.0f - nDotVBias);
	mSSAOShader->Uniforms->AOStrength = aoStrength;
	mSSAOShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mSSAOShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mSSAOShader->Uniforms.Set();
	RenderScreenQuad();

	// Blur SSAO texture
	if (gl_ssao_debug < 2)
	{
		mBuffers->AmbientFB0.Bind();
		mBuffers->AmbientTexture1.Bind(0);
		mDepthBlurShader->Bind(NOQUEUE, false);
		mDepthBlurShader->Uniforms[false]->BlurSharpness = blurSharpness;
		mDepthBlurShader->Uniforms[false]->InvFullResolution = { 1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight };
		mDepthBlurShader->Uniforms[false].Set();
		RenderScreenQuad();

		mBuffers->AmbientFB1.Bind();
		mBuffers->AmbientTexture0.Bind(0);
		mDepthBlurShader->Bind(NOQUEUE, true);
		mDepthBlurShader->Uniforms[true]->BlurSharpness = blurSharpness;
		mDepthBlurShader->Uniforms[true]->InvFullResolution = { 1.0f / mBuffers->AmbientWidth, 1.0f / mBuffers->AmbientHeight };
		mDepthBlurShader->Uniforms[true]->PowExponent = gl_ssao_exponent;
		mDepthBlurShader->Uniforms[true].Set();
		RenderScreenQuad();
	}

	// Add SSAO back to scene texture:
	mBuffers->BindSceneFB(false);
	glViewport(screen->mSceneViewport.left, screen->mSceneViewport.top, screen->mSceneViewport.width, screen->mSceneViewport.height);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (gl_ssao_debug != 0)
	{
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	mBuffers->AmbientTexture1.Bind(0, GL_LINEAR);
	mBuffers->BindSceneFogTexture(1);
	mSSAOCombineShader->Bind(NOQUEUE);
	if (gl_multisample > 1) mSSAOCombineShader->Uniforms->SampleCount = gl_multisample;
	mSSAOCombineShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mSSAOCombineShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mSSAOCombineShader->Uniforms.Set();
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

	const auto &mSceneViewport = screen->mSceneViewport;
	const auto &mScreenViewport = screen->mScreenViewport;

	// Extract light level from scene texture:
	auto &level0 = mBuffers->ExposureLevels[0];
	level0.Framebuffer.Bind();
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindCurrentTexture(0, GL_LINEAR);
	mExposureExtractShader->Bind(NOQUEUE);
	mExposureExtractShader->Uniforms->Scale = { mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height };
	mExposureExtractShader->Uniforms->Offset = { mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height };
	mExposureExtractShader->Uniforms.Set();
	RenderScreenQuad();

	// Find the average value:
	for (unsigned int i = 0; i + 1 < mBuffers->ExposureLevels.Size(); i++)
	{
		auto &level = mBuffers->ExposureLevels[i];
		auto &next = mBuffers->ExposureLevels[i + 1];

		next.Framebuffer.Bind();
		glViewport(0, 0, next.Width, next.Height);
		level.Texture.Bind(0);
		mExposureAverageShader->Bind(NOQUEUE);
		RenderScreenQuad();
	}

	// Combine average value with current camera exposure:
	mBuffers->ExposureFB.Bind();
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
	mBuffers->ExposureLevels.Last().Texture.Bind(0);
	mExposureCombineShader->Bind(NOQUEUE);
	mExposureCombineShader->Uniforms->ExposureBase = gl_exposure_base;
	mExposureCombineShader->Uniforms->ExposureMin = gl_exposure_min;
	mExposureCombineShader->Uniforms->ExposureScale = gl_exposure_scale;
	mExposureCombineShader->Uniforms->ExposureSpeed = gl_exposure_speed;
	mExposureCombineShader->Uniforms.Set();
	RenderScreenQuad();
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);

	FGLDebug::PopGroup();
}

//-----------------------------------------------------------------------------
//
// Adds bloom contribution to scene texture
//
//-----------------------------------------------------------------------------

static float ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / sqrtf(2 * (float)M_PI * theta)) * expf(-(n * n) / (2.0f * theta * theta)));
}

static void ComputeBlurSamples(int sampleCount, float blurAmount, float *sampleWeights)
{
	sampleWeights[0] = ComputeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}

static void RenderBlur(FGLRenderer *renderer, float blurAmount, PPTexture input, PPFrameBuffer output, int width, int height, bool vertical)
{
	ComputeBlurSamples(7, blurAmount, renderer->mBlurShader->Uniforms[vertical]->SampleWeights);

	renderer->mBlurShader->Bind(NOQUEUE, vertical);
	renderer->mBlurShader->Uniforms[vertical].Set(POSTPROCESS_BINDINGPOINT);

	input.Bind(0);
	output.Bind();
	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);
	renderer->RenderScreenQuad();
}

void FGLRenderer::BloomScene(int fixedcm)
{
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || fixedcm != CM_DEFAULT || gl_ssao_debug)
		return;

	FGLDebug::PushGroup("BloomScene");

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	const float blurAmount = gl_bloom_amount;

	auto &level0 = mBuffers->BloomLevels[0];

	const auto &mSceneViewport = screen->mSceneViewport;
	const auto &mScreenViewport = screen->mScreenViewport;

	// Extract blooming pixels from scene texture:
	level0.VFramebuffer.Bind();
	glViewport(0, 0, level0.Width, level0.Height);
	mBuffers->BindCurrentTexture(0, GL_LINEAR);
	mBuffers->ExposureTexture.Bind(1);
	mBloomExtractShader->Bind(NOQUEUE);
	mBloomExtractShader->Uniforms->Scale = { mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height };
	mBloomExtractShader->Uniforms->Offset = { mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height };
	mBloomExtractShader->Uniforms.Set();
	RenderScreenQuad();

	// Blur and downscale:
	for (int i = 0; i < FGLRenderBuffers::NumBloomLevels - 1; i++)
	{
		auto &level = mBuffers->BloomLevels[i];
		auto &next = mBuffers->BloomLevels[i + 1];
		RenderBlur(this, blurAmount, level.VTexture, level.HFramebuffer, level.Width, level.Height, false);
		RenderBlur(this, blurAmount, level.HTexture, next.VFramebuffer, next.Width, next.Height, true);
	}

	// Blur and upscale:
	for (int i = FGLRenderBuffers::NumBloomLevels - 1; i > 0; i--)
	{
		auto &level = mBuffers->BloomLevels[i];
		auto &next = mBuffers->BloomLevels[i - 1];

		RenderBlur(this, blurAmount, level.VTexture, level.HFramebuffer, level.Width, level.Height, false);
		RenderBlur(this, blurAmount, level.HTexture, level.VFramebuffer, level.Width, level.Height, true);

		// Linear upscale:
		next.VFramebuffer.Bind();
		glViewport(0, 0, next.Width, next.Height);
		level.VTexture.Bind(0, GL_LINEAR);
		mBloomCombineShader->Bind(NOQUEUE);
		RenderScreenQuad();
	}

	RenderBlur(this, blurAmount, level0.VTexture, level0.HFramebuffer, level0.Width, level0.Height, false);
	RenderBlur(this, blurAmount, level0.HTexture, level0.VFramebuffer, level0.Width, level0.Height, true);

	// Add bloom back to scene texture:
	mBuffers->BindCurrentFB();
	glViewport(mSceneViewport.left, mSceneViewport.top, mSceneViewport.width, mSceneViewport.height);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	level0.VTexture.Bind(0, GL_LINEAR);
	mBloomCombineShader->Bind(NOQUEUE);
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

	int numLevels = 3; // Must be 4 or less (since FGLRenderBuffers::NumBloomLevels is 4 and we are using its buffers).
	assert(numLevels <= FGLRenderBuffers::NumBloomLevels);

	const auto &viewport = screen->mScreenViewport; // The area we want to blur. Could also be mSceneViewport if only the scene area is to be blured

	const auto &level0 = mBuffers->BloomLevels[0];

	// Grab the area we want to bloom:
	mBuffers->BlitLinear(mBuffers->GetCurrentFB(), level0.VFramebuffer, viewport.left, viewport.top, viewport.width, viewport.height, 0, 0, level0.Width, level0.Height);

	// Blur and downscale:
	for (int i = 0; i < numLevels - 1; i++)
	{
		auto &level = mBuffers->BloomLevels[i];
		auto &next = mBuffers->BloomLevels[i + 1];
		RenderBlur(this, blurAmount, level.VTexture, level.HFramebuffer, level.Width, level.Height, false);
		RenderBlur(this, blurAmount, level.HTexture, next.VFramebuffer, next.Width, next.Height, true);
	}

	// Blur and upscale:
	for (int i = numLevels - 1; i > 0; i--)
	{
		auto &level = mBuffers->BloomLevels[i];
		auto &next = mBuffers->BloomLevels[i - 1];

		RenderBlur(this, blurAmount, level.VTexture, level.HFramebuffer, level.Width, level.Height, false);
		RenderBlur(this, blurAmount, level.HTexture, level.VFramebuffer, level.Width, level.Height, true);

		// Linear upscale:
		next.VFramebuffer.Bind();
		glViewport(0, 0, next.Width, next.Height);
		level.VTexture.Bind(0, GL_LINEAR);
		mBloomCombineShader->Bind(NOQUEUE);
		RenderScreenQuad();
	}

	RenderBlur(this, blurAmount, level0.VTexture, level0.HFramebuffer, level0.Width, level0.Height, false);
	RenderBlur(this, blurAmount, level0.HTexture, level0.VFramebuffer, level0.Width, level0.Height, true);

	// Copy blur back to scene texture:
	mBuffers->BlitLinear(level0.VFramebuffer, mBuffers->GetCurrentFB(), 0, 0, level0.Width, level0.Height, viewport.left, viewport.top, viewport.width, viewport.height);

	glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

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
	mTonemapShader->Bind(NOQUEUE);

	if (mTonemapShader->IsPaletteMode())
	{
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, mTonemapPalette->GetTextureHandle(0));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glActiveTexture(GL_TEXTURE0);
	}
	else
	{
		mBuffers->ExposureTexture.Bind(1);
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
					PalEntry color = GPalette.BaseColors[(uint8_t)PTM_BestColor((uint32_t *)GPalette.BaseColors, (r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4), 
						gl_paltonemap_reverselookup, gl_paltonemap_powtable, 0, 256)];
					int index = ((r * 64 + g) * 64 + b) * 4;
					lut[index] = color.b;
					lut[index + 1] = color.g;
					lut[index + 2] = color.r;
					lut[index + 3] = 255;
				}
			}
		}

		mTonemapPalette = new FHardwareTexture(true);
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
	mColormapShader->Bind(NOQUEUE);
	
	FSpecialColormap *scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];
	float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
		scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

	mColormapShader->Uniforms->MapStart = { scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], 0.f };
	mColormapShader->Uniforms->MapRange = m;
	mColormapShader->Uniforms.Set();

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

	float aspect = screen->mSceneViewport.width / (float)screen->mSceneViewport.height;

	// Scale factor to keep sampling within the input texture
	float r2 = aspect * aspect * 0.25 + 0.25f;
	float sqrt_r2 = sqrt(r2);
	float f0 = 1.0f + MAX(r2 * (k[0] + kcube[0] * sqrt_r2), 0.0f);
	float f2 = 1.0f + MAX(r2 * (k[2] + kcube[2] * sqrt_r2), 0.0f);
	float f = MAX(f0, f2);
	float scale = 1.0f / f;

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0, GL_LINEAR);
	mLensShader->Bind(NOQUEUE);
	mLensShader->Uniforms->AspectRatio = aspect;
	mLensShader->Uniforms->Scale = scale;
	mLensShader->Uniforms->LensDistortionCoefficient = k;
	mLensShader->Uniforms->CubicDistortionValue = kcube;
	mLensShader->Uniforms.Set();
	RenderScreenQuad();
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

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mFXAALumaShader->Bind(NOQUEUE);
	RenderScreenQuad();
	mBuffers->NextTexture();

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0, GL_LINEAR);
	mFXAAShader->Bind(NOQUEUE);
	mFXAAShader->Uniforms->ReciprocalResolution = { 1.0f / mBuffers->GetWidth(), 1.0f / mBuffers->GetHeight() };
	mFXAAShader->Uniforms.Set();
	RenderScreenQuad();
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
	const auto &mSceneViewport = screen->mSceneViewport;
	const auto &mScreenViewport = screen->mScreenViewport;

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
			screen->Draw2D();
			FGLDebug::PopGroup();
		}
		screen->Clear2D();

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

void FGLRenderer::CopyToBackbuffer(const IntRect *bounds, bool applyGamma)
{
	screen->Draw2D();	// draw all pending 2D stuff before copying the buffer
	screen->Clear2D();

	mCustomPostProcessShaders->Run("screen");

	FGLDebug::PushGroup("CopyToBackbuffer");
	if (FGLRenderBuffers::IsEnabled())
	{
		FGLPostProcessState savedState;
		mBuffers->BindOutputFB();

		IntRect box;
		if (bounds)
		{
			box = *bounds;
		}
		else
		{
			ClearBorders();
			box = screen->mOutputLetterbox;
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

void FGLRenderer::DrawPresentTexture(const IntRect &box, bool applyGamma)
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

	mPresentShader->Bind(NOQUEUE);
	if (!applyGamma || framebuffer->IsHWGammaActive())
	{
		mPresentShader->Uniforms->InvGamma = 1.0f;
		mPresentShader->Uniforms->Contrast = 1.0f;
		mPresentShader->Uniforms->Brightness = 0.0f;
		mPresentShader->Uniforms->Saturation = 1.0f;
	}
	else
	{
		mPresentShader->Uniforms->InvGamma = 1.0f / clamp<float>(Gamma, 0.1f, 4.f);
		mPresentShader->Uniforms->Contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		mPresentShader->Uniforms->Brightness = clamp<float>(vid_brightness, -0.8f, 0.8f);
		mPresentShader->Uniforms->Saturation = clamp<float>(vid_saturation, -15.0f, 15.f);
		mPresentShader->Uniforms->GrayFormula = static_cast<int>(gl_satformula);
	}
	mPresentShader->Uniforms->Scale = { screen->mScreenViewport.width / (float)mBuffers->GetWidth(), screen->mScreenViewport.height / (float)mBuffers->GetHeight() };
	mPresentShader->Uniforms.Set();
	RenderScreenQuad();
}

//-----------------------------------------------------------------------------
//
// Fills the black bars around the screen letterbox
//
//-----------------------------------------------------------------------------

void FGLRenderer::ClearBorders()
{
	const auto &box = screen->mOutputLetterbox;

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

