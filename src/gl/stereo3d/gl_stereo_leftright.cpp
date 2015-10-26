#include "gl_stereo_leftright.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "gl/system/gl_cvars.h"
#include <cmath>

EXTERN_CVAR(Float, vr_screendist)
EXTERN_CVAR(Float, vr_hunits_per_meter)

namespace s3d {


/* virtual */
void ShiftedEyePose::GetProjection(float fov, float aspectRatio, float fovRatio, GLdouble m[4][4]) const
{
	// Lifted from gl_scene.cpp FGLRenderer::SetProjection()
	float fovy = 2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio));
	double zNear = 5.0;
	double zFar = 65536.0;

	// For stereo 3D, use asymmetric frustum shift in projection matrix
	// Q: shouldn't shift vary with roll angle, at least for desktop display?
	// A: (lab) roll is not measured on desktop display (yet)
	double frustumShift = zNear * shift / vr_screendist; // meters cancel
	// double frustumShift = 0; // Turning off shift for debugging
	double fH = tan(fovy / 360 * M_PI) * zNear;
	double fW = fH * aspectRatio;
	// Emulate glFrustum command:
	// glFrustum(-fW - frustumShift, fW - frustumShift, -fH, fH, zNear, zFar);
	double left = -fW - frustumShift;
	double right = fW - frustumShift;
	double bottom = -fH;
	double top = fH;
	double deltaZ = zFar - zNear;

	memset(m, 0, 16 * sizeof(GLdouble)); // set all elements to zero, cleverly

	// https://www.opengl.org/sdk/docs/man2/xhtml/glFrustum.xml
	m[0][0] = 2 * zNear / (right - left);
	m[1][1] = 2 * zNear / (top - bottom);
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[2][3] = -1;
	m[3][2] = -2 * zNear * zFar / deltaZ;
	// m[3][3] = 0; // redundant
	// m[2][1] = (top + bottom) / (top - bottom); // zero for the cases I know of...
	m[2][0] = (right + left) / (right - left); // asymmetric shift is in this term
}


/* virtual */
void ShiftedEyePose::GetViewShift(float yaw, float outViewShift[3]) const
{
	float dx = cos(DEG2RAD(yaw)) * vr_hunits_per_meter * shift;
	float dy = sin(DEG2RAD(yaw)) * vr_hunits_per_meter * shift;
	outViewShift[0] = dx;
	outViewShift[1] = dy;
	outViewShift[2] = 0;
}


/* static */
const LeftEyeView& LeftEyeView::getInstance(float ipd)
{
	static LeftEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}


/* static */
const RightEyeView& RightEyeView::getInstance(float ipd)
{
	static RightEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}


} /* namespace s3d */
