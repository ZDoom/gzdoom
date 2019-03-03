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

#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hw_viewpointbuffer.h"

static const int INITIAL_BUFFER_SIZE = 100;	// 100 viewpoints per frame should nearly always be enough

GLViewpointBuffer::GLViewpointBuffer()
{
	mBufferSize = INITIAL_BUFFER_SIZE;
	mBlockAlign = ((sizeof(HWViewpointUniforms) / screen->uniformblockalignment) + 1) * screen->uniformblockalignment;
	mByteSize = mBufferSize * mBlockAlign;
	mBuffer = screen->CreateDataBuffer(VIEWPOINT_BINDINGPOINT, false);
	mBuffer->SetData(mByteSize, nullptr, false);
	Clear();
	mLastMappedIndex = UINT_MAX;
	mClipPlaneInfo.Push(0);
}

GLViewpointBuffer::~GLViewpointBuffer()
{
	delete mBuffer;
}


void GLViewpointBuffer::CheckSize()
{
	if (mUploadIndex >= mBufferSize)
	{
		mBufferSize *= 2;
		mByteSize *= 2;
		mBuffer->Resize(mByteSize);
		m2DHeight = m2DWidth = -1;
	}
}

int GLViewpointBuffer::Bind(FRenderState &di, unsigned int index)
{
	if (index != mLastMappedIndex)
	{
		mLastMappedIndex = index;
		mBuffer->BindRange(index * mBlockAlign, mBlockAlign);
		di.EnableClipDistance(0, mClipPlaneInfo[index]);
	}
	return index;
}

void GLViewpointBuffer::Set2D(FRenderState &di, int width, int height)
{
	if (width != m2DWidth || height != m2DHeight)
	{
		HWViewpointUniforms matrices;
		matrices.SetDefaults();
		matrices.mProjectionMatrix.ortho(0, (float)width, (float)height, 0, -1.0f, 1.0f);
		matrices.CalcDependencies();
		mBuffer->Map();
		memcpy(mBuffer->Memory(), &matrices, sizeof(matrices));
		mBuffer->Unmap();
		m2DWidth = width;
		m2DHeight = height;
		mLastMappedIndex = -1;
	}
	Bind(di, 0);
}

int GLViewpointBuffer::SetViewpoint(FRenderState &di, HWViewpointUniforms *vp)
{
	CheckSize();
	mBuffer->Map();
	memcpy(((char*)mBuffer->Memory()) + mUploadIndex * mBlockAlign, vp, sizeof(*vp));
	mBuffer->Unmap();

	mClipPlaneInfo.Push(vp->mClipHeightDirection != 0.f || vp->mClipLine.X > -10000000.0f);
	return Bind(di, mUploadIndex++);
}

void GLViewpointBuffer::Clear()
{
	// Index 0 is reserved for the 2D projection.
	mUploadIndex = 1;
	mClipPlaneInfo.Resize(1);
}

