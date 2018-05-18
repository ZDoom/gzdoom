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
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_present3dRowshader.h"

#ifdef _WIN32
#include "hardware.h"
#endif // _WIN32

EXTERN_CVAR(Float, vid_saturation)
EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)
EXTERN_CVAR(Int, gl_satformula)
EXTERN_CVAR(Bool, fullscreen)
EXTERN_CVAR(Int, win_x) // screen pixel position of left of display window
EXTERN_CVAR(Int, win_y) // screen pixel position of top of display window

namespace s3d {

/* static */
const CheckerInterleaved3D& CheckerInterleaved3D::getInstance(float ipd)
{
	static CheckerInterleaved3D instance(ipd);
	return instance;
}

/* static */
const ColumnInterleaved3D& ColumnInterleaved3D::getInstance(float ipd)
{
	static ColumnInterleaved3D instance(ipd);
	return instance;
}

/* static */
const RowInterleaved3D& RowInterleaved3D::getInstance(float ipd)
{
	static RowInterleaved3D instance(ipd);
	return instance;
}

static void prepareInterleavedPresent(FPresentStereoShaderBase& shader)
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();


	// Bind each eye texture, for composition in the shader
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->mBuffers->BindEyeTexture(1, 1);

	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	const IntRect& box = screen->mOutputLetterbox;
	glViewport(box.left, box.top, box.width, box.height);

	shader.Bind();
	shader.LeftEyeTexture.Set(0);
	shader.RightEyeTexture.Set(1);

	if ( GLRenderer->framebuffer->IsHWGammaActive() )
	{
		shader.InvGamma.Set(1.0f);
		shader.Contrast.Set(1.0f);
		shader.Brightness.Set(0.0f);
		shader.Saturation.Set(1.0f);
	}
	else
	{
		shader.InvGamma.Set(1.0f / clamp<float>(Gamma, 0.1f, 4.f));
		shader.Contrast.Set(clamp<float>(vid_contrast, 0.1f, 3.f));
		shader.Brightness.Set(clamp<float>(vid_brightness, -0.8f, 0.8f));
		shader.Saturation.Set(clamp<float>(vid_saturation, -15.0f, 15.0f));
		shader.GrayFormula.Set(static_cast<int>(gl_satformula));
	}
	shader.Scale.Set(
		screen->mScreenViewport.width / (float)GLRenderer->mBuffers->GetWidth(),
		screen->mScreenViewport.height / (float)GLRenderer->mBuffers->GetHeight());
}

// fixme: I don't know how to get absolute window position on Mac and Linux
// fixme: I don't know how to get window border decoration size anywhere
// So for now I'll hard code the border effect on my test machine.
// Workaround for others is to fuss with vr_swap_eyes CVAR until it looks right.
// Presumably the top/left window border on my test machine has an odd number of pixels
//  in the horizontal direction, and an even number in the vertical direction.
#define WINDOW_BORDER_HORIZONTAL_PARITY 1
#define WINDOW_BORDER_VERTICAL_PARITY 0

void CheckerInterleaved3D::Present() const
{
	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);
	prepareInterleavedPresent(*GLRenderer->mPresent3dCheckerShader);

	// Compute absolute offset from top of screen to top of current display window
	// because we need screen-relative, not window-relative, scan line parity
	int windowVOffset = 0;
	int windowHOffset = 0;

#ifdef _WIN32
	if (!fullscreen) {
		I_SaveWindowedPos(); // update win_y CVAR
		windowHOffset = (win_x + WINDOW_BORDER_HORIZONTAL_PARITY) % 2;
		windowVOffset = (win_y + WINDOW_BORDER_VERTICAL_PARITY) % 2;
	}
#endif // _WIN32

	GLRenderer->mPresent3dCheckerShader->WindowPositionParity.Set(
		(windowVOffset
			+ windowHOffset
			+ screen->mOutputLetterbox.height + 1 // +1 because of origin at bottom
		) % 2 // because we want the top pixel offset, but gl_FragCoord.y is the bottom pixel offset
	);

	GLRenderer->RenderScreenQuad();
}

void s3d::CheckerInterleaved3D::AdjustViewports() const
{
	// decrease the total pixel count by 2, but keep the same aspect ratio
	const float sqrt2 = 1.41421356237f;
	// Change size of renderbuffer, and align to screen
	screen->mSceneViewport.height /= sqrt2;
	screen->mSceneViewport.top /= sqrt2;
	screen->mSceneViewport.width /= sqrt2;
	screen->mSceneViewport.left /= sqrt2;

	screen->mScreenViewport.height /= sqrt2;
	screen->mScreenViewport.top /= sqrt2;
	screen->mScreenViewport.width /= sqrt2;
	screen->mScreenViewport.left /= sqrt2;
}

void ColumnInterleaved3D::Present() const
{
	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);
	prepareInterleavedPresent(*GLRenderer->mPresent3dColumnShader);

	// Compute absolute offset from top of screen to top of current display window
	// because we need screen-relative, not window-relative, scan line parity
	int windowHOffset = 0;

#ifdef _WIN32
	if (!fullscreen) {
		I_SaveWindowedPos(); // update win_y CVAR
		windowHOffset = (win_x + WINDOW_BORDER_HORIZONTAL_PARITY) % 2;
	}
#endif // _WIN32

	GLRenderer->mPresent3dColumnShader->WindowPositionParity.Set(windowHOffset);

	GLRenderer->RenderScreenQuad();
}

void RowInterleaved3D::Present() const
{
	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);
	prepareInterleavedPresent(*GLRenderer->mPresent3dRowShader);

	// Compute absolute offset from top of screen to top of current display window
	// because we need screen-relative, not window-relative, scan line parity
	int windowVOffset = 0;
	
#ifdef _WIN32
	if (! fullscreen) {
		I_SaveWindowedPos(); // update win_y CVAR
		windowVOffset = (win_y + WINDOW_BORDER_VERTICAL_PARITY) % 2;
	}
#endif // _WIN32

	GLRenderer->mPresent3dRowShader->WindowPositionParity.Set(
		(windowVOffset
			+ screen->mOutputLetterbox.height + 1 // +1 because of origin at bottom
		) % 2
	);

	GLRenderer->RenderScreenQuad();
}


} /* namespace s3d */
