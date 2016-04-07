/*
** gl_stereo3d.cpp
** Stereoscopic 3D API
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

#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/renderer/gl_renderer.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI

namespace s3d {


/* virtual */
void EyePose::GetProjection(float fov, float aspectRatio, float fovRatio, float m[4][4]) const
{
	// Lifted from gl_scene.cpp FGLRenderer::SetProjection()
	float fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio)));
	const float zNear = 5.0;
	const float zFar = 65536.0;

	double radians = fovy / 2 * M_PI / 180;

	float deltaZ = zFar - zNear;
	double sine = sin(radians);
	if ((deltaZ == 0) || (sine == 0) || (aspectRatio == 0)) {
		return;
	}
	float cotangent = float(cos(radians) / sine);

	memset(m, 0, 16*sizeof(float));
	m[0][0] = cotangent / aspectRatio;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[2][3] = -1;
	m[3][2] = -2 * zNear * zFar / deltaZ;
	m[3][3] = 0;
}

/* virtual */
Viewport EyePose::GetViewport(const Viewport& fullViewport) const 
{
	return fullViewport;
}


/* virtual */
void EyePose::GetViewShift(float yaw, float outViewShift[3]) const
{
	// pass-through for Mono view
	outViewShift[0] = 0;
	outViewShift[1] = 0;
	outViewShift[2] = 0;
}


Stereo3DMode::Stereo3DMode()
{
}

Stereo3DMode::~Stereo3DMode()
{
}

// Avoid static initialization order fiasco by declaring first Mode type (Mono) here in the
// same source file as Stereo3DMode::getCurrentMode()
// https://isocpp.org/wiki/faq/ctors#static-init-order

/* static */
const MonoView& MonoView::getInstance() 
{
	static MonoView instance;
	return instance;
}

} /* namespace s3d */
