// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
** gl_quadstereo.cpp
** Quad-buffered OpenGL stereoscopic 3D mode for GZDoom
**
*/

#include "gl_quadstereo.h"
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
	static int contextCheckCount = 0;
	if ( (! bDecentContextWasFound) && (contextCheckCount < 200) )
	{
		contextCheckCount += 1;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // This question is about the main screen display context
		GLboolean supportsStereo, supportsBuffered;
		glGetBooleanv(GL_DOUBLEBUFFER, &supportsBuffered);
		if (supportsBuffered) // Finally, a useful OpenGL context
		{
			// This block will be executed exactly ONCE during a game run
			bDecentContextWasFound = true; // now we can stop checking every frame...
			// Now check whether this context supports hardware stereo
			glGetBooleanv(GL_STEREO, &supportsStereo);
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
		GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);

		glDrawBuffer(GL_BACK_RIGHT);
		GLRenderer->ClearBorders();
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);

		glDrawBuffer(GL_BACK);
	}
	else
	{
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();
		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);
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
