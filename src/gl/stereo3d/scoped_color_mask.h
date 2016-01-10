/*
** scoped_color_mask.h
** Stack-scoped class for temporarily changing the OpenGL color mask setting.
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

#ifndef GL_STEREO3D_SCOPED_COLOR_MASK_H_
#define GL_STEREO3D_SCOPED_COLOR_MASK_H_

#include "gl/system/gl_system.h"

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
	}
	~ScopedColorMask() {
		gl_RenderState.SetColorMask(saved[0], saved[1], saved[2], saved[3]);
		gl_RenderState.ApplyColorMask();
	}
private:
	bool saved[4];
};


#endif // GL_STEREO3D_SCOPED_COLOR_MASK_H_
