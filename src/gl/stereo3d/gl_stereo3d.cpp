#include "gl/stereo3d/gl_stereo3d.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI

namespace s3d {


/* virtual */
VSMatrix EyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	VSMatrix result;

	// Lifted from gl_scene.cpp FGLRenderer::SetProjection()
	float fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio)));
	result.perspective(fovy, aspectRatio, 5.f, 65536.f);

	return result;
}

/* virtual */
Viewport EyePose::GetViewport(const Viewport& fullViewport) const 
{
	return fullViewport;
}


/* virtual */
void EyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
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
