/*
** gl_anaglyph.cpp
** Color mask based stereoscopic 3D modes for GZDoom
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

#include "gl_anaglyph.h"

namespace s3d {

MaskAnaglyph::MaskAnaglyph(const ColorMask& leftColorMask, double ipdMeters)
	: leftEye(leftColorMask, ipdMeters), rightEye(leftColorMask.inverse(), ipdMeters)
{
	eye_ptrs.Push(&leftEye);
	eye_ptrs.Push(&rightEye);
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
