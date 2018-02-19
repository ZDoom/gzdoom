// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl1_renderer.cpp
** Renderer interface
**
*/

#include "gl/system/gl_system.h"
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "r_data/r_translate.h"
#include "m_png.h"
#include "m_crc32.h"
#include "w_wad.h"
//#include "gl/gl_intern.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "doomstat.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_2ddrawer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/shaders/gl_ambientshader.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_colormapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_fxaashader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/shaders/gl_present3dRowshader.h"
#include "gl/shaders/gl_shadowmapshader.h"
#include "gl/shaders/gl_postprocessshader.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/models/gl_models.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "r_videoscale.h"

EXTERN_CVAR(Int, screenblocks)

CVAR(Bool, gl_scale_viewport, true, CVAR_ARCHIVE);

//===========================================================================
// 
// Renderer interface
//
//===========================================================================

//-----------------------------------------------------------------------------
//
// Initialize
//
//-----------------------------------------------------------------------------

FGLRenderer::FGLRenderer(OpenGLFrameBuffer *fb) 
{
	framebuffer = fb;
	mClipPortal = nullptr;
	mCurrentPortal = nullptr;
	mMirrorCount = 0;
	mPlaneMirrorCount = 0;
	mLightCount = 0;
	mAngles = FRotator(0.f, 0.f, 0.f);
	mViewVector = FVector2(0,0);
	mVBO = nullptr;
	mSkyVBO = nullptr;
	gl_spriteindex = 0;
	mShaderManager = nullptr;
	mLights = nullptr;
	m2DDrawer = nullptr;
	mTonemapPalette = nullptr;
	mBuffers = nullptr;
	mPresentShader = nullptr;
	mPresent3dCheckerShader = nullptr;
	mPresent3dColumnShader = nullptr;
	mPresent3dRowShader = nullptr;
	mBloomExtractShader = nullptr;
	mBloomCombineShader = nullptr;
	mExposureExtractShader = nullptr;
	mExposureAverageShader = nullptr;
	mExposureCombineShader = nullptr;
	mBlurShader = nullptr;
	mTonemapShader = nullptr;
	mTonemapPalette = nullptr;
	mColormapShader = nullptr;
	mLensShader = nullptr;
	mLinearDepthShader = nullptr;
	mDepthBlurShader = nullptr;
	mSSAOShader = nullptr;
	mSSAOCombineShader = nullptr;
	mFXAAShader = nullptr;
	mFXAALumaShader = nullptr;
	mShadowMapShader = nullptr;
	mCustomPostProcessShaders = nullptr;
}

void gl_LoadModels();
void gl_FlushModels();

void FGLRenderer::Initialize(int width, int height)
{
	mBuffers = new FGLRenderBuffers();
	mLinearDepthShader = new FLinearDepthShader();
	mDepthBlurShader = new FDepthBlurShader();
	mSSAOShader = new FSSAOShader();
	mSSAOCombineShader = new FSSAOCombineShader();
	mBloomExtractShader = new FBloomExtractShader();
	mBloomCombineShader = new FBloomCombineShader();
	mExposureExtractShader = new FExposureExtractShader();
	mExposureAverageShader = new FExposureAverageShader();
	mExposureCombineShader = new FExposureCombineShader();
	mBlurShader = new FBlurShader();
	mTonemapShader = new FTonemapShader();
	mColormapShader = new FColormapShader();
	mTonemapPalette = nullptr;
	mLensShader = new FLensShader();
	mFXAAShader = new FFXAAShader;
	mFXAALumaShader = new FFXAALumaShader;
	mPresentShader = new FPresentShader();
	mPresent3dCheckerShader = new FPresent3DCheckerShader();
	mPresent3dColumnShader = new FPresent3DColumnShader();
	mPresent3dRowShader = new FPresent3DRowShader();
	mShadowMapShader = new FShadowMapShader();
	mCustomPostProcessShaders = new FCustomPostProcessShaders();
	m2DDrawer = new F2DDrawer;

	GetSpecialTextures();

	// needed for the core profile, because someone decided it was a good idea to remove the default VAO.
	if (!gl.legacyMode)
	{
		glGenVertexArrays(1, &mVAOID);
		glBindVertexArray(mVAOID);
		FGLDebug::LabelObject(GL_VERTEX_ARRAY, mVAOID, "FGLRenderer.mVAOID");
	}
	else mVAOID = 0;

	mVBO = new FFlatVertexBuffer(width, height);
	mSkyVBO = new FSkyVertexBuffer;
	if (!gl.legacyMode) mLights = new FLightBuffer();
	else mLights = NULL;
	gl_RenderState.SetVertexBuffer(mVBO);
	mFBID = 0;
	mOldFBID = 0;

	SetupLevel();
	mShaderManager = new FShaderManager;
	mSamplerManager = new FSamplerManager;
	gl_LoadModels();

	GLPortal::Initialize();
}

FGLRenderer::~FGLRenderer() 
{
	GLPortal::Shutdown();

	gl_FlushModels();
	AActor::DeleteAllAttachedLights();
	FMaterial::FlushAll();
	if (m2DDrawer != nullptr) delete m2DDrawer;
	if (mShaderManager != NULL) delete mShaderManager;
	if (mSamplerManager != NULL) delete mSamplerManager;
	if (mVBO != NULL) delete mVBO;
	if (mSkyVBO != NULL) delete mSkyVBO;
	if (mLights != NULL) delete mLights;
	if (mFBID != 0) glDeleteFramebuffers(1, &mFBID);
	if (mVAOID != 0)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mVAOID);
	}
	if (mBuffers) delete mBuffers;
	if (mPresentShader) delete mPresentShader;
	if (mLinearDepthShader) delete mLinearDepthShader;
	if (mDepthBlurShader) delete mDepthBlurShader;
	if (mSSAOShader) delete mSSAOShader;
	if (mSSAOCombineShader) delete mSSAOCombineShader;
	if (mPresent3dCheckerShader) delete mPresent3dCheckerShader;
	if (mPresent3dColumnShader) delete mPresent3dColumnShader;
	if (mPresent3dRowShader) delete mPresent3dRowShader;
	if (mBloomExtractShader) delete mBloomExtractShader;
	if (mBloomCombineShader) delete mBloomCombineShader;
	if (mExposureExtractShader) delete mExposureExtractShader;
	if (mExposureAverageShader) delete mExposureAverageShader;
	if (mExposureCombineShader) delete mExposureCombineShader;
	if (mBlurShader) delete mBlurShader;
	if (mTonemapShader) delete mTonemapShader;
	if (mTonemapPalette) delete mTonemapPalette;
	if (mColormapShader) delete mColormapShader;
	if (mLensShader) delete mLensShader;
	if (mShadowMapShader) delete mShadowMapShader;
	delete mCustomPostProcessShaders;
	delete mFXAAShader;
	delete mFXAALumaShader;
}


void FGLRenderer::GetSpecialTextures()
{
	if (gl.legacyMode) glLight = TexMan.CheckForTexture("glstuff/gllight.png", FTexture::TEX_MiscPatch);
	glPart2 = TexMan.CheckForTexture("glstuff/glpart2.png", FTexture::TEX_MiscPatch);
	glPart = TexMan.CheckForTexture("glstuff/glpart.png", FTexture::TEX_MiscPatch);
	mirrorTexture = TexMan.CheckForTexture("glstuff/mirror.png", FTexture::TEX_MiscPatch);

}

//==========================================================================
//
// Calculates the viewport values needed for 2D and 3D operations
//
//==========================================================================

void FGLRenderer::SetOutputViewport(GL_IRECT *bounds)
{
	if (bounds)
	{
		mSceneViewport = *bounds;
		mScreenViewport = *bounds;
		mOutputLetterbox = *bounds;
		return;
	}

	// Special handling so the view with a visible status bar displays properly
	int height, width;
	if (screenblocks >= 10)
	{
		height = framebuffer->GetHeight();
		width = framebuffer->GetWidth();
	}
	else
	{
		height = (screenblocks*framebuffer->GetHeight() / 10) & ~7;
		width = (screenblocks*framebuffer->GetWidth() / 10);
	}

	// Back buffer letterbox for the final output
	int clientWidth = framebuffer->GetClientWidth();
	int clientHeight = framebuffer->GetClientHeight();
	if (clientWidth == 0 || clientHeight == 0)
	{
		// When window is minimized there may not be any client area.
		// Pretend to the rest of the render code that we just have a very small window.
		clientWidth = 160;
		clientHeight = 120;
	}
	int screenWidth = framebuffer->GetWidth();
	int screenHeight = framebuffer->GetHeight();
	float scaleX, scaleY;
	if (ViewportIsScaled43())
	{
		scaleX = MIN(clientWidth / (float)screenWidth, clientHeight / (screenHeight * 1.2f));
		scaleY = scaleX * 1.2f;
	}
	else
	{
		scaleX = MIN(clientWidth / (float)screenWidth, clientHeight / (float)screenHeight);
		scaleY = scaleX;
	}
	mOutputLetterbox.width = (int)round(screenWidth * scaleX);
	mOutputLetterbox.height = (int)round(screenHeight * scaleY);
	mOutputLetterbox.left = (clientWidth - mOutputLetterbox.width) / 2;
	mOutputLetterbox.top = (clientHeight - mOutputLetterbox.height) / 2;

	// The entire renderable area, including the 2D HUD
	mScreenViewport.left = 0;
	mScreenViewport.top = 0;
	mScreenViewport.width = screenWidth;
	mScreenViewport.height = screenHeight;

	// Viewport for the 3D scene
	mSceneViewport.left = viewwindowx;
	mSceneViewport.top = screenHeight - (height + viewwindowy - ((height - viewheight) / 2));
	mSceneViewport.width = viewwidth;
	mSceneViewport.height = height;

	// Scale viewports to fit letterbox
	bool notScaled = ((mScreenViewport.width == ViewportScaledWidth(mScreenViewport.width, mScreenViewport.height)) &&
		(mScreenViewport.width == ViewportScaledHeight(mScreenViewport.width, mScreenViewport.height)) &&
		!ViewportIsScaled43());
	if ((gl_scale_viewport && !framebuffer->IsFullscreen() && notScaled) || !FGLRenderBuffers::IsEnabled())
	{
		mScreenViewport.width = mOutputLetterbox.width;
		mScreenViewport.height = mOutputLetterbox.height;
		mSceneViewport.left = (int)round(mSceneViewport.left * scaleX);
		mSceneViewport.top = (int)round(mSceneViewport.top * scaleY);
		mSceneViewport.width = (int)round(mSceneViewport.width * scaleX);
		mSceneViewport.height = (int)round(mSceneViewport.height * scaleY);

		// Without render buffers we have to render directly to the letterbox
		if (!FGLRenderBuffers::IsEnabled())
		{
			mScreenViewport.left += mOutputLetterbox.left;
			mScreenViewport.top += mOutputLetterbox.top;
			mSceneViewport.left += mOutputLetterbox.left;
			mSceneViewport.top += mOutputLetterbox.top;
		}
	}

	s3d::Stereo3DMode::getCurrentMode().AdjustViewports();
}

//===========================================================================
// 
// Calculates the OpenGL window coordinates for a zdoom screen position
//
//===========================================================================

int FGLRenderer::ScreenToWindowX(int x)
{
	return mScreenViewport.left + (int)round(x * mScreenViewport.width / (float)framebuffer->GetWidth());
}

int FGLRenderer::ScreenToWindowY(int y)
{
	return mScreenViewport.top + mScreenViewport.height - (int)round(y * mScreenViewport.height / (float)framebuffer->GetHeight());
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::SetupLevel()
{
	mVBO->CreateVBO();
}

void FGLRenderer::Begin2D()
{
	if (mBuffers->Setup(mScreenViewport.width, mScreenViewport.height, mSceneViewport.width, mSceneViewport.height))
	{
		if (mDrawingScene2D)
			mBuffers->BindSceneFB(false);
		else
			mBuffers->BindCurrentFB();
	}
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
	glScissor(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);

	gl_RenderState.EnableFog(false);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::FlushTextures()
{
	FMaterial::FlushAll();
}

//===========================================================================
// 
//
//
//===========================================================================

bool FGLRenderer::StartOffscreen()
{
	bool firstBind = (mFBID == 0);
	if (mFBID == 0)
		glGenFramebuffers(1, &mFBID);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mOldFBID);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBID);
	if (firstBind)
		FGLDebug::LabelObject(GL_FRAMEBUFFER, mFBID, "OffscreenFB");
	return true;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::EndOffscreen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mOldFBID); 
}

//===========================================================================
// 
//
//
//===========================================================================

unsigned char *FGLRenderer::GetTextureBuffer(FTexture *tex, int &w, int &h)
{
	FMaterial * gltex = FMaterial::ValidateTexture(tex, false);
	if (gltex)
	{
		return gltex->CreateTexBuffer(0, w, h);
	}
	return NULL;
}
