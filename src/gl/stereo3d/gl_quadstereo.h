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
** gl_quadstereo.h
** Quad-buffered OpenGL stereoscopic 3D mode for GZDoom
**
*/

#ifndef GL_QUADSTEREO_H_
#define GL_QUADSTEREO_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "gl_load/gl_system.h"

namespace s3d {


class QuadStereoLeftPose : public LeftEyePose
{
public:
	QuadStereoLeftPose(float ipd) : LeftEyePose(ipd), bQuadStereoSupported(false) {}
	bool bQuadStereoSupported;
};

class QuadStereoRightPose : public RightEyePose
{
public:
	QuadStereoRightPose(float ipd) : RightEyePose(ipd), bQuadStereoSupported(false){}
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
	void Present() const override;
	void SetUp() const override;
	static const QuadStereo& getInstance(float ipd);
private:
	QuadStereoLeftPose leftEye;
	QuadStereoRightPose rightEye;
	bool bQuadStereoSupported;
	void checkInitialRenderContextState();
};


} /* namespace s3d */


#endif /* GL_QUADSTEREO_H_ */
