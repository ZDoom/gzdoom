/*
** gl_stereo_leftright.h
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

#ifndef GL_STEREO_LEFTRIGHT_H_
#define GL_STEREO_LEFTRIGHT_H_

#include "gl_stereo3d.h"

namespace s3d {


class ShiftedEyePose : public EyePose
{
public:
	ShiftedEyePose(float shift) : shift(shift) {};
	float getShift() const { return shift; }
	void setShift(float shift) { this->shift = shift; }
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;
protected:
	float shift;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(float ipd) : ShiftedEyePose( float(-0.5) * ipd) {}
	float getIpd() const { return float(-2.0)*getShift(); }
	void setIpd(float ipd) { setShift(float(-0.5)*ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(float ipd) : ShiftedEyePose(float(+0.5)*ipd) {}
	float getIpd() const { return float(+2.0)*shift; }
	void setIpd(float ipd) { setShift(float(+0.5)*ipd); }
};


/**
 * As if viewed through the left eye only
 */
class LeftEyeView : public Stereo3DMode
{
public:
	static const LeftEyeView& getInstance(float ipd);

	LeftEyeView(float ipd) : eye(ipd) { eye_ptrs.Push(&eye); }
	float getIpd() const { return eye.getIpd(); }
	void setIpd(float ipd) { eye.setIpd(ipd); }
protected:
	LeftEyePose eye;
};


class RightEyeView : public Stereo3DMode
{
public:
	static const RightEyeView& getInstance(float ipd);

	RightEyeView(float ipd) : eye(ipd) { eye_ptrs.Push(&eye); }
	float getIpd() const { return eye.getIpd(); }
	void setIpd(float ipd) { eye.setIpd(ipd); }
protected:
	RightEyePose eye;
};


} /* namespace s3d */

#endif /* GL_STEREO_LEFTRIGHT_H_ */
