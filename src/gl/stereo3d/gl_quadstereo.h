/*
** gl_quadstereo.h
** Quad-buffered OpenGL stereoscopic 3D mode for GZDoom
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

#ifndef GL_QUADSTEREO_H_
#define GL_QUADSTEREO_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "gl/system/gl_system.h"

namespace s3d {


class QuadStereoLeftPose : public LeftEyePose
{
public:
	QuadStereoLeftPose(float ipd) : LeftEyePose(ipd), bQuadStereoSupported(false) {}
	virtual void SetUp() const {
		if (bQuadStereoSupported)
			glDrawBuffer(GL_BACK_LEFT);
	}
	virtual void TearDown() const { 
		if (bQuadStereoSupported)
			glDrawBuffer(GL_BACK);
	}
	bool bQuadStereoSupported;
};

class QuadStereoRightPose : public RightEyePose
{
public:
	QuadStereoRightPose(float ipd) : RightEyePose(ipd), bQuadStereoSupported(false){}
	virtual void SetUp() const { 
		if (bQuadStereoSupported)
			glDrawBuffer(GL_BACK_RIGHT);
	}
	virtual void TearDown() const { 
		if (bQuadStereoSupported)
			glDrawBuffer(GL_BACK);
	}
	bool bQuadStereoSupported;
};

// To use Quad-buffered stereo mode with nvidia 3d vision glasses, 
// you must either:
// A) be using a Quadro series video card, OR
//
// B) be using nvidia driver version 314.07 or later
//    AND have your monitor set to 120 Hz refresh rate
//    AND have gzdoom in true full screen mode
class QuadStereo : public Stereo3DMode
{
public:
	QuadStereo(double ipdMeters);
	static const QuadStereo& getInstance(float ipd);
private:
	QuadStereoLeftPose leftEye;
	QuadStereoRightPose rightEye;
};


} /* namespace s3d */


#endif /* GL_QUADSTEREO_H_ */
