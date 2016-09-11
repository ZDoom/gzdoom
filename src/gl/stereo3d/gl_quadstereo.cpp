/*
** gl_quadstereo.cpp
** Quad-buffered OpenGL stereoscopic 3D mode for GZDoom
**
**---------------------------------------------------------------------------
** Copyright 2015 Christopher Bruns
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

#include "gl_quadstereo.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"

namespace s3d {

QuadStereo::QuadStereo(double ipdMeters)
	: leftEye(ipdMeters), rightEye(ipdMeters)
{
	// Check whether quad-buffered stereo is supported in the current context
	// We are assuming the OpenGL context is already current at this point,
	// i.e. this constructor is called "just in time".

	// First initialize to mono-ish initial state
	bQuadStereoSupported = leftEye.bQuadStereoSupported = rightEye.bQuadStereoSupported = false;
	eye_ptrs.Push(&leftEye); // We ALWAYS want to show at least this one view...
	// We will possibly advance to true stereo mode in the Setup() method...
}

// Sometimes the stereo render context is not ready immediately at start up
/* private */
void QuadStereo::checkInitialRenderContextState()
{
	// Keep trying until we see at least one good OpenGL context to render to
	static bool bDecentContextWasFound = false;
	if (!bDecentContextWasFound) {
		// I'm using a "random" OpenGL call (glGetFramebufferAttachmentParameteriv)
		// that appears to correlate with whether the context is ready 
		GLint attachmentType = GL_NONE;
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);
		if (attachmentType != GL_NONE) // Finally, a useful OpenGL context
		{
			// This block will be executed exactly ONCE during a game run
			bDecentContextWasFound = true; // now we can stop checking every frame...
			// Now check whether this context supports hardware stereo
			GLboolean supportsStereo, supportsBuffered;
			glGetBooleanv(GL_STEREO, &supportsStereo);
			glGetBooleanv(GL_DOUBLEBUFFER, &supportsBuffered);
			bQuadStereoSupported = supportsStereo && supportsBuffered;
			leftEye.bQuadStereoSupported = bQuadStereoSupported;
			rightEye.bQuadStereoSupported = bQuadStereoSupported;
			if (bQuadStereoSupported)
				eye_ptrs.Push(&rightEye); // Use the other eye too, if we can do stereo
		}
	}
}

void QuadStereo::Present() const
{
	if (bQuadStereoSupported)
	{
		GLRenderer->mBuffers->BindOutputFB();

		glDrawBuffer(GL_BACK_LEFT);
		GLRenderer->ClearBorders();
		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(GLRenderer->mOutputLetterbox, true);

		glDrawBuffer(GL_BACK_RIGHT);
		GLRenderer->ClearBorders();
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(GLRenderer->mOutputLetterbox, true);

		glDrawBuffer(GL_BACK);
	}
	else
	{
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();
		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(GLRenderer->mOutputLetterbox, true);
	}
}

void QuadStereo::SetUp() const
{
	Stereo3DMode::SetUp();
	// Maybe advance to true stereo mode (ONCE), after the stereo context is finally ready
	const_cast<QuadStereo*>(this)->checkInitialRenderContextState();
}

/* static */
const QuadStereo& QuadStereo::getInstance(float ipd)
{
	static QuadStereo instance(ipd);
	return instance;
}

} /* namespace s3d */
