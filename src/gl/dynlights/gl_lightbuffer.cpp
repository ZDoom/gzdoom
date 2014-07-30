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
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/system/gl_interface.h"

static const int LIGHTBUF_BINDINGPOINT = 1;

FLightBuffer::FLightBuffer()
{
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
	{
		mBufferType = GL_SHADER_STORAGE_BUFFER;
		mBufferSize = 80000;	// 40000 lights per scene should be plenty. The largest I've ever seen was around 5000.
	}
	else
	{
		mBufferType = GL_UNIFORM_BUFFER;
		mBufferSize = gl.maxuniformblock / 4 - 100;	// we need to be a bit careful here so don't use the full buffer size
	}
	AddBuffer();
	Clear();
}

FLightBuffer::~FLightBuffer()
{
	glBindBuffer(mBufferType, 0);
	for (unsigned int i = 0; i < mBufferIds.Size(); i++)
	{
		glDeleteBuffers(1, &mBufferIds[i]);
	}
}

void FLightBuffer::AddBuffer()
{
	unsigned int id;
	glGenBuffers(1, &id);
	mBufferIds.Push(id);
	glBindBuffer(mBufferType, id);
	unsigned int bytesize = mBufferSize * 8 * sizeof(float);
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		glBufferStorage(mBufferType, bytesize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		void *map = glMapBufferRange(mBufferType, 0, bytesize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointers.Push((float*)map);
	}
	else
	{
		glBufferData(mBufferType, bytesize, NULL, GL_STREAM_DRAW);
	}
}

void FLightBuffer::Clear()
{
	mBufferNum = 0;
	mIndex = 0;
	mBufferArray.Clear();
	mBufferStart.Clear();
}

void FLightBuffer::UploadLights(FDynLightData &data, unsigned int &buffernum, unsigned int &bufferindex)
{
	int size0 = data.arrays[0].Size()/4;
	int size1 = data.arrays[1].Size()/4;
	int size2 = data.arrays[2].Size()/4;
	int totalsize = size0 + size1 + size2 + 1;

	if (totalsize == 0) return;

	if (mIndex + totalsize > mBufferSize)
	{
		if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
		{
			return;	// we do not want multiple shader storage blocks. 40000 lights is too much already
		}
		else
		{
			mBufferNum++;
			mBufferStart.Push(mIndex);
			mIndex = 0;
			if (mBufferIds.Size() <= mBufferNum) AddBuffer();
		}
	}

	float *copyptr;

	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		copyptr = mBufferPointers[mBufferNum] + mIndex * 4;
	}
	else
	{
		unsigned int pos = mBufferArray.Reserve(totalsize * 4);
		copyptr = &mBufferArray[pos];
	}

	float parmcnt[] = { mIndex + 1, mIndex + 1 + size0, mIndex + 1 + size0 + size1, mIndex + totalsize };

	memcpy(&copyptr[0], parmcnt, 4 * sizeof(float));
	memcpy(&copyptr[1], &data.arrays[0][0], 4 * size0*sizeof(float));
	memcpy(&copyptr[1 + size0], &data.arrays[1][0], 4 * size1*sizeof(float));
	memcpy(&copyptr[1 + size0 + size1], &data.arrays[2][0], 4 * size2*sizeof(float));
	buffernum = mBufferNum;
	bufferindex = mIndex;
	mIndex += totalsize;
}

void FLightBuffer::Finish()
{
	//glBindBufferBase(mBufferType, LIGHTBUF_BINDINGPOINT, mBufferIds[0]);
}



