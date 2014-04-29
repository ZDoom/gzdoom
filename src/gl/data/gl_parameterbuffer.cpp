/*
** gl_parameterbuffer.cpp
** Shader Storage Buffer for passing parameters to the shaders
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
#include "gl/data/gl_parameterbuffer.h"

#define PARAMETER_BUFFER_SIZE 250000
#define ATTRIB_BUFFER_SIZE 100000

//==========================================================================
//
// 
//
//==========================================================================

FDataBuffer::FDataBuffer(unsigned int bytesize, int bindingpoint)
{
	mPointer = 0;
	mStartIndex = 0;
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingpoint, mBufferId);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, bytesize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	mMappedBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
}

//==========================================================================
//
// 
//
//==========================================================================

FDataBuffer::~FDataBuffer()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferId);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glDeleteBuffers(1, &mBufferId);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDataBuffer::StartFrame()
{
	mStartIndex = mPointer;
}

//==========================================================================
//
// 
//
//==========================================================================

FParameterBuffer::FParameterBuffer()
: FDataBuffer(PARAMETER_BUFFER_SIZE*sizeof(ParameterBufferElement), 3)
{
	mSize = PARAMETER_BUFFER_SIZE;
}

//==========================================================================
//
// 
//
//==========================================================================

unsigned int FParameterBuffer::Reserve(unsigned int amount, ParameterBufferElement **pptr)
{
	if (mPointer + amount > mSize) mPointer = 0;
	if (mPointer < mStartIndex && mPointer + amount > mStartIndex)
	{
		// DPrintf("Warning: Parameter buffer wraparound!\n");
	}
	unsigned int here = mPointer;
	mPointer += amount;
	ParameterBufferElement *pb = (ParameterBufferElement *)mMappedBuffer;
	*pptr = &pb[here];
	return here;
}

//==========================================================================
//
// 
//
//==========================================================================

FAttribBuffer::FAttribBuffer()
: FDataBuffer(ATTRIB_BUFFER_SIZE*sizeof(AttribBufferElement), 2)
{
	mSize = ATTRIB_BUFFER_SIZE;
}

//==========================================================================
//
// 
//
//==========================================================================

unsigned int FAttribBuffer::Reserve(unsigned int amount, AttribBufferElement **pptr)
{
	if (mPointer + amount > mSize) mPointer = 0;
	if (mPointer < mStartIndex && mPointer + amount > mStartIndex)
	{
		// DPrintf("Warning: Parameter buffer wraparound!\n");
	}
	unsigned int here = mPointer;
	mPointer += amount;
	AttribBufferElement *pb = (AttribBufferElement *)mMappedBuffer;
	*pptr = &pb[here];
	return here;
}
