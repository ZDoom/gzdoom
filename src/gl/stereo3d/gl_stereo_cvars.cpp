/*
** gl_stereo_cvars.cpp
** Console variables related to stereoscopic 3D in GZDoom
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

#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/stereo3d/gl_stereo_leftright.h"
#include "gl/stereo3d/gl_anaglyph.h"
#include "gl/stereo3d/gl_quadstereo.h"
#include "gl/system/gl_cvars.h"

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 0, CVAR_GLOBALCONFIG)

// For broadest GL compatibility, require user to explicitly enable quad-buffered stereo mode.
// Setting vr_enable_quadbuffered_stereo does not automatically invoke quad-buffered stereo,
// but makes it possible for subsequent "vr_mode 7" to invoke quad-buffered stereo
CVAR(Bool, vr_enable_quadbuffered, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

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
	// NOTE: Ensure that these vr_mode values correspond to the ones in wadsrc/static/menudef.z
	switch (vr_mode)
	{
	case 1:
		setCurrentMode(GreenMagenta::getInstance(vr_ipd));
		break;
	case 2:
		setCurrentMode(RedCyan::getInstance(vr_ipd));
		break;
	// TODO: missing indices 3, 4 for not-yet-implemented side-by-side modes, to match values from GZ3Doom
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
		break;	case 0:
	default:
		setCurrentMode(MonoView::getInstance());
		break;
	}
	return *currentStereo3DMode;
}

} /* namespace s3d */

