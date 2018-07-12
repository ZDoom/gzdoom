/*
** gl_interleaved3d.h
** Interleaved stereoscopic 3D modes for GZDoom
**
**---------------------------------------------------------------------------
** Copyright 2016 Christopher Bruns
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

#ifndef GL_INTERLEAVED3D_H_
#define GL_INTERLEAVED3D_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "gl_sidebyside3d.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderstate.h"

namespace s3d {

class CheckerInterleaved3D : public SideBySideSquished
{
public:
	static const CheckerInterleaved3D& getInstance(float ipd);
	CheckerInterleaved3D(double ipdMeters) : SideBySideSquished(ipdMeters) {}
	void Present() const override;
	void AdjustViewports() const override;
};

class ColumnInterleaved3D : public SideBySideSquished
{
public:
	static const ColumnInterleaved3D& getInstance(float ipd);
	ColumnInterleaved3D(double ipdMeters) : SideBySideSquished(ipdMeters) {}
	void Present() const override;
};

class RowInterleaved3D : public TopBottom3D
{
public:
	static const RowInterleaved3D& getInstance(float ipd);
	RowInterleaved3D(double ipdMeters) : TopBottom3D(ipdMeters) {}
	void Present() const override;
};

} /* namespace s3d */


#endif /* GL_INTERLEAVED3D_H_ */
