//
//---------------------------------------------------------------------------
//
// Copyright(C) 2016-2017 Christopher Bruns
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
** gl_openvr.h
** Stereoscopic virtual reality mode for the HTC Vive headset
*/

#ifndef GL_OPENVR_H_
#define GL_OPENVR_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"

// forward declaration from openvr.h
namespace vr {
	class IVRSystem;
	struct HmdMatrix44_t;
	struct Texture_t;
	struct TrackedDevicePose_t;
}

/* stereoscopic 3D API */
namespace s3d {

class OpenVREyePose : public ShiftedEyePose
{
public:
	OpenVREyePose(int eye);
	virtual ~OpenVREyePose() override;
	virtual VSMatrix GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const override;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const override;

	void initialize(vr::IVRSystem& vrsystem);
	void dispose();
	void setCurrentHmdPose(const vr::TrackedDevicePose_t * pose) const {currentPose = pose;}
	bool submitFrame() const;

protected:
	VSMatrix projectionMatrix;
	VSMatrix eyeToHeadTransform;
	VSMatrix otherEyeToHeadTransform;
	vr::Texture_t* eyeTexture;
	int eye;

	// TODO: adjust doomUnitsPerMeter according to player height
	float verticalDoomUnitsPerMeter;

	mutable const vr::TrackedDevicePose_t * currentPose;
};

class OpenVRMode : public Stereo3DMode
{
public:
	static const Stereo3DMode& getInstance(); // Might return Mono mode, if no HMD available

	virtual ~OpenVRMode() override;
	virtual void SetUp() const override; // called immediately before rendering a scene frame
	virtual void TearDown() const override; // called immediately after rendering a scene frame
	virtual void Present() const override;
	virtual void AdjustViewports() const override;
	virtual void Adjust2DMatrices(int eye_index) const override;

protected:
	OpenVRMode();
	// void updateDoomViewDirection() const;
	void updateHmdPose(double hmdYawRadians, double hmdPitchRadians, double hmdRollRadians) const;

	OpenVREyePose leftEyeView;
	OpenVREyePose rightEyeView;

	vr::IVRSystem* vrSystem;
	mutable int cachedScreenBlocks;

private:
	typedef Stereo3DMode super;
	bool hmdWasFound;
	uint32_t sceneWidth, sceneHeight;
};

} /* namespace st3d */


#endif /* GL_OPENVR_H_ */
