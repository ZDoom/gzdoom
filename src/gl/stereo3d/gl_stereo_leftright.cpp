#include "gl_stereo_leftright.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "gl/system/gl_cvars.h"
#include <cmath>

EXTERN_CVAR(Float, vr_screendist)
EXTERN_CVAR(Float, vr_hunits_per_meter)

namespace s3d {


/* virtual */
VSMatrix ShiftedEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	double zNear = 5.0;
	double zFar = 65536.0;

	// For stereo 3D, use asymmetric frustum shift in projection matrix
	// Q: shouldn't shift vary with roll angle, at least for desktop display?
	// A: No. (lab) roll is not measured on desktop display (yet)
	double frustumShift = zNear * shift / vr_screendist; // meters cancel, leaving doom units
	// double frustumShift = 0; // Turning off shift for debugging
	double fH = zNear * tan(DEG2RAD(fov) / 2) / fovRatio;
	double fW = fH * aspectRatio;
	double left = -fW - frustumShift;
	double right = fW - frustumShift;
	double bottom = -fH;
	double top = fH;

	VSMatrix result(1);
	result.frustum(left, right, bottom, top, zNear, zFar);
	return result;
}


/* virtual */
void ShiftedEyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
{
	FLOATTYPE dx = cos(DEG2RAD(yaw)) * vr_hunits_per_meter * shift;
	FLOATTYPE dy = sin(DEG2RAD(yaw)) * vr_hunits_per_meter * shift;
	outViewShift[0] = dx;
	outViewShift[1] = dy;
	outViewShift[2] = 0;
}


/* static */
const LeftEyeView& LeftEyeView::getInstance(FLOATTYPE ipd)
{
	static LeftEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}


/* static */
const RightEyeView& RightEyeView::getInstance(FLOATTYPE ipd)
{
	static RightEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}


} /* namespace s3d */
