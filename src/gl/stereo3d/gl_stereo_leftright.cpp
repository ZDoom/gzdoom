/*
** gl_stereo_leftright.cpp
** Offsets for left and right eye views
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

#include "gl_stereo_leftright.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include <cmath>

EXTERN_CVAR(Float, vr_screendist)
EXTERN_CVAR(Float, vr_hunits_per_meter)

namespace s3d {


/* virtual */
VSMatrix ShiftedEyePose::GetProjection(float fov, float aspectRatio, float fovRatio) const
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
void ShiftedEyePose::GetViewShift(float yaw, float outViewShift[3]) const
{
	float dx = -cos(DEG2RAD(yaw)) * vr_hunits_per_meter * shift;
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
