/*
** gl1_renderer.cpp
** Renderer interface
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_2ddrawer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/shaders/gl_shader.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/models/gl_models.h"
#include "gl/dynlights/gl_lightbuffer.h"

EXTERN_CVAR(Int, screenblocks)

CVAR(Bool, gl_scale_viewport, true, 0);

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
	mClipPortal = NULL;
	mCurrentPortal = NULL;
	mMirrorCount = 0;
	mPlaneMirrorCount = 0;
	mLightCount = 0;
	mAngles = FRotator(0.f, 0.f, 0.f);
	mViewVector = FVector2(0,0);
	mVBO = NULL;
	mSkyVBO = NULL;
	gl_spriteindex = 0;
	mShaderManager = NULL;
	gllight = glpart2 = glpart = mirrortexture = NULL;
	mLights = NULL;
	m2DDrawer = nullptr;
}

void gl_LoadModels();
void gl_FlushModels();

void FGLRenderer::Initialize(int width, int height)
{
	mBuffers = new FGLRenderBuffers();
	mBloomExtractShader = new FBloomExtractShader();
	mBloomCombineShader = new FBloomCombineShader();
	mBlurShader = new FBlurShader();
	mTonemapShader = new FTonemapShader();
	mLensShader = new FLensShader();
	mPresentShader = new FPresentShader();
	m2DDrawer = new F2DDrawer;

	// Only needed for the core profile, because someone decided it was a good idea to remove the default VAO.
	if (gl.version >= 4.0)
	{
		glGenVertexArrays(1, &mVAOID);
		glBindVertexArray(mVAOID);
	}
	else mVAOID = 0;

	gllight = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/gllight.png"), FTexture::TEX_MiscPatch);
	glpart2 = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/glpart2.png"), FTexture::TEX_MiscPatch);
	glpart = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/glpart.png"), FTexture::TEX_MiscPatch);
	mirrortexture = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/mirror.png"), FTexture::TEX_MiscPatch);

	mVBO = new FFlatVertexBuffer(width, height);
	mSkyVBO = new FSkyVertexBuffer;
	if (gl.lightmethod != LM_SOFTWARE) mLights = new FLightBuffer();
	else mLights = NULL;
	gl_RenderState.SetVertexBuffer(mVBO);
	mFBID = 0;
	mOldFBID = 0;

	SetupLevel();
	mShaderManager = new FShaderManager;
	mSamplerManager = new FSamplerManager;
	gl_LoadModels();
}

FGLRenderer::~FGLRenderer() 
{
	gl_FlushModels();
	gl_DeleteAllAttachedLights();
	FMaterial::FlushAll();
	if (m2DDrawer != nullptr) delete m2DDrawer;
	if (mShaderManager != NULL) delete mShaderManager;
	if (mSamplerManager != NULL) delete mSamplerManager;
	if (mVBO != NULL) delete mVBO;
	if (mSkyVBO != NULL) delete mSkyVBO;
	if (mLights != NULL) delete mLights;
	if (glpart2) delete glpart2;
	if (glpart) delete glpart;
	if (mirrortexture) delete mirrortexture;
	if (mFBID != 0) glDeleteFramebuffers(1, &mFBID);
	if (mVAOID != 0)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mVAOID);
	}
	if (mBuffers) delete mBuffers;
	if (mPresentShader) delete mPresentShader;
	if (mBloomExtractShader) delete mBloomExtractShader;
	if (mBloomCombineShader) delete mBloomCombineShader;
	if (mBlurShader) delete mBlurShader;
	if (mTonemapShader) delete mTonemapShader;
	if (mLensShader) delete mLensShader;
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
	int screenWidth = framebuffer->GetWidth();
	int screenHeight = framebuffer->GetHeight();
	float scale = MIN(clientWidth / (float)screenWidth, clientHeight / (float)screenHeight);
	mOutputLetterbox.width = (int)round(screenWidth * scale);
	mOutputLetterbox.height = (int)round(screenHeight * scale);
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
	if (gl_scale_viewport || !FGLRenderBuffers::IsEnabled())
	{
		mScreenViewport.width = mOutputLetterbox.width;
		mScreenViewport.height = mOutputLetterbox.height;
		mSceneViewport.left = (int)round(mSceneViewport.left * scale);
		mSceneViewport.top = (int)round(mSceneViewport.top * scale);
		mSceneViewport.width = (int)round(mSceneViewport.width * scale);
		mSceneViewport.height = (int)round(mSceneViewport.height * scale);

		// Without render buffers we have to render directly to the letterbox
		if (!FGLRenderBuffers::IsEnabled())
		{
			mScreenViewport.left += mOutputLetterbox.left;
			mScreenViewport.top += mOutputLetterbox.top;
			mSceneViewport.left += mOutputLetterbox.left;
			mSceneViewport.top += mOutputLetterbox.top;
		}
	}
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
	if (FGLRenderBuffers::IsEnabled())
	{
		mBuffers->Setup(mScreenViewport.width, mScreenViewport.height, mSceneViewport.width, mSceneViewport.height);
		if (mDrawingScene2D)
			mBuffers->BindSceneFB();
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

void FGLRenderer::ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	GLWall wall;
	wall.ProcessLowerMiniseg(seg, frontsector, backsector);
	rendered_lines++;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessSprite(AActor *thing, sector_t *sector, bool thruportal)
{
	GLSprite glsprite;
	glsprite.Process(thing, sector, thruportal);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessParticle(particle_t *part, sector_t *sector)
{
	GLSprite glsprite;
	glsprite.ProcessParticle(part, sector);//, 0, 0);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessSector(sector_t *fakesector)
{
	GLFlat glflat;
	glflat.ProcessSector(fakesector);
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
	if (mFBID == 0) glGenFramebuffers(1, &mFBID);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mOldFBID);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBID);
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
