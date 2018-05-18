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
** gl_anaglyph.h
** Color mask based stereoscopic 3D modes for GZDoom
**
*/

#ifndef GL_ANAGLYPH_H_
#define GL_ANAGLYPH_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderstate.h"


namespace s3d {


class ColorMask
{
public:
	ColorMask(bool r, bool g, bool b) : r(r), g(g), b(b) {}
	ColorMask inverse() const { return ColorMask(!r, !g, !b); }

	bool r;
	bool g;
	bool b;
};


class AnaglyphLeftPose : public LeftEyePose
{
public:
	AnaglyphLeftPose(const ColorMask& colorMask, float ipd) : LeftEyePose(ipd), colorMask(colorMask) {}
	ColorMask GetColorMask() const { return colorMask; }

private:
	ColorMask colorMask;
};

class AnaglyphRightPose : public RightEyePose
{
public:
	AnaglyphRightPose(const ColorMask& colorMask, float ipd) : RightEyePose(ipd), colorMask(colorMask) {}
	ColorMask GetColorMask() const { return colorMask; }

private:
	ColorMask colorMask;
};

class MaskAnaglyph : public Stereo3DMode
{
public:
	MaskAnaglyph(const ColorMask& leftColorMask, double ipdMeters);
	void Present() const override;
private:
	AnaglyphLeftPose leftEye;
	AnaglyphRightPose rightEye;
};


class RedCyan : public MaskAnaglyph
{
public:
	static const RedCyan& getInstance(float ipd);

	RedCyan(float ipd) : MaskAnaglyph(ColorMask(true, false, false), ipd) {}
};

class GreenMagenta : public MaskAnaglyph
{
public:
	static const GreenMagenta& getInstance(float ipd);

	GreenMagenta(float ipd) : MaskAnaglyph(ColorMask(false, true, false), ipd) {}
};

class AmberBlue : public MaskAnaglyph
{
public:
	static const AmberBlue& getInstance(float ipd);

	AmberBlue(float ipd) : MaskAnaglyph(ColorMask(true, true, false), ipd) {}
};

// TODO matrix anaglyph


} /* namespace s3d */


#endif /* GL_ANAGLYPH_H_ */
