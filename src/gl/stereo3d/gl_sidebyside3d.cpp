/*
** gl_sidebyside3d.cpp
** Mosaic image stereoscopic 3D modes for GZDoom
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

#include "gl_sidebyside3d.h"
#include "gl/renderer/gl_renderbuffers.h"

namespace s3d {

void SideBySideBase::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();

	// Compute screen regions to use for left and right eye views
	int leftWidth = screen->mOutputLetterbox.width / 2;
	int rightWidth = screen->mOutputLetterbox.width - leftWidth;
	IntRect leftHalfScreen = screen->mOutputLetterbox;
	leftHalfScreen.width = leftWidth;
	IntRect rightHalfScreen = screen->mOutputLetterbox;
	rightHalfScreen.width = rightWidth;
	rightHalfScreen.left += leftWidth;

	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(leftHalfScreen, true);

	GLRenderer->mBuffers->BindEyeTexture(1, 0);
	GLRenderer->DrawPresentTexture(rightHalfScreen, true);
}

// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void SideBySideBase::AdjustViewports() const
{
	// Change size of renderbuffer, and align to screen
	screen->mSceneViewport.width /= 2;
	screen->mSceneViewport.left /= 2;
	screen->mScreenViewport.width /= 2;
	screen->mScreenViewport.left /= 2;
}

/* static */
const SideBySideSquished& SideBySideSquished::getInstance(float ipd)
{
	static SideBySideSquished instance(ipd);
	return instance;
}

SideBySideSquished::SideBySideSquished(double ipdMeters)
	: leftEye(ipdMeters), rightEye(ipdMeters)
{
	eye_ptrs.Push(&leftEye);
	eye_ptrs.Push(&rightEye);
}

/* static */
const SideBySideFull& SideBySideFull::getInstance(float ipd)
{
	static SideBySideFull instance(ipd);
	return instance;
}

SideBySideFull::SideBySideFull(double ipdMeters)
	: leftEye(ipdMeters), rightEye(ipdMeters)
{
	eye_ptrs.Push(&leftEye);
	eye_ptrs.Push(&rightEye);
}

/* virtual */
void SideBySideFull::AdjustPlayerSprites() const /* override */ 
{
	// Show weapon at double width, so it would appear normal width after rescaling
	int w = screen->mScreenViewport.width;
	int h = screen->mScreenViewport.height;
	gl_RenderState.mProjectionMatrix.ortho(w/2, w + w/2, h, 0, -1.0f, 1.0f);
	gl_RenderState.ApplyMatrices();
}

/* static */
const TopBottom3D& TopBottom3D::getInstance(float ipd)
{
	static TopBottom3D instance(ipd);
	return instance;
}

void TopBottom3D::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();

	// Compute screen regions to use for left and right eye views
	int topHeight = screen->mOutputLetterbox.height / 2;
	int bottomHeight = screen->mOutputLetterbox.height - topHeight;
	IntRect topHalfScreen = screen->mOutputLetterbox;
	topHalfScreen.height = topHeight;
	topHalfScreen.top = topHeight;
	IntRect bottomHalfScreen = screen->mOutputLetterbox;
	bottomHalfScreen.height = bottomHeight;
	bottomHalfScreen.top = 0;

	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(topHalfScreen, true);

	GLRenderer->mBuffers->BindEyeTexture(1, 0);
	GLRenderer->DrawPresentTexture(bottomHalfScreen, true);
}

// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void TopBottom3D::AdjustViewports() const
{
	// Change size of renderbuffer, and align to screen
	screen->mSceneViewport.height /= 2;
	screen->mSceneViewport.top /= 2;
	screen->mScreenViewport.height /= 2;
	screen->mScreenViewport.top /= 2;
}

} /* namespace s3d */
