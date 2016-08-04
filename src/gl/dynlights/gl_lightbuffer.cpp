/*
** gl_lightbuffer.cpp
** Buffer data maintenance for dynamic lights
**
**---------------------------------------------------------------------------
** Copyright 2014 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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
*/

#include "gl/system/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/system/gl_interface.h"
#include "gl/utility//gl_clock.h"

static const int INITIAL_BUFFER_SIZE = 160000;	// This means 80000 lights per frame and 160000*16 bytes == 2.56 MB.

float *mMap;

FLightBuffer::FLightBuffer()
{

	mBufferSize = INITIAL_BUFFER_SIZE;
	mByteSize = mBufferSize * sizeof(float);
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
	{
		mBufferType = GL_SHADER_STORAGE_BUFFER;
		mBlockAlign = 0;
		mBlockSize = mBufferSize;
	}
	else
	{
		mBufferType = GL_UNIFORM_BUFFER;
		mBlockSize = gl.maxuniformblock / 16;
		if (mBlockSize > 2048) mBlockSize = 2048;	// we don't really need a larger buffer
		mBlockAlign = mBlockSize / 2;
	}

	glGenBuffers(1, &mBufferId);
	glBindBufferBase(mBufferType, LIGHTBUF_BINDINGPOINT, mBufferId);
	glBindBuffer(mBufferType, mBufferId);	// Note: Some older AMD drivers don't do that in glBindBufferBase, as they should.
	if (gl.lightmethod == LM_DIRECT)
	{
		glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	}
	else
	{
		glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
		mBufferPointer = NULL;
	}

	Clear();
	mLastMappedIndex = UINT_MAX;
}

FLightBuffer::~FLightBuffer()
{
	glBindBuffer(mBufferType, 0);
	glDeleteBuffers(1, &mBufferId);
}

void FLightBuffer::Clear()
{
	mIndex = 0;
	mIndices.Clear();
	mUploadIndex = 0;
}

int FLightBuffer::UploadLights(FDynLightData &data)
{
	int size0 = data.arrays[0].Size()/4;
	int size1 = data.arrays[1].Size()/4;
	int size2 = data.arrays[2].Size()/4;
	int totalsize = size0 + size1 + size2 + 1;

	// pointless type casting because some compilers can't print enough warnings.
	if (mBlockAlign > 0 && (unsigned int)totalsize + (mIndex % mBlockAlign) > mBlockSize)
	{
		mIndex = ((mIndex + mBlockAlign) / mBlockAlign) * mBlockAlign;

		// can't be rendered all at once.
		if ((unsigned int)totalsize > mBlockSize)
		{
			int diff = totalsize - (int)mBlockSize;

			size2 -= diff;
			if (size2 < 0)
			{
				size1 += size2;
				size2 = 0;
			}
			if (size1 < 0)
			{
				size0 += size1;
				size1 = 0;
			}
			totalsize = size0 + size1 + size2 + 1;
		}
	}

	if (totalsize <= 1) return -1;

	if (mIndex + totalsize > mBufferSize/4)
	{
		// reallocate the buffer with twice the size
		unsigned int newbuffer;

		// first unmap the old buffer
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);

		// create and bind the new buffer, bind the old one to a copy target (too bad that DSA is not yet supported well enough to omit this crap.)
		glGenBuffers(1, &newbuffer);
		glBindBufferBase(mBufferType, LIGHTBUF_BINDINGPOINT, newbuffer);
		glBindBuffer(mBufferType, newbuffer);	// Note: Some older AMD drivers don't do that in glBindBufferBase, as they should.
		glBindBuffer(GL_COPY_READ_BUFFER, mBufferId);

		// create the new buffer's storage (twice as large as the old one)
		mBufferSize *= 2;
		mByteSize *= 2;
		if (gl.lightmethod == LM_DIRECT)
		{
			glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
			mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		}
		else
		{
			glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
			mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
		}

		// copy contents and delete the old buffer.
		glCopyBufferSubData(GL_COPY_READ_BUFFER, mBufferType, 0, 0, mByteSize/2);
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glDeleteBuffers(1, &mBufferId);
		mBufferId = newbuffer;
	}

	float *copyptr;
	
	assert(mBufferPointer != NULL);
	if (mBufferPointer == NULL) return -1;
	copyptr = mBufferPointer + mIndex * 4;

	float parmcnt[] = { 0, float(size0), float(size0 + size1), float(size0 + size1 + size2) };

	memcpy(&copyptr[0], parmcnt, 4 * sizeof(float));
	memcpy(&copyptr[4], &data.arrays[0][0], 4 * size0*sizeof(float));
	memcpy(&copyptr[4 + 4*size0], &data.arrays[1][0], 4 * size1*sizeof(float));
	memcpy(&copyptr[4 + 4*(size0 + size1)], &data.arrays[2][0], 4 * size2*sizeof(float));

	unsigned int bufferindex = mIndex;
	mIndex += totalsize;
	draw_dlight += (totalsize-1) / 2;
	return bufferindex;
}

void FLightBuffer::Begin()
{
	if (gl.lightmethod == LM_DEFERRED)
	{
		glBindBuffer(mBufferType, mBufferId);
		mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void FLightBuffer::Finish()
{
	if (gl.lightmethod == LM_DEFERRED)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		mBufferPointer = NULL;
	}
}

int FLightBuffer::BindUBO(unsigned int index)
{
	unsigned int offset = (index / mBlockAlign) * mBlockAlign;

	if (offset != mLastMappedIndex)
	{
		// this will only get called if a uniform buffer is used. For a shader storage buffer we only need to bind the buffer once at the start to all shader programs
		mLastMappedIndex = offset;
		glBindBufferRange(GL_UNIFORM_BUFFER, LIGHTBUF_BINDINGPOINT, mBufferId, offset*16, mBlockSize*16);	// we go from counting vec4's to counting bytes here.
	}
	return (index - offset);
}



