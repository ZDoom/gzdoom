//
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
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
** gl_uniformbuffer.cpp
** Uniform buffer abstraction class.
**
**/


#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "gl_uniformbuffer.h"


//==========================================================================
//
//
//
//==========================================================================

GLUniformBuffer::GLUniformBuffer(size_t size, bool staticdraw)
: IUniformBuffer(size), mStaticDraw(staticdraw)
{
	glGenBuffers(1, &mBufferId);
}

//==========================================================================
//
//
//
//==========================================================================

GLUniformBuffer::~GLUniformBuffer()
{
	if (mBufferId != 0)
	{
		glDeleteBuffers(1, &mBufferId);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void GLUniformBuffer::SetData(const void *data)
{
	if (mBufferId != 0)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glBufferData(GL_UNIFORM_BUFFER, mSize, data, mStaticDraw? GL_STATIC_DRAW : GL_STREAM_DRAW);
	}
}

//==========================================================================
//
// This needs to go away later.
//
//==========================================================================

void GLUniformBuffer::Bind(int bindingpoint)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingpoint, mBufferId);
}
