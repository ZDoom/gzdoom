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
** scoped_color_mask.h
** Stack-scoped class for temporarily changing the OpenGL color mask setting.
**
*/

#ifndef GL_STEREO3D_SCOPED_COLOR_MASK_H_
#define GL_STEREO3D_SCOPED_COLOR_MASK_H_

#include "gl_load/gl_system.h"

/**
* Temporarily change color mask
*/
class ScopedColorMask
{
public:
	ScopedColorMask(bool r, bool g, bool b, bool a) 
	{
		gl_RenderState.GetColorMask(saved[0], saved[1], saved[2], saved[3]);
		gl_RenderState.SetColorMask(r, g, b, a);
		gl_RenderState.ApplyColorMask();
		gl_RenderState.EnableDrawBuffers(1);
	}
	~ScopedColorMask() {
		gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		gl_RenderState.SetColorMask(saved[0], saved[1], saved[2], saved[3]);
		gl_RenderState.ApplyColorMask();
	}
private:
	bool saved[4];
};


#endif // GL_STEREO3D_SCOPED_COLOR_MASK_H_
