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

namespace s3d {

/* static */
const RowInterleaved3D& RowInterleaved3D::getInstance(float ipd)
{
	static RowInterleaved3D instance(ipd);
	return instance;
}

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

	GLRenderer->mBuffers->BlitFromEyeTexture(0, &topHalfScreen);
	GLRenderer->mBuffers->BlitFromEyeTexture(1, &bottomHalfScreen);
}


} /* namespace s3d */
