#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/stereo3d/gl_stereo_leftright.h"
#include "gl/stereo3d/gl_anaglyph.h"
#include "gl/system/gl_cvars.h"

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 0, CVAR_GLOBALCONFIG)
// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS
CVAR(Float, vr_screendist, 0.80f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CVAR(Float, vr_hunits_per_meter, 41.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

// Manage changing of 3D modes:
namespace s3d {

// Initialize static member
Stereo3DMode const * Stereo3DMode::currentStereo3DMode = nullptr;

/* static */
void Stereo3DMode::setCurrentMode(const Stereo3DMode& mode) {
	Stereo3DMode::currentStereo3DMode = &mode;
}

/* static */
const Stereo3DMode& Stereo3DMode::getCurrentMode() 
{
	// NOTE: Ensure that these vr_mode values correspond to the ones in wadsrc/static/menudef.z
	switch (vr_mode)
	{
	case 1:
		setCurrentMode(GreenMagenta::getInstance(vr_ipd));
		break;
	case 2:
		setCurrentMode(RedCyan::getInstance(vr_ipd));
		break;
	case 3:
		setCurrentMode(LeftEyeView::getInstance(vr_ipd));
		break;
	case 4:
		setCurrentMode(RightEyeView::getInstance(vr_ipd));
		break;
	case 0:
	default:
		setCurrentMode(MonoView::getInstance());
		break;
	}
	return *currentStereo3DMode;
}

} /* namespace s3d */

