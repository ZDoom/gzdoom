/*
** gl_anaglyph.h
** Color mask based stereoscopic 3D modes for GZDoom
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

#ifndef GL_ANAGLYPH_H_
#define GL_ANAGLYPH_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "gl/system/gl_system.h"
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
	virtual void SetUp() const { 
		gl_RenderState.SetColorMask(colorMask.r, colorMask.g, colorMask.b, true);
		gl_RenderState.ApplyColorMask();
	}
	virtual void TearDown() const { 
		gl_RenderState.ResetColorMask();
		gl_RenderState.ApplyColorMask();
	}
private:
	ColorMask colorMask;
};

class AnaglyphRightPose : public RightEyePose
{
public:
	AnaglyphRightPose(const ColorMask& colorMask, float ipd) : RightEyePose(ipd), colorMask(colorMask) {}
	virtual void SetUp() const { 
		gl_RenderState.SetColorMask(colorMask.r, colorMask.g, colorMask.b, true);
		gl_RenderState.ApplyColorMask();
	}
	virtual void TearDown() const { 
		gl_RenderState.ResetColorMask();
		gl_RenderState.ApplyColorMask();
	}
private:
	ColorMask colorMask;
};

class MaskAnaglyph : public Stereo3DMode
{
public:
	MaskAnaglyph(const ColorMask& leftColorMask, double ipdMeters);
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
