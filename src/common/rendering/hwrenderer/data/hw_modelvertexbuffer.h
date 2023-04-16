#pragma once

#include "hwrenderer/data/buffers.h"
#include "i_modelvertexbuffer.h"

class FModelRenderer;

class FModelVertexBuffer : public IModelVertexBuffer
{
	IBuffer* mVertexBuffer;
	IBuffer* mIndexBuffer;

public:

	FModelVertexBuffer(bool needindex, bool singleframe);
	~FModelVertexBuffer();

	FModelVertex *LockVertexBuffer(unsigned int size) override;
	void UnlockVertexBuffer() override;

	unsigned int *LockIndexBuffer(unsigned int size) override;
	void UnlockIndexBuffer() override;

	IBuffer* vertexBuffer() const { return mVertexBuffer; }
	IBuffer* indexBuffer() const { return mIndexBuffer; }
};
