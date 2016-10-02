/*
** gl_interleaved3d.cpp
** Interleaved image stereoscopic 3D modes for GZDoom
**
**---------------------------------------------------------------------------
** Copyright 2016 Christopher Bruns
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
**
*/

#include "gl_interleaved3d.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_present3dRowshader.h"

EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)

namespace s3d {

/* static */
const RowInterleaved3D& RowInterleaved3D::getInstance(float ipd)
{
	static RowInterleaved3D instance(ipd);
	return instance;
}

RowInterleaved3D::RowInterleaved3D(double ipdMeters) 
	: TopBottom3D(ipdMeters)
{}

void RowInterleaved3D::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();

	// Compute screen regions to use for left and right eye views
	int topHeight = GLRenderer->mOutputLetterbox.height / 2;
	GL_IRECT topHalfScreen = GLRenderer->mOutputLetterbox;
	topHalfScreen.height = topHeight;
	topHalfScreen.top = topHeight;
	GL_IRECT bottomHalfScreen = GLRenderer->mOutputLetterbox;
	bottomHalfScreen.height = topHeight;
	bottomHalfScreen.top = 0;

	// Bind each eye texture, for composition in the shader
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->mBuffers->BindEyeTexture(1, 1);

	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	const GL_IRECT& box = GLRenderer->mOutputLetterbox;
	glViewport(box.left, box.top, box.width, box.height);

	bool applyGamma = true;

	GLRenderer->mPresent3dRowShader->Bind();
	GLRenderer->mPresent3dRowShader->LeftEyeTexture.Set(0);
	GLRenderer->mPresent3dRowShader->RightEyeTexture.Set(1);
	if (!applyGamma || GLRenderer->framebuffer->IsHWGammaActive())
	{
		GLRenderer->mPresent3dRowShader->InvGamma.Set(1.0f);
		GLRenderer->mPresent3dRowShader->Contrast.Set(1.0f);
		GLRenderer->mPresent3dRowShader->Brightness.Set(0.0f);
	}
	else
	{
		GLRenderer->mPresent3dRowShader->InvGamma.Set(1.0f / clamp<float>(Gamma, 0.1f, 4.f));
		GLRenderer->mPresent3dRowShader->Contrast.Set(clamp<float>(vid_contrast, 0.1f, 3.f));
		GLRenderer->mPresent3dRowShader->Brightness.Set(clamp<float>(vid_brightness, -0.8f, 0.8f));
	}
	GLRenderer->mPresent3dRowShader->Scale.Set(
		GLRenderer->mScreenViewport.width / (float)GLRenderer->mBuffers->GetWidth(),
		GLRenderer->mScreenViewport.height / (float)GLRenderer->mBuffers->GetHeight());
	GLRenderer->mPresent3dRowShader->VerticalPixelOffset.Set(0); // fixme: vary with window location
	GLRenderer->RenderScreenQuad();
}


} /* namespace s3d */
