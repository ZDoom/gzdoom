#ifndef GL_ANAGLYPH_H_
#define GL_ANAGLYPH_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"


namespace s3d {


class ColorMask
{
public:
	ColorMask(bool r, bool g, bool b) : r(r), g(g), b(b) {}
	ColorMask inverse() const { return ColorMask(!r, !g, !b); }

	GLboolean r;
	GLboolean g;
	GLboolean b;
};


class AnaglyphLeftPose : public LeftEyePose
{
public:
	AnaglyphLeftPose(const ColorMask& colorMask, float ipd) : LeftEyePose(ipd), colorMask(colorMask) {}
	virtual void SetUp() const { glColorMask(colorMask.r, colorMask.g, colorMask.b, true); }
	virtual void TearDown() const { glColorMask(1,1,1,1); }
private:
	ColorMask colorMask;
};

class AnaglyphRightPose : public RightEyePose
{
public:
	AnaglyphRightPose(const ColorMask& colorMask, float ipd) : RightEyePose(ipd), colorMask(colorMask) {}
	virtual void SetUp() const { glColorMask(colorMask.r, colorMask.g, colorMask.b, true); }
	virtual void TearDown() const { glColorMask(1,1,1,1); }
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

// TODO matrix anaglyph


} /* namespace st3d */


#endif /* GL_ANAGLYPH_H_ */
