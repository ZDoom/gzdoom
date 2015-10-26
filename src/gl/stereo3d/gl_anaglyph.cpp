#include "gl_anaglyph.h"

namespace s3d {

MaskAnaglyph::MaskAnaglyph(const ColorMask& leftColorMask, double ipdMeters)
	: leftEye(leftColorMask, ipdMeters), rightEye(leftColorMask.inverse(), ipdMeters)
{
	eye_ptrs.push_back(&leftEye);
	eye_ptrs.push_back(&rightEye);
}


/* static */
const GreenMagenta& GreenMagenta::getInstance(float ipd)
{
	static GreenMagenta instance(ipd);
	return instance;
}


/* static */
const RedCyan& RedCyan::getInstance(float ipd)
{
	static RedCyan instance(ipd);
	return instance;
}


} /* namespace s3d */
