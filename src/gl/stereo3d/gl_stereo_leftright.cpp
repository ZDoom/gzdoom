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
** gl_stereo_leftright.cpp
** Offsets for left and right eye views
**
*/

#include "gl_stereo_leftright.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "hwrenderer/utility/hw_cvars.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"

namespace s3d {


/* static */
const LeftEyeView& LeftEyeView::getInstance(float ipd)
{
	static LeftEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}

void LeftEyeView::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);
}

/* static */
const RightEyeView& RightEyeView::getInstance(float ipd)
{
	static RightEyeView instance(ipd);
	instance.setIpd(ipd);
	return instance;
}

void RightEyeView::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);
}


} /* namespace s3d */
