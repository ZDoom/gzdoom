#pragma once

#include "r_data/matrix.h"


/* Viewpoint of one eye */
class EyePose 
{
public:
	EyePose() {}
	virtual ~EyePose() {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;
	virtual void SetUp() const {}
};

class ShiftedEyePose : public EyePose
{
public:
	ShiftedEyePose(float shift, float squish) : shift(shift), squish(squish) {};
	float getShift() const;
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;

protected:
	void setShift(float shift) { this->shift = shift; }

private:
	float shift;
	float squish;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(float ipd, float squish = 1.f) : ShiftedEyePose( -0.5f * ipd, squish) {}
	void setIpd(float ipd) { setShift(-0.5f * ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(float ipd, float squish = 1.f) : ShiftedEyePose(0.5f * ipd, squish) {}
	void setIpd(float ipd) { setShift(0.5f * ipd); }
};
