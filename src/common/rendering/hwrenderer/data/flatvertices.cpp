/*
** hw_flatvertices.cpp
** Creates flat vertex data for hardware rendering.
**
**---------------------------------------------------------------------------
** Copyright 2010-2020 Christoph Oelckers
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
*/

#include "c_cvars.h"
#include "flatvertices.h"
#include "v_video.h"
#include "cmdlib.h"
#include "printf.h"
#include "hwrenderer/data/buffers.h"
#include "hw_renderstate.h"

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer(DFrameBuffer* fb, int width, int height, int pipelineNbr) : fb(fb), mPipelineNbr(pipelineNbr)
{
	mIndexBuffer = fb->CreateIndexBuffer();
	int data[4] = {};
	mIndexBuffer->SetData(4, data, BufferUsageType::Static); // On Vulkan this may not be empty, so set some dummy defaults to avoid crashes.


	for (int n = 0; n < mPipelineNbr; n++)
	{
		static const FVertexBufferAttribute format[] =
		{
			{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FFlatVertex, x) },
			{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FFlatVertex, u) },
			{ 0, VATTR_LIGHTMAP, VFmt_Float3, (int)myoffsetof(FFlatVertex, lu) },
		};

		mVertexBufferPipeline[n] = fb->CreateVertexBuffer(1, 3, sizeof(FFlatVertex), format);
		mVertexBufferPipeline[n]->SetData(BUFFER_SIZE * sizeof(FFlatVertex), nullptr, BufferUsageType::Persistent);
	}

	mVertexBuffer = mVertexBufferPipeline[mPipelinePos];

	mIndex = mCurIndex = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	for (int n = 0; n < mPipelineNbr; n++)
	{
		delete mVertexBufferPipeline[n];
	}

	delete mIndexBuffer;
	mIndexBuffer = nullptr;
	mVertexBuffer = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

std::pair<FFlatVertex *, unsigned int> FFlatVertexBuffer::AllocVertices(unsigned int count)
{
	FFlatVertex *p = GetBuffer();
	auto index = mCurIndex.fetch_add(count);
	if (index + count >= BUFFER_SIZE_TO_USE)
	{
		// If a single scene needs 2'000'000 vertices there must be something very wrong. 
		I_FatalError("Out of vertex memory. Tried to allocate more than %u vertices for a single frame", index + count);
	}
	return std::make_pair(p, index);
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::Copy(int start, int count)
{
	IBuffer* old = mVertexBuffer;

	for (int n = 0; n < mPipelineNbr; n++)
	{
		mVertexBuffer = mVertexBufferPipeline[n];
		Map();
		memcpy(GetBuffer(start), vbo_shadowdata.Data(), count * sizeof(FFlatVertex));
		Unmap();
		mVertexBuffer->Upload(start * sizeof(FFlatVertex), count * sizeof(FFlatVertex));
	}

	mVertexBuffer = old;
}
