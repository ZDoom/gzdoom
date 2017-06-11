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
** gl_openvr.cpp
** Stereoscopic virtual reality mode for the HTC Vive headset
**
*/

#ifdef USE_OPENVR

#include "gl_openvr.h"
#include "openvr_capi.h"
#include <string>
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "g_levellocals.h" // pixelstretch
#include "math/cmath.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "LSMatrix.h"

#ifdef DYN_OPENVR
// Dynamically load OpenVR

#include "i_module.h"
FModule OpenVRModule{ "OpenVR" };

/** Pointer-to-function type, useful for dynamically getting OpenVR entry points. */
// Derived from global entry at the bottom of openvr_capi.h, plus a few other functions
typedef intptr_t (*LVR_InitInternal)(EVRInitError *peError, EVRApplicationType eType);
typedef void (*LVR_ShutdownInternal)();
typedef bool (*LVR_IsHmdPresent)();
typedef intptr_t (*LVR_GetGenericInterface)(const char *pchInterfaceVersion, EVRInitError *peError);
typedef bool (*LVR_IsRuntimeInstalled)();
typedef const char * (*LVR_GetVRInitErrorAsSymbol)(EVRInitError error);
typedef const char * (*LVR_GetVRInitErrorAsEnglishDescription)(EVRInitError error);
typedef bool (*LVR_IsInterfaceVersionValid)(const char * version);
typedef uint32_t (*LVR_GetInitToken)();

#define DEFINE_ENTRY(name) static TReqProc<OpenVRModule, L##name> name{#name};
DEFINE_ENTRY(VR_InitInternal)
DEFINE_ENTRY(VR_ShutdownInternal)
DEFINE_ENTRY(VR_IsHmdPresent)
DEFINE_ENTRY(VR_GetGenericInterface)
DEFINE_ENTRY(VR_IsRuntimeInstalled)
DEFINE_ENTRY(VR_GetVRInitErrorAsSymbol)
DEFINE_ENTRY(VR_GetVRInitErrorAsEnglishDescription)
DEFINE_ENTRY(VR_IsInterfaceVersionValid)
DEFINE_ENTRY(VR_GetInitToken)

#ifdef _WIN32
#define OPENVRLIB "openvr_api.dll"
#elif defined(__APPLE__)
#define OPENVRLIB "libopenvr_api.dylib"
#else
#define OPENVRLIB "libopenvr_api.so"
#endif

#else
// Non-dynamic loading of OpenVR

// OpenVR Global entry points
S_API intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsRuntimeInstalled();
S_API const char * VR_GetVRInitErrorAsSymbol(EVRInitError error);
S_API const char * VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);
S_API bool VR_IsInterfaceVersionValid(const char * version);
S_API uint32_t VR_GetInitToken();

#endif

bool IsOpenVRPresent()
{
#ifndef USE_OPENVR
	return false;
#elif !defined DYN_OPENVR
	return true;
#else
	static bool cached_result = false;
	static bool done = false;

	if (!done)
	{
		done = true;
		cached_result = OpenVRModule.Load({ NicePath("$PROGDIR/" OPENVRLIB), OPENVRLIB });
	}
	return cached_result;
#endif
}

// For conversion between real-world and doom units
#define VERTICAL_DOOM_UNITS_PER_METER 27.0f

EXTERN_CVAR(Int, screenblocks);

// feature toggles, for testing and debugging
static const bool doTrackHmdYaw = true;
static const bool doTrackHmdPitch = true;
static const bool doTrackHmdRoll = true;
static const bool doLateScheduledRotationTracking = true;
static const bool doStereoscopicViewpointOffset = true;
static const bool doRenderToDesktop = true; // mirroring to the desktop is very helpful for debugging
static const bool doRenderToHmd = true;
static const bool doTrackHmdVerticalPosition = false; // todo:
static const bool doTrackHmdHorizontalPostion = false; // todo:
static const bool doTrackVrControllerPosition = false; // todo:

namespace s3d 
{

/* static */
const Stereo3DMode& OpenVRMode::getInstance()
{
		static OpenVRMode instance;
		if (! instance.hmdWasFound)
			return  MonoView::getInstance();
		return instance;
}

static HmdVector3d_t eulerAnglesFromQuat(HmdQuaternion_t quat) {
	double q0 = quat.w;
	// permute axes to make "Y" up/yaw
	double q2 = quat.x;
	double q3 = quat.y;
	double q1 = quat.z;

	// http://stackoverflow.com/questions/18433801/converting-a-3x3-matrix-to-euler-tait-bryan-angles-pitch-yaw-roll
	double roll = atan2(2 * (q0*q1 + q2*q3), 1 - 2 * (q1*q1 + q2*q2));
	double pitch = asin(2 * (q0*q2 - q3*q1));
	double yaw = atan2(2 * (q0*q3 + q1*q2), 1 - 2 * (q2*q2 + q3*q3));

	return HmdVector3d_t{ yaw, pitch, roll };
}

static HmdQuaternion_t quatFromMatrix(HmdMatrix34_t matrix) {
	HmdQuaternion_t q;
	typedef float f34[3][4];
	f34& a = matrix.m;
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	float trace = a[0][0] + a[1][1] + a[2][2]; // I removed + 1.0f; see discussion with Ethan
	if (trace > 0) {// I changed M_EPSILON to 0
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (a[2][1] - a[1][2]) * s;
		q.y = (a[0][2] - a[2][0]) * s;
		q.z = (a[1][0] - a[0][1]) * s;
	}
	else {
		if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);
			q.w = (a[2][1] - a[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (a[0][1] + a[1][0]) / s;
			q.z = (a[0][2] + a[2][0]) / s;
		}
		else if (a[1][1] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);
			q.w = (a[0][2] - a[2][0]) / s;
			q.x = (a[0][1] + a[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (a[1][2] + a[2][1]) / s;
		}
		else {
			float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);
			q.w = (a[1][0] - a[0][1]) / s;
			q.x = (a[0][2] + a[2][0]) / s;
			q.y = (a[1][2] + a[2][1]) / s;
			q.z = 0.25f * s;
		}
	}

	return q;
}

static HmdVector3d_t eulerAnglesFromMatrix(HmdMatrix34_t mat) {
	return eulerAnglesFromQuat(quatFromMatrix(mat));
}

OpenVREyePose::OpenVREyePose(int eye)
	: ShiftedEyePose( 0.0f )
	, eye(eye)
	, eyeTexture(nullptr)
	, currentPose(nullptr)
{
}


/* virtual */
OpenVREyePose::~OpenVREyePose() 
{
	dispose();
}

static void vSMatrixFromHmdMatrix34(VSMatrix& m1, const HmdMatrix34_t& m2)
{
	float tmp[16];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp[4 * i + j] = m2.m[i][j];
		}
	}
	int i = 3;
	for (int j = 0; j < 4; ++j) {
		tmp[4 * i + j] = 0;
	}
	tmp[15] = 1;
	m1.loadMatrix(&tmp[0]);
}


/* virtual */
void OpenVREyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
{
	outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

	if (currentPose == nullptr)
		return;
	const TrackedDevicePose_t& hmd = *currentPose;
	if (! hmd.bDeviceIsConnected)
		return;
	if (! hmd.bPoseIsValid)
		return;

	if (! doStereoscopicViewpointOffset)
		return;

	const HmdMatrix34_t& hmdPose = hmd.mDeviceToAbsoluteTracking;

	// Pitch and Roll are identical between OpenVR and Doom worlds.
	// But yaw can differ, depending on starting state, and controller movement.
	float doomYawDegrees = yaw;
	float openVrYawDegrees = RAD2DEG(-eulerAnglesFromMatrix(hmdPose).v[0]);
	float deltaYawDegrees = doomYawDegrees - openVrYawDegrees;
	while (deltaYawDegrees > 180)
		deltaYawDegrees -= 360;
	while (deltaYawDegrees < -180)
		deltaYawDegrees += 360;

	// extract rotation component from hmd transform
	LSMatrix44 openvr_X_hmd(hmdPose);
	LSMatrix44 hmdRot = openvr_X_hmd.getWithoutTranslation(); // .transpose();

	/// In these eye methods, just get local inter-eye stereoscopic shift, not full position shift ///

	// compute local eye shift
	LSMatrix44 eyeShift2;
	eyeShift2.loadIdentity();
	eyeShift2 = eyeShift2 * eyeToHeadTransform; // eye to head
	eyeShift2 = eyeShift2 * hmdRot; // head to openvr

	LSVec3 eye_EyePos = LSVec3(0, 0, 0); // eye position in eye frame
	LSVec3 hmd_EyePos = LSMatrix44(eyeToHeadTransform) * eye_EyePos;
	LSVec3 hmd_HmdPos = LSVec3(0, 0, 0); // hmd position in hmd frame
	LSVec3 openvr_EyePos = openvr_X_hmd * hmd_EyePos;
	LSVec3 openvr_HmdPos = openvr_X_hmd * hmd_HmdPos;
	LSVec3 hmd_OtherEyePos = LSMatrix44(otherEyeToHeadTransform) * eye_EyePos;
	LSVec3 openvr_OtherEyePos = openvr_X_hmd * hmd_OtherEyePos;
	LSVec3 openvr_EyeOffset = openvr_EyePos - openvr_HmdPos;

	VSMatrix doomInOpenVR = VSMatrix();
	doomInOpenVR.loadIdentity();
	// permute axes
	float permute[] = { // Convert from OpenVR to Doom axis convention, including mirror inversion
		-1,  0,  0,  0, // X-right in OpenVR -> X-left in Doom
			0,  0,  1,  0, // Z-backward in OpenVR -> Y-backward in Doom
			0,  1,  0,  0, // Y-up in OpenVR -> Z-up in Doom
			0,  0,  0,  1};
	doomInOpenVR.multMatrix(permute);
	doomInOpenVR.scale(VERTICAL_DOOM_UNITS_PER_METER, VERTICAL_DOOM_UNITS_PER_METER, VERTICAL_DOOM_UNITS_PER_METER); // Doom units are not meters
	doomInOpenVR.scale(level.info->pixelstretch, level.info->pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio
	doomInOpenVR.rotate(deltaYawDegrees, 0, 0, 1);

	LSVec3 doom_EyeOffset = LSMatrix44(doomInOpenVR) * openvr_EyeOffset;
	outViewShift[0] = doom_EyeOffset[0];
	outViewShift[1] = doom_EyeOffset[1];
	outViewShift[2] = doom_EyeOffset[2];
}

/* virtual */
VSMatrix OpenVREyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	// Ignore those arguments and get the projection from the SDK
	// VSMatrix vs1 = ShiftedEyePose::GetProjection(fov, aspectRatio, fovRatio);
	return projectionMatrix;
}

void OpenVREyePose::initialize(VR_IVRSystem_FnTable * vrsystem)
{
	float zNear = 5.0;
	float zFar = 65536.0;
	HmdMatrix44_t projection = vrsystem->GetProjectionMatrix(
			EVREye(eye), zNear, zFar);
	HmdMatrix44_t proj_transpose;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			proj_transpose.m[i][j] = projection.m[j][i];
		}
	}
	projectionMatrix.loadIdentity();
	projectionMatrix.multMatrix(&proj_transpose.m[0][0]);

	HmdMatrix34_t eyeToHead = vrsystem->GetEyeToHeadTransform(EVREye(eye));
	vSMatrixFromHmdMatrix34(eyeToHeadTransform, eyeToHead);
	HmdMatrix34_t otherEyeToHead = vrsystem->GetEyeToHeadTransform(eye == EVREye_Eye_Left ? EVREye_Eye_Right : EVREye_Eye_Left);
	vSMatrixFromHmdMatrix34(otherEyeToHeadTransform, otherEyeToHead);

	if (eyeTexture == nullptr)
		eyeTexture = new Texture_t();
	eyeTexture->handle = nullptr; // TODO: populate this at resolve time
	eyeTexture->eType = ETextureType_TextureType_OpenGL;
	eyeTexture->eColorSpace = EColorSpace_ColorSpace_Linear;
}

void OpenVREyePose::dispose()
{
	if (eyeTexture) {
		delete eyeTexture;
		eyeTexture = nullptr;
	}
}

bool OpenVREyePose::submitFrame(VR_IVRCompositor_FnTable * vrCompositor) const
{
	if (eyeTexture == nullptr)
		return false;
	if (vrCompositor == nullptr)
		return false;
	// Copy HDR framebuffer into 24-bit RGB texture
	GLRenderer->mBuffers->BindEyeFB(eye, true);
	if (eyeTexture->handle == nullptr) {
		GLuint handle;
		glGenTextures(1, &handle);
		eyeTexture->handle = (void *)(std::ptrdiff_t)handle;
		glBindTexture(GL_TEXTURE_2D, handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, GLRenderer->mSceneViewport.width,
			GLRenderer->mSceneViewport.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)(std::ptrdiff_t)eyeTexture->handle);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 0, 0,
		GLRenderer->mSceneViewport.width,
		GLRenderer->mSceneViewport.height, 0);
	vrCompositor->Submit(EVREye(eye), eyeTexture, nullptr, EVRSubmitFlags_Submit_Default);
	return true;
}

void OpenVREyePose::Adjust2DMatrices() const
{
	const bool doAdjust2D = true; // Don't use default 2D projection
	const bool doRecomputeDefault2D = false; // For debugging, recapitulate default 2D projection

	if (!doAdjust2D) // for debugging
		return;

	VSMatrix new_projection;
	new_projection.loadIdentity();
	if (doRecomputeDefault2D) { // for debugging
								// quad coordinates from pixel coordinates [xy range -1,1]
		new_projection.translate(-1, 1, 0);
		new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
	}
	else {
		// doom_units from meters
		new_projection.scale(
			-VERTICAL_DOOM_UNITS_PER_METER,
			VERTICAL_DOOM_UNITS_PER_METER,
			-VERTICAL_DOOM_UNITS_PER_METER);
		if (level.info != nullptr)
			new_projection.scale(level.info->pixelstretch, level.info->pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

		// eye coordinates from hmd coordinates
		LSMatrix44 e2h(eyeToHeadTransform);
		new_projection.multMatrix(e2h.transpose());

		if (currentPose != nullptr) {
			// Un-apply HMD yaw angle, to keep the menu in front of the player
			float openVrYawDegrees = RAD2DEG(-eulerAnglesFromMatrix(currentPose->mDeviceToAbsoluteTracking).v[0]);
			new_projection.rotate(openVrYawDegrees, 0, 1, 0);

			// apply inverse of hmd rotation, to keep the menu fixed in the world
			LSMatrix44 hmdPose(currentPose->mDeviceToAbsoluteTracking);
			hmdPose = hmdPose.getWithoutTranslation();
			new_projection.multMatrix(hmdPose.transpose());
		}

		// Tilt the HUD downward, so its not so high up
		new_projection.rotate(15, 1, 0, 0);

		// hmd coordinates (meters) from ndc coordinates
		const float menu_distance_meters = 1.0f;
		const float menu_width_meters = 0.4f * menu_distance_meters;
		const float aspect = SCREENWIDTH / float(SCREENHEIGHT);
		new_projection.translate(0.0, 0.0, menu_distance_meters);
		new_projection.scale(
			-menu_width_meters,
			menu_width_meters / aspect,
			-menu_width_meters);

		// ndc coordinates from pixel coordinates
		new_projection.translate(-1.0, 1.0, 0);
		new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

		// projection matrix - clip coordinates from eye coordinates
		VSMatrix proj(projectionMatrix);
		proj.multMatrix(new_projection);
		new_projection = proj;
	}

	gl_RenderState.mProjectionMatrix = new_projection;
	gl_RenderState.ApplyMatrices();
}

OpenVRMode::OpenVRMode() 
	: vrSystem(nullptr)
	, leftEyeView(EVREye_Eye_Left)
	, rightEyeView(EVREye_Eye_Right)
	, hmdWasFound(false)
	, sceneWidth(0), sceneHeight(0)
	, vrCompositor(nullptr)
	, vrToken(0)
{
	eye_ptrs.Push(&leftEyeView); // initially default behavior to Mono non-stereo rendering

	if ( ! IsOpenVRPresent() ) return; // failed to load openvr API dynamically

	if ( ! VR_IsRuntimeInstalled() ) return; // failed to find OpenVR implementation

	if ( ! VR_IsHmdPresent() ) return; // no VR headset is attached

	EVRInitError eError;
	// Code below recapitulates the effects of C++ call vr::VR_Init()
	VR_InitInternal(&eError, EVRApplicationType_VRApplication_Scene);
	if (eError != EVRInitError_VRInitError_None) {
		std::string errMsg = VR_GetVRInitErrorAsEnglishDescription(eError);
		return;
	}
	if (! VR_IsInterfaceVersionValid(IVRSystem_Version))
	{
		VR_ShutdownInternal();
		return;
	}
	vrToken = VR_GetInitToken();
	const std::string sys_key = std::string("FnTable:") + std::string(IVRSystem_Version);
	vrSystem = (VR_IVRSystem_FnTable*) VR_GetGenericInterface(sys_key.c_str() , &eError);
	if (vrSystem == nullptr)
		return;

	vrSystem->GetRecommendedRenderTargetSize(&sceneWidth, &sceneHeight);

	leftEyeView.initialize(vrSystem);
	rightEyeView.initialize(vrSystem);

	const std::string comp_key = std::string("FnTable:") + std::string(IVRCompositor_Version);
	vrCompositor = (VR_IVRCompositor_FnTable*)VR_GetGenericInterface(comp_key.c_str(), &eError);
	if (vrCompositor == nullptr)
		return;

	eye_ptrs.Push(&rightEyeView); // NOW we render to two eyes
	hmdWasFound = true;
}

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void OpenVRMode::AdjustViewports() const
{
	// Draw the 3D scene into the entire framebuffer
	GLRenderer->mSceneViewport.width = sceneWidth;
	GLRenderer->mSceneViewport.height = sceneHeight;
	GLRenderer->mSceneViewport.left = 0;
	GLRenderer->mSceneViewport.top = 0;

	GLRenderer->mScreenViewport.width = sceneWidth;
	GLRenderer->mScreenViewport.height = sceneHeight;
}

void OpenVRMode::AdjustPlayerSprites() const
{
	Stereo3DMode::AdjustPlayerSprites();

	// TODO: put weapon onto a 3D quad
	const bool doProjectWeaponSprite = true; // Don't use default 2D projection
	const bool doRecomputeDefault2D = false; // For debugging, recapitulate default 2D projection

	if (!doProjectWeaponSprite) // for debugging
		return;

	VSMatrix new_projection;
	new_projection.loadIdentity();
	if (doRecomputeDefault2D) { // for debugging
								// quad coordinates from pixel coordinates [xy range -1,1]
		new_projection.translate(-1, 1, 0);
		new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
	}
	else {
		// doom_units from meters
		new_projection.scale(
			-VERTICAL_DOOM_UNITS_PER_METER,
			 VERTICAL_DOOM_UNITS_PER_METER,
			-VERTICAL_DOOM_UNITS_PER_METER);
		new_projection.scale(level.info->pixelstretch, level.info->pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

		const OpenVREyePose * activeEye = &rightEyeView;
		if (! activeEye->isActive())
			activeEye = &leftEyeView;

		// eye coordinates from hmd coordinates
		LSMatrix44 e2h(activeEye->eyeToHeadTransform);
		new_projection.multMatrix(e2h.transpose());

		// Follow HMD orientation, EXCEPT for roll angle (keep weapon upright)
		float openVrRollDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[2]);
		new_projection.rotate(-openVrRollDegrees, 0, 0, 1);

		// hmd coordinates (meters) from ndc coordinates
		const float weapon_distance_meters = 0.55f;
		const float weapon_width_meters = 0.3f;
		const float aspect = SCREENWIDTH / float(SCREENHEIGHT);
		new_projection.translate(0.0, 0.0, weapon_distance_meters);
		new_projection.scale(
			-weapon_width_meters,
			 weapon_width_meters / aspect,
			-weapon_width_meters);

		// ndc coordinates from pixel coordinates
		new_projection.translate(-1.0, 1.0, 0);
		new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

		// projection matrix - clip coordinates from eye coordinates
		VSMatrix proj(activeEye->projectionMatrix);
		proj.multMatrix(new_projection);
		new_projection = proj;
	}

	gl_RenderState.mProjectionMatrix = new_projection;
	gl_RenderState.ApplyMatrices();
}

/* virtual */
void OpenVRMode::Present() const {
	// TODO: For performance, don't render to the desktop screen here
	if (doRenderToDesktop) {
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();

		// Compute screen regions to use for left and right eye views
		int leftWidth = GLRenderer->mOutputLetterbox.width / 2;
		int rightWidth = GLRenderer->mOutputLetterbox.width - leftWidth;
		GL_IRECT leftHalfScreen = GLRenderer->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		GL_IRECT rightHalfScreen = GLRenderer->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;

		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(leftHalfScreen, true);
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(rightHalfScreen, true);
	}

	if (doRenderToHmd) 
	{
		leftEyeView.submitFrame(vrCompositor);
		rightEyeView.submitFrame(vrCompositor);
	}
}

static int mAngleFromRadians(double radians) 
{
	double m = std::round(65535.0 * radians / (2.0 * M_PI));
	return int(m);
}

void OpenVRMode::updateHmdPose(
	double hmdYawRadians, 
	double hmdPitchRadians, 
	double hmdRollRadians) const 
{
	double hmdyaw = hmdYawRadians;
	double hmdpitch = hmdPitchRadians;
	double hmdroll = hmdRollRadians;

	double dYaw = 0;
	if (doTrackHmdYaw) {
		// Set HMD angle game state parameters for NEXT frame
		static double previousYaw = 0;
		static bool havePreviousYaw = false;
		if (!havePreviousYaw) {
			previousYaw = hmdyaw;
			havePreviousYaw = true;
		}
		dYaw = hmdyaw - previousYaw;
		G_AddViewAngle(mAngleFromRadians(-dYaw));
		previousYaw = hmdyaw;
	}

	/* */
	// Pitch
	if (doTrackHmdPitch) {
		double hmdPitchInDoom = -atan(tan(hmdpitch) / level.info->pixelstretch);
		double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
		double dPitch = hmdPitchInDoom - viewPitchInDoom;
		G_AddViewPitch(mAngleFromRadians(-dPitch));
	}

	// Roll can be local, because it doesn't affect gameplay.
	if (doTrackHmdRoll)
		GLRenderer->mAngles.Roll = RAD2DEG(-hmdroll);

	// Late-schedule update to renderer angles directly, too
	if (doLateScheduledRotationTracking) {
		if (doTrackHmdPitch)
			GLRenderer->mAngles.Pitch = RAD2DEG(-hmdpitch);
		if (doTrackHmdYaw) {
			// TODO: this is not working, especially in menu GS_TITLESCREEN mode
			GLRenderer->mAngles.Yaw += RAD2DEG(dYaw); // "plus" is the correct direction
			// Printf("In updateHmdPose: %.1f\n", r_viewpoint.Angles.Yaw.Degrees);
		}
	}
}

/* virtual */
void OpenVRMode::SetUp() const
{
	super::SetUp();

	cachedScreenBlocks = screenblocks;
	screenblocks = 12; // always be full-screen during 3D scene render

	if (vrCompositor == nullptr)
		return;

	static TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
	vrCompositor->WaitGetPoses(
		poses, k_unMaxTrackedDeviceCount, // current pose
		nullptr, 0 // future pose?
	);

	TrackedDevicePose_t& hmdPose0 = poses[k_unTrackedDeviceIndex_Hmd];

	if (hmdPose0.bPoseIsValid) {
		const HmdMatrix34_t& hmdPose = hmdPose0.mDeviceToAbsoluteTracking;
		HmdVector3d_t eulerAngles = eulerAnglesFromMatrix(hmdPose);
		// Printf("%.1f %.1f %.1f\n", eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		updateHmdPose(eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		leftEyeView.setCurrentHmdPose(&hmdPose0);
		rightEyeView.setCurrentHmdPose(&hmdPose0);
		// TODO: position tracking
	}
}

/* virtual */
void OpenVRMode::TearDown() const
{
	screenblocks = cachedScreenBlocks;
	super::TearDown();
}

/* virtual */
OpenVRMode::~OpenVRMode() 
{
	if (vrSystem != nullptr) {
		VR_ShutdownInternal();
		vrSystem = nullptr;
		vrCompositor = nullptr;
		leftEyeView.dispose();
		rightEyeView.dispose();
	}
}

} /* namespace s3d */

#endif

