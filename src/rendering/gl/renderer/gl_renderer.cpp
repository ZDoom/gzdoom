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

#include "gl_system.h"
#include "files.h"
#include "v_video.h"
#include "m_png.h"
#include "filesystem.h"
#include "doomstat.h"
#include "i_time.h"
#include "p_effect.h"
#include "d_player.h"
#include "a_dynlight.h"
#include "cmdlib.h"
#include "g_game.h"
#include "swrenderer/r_swscene.h"
#include "hwrenderer/utility/hw_clock.h"

#include "gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "hw_vrmodes.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "gl/textures/gl_samplers.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "r_videoscale.h"
#include "r_data/models/models.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/system/gl_buffers.h"
#include "texturemanager.h"

EXTERN_CVAR(Int, screenblocks)

namespace OpenGLRenderer
{

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
}

void FGLRenderer::Initialize(int width, int height)
{
	mScreenBuffers = new FGLRenderBuffers();
	mSaveBuffers = new FGLRenderBuffers();
	mBuffers = mScreenBuffers;
	mPresentShader = new FPresentShader();
	mPresent3dCheckerShader = new FPresent3DCheckerShader();
	mPresent3dColumnShader = new FPresent3DColumnShader();
	mPresent3dRowShader = new FPresent3DRowShader();
	mShadowMapShader = new FShadowMapShader();

	// needed for the core profile, because someone decided it was a good idea to remove the default VAO.
	glGenQueries(1, &PortalQueryObject);

	glGenVertexArrays(1, &mVAOID);
	glBindVertexArray(mVAOID);
	FGLDebug::LabelObject(GL_VERTEX_ARRAY, mVAOID, "FGLRenderer.mVAOID");

	mFBID = 0;
	mOldFBID = 0;

	mShaderManager = new FShaderManager;
	mSamplerManager = new FSamplerManager;
}

FGLRenderer::~FGLRenderer() 
{
	FlushModels();
	TexMan.FlushAll();
	if (mShaderManager != nullptr) delete mShaderManager;
	if (mSamplerManager != nullptr) delete mSamplerManager;
	if (mFBID != 0) glDeleteFramebuffers(1, &mFBID);
	if (mVAOID != 0)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mVAOID);
	}
	if (PortalQueryObject != 0) glDeleteQueries(1, &PortalQueryObject);

	if (mBuffers) delete mBuffers;
	if (mSaveBuffers) delete mSaveBuffers;
	if (mPresentShader) delete mPresentShader;
	if (mPresent3dCheckerShader) delete mPresent3dCheckerShader;
	if (mPresent3dColumnShader) delete mPresent3dColumnShader;
	if (mPresent3dRowShader) delete mPresent3dRowShader;
	if (mShadowMapShader) delete mShadowMapShader;
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

void FGLRenderer::UpdateShadowMap()
{
	if (screen->mShadowMap.PerformUpdate())
	{
		FGLDebug::PushGroup("ShadowMap");

		FGLPostProcessState savedState;

		static_cast<GLDataBuffer*>(screen->mShadowMap.mLightList)->BindBase();
		static_cast<GLDataBuffer*>(screen->mShadowMap.mNodesBuffer)->BindBase();
		static_cast<GLDataBuffer*>(screen->mShadowMap.mLinesBuffer)->BindBase();

		mBuffers->BindShadowMapFB();

		mShadowMapShader->Bind();
		mShadowMapShader->Uniforms->ShadowmapQuality = gl_shadowmap_quality;
		mShadowMapShader->Uniforms->NodesCount = screen->mShadowMap.NodesCount();
		mShadowMapShader->Uniforms.SetData();
		static_cast<GLDataBuffer*>(mShadowMapShader->Uniforms.GetBuffer())->BindBase();

		glViewport(0, 0, gl_shadowmap_quality, 1024);
		RenderScreenQuad();

		const auto &viewport = screen->mScreenViewport;
		glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

		mBuffers->BindShadowMapTexture(16);
		FGLDebug::PopGroup();
		screen->mShadowMap.FinishUpdate();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FGLRenderer::BindToFrameBuffer(FTexture *tex)
{
	auto BaseLayer = static_cast<FHardwareTexture*>(tex->GetHardwareTexture(0, 0));

	if (BaseLayer == nullptr)
	{
		// must create the hardware texture first
		BaseLayer->BindOrCreate(tex, 0, 0, 0, 0);
		FHardwareTexture::Unbind(0);
		gl_RenderState.ClearLastMaterial();
	}
	BaseLayer->BindToFrameBuffer(tex->GetWidth(), tex->GetHeight());
}

//===========================================================================
//
//
//
//===========================================================================

void FGLRenderer::BeginFrame()
{
	mScreenBuffers->Setup(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
	mSaveBuffers->Setup(SAVEPICWIDTH, SAVEPICHEIGHT, SAVEPICWIDTH, SAVEPICHEIGHT);
}

}
