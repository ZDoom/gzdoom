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
** gl_anaglyph.cpp
** Color mask based stereoscopic 3D modes for GZDoom
**
*/

#include "gl_anaglyph.h"
#include "gl/renderer/gl_renderbuffers.h"

namespace s3d {

MaskAnaglyph::MaskAnaglyph(const ColorMask& leftColorMask, double ipdMeters)
	: leftEye(leftColorMask, ipdMeters), rightEye(leftColorMask.inverse(), ipdMeters)
{
	eye_ptrs.Push(&leftEye);
	eye_ptrs.Push(&rightEye);
}

void MaskAnaglyph::Present() const
{
	GLRenderer->mBuffers->BindOutputFB();
	GLRenderer->ClearBorders();

	gl_RenderState.SetColorMask(leftEye.GetColorMask().r, leftEye.GetColorMask().g, leftEye.GetColorMask().b, true);
	gl_RenderState.ApplyColorMask();
	GLRenderer->mBuffers->BindEyeTexture(0, 0);
 	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);

	gl_RenderState.SetColorMask(rightEye.GetColorMask().r, rightEye.GetColorMask().g, rightEye.GetColorMask().b, true);
	gl_RenderState.ApplyColorMask();
	GLRenderer->mBuffers->BindEyeTexture(1, 0);
	GLRenderer->DrawPresentTexture(screen->mOutputLetterbox, true);

	gl_RenderState.ResetColorMask();
	gl_RenderState.ApplyColorMask();
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


/* static */
const AmberBlue& AmberBlue::getInstance(float ipd)
{
	static AmberBlue instance(ipd);
	return instance;
}


} /* namespace s3d */
