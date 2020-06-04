#pragma once

#include "hwrenderer/data/buffers.h"
#include "i_modelvertexbuffer.h"

class FModelRenderer;

class FModelVertexBuffer : public IModelVertexBuffer
{
	IVertexBuffer *mVertexBuffer;
	IIndexBuffer *mIndexBuffer;

public:

	FModelVertexBuffer(bool needindex, bool singleframe);
	~FModelVertexBuffer();

	FModelVertex *LockVertexBuffer(unsigned int size) override;
	void UnlockVertexBuffer() override;

	unsigned int *LockIndexBuffer(unsigned int size) override;
	void UnlockIndexBuffer() override;

	IVertexBuffer* vertexBuffer() const { return mVertexBuffer; }
	IIndexBuffer* indexBuffer() const { return mIndexBuffer; }
};
