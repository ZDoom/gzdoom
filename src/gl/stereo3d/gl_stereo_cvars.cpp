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
** gl_stereo_cvars.cpp
** Console variables related to stereoscopic 3D in GZDoom
**
*/

#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/stereo3d/gl_stereo_leftright.h"
#include "gl/stereo3d/gl_anaglyph.h"
#include "gl/stereo3d/gl_quadstereo.h"
#include "gl/stereo3d/gl_sidebyside3d.h"
#include "gl/stereo3d/gl_interleaved3d.h"
#include "version.h"

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 0, CVAR_GLOBALCONFIG)

// switch left and right eye views
CVAR(Bool, vr_swap_eyes, false, CVAR_GLOBALCONFIG)

// For broadest GL compatibility, require user to explicitly enable quad-buffered stereo mode.
// Setting vr_enable_quadbuffered_stereo does not automatically invoke quad-buffered stereo,
// but makes it possible for subsequent "vr_mode 7" to invoke quad-buffered stereo
CUSTOM_CVAR(Bool, vr_enable_quadbuffered, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("You must restart " GAMENAME " to switch quad stereo mode\n");
}

// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS

// distance between viewer and the display screen
CVAR(Float, vr_screendist, 0.80f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

// default conversion between (vertical) DOOM units and meters
CVAR(Float, vr_hunits_per_meter, 41.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

// Manage changing of 3D modes:
namespace s3d {

// Initialize static member
Stereo3DMode const * Stereo3DMode::currentStereo3DMode = 0; // "nullptr" not resolved on linux (presumably not C++11)

/* static */
void Stereo3DMode::setCurrentMode(const Stereo3DMode& mode) {
	Stereo3DMode::currentStereo3DMode = &mode;
}

/* static */
const Stereo3DMode& Stereo3DMode::getCurrentMode() 
{
	if (gl.legacyMode) vr_mode = 0;	// GL 2 does not support this feature.

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
		setCurrentMode(SideBySideFull::getInstance(vr_ipd));
		break;
	case 4:
		setCurrentMode(SideBySideSquished::getInstance(vr_ipd));
		break;
	case 5:
		setCurrentMode(LeftEyeView::getInstance(vr_ipd));
		break;
	case 6:
		setCurrentMode(RightEyeView::getInstance(vr_ipd));
		break;
	case 7:
		if (vr_enable_quadbuffered) {
			setCurrentMode(QuadStereo::getInstance(vr_ipd));
		}
		else {
			setCurrentMode(MonoView::getInstance());
		}
		break;
	// TODO: 8: Oculus Rift
	case 9:
		setCurrentMode(AmberBlue::getInstance(vr_ipd));
		break;	
	// TODO: 10: HTC Vive/OpenVR
	case 11:
		setCurrentMode(TopBottom3D::getInstance(vr_ipd));
		break;
	case 12:
		setCurrentMode(RowInterleaved3D::getInstance(vr_ipd));
		break;
	case 13:
		setCurrentMode(ColumnInterleaved3D::getInstance(vr_ipd));
		break;
	case 14:
		setCurrentMode(CheckerInterleaved3D::getInstance(vr_ipd));
		break;
	case 0:
	default:
		setCurrentMode(MonoView::getInstance());
		break;
	}
	return *currentStereo3DMode;
}

const Stereo3DMode& Stereo3DMode::getMonoMode()
{
	setCurrentMode(MonoView::getInstance());
	return *currentStereo3DMode;
}


} /* namespace s3d */

