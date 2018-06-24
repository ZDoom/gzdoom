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
	ShiftedEyePose(float shift) : shift(shift) {};
	float getShift() const;
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;

protected:
	void setShift(float shift) { this->shift = shift; }

private:
	float shift;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(float ipd) : ShiftedEyePose( -0.5f * ipd) {}
	void setIpd(float ipd) { setShift(-0.5f * ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(float ipd) : ShiftedEyePose(0.5f * ipd) {}
	void setIpd(float ipd) { setShift(0.5f * ipd); }
};


class SBSFLeftEyePose : public LeftEyePose {
public:
	SBSFLeftEyePose(float ipdMeters) : LeftEyePose(ipdMeters) {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const override {
		return LeftEyePose::GetProjection(fov, 0.5f * aspectRatio, fovRatio);
	}
};

class SBSFRightEyePose : public RightEyePose {
public:
	SBSFRightEyePose(float ipdMeters) : RightEyePose(ipdMeters) {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const override {
		return RightEyePose::GetProjection(fov, 0.5f * aspectRatio, fovRatio);
	}
};
