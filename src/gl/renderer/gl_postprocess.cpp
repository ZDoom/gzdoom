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
#include "a_hexenglobal.h"
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
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_colormapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/renderer/gl_2ddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"

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
	if (self < 0 || self > 5)
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
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, FFlatVertexBuffer::PRESENT_INDEX, 4);
}

//-----------------------------------------------------------------------------
//
// Adds bloom contribution to scene texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::BloomScene()
{
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || gl_fixedcolormap != CM_DEFAULT)
		return;

	FGLDebug::PushGroup("BloomScene");

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

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mTonemapShader->Bind();
	mTonemapShader->SceneTexture.Set(0);

	if (mTonemapShader->IsPaletteMode())
	{
		mTonemapShader->PaletteLUT.Set(1);
		BindTonemapPalette(1);
	}
	else
	{
		mTonemapShader->Exposure.Set(mCameraExposure);
	}

	RenderScreenQuad();
	mBuffers->NextTexture();

	FGLDebug::PopGroup();
}

void FGLRenderer::BindTonemapPalette(int texunit)
{
	if (mTonemapPalette)
	{
		mTonemapPalette->Bind(texunit, 0, false);
	}
	else
	{
		TArray<unsigned char> lut;
		lut.Resize(512 * 512 * 4);
		for (int r = 0; r < 64; r++)
		{
			for (int g = 0; g < 64; g++)
			{
				for (int b = 0; b < 64; b++)
				{
					PalEntry color = GPalette.BaseColors[(BYTE)PTM_BestColor((uint32 *)GPalette.BaseColors, (r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4), 0, 256)];
					int index = ((r * 64 + g) * 64 + b) * 4;
					lut[index] = color.r;
					lut[index + 1] = color.g;
					lut[index + 2] = color.b;
					lut[index + 3] = 255;
				}
			}
		}

		mTonemapPalette = new FHardwareTexture(512, 512, true);
		mTonemapPalette->CreateTexture(&lut[0], 512, 512, texunit, false, 0, "mTonemapPalette");

		glActiveTexture(GL_TEXTURE0 + texunit);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glActiveTexture(GL_TEXTURE0);
	}
}

void FGLRenderer::ClearTonemapPalette()
{
	delete mTonemapPalette;
	mTonemapPalette = nullptr;
}

//-----------------------------------------------------------------------------
//
// Colormap scene texture and place the result in the HUD/2D texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::ColormapScene()
{
	if (gl_fixedcolormap < CM_FIRSTSPECIALCOLORMAP || gl_fixedcolormap >= CM_MAXCOLORMAP)
		return;

	FGLDebug::PushGroup("ColormapScene");

	FGLPostProcessState savedState;

	mBuffers->BindNextFB();
	mBuffers->BindCurrentTexture(0);
	mColormapShader->Bind();
	
	FSpecialColormap *scm = &SpecialColormaps[gl_fixedcolormap - CM_FIRSTSPECIALCOLORMAP];
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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

int FGLRenderer::PTM_BestColor (const uint32 *pal_in, int r, int g, int b, int first, int num)
{
	const PalEntry *pal = (const PalEntry *)pal_in;
	static double powtable[256];
	static bool firstTime = true;

	double fbestdist, fdist;
	int bestcolor;

	if (firstTime)
	{
		firstTime = false;
		for (int x = 0; x < 256; x++) powtable[x] = pow((double)x/255,1.2);
	}

	for (int color = first; color < num; color++)
	{
		double x = powtable[abs(r-pal[color].r)];
		double y = powtable[abs(g-pal[color].g)];
		double z = powtable[abs(b-pal[color].b)];
		fdist = x + y + z;
		if (color == first || fdist < fbestdist)
		{
			if (fdist == 0)
				return color;

			fbestdist = fdist;
			bestcolor = color;
		}
	}
	return bestcolor;
}

