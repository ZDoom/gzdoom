// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2020 Christoph Oelckers
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
** gl_models.cpp
**
** hardware renderer model handling code
**
**/


#include "v_video.h"
#include "cmdlib.h"
#include "hw_modelvertexbuffer.h"

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::FModelVertexBuffer(bool needindex, bool singleframe)
{
	mVertexBuffer = screen->CreateVertexBuffer();
	mIndexBuffer = needindex ? screen->CreateIndexBuffer() : nullptr;

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FModelVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FModelVertex, u) },
		{ 0, VATTR_NORMAL, VFmt_Packed_A2R10G10B10, (int)myoffsetof(FModelVertex, packedNormal) },
		{ 1, VATTR_VERTEX2, VFmt_Float3, (int)myoffsetof(FModelVertex, x) },
		{ 1, VATTR_NORMAL2, VFmt_Packed_A2R10G10B10, (int)myoffsetof(FModelVertex, packedNormal) }
	};
	mVertexBuffer->SetFormat(2, 5, sizeof(FModelVertex), format);
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::~FModelVertexBuffer()
{
	if (mIndexBuffer) delete mIndexBuffer;
	delete mVertexBuffer;
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertex *FModelVertexBuffer::LockVertexBuffer(unsigned int size)
{
	return static_cast<FModelVertex*>(mVertexBuffer->Lock(size * sizeof(FModelVertex)));
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockVertexBuffer()
{
	mVertexBuffer->Unlock();
}

//===========================================================================
//
//
//
//===========================================================================

unsigned int *FModelVertexBuffer::LockIndexBuffer(unsigned int size)
{
	if (mIndexBuffer) return static_cast<unsigned int*>(mIndexBuffer->Lock(size * sizeof(unsigned int)));
	else return nullptr;
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockIndexBuffer()
{
	if (mIndexBuffer) mIndexBuffer->Unlock();
}


