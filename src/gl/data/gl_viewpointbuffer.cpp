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
** gl_viewpointbuffer.cpp
** Buffer data maintenance for per viewpoint uniform data
**
**/

#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "gl_viewpointbuffer.h"

static const int INITIAL_BUFFER_SIZE = 100;	// 100 viewpoints per frame should nearly always be enough

GLViewpointBuffer::GLViewpointBuffer()
{
	mBufferSize = INITIAL_BUFFER_SIZE;
	mBlockAlign = ((sizeof(HWViewpointUniforms) / gl.uniformblockalignment) + 1) * gl.uniformblockalignment;
	mByteSize = mBufferSize * mBlockAlign;
	Allocate();
	Clear();
	mLastMappedIndex = UINT_MAX;
	mClipPlaneInfo.Push(0);
}

GLViewpointBuffer::~GLViewpointBuffer()
{
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glDeleteBuffers(1, &mBufferId);
}

void GLViewpointBuffer::Allocate()
{
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(GL_UNIFORM_BUFFER, VIEWPOINT_BINDINGPOINT, mBufferId);
	glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);	// Note: Some older AMD drivers don't do that in glBindBufferBase, as they should.
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		glBufferStorage(GL_UNIFORM_BUFFER, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
		mBufferPointer = glMapBufferRange(GL_UNIFORM_BUFFER, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
	}
	else
	{
		glBufferData(GL_UNIFORM_BUFFER, mByteSize, NULL, GL_STATIC_DRAW);
		mBufferPointer = NULL;
	}
}

void GLViewpointBuffer::CheckSize()
{
	if (mUploadIndex >= mBufferSize)
	{
		// reallocate the buffer with twice the size
		unsigned int oldbuffer = mBufferId;

		mBufferSize *= 2;
		mByteSize *= 2;
		
		// first unmap the old buffer
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glUnmapBuffer(GL_UNIFORM_BUFFER);
		
		Allocate();
		glBindBuffer(GL_COPY_READ_BUFFER, oldbuffer);

		// copy contents and delete the old buffer.
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_UNIFORM_BUFFER, 0, 0, mByteSize / 2);	// old size is half of the current one.
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glDeleteBuffers(1, &oldbuffer);
	}
}

void GLViewpointBuffer::Map()
{
	if (!(gl.flags & RFL_BUFFER_STORAGE))
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		mBufferPointer = (float*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void GLViewpointBuffer::Unmap()
{
	if (!(gl.flags & RFL_BUFFER_STORAGE))
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glUnmapBuffer(GL_UNIFORM_BUFFER);
		mBufferPointer = nullptr;
	}
	else
	{
		glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
	}
}

int GLViewpointBuffer::Bind(unsigned int index)
{
	if (index != mLastMappedIndex)
	{
		mLastMappedIndex = index;
		glBindBufferRange(GL_UNIFORM_BUFFER, VIEWPOINT_BINDINGPOINT, mBufferId, index * mBlockAlign, mBlockAlign);

		// Update the viewpoint-related clip plane setting.
		if (!(gl.flags & RFL_NO_CLIP_PLANES))
		{
			if (mClipPlaneInfo[index])
			{
				glEnable(GL_CLIP_DISTANCE0);
			}
			else
			{
				glDisable(GL_CLIP_DISTANCE0);
			}
		}
	}
	return index;
}

void GLViewpointBuffer::Set2D(int width, int height)
{
	if (width != m2DWidth || height != m2DHeight)
	{
		HWViewpointUniforms matrices;
		matrices.SetDefaults();
		matrices.mProjectionMatrix.ortho(0, width, height, 0, -1.0f, 1.0f);
		matrices.CalcDependencies();
		Map();
		memcpy(mBufferPointer, &matrices, sizeof(matrices));
		Unmap();
		m2DWidth = width;
		m2DHeight = height;
		mLastMappedIndex = -1;
	}
	Bind(0);
}

int GLViewpointBuffer::SetViewpoint(HWViewpointUniforms *vp)
{
	CheckSize();
	Map();
	memcpy(((char*)mBufferPointer) + mUploadIndex * mBlockAlign, vp, sizeof(*vp));
	Unmap();

	mClipPlaneInfo.Push(vp->mClipHeightDirection != 0.f || vp->mClipLine.X > -10000000.0f);
	return Bind(mUploadIndex++);
}

void GLViewpointBuffer::Clear()
{
	// Index 0 is reserved for the 2D projection.
	mUploadIndex = 1;
	mClipPlaneInfo.Resize(1);
}

