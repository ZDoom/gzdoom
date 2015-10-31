#ifndef GL_STEREO_LEFTRIGHT_H_
#define GL_STEREO_LEFTRIGHT_H_

#include "gl_stereo3d.h"

namespace s3d {


class ShiftedEyePose : public EyePose
{
public:
	ShiftedEyePose(FLOATTYPE shift) : shift(shift) {};
	FLOATTYPE getShift() const { return shift; }
	void setShift(FLOATTYPE shift) { this->shift = shift; }
	virtual void GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio, FLOATTYPE outMatrix[4][4]) const;
	virtual void GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const;
protected:
	FLOATTYPE shift;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(FLOATTYPE ipd) : ShiftedEyePose( FLOATTYPE(-0.5) * ipd) {}
	FLOATTYPE getIpd() const { return FLOATTYPE(-2.0)*getShift(); }
	void setIpd(FLOATTYPE ipd) { setShift(FLOATTYPE(-0.5)*ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(FLOATTYPE ipd) : ShiftedEyePose(FLOATTYPE(+0.5)*ipd) {}
	FLOATTYPE getIpd() const { return FLOATTYPE(+2.0)*shift; }
	void setIpd(FLOATTYPE ipd) { setShift(FLOATTYPE(+0.5)*ipd); }
};


/**
 * As if viewed through the left eye only
 */
class LeftEyeView : public Stereo3DMode
{
public:
	static const LeftEyeView& getInstance(FLOATTYPE ipd);

	LeftEyeView(FLOATTYPE ipd) : eye(ipd) { eye_ptrs.push_back(&eye); }
	FLOATTYPE getIpd() const { return eye.getIpd(); }
	void setIpd(FLOATTYPE ipd) { eye.setIpd(ipd); }
protected:
	LeftEyePose eye;
};


class RightEyeView : public Stereo3DMode
{
public:
	static const RightEyeView& getInstance(FLOATTYPE ipd);

	RightEyeView(FLOATTYPE ipd) : eye(ipd) { eye_ptrs.push_back(&eye); }
	FLOATTYPE getIpd() const { return eye.getIpd(); }
	void setIpd(FLOATTYPE ipd) { eye.setIpd(ipd); }
protected:
	RightEyePose eye;
};


} /* namespace s3d */

#endif /* GL_STEREO_LEFTRIGHT_H_ */
