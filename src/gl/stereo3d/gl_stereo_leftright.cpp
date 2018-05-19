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
** gl_stereo_leftright.cpp
** Offsets for left and right eye views
**
*/

#include "gl_stereo_leftright.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "hwrenderer/utility/hw_cvars.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"

EXTERN_CVAR(Float, vr_screendist)
EXTERN_CVAR(Float, vr_hunits_per_meter)
EXTERN_CVAR(Bool, vr_swap_eyes)

namespace s3d {


/* virtual */
VSMatrix ShiftedEyePose::GetProjection(float fov, float aspectRatio, float fovRatio) const
{
	double zNear = 5.0;
	double zFar = 65536.0;

	// For stereo 3D, use asymmetric frustum shift in projection matrix
	// Q: shouldn't shift vary with roll angle, at least for desktop display?
	// A: No. (lab) roll is not measured on desktop display (yet)
	double frustumShift = zNear * getShift() / vr_screendist; // meters cancel, leaving doom units
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
	float dx = -cos(DEG2RAD(yaw)) * vr_hunits_per_meter * getShift();
	float dy = sin(DEG2RAD(yaw)) * vr_hunits_per_meter * getShift();
	outViewShift[0] = dx;
	outViewShift[1] = dy;
	outViewShift[2] = 0;
}

float ShiftedEyePose::getShift() const 
{
	return vr_swap_eyes ? -shift : shift;
}

/* static */
const LeftEyeView& LeftEyeView::getInstance(float ipd)
{
	static LeftEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}

void LeftEyeView::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);
}

/* static */
const RightEyeView& RightEyeView::getInstance(float ipd)
{
	static RightEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}

void RightEyeView::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);
}


} /* namespace s3d */
