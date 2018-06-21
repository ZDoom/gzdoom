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
#include "hwrenderer/postprocessing/hw_tonemapshader.h"
#include "hwrenderer/postprocessing/hw_colormapshader.h"
#include "hwrenderer/postprocessing/hw_presentshader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
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
	mBuffers->BindCurrentFB();
	afterBloomDrawEndScene2D();
	TonemapScene();
	ColormapScene(fixedcm);
	LensDistortScene();
	ApplyFXAA();
	mCustomPostProcessShaders->Run("scene");
}

void FGLRenderBuffers::RenderEffect(const FString &name)
{
	// Create/update textures (To do: move out of RunEffect)
	{
		TMap<FString, PPTextureDesc>::Iterator it(hw_postprocess.Textures);
		TMap<FString, PPTextureDesc>::Pair *pair;
		while (it.NextPair(pair))
		{
			auto &gltexture = GLTextures[pair->Key];
			auto &glframebuffer = GLTextureFBs[pair->Key];

			int glformat;
			switch (pair->Value.Format)
			{
			default:
			case PixelFormat::Rgba8: glformat = GL_RGBA8; break;
			case PixelFormat::Rgba16f: glformat = GL_RGBA16F; break;
			case PixelFormat::R32f: glformat = GL_R32F; break;
			}

			if (gltexture && (gltexture.Width != pair->Value.Width || gltexture.Height != pair->Value.Height))
			{
				glDeleteTextures(1, &gltexture.handle);
				glDeleteFramebuffers(1, &glframebuffer.handle);
				gltexture.handle = 0;
				glframebuffer.handle = 0;
			}

			if (!gltexture)
			{
				gltexture = Create2DTexture(name.GetChars(), glformat, pair->Value.Width, pair->Value.Height);
				gltexture.Width = pair->Value.Width;
				gltexture.Height = pair->Value.Height;
				glframebuffer = CreateFrameBuffer(name.GetChars(), gltexture);
			}
		}
	}

	// Compile shaders (To do: move out of RunEffect)
	{
		TMap<FString, PPShader>::Iterator it(hw_postprocess.Shaders);
		TMap<FString, PPShader>::Pair *pair;
		while (it.NextPair(pair))
		{
			const auto &desc = pair->Value;
			auto &glshader = GLShaders[pair->Key];
			if (!glshader)
			{
				glshader = std::make_unique<FShaderProgram>();

				FString prolog;
				if (!desc.Uniforms.empty())
					prolog = UniformBlockDecl::Create("Uniforms", desc.Uniforms, POSTPROCESS_BINDINGPOINT);
				prolog += desc.Defines;

				glshader->Compile(IShaderProgram::Vertex, desc.VertexShader, "", desc.Version);
				glshader->Compile(IShaderProgram::Fragment, desc.FragmentShader, prolog, desc.Version);
				glshader->Link(pair->Key.GetChars());
				if (!desc.Uniforms.empty())
					glshader->SetUniformBufferLocation(POSTPROCESS_BINDINGPOINT, "Uniforms");
			}
		}
	}

	// Render the effect

	FGLDebug::PushGroup(name.GetChars());

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(3);

	for (const PPStep &step : hw_postprocess.Effects[name])
	{
		// Bind input textures
		for (unsigned int index = 0; index < step.Textures.Size(); index++)
		{
			const PPTextureInput &input = step.Textures[index];
			int filter = (input.Filter == PPFilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;

			switch (input.Type)
			{
			default:
			case PPTextureType::CurrentPipelineTexture:
				BindCurrentTexture(index, filter);
				break;

			case PPTextureType::NextPipelineTexture:
				I_FatalError("PPTextureType::NextPipelineTexture not allowed as input\n");
				break;

			case PPTextureType::PPTexture:
				GLTextures[input.Texture].Bind(index, filter);
				break;
			}
		}

		// Set render target
		switch (step.Output.Type)
		{
		default:
		case PPTextureType::CurrentPipelineTexture:
			BindCurrentFB();
			break;

		case PPTextureType::NextPipelineTexture:
			BindNextFB();
			break;

		case PPTextureType::PPTexture:
			GLTextureFBs[step.Output.Texture].Bind();
			break;
		}

		// Set blend mode
		if (step.BlendMode.BlendOp == STYLEOP_Add && step.BlendMode.SrcAlpha == STYLEALPHA_One && step.BlendMode.DestAlpha == STYLEALPHA_Zero && step.BlendMode.Flags == 0)
		{
			glDisable(GL_BLEND);
		}
		else
		{
			// To do: support all the modes
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			if (step.BlendMode.SrcAlpha == STYLEALPHA_One && step.BlendMode.DestAlpha == STYLEALPHA_One)
				glBlendFunc(GL_ONE, GL_ONE);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		// Setup viewport
		glViewport(step.Viewport.left, step.Viewport.top, step.Viewport.width, step.Viewport.height);

		auto &shader = GLShaders[step.ShaderName];

		// Set uniforms
		if (step.Uniforms.Size > 0)
		{
			if (!shader->Uniforms)
				shader->Uniforms.reset(screen->CreateUniformBuffer(step.Uniforms.Size));
			shader->Uniforms->SetData(step.Uniforms.Data);
			shader->Uniforms->Bind(POSTPROCESS_BINDINGPOINT);
		}

		// Set shader
		shader->Bind(NOQUEUE);

		// Draw the screen quad
		GLRenderer->RenderScreenQuad();

		// Advance to next PP texture if our output was sent there
		if (step.Output.Type == PPTextureType::NextPipelineTexture)
			NextTexture();
	}

	glViewport(screen->mScreenViewport.left, screen->mScreenViewport.top, screen->mScreenViewport.width, screen->mScreenViewport.height);

	FGLDebug::PopGroup();
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

	auto sceneScale = screen->SceneScale();
	auto sceneOffset = screen->SceneOffset();

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
	mLinearDepthShader->Uniforms->Scale = sceneScale;
	mLinearDepthShader->Uniforms->Offset = sceneOffset;
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
	mSSAOShader->Uniforms->Scale = sceneScale;
	mSSAOShader->Uniforms->Offset = sceneOffset;
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
	mSSAOCombineShader->Uniforms->Scale = screen->SceneScale();
	mSSAOCombineShader->Uniforms->Offset = screen->SceneOffset();
	mSSAOCombineShader->Uniforms.Set();
	RenderScreenQuad();

	FGLDebug::PopGroup();
}

void FGLRenderer::UpdateCameraExposure()
{
	PPCameraExposure exposure;
	exposure.DeclareShaders();
	exposure.UpdateTextures(mBuffers->GetSceneWidth(), mBuffers->GetSceneHeight());
	exposure.UpdateSteps();
	mBuffers->RenderEffect("UpdateCameraExposure");
}

void FGLRenderer::BloomScene(int fixedcm)
{
	if (mBuffers->GetSceneWidth() <= 0 || mBuffers->GetSceneHeight() <= 0)
		return;

	PPBloom bloom;
	bloom.DeclareShaders();
	bloom.UpdateTextures(mBuffers->GetSceneWidth(), mBuffers->GetSceneHeight());
	bloom.UpdateSteps(fixedcm);
	mBuffers->RenderEffect("BloomScene");
}

void FGLRenderer::BlurScene(float gameinfobluramount)
{
	if (mBuffers->GetSceneWidth() <= 0 || mBuffers->GetSceneHeight() <= 0)
		return;

	PPBloom blur;
	blur.DeclareShaders();
	blur.UpdateTextures(mBuffers->GetSceneWidth(), mBuffers->GetSceneHeight());
	blur.UpdateBlurSteps(gameinfobluramount);
	mBuffers->RenderEffect("BlurScene");
}

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
		//mBuffers->ExposureTexture.Bind(1);
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
	PPLensDistort lens;
	lens.DeclareShaders();
	lens.UpdateSteps();
	mBuffers->RenderEffect("LensDistortScene");
}

//-----------------------------------------------------------------------------
//
// Apply FXAA and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::ApplyFXAA()
{
	PPFXAA fxaa;
	fxaa.DeclareShaders();
	fxaa.UpdateSteps();
	mBuffers->RenderEffect("ApplyFXAA");
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

	if (stereo3dMode.IsMono())
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
	mPresentShader->Uniforms->Scale = screen->SceneScale();
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

