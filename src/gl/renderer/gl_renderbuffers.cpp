/*
** gl_renderbuffers.cpp
** Render buffers used during rendering
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
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "w_wad.h"
#include "i_system.h"
#include "doomerrors.h"

//==========================================================================
//
// Initialize render buffers and textures used in rendering passes
//
//==========================================================================

FGLRenderBuffers::FGLRenderBuffers()
{
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&mOutputFB);
}

//==========================================================================
//
// Free render buffer resources
//
//==========================================================================

FGLRenderBuffers::~FGLRenderBuffers()
{
	if (mSceneFB != 0)
		glDeleteFramebuffers(1, &mSceneFB);
	if (mSceneTexture != 0)
		glDeleteTextures(1, &mSceneTexture);
	if (mSceneDepthStencil != 0)
		glDeleteRenderbuffers(1, &mSceneDepthStencil);
}

//==========================================================================
//
// Makes sure all render buffers have sizes suitable for rending at the
// specified resolution
//
//==========================================================================

void FGLRenderBuffers::Setup(int width, int height)
{
	if (width <= mWidth && height <= mHeight)
		return;

	if (mSceneFB != 0)
		glDeleteFramebuffers(1, &mSceneFB);
	if (mSceneTexture != 0)
		glDeleteTextures(1, &mSceneTexture);
	if (mSceneDepthStencil != 0)
		glDeleteRenderbuffers(1, &mSceneDepthStencil);

	glGenFramebuffers(1, &mSceneFB);
	glGenTextures(1, &mSceneTexture);
	glGenRenderbuffers(1, &mSceneDepthStencil);

	glBindTexture(GL_TEXTURE_2D, mSceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, mSceneDepthStencil);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFB);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSceneTexture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mSceneDepthStencil);
	glBindFramebuffer(GL_FRAMEBUFFER, mOutputFB);

	mWidth = width;
	mHeight = height;
}

//==========================================================================
//
// Makes the scene frame buffer active 
//
//==========================================================================

void FGLRenderBuffers::BindSceneFB()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFB);
}

//==========================================================================
//
// Makes the screen frame buffer active
//
//==========================================================================

void FGLRenderBuffers::BindOutputFB()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mOutputFB);
}

//==========================================================================
//
// Binds the scene frame buffer texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindSceneTexture(int index)
{
	glActiveTexture(index);
	glBindTexture(GL_TEXTURE_2D, mSceneTexture);
}
