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
** gl_stereo3d.h
** Stereoscopic 3D API
**
*/

#ifndef GL_STEREO3D_H_
#define GL_STEREO3D_H_

#include <cstring> // needed for memcpy on linux, which is needed by VSMatrix copy ctor
#include "tarray.h"
#include "r_data/matrix.h"
#include "gl/renderer/gl_renderer.h"


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
	static const Stereo3DMode& getMonoMode();

	Stereo3DMode();
	virtual ~Stereo3DMode();
	virtual int eye_count() const { return eye_ptrs.Size(); }
	virtual const EyePose * getEyePose(int ix) const { return eye_ptrs(ix); }

	/* hooks for setup and cleanup operations for each stereo mode */
	virtual void SetUp() const {};
	virtual void TearDown() const {};

	virtual bool IsMono() const { return false; }
	virtual void AdjustViewports() const {};
	virtual void AdjustPlayerSprites() const {};
	virtual void Present() const = 0;

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

	bool IsMono() const override { return true; }
	void Present() const override { }

protected:
	MonoView() { eye_ptrs.Push(&centralEye); }
	EyePose centralEye;
};


} /* namespace st3d */


#endif /* GL_STEREO3D_H_ */
