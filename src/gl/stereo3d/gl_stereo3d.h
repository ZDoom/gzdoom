/*
** gl_stereo3d.h
** Stereoscopic 3D API
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

#ifndef GL_STEREO3D_H_
#define GL_STEREO3D_H_

#include <cstring> // needed for memcpy on linux, which is needed by VSMatrix copy ctor
#include "tarray.h"
#include "gl/data/gl_matrix.h"


/* stereoscopic 3D API */
namespace s3d {


/* Subregion of current display window */
class Viewport
{
public:
	int x, y;
	int width, height;
};


/* Viewpoint of one eye */
class EyePose 
{
public:
	EyePose() {}
	virtual ~EyePose() {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual Viewport GetViewport(const Viewport& fullViewport) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;
	virtual void SetUp() const {};
	virtual void TearDown() const {};
};


/* Base class for stereoscopic 3D rendering modes */
class Stereo3DMode
{
public:
	/* static methods for managing the selected stereoscopic view state */
	static const Stereo3DMode& getCurrentMode();

	Stereo3DMode();
	virtual ~Stereo3DMode();
	virtual int eye_count() const { return eye_ptrs.Size(); }
	virtual const EyePose * getEyePose(int ix) const { return eye_ptrs(ix); }

	/* hooks for setup and cleanup operations for each stereo mode */
	virtual void SetUp() const {};
	virtual void TearDown() const {};

protected:
	TArray<const EyePose *> eye_ptrs;

private:
	static Stereo3DMode const * currentStereo3DMode;
	static void setCurrentMode(const Stereo3DMode& mode);
};


/**
*  Ordinary non-3D rendering
*/
class MonoView : public Stereo3DMode
{
public:
	static const MonoView& getInstance();

protected:
	MonoView() { eye_ptrs.Push(&centralEye); }
	EyePose centralEye;
};


} /* namespace st3d */


#endif /* GL_STEREO3D_H_ */
