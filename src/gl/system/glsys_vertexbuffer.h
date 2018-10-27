#pragma once

#include "hwrenderer/data/vertexbuffer.h"

class GLVertexBuffer : public IVertexBuffer
{
	// If this could use the modern (since GL 4.3) binding system, things would be simpler... :(
	struct GLVertexBufferAttribute
	{
		int bindingpoint;
		int format;
		int size;
		int offset;
	};
	
	unsigned int vbo_id = 0;
	int mNumBindingPoints;
	bool mPersistent = false; 
	GLVertexBufferAttribute mAttributeInfo[VATTR_MAX] = {};	// Thanks to OpenGL's state system this needs to contain info about every attribute that may ever be in use throughout the entire renderer.
	size_t mStride = 0;

public:
	GLVertexBuffer();
	~GLVertexBuffer();
	void SetData(size_t size, void *data, bool staticdata) override;
	void SetFormat(int numBindingPoints, int numAttributes, size_t stride, FVertexBufferAttribute *attrs) override;
	void Bind(size_t *offsets);
	void Map() override;
	void Unmap() override;
};

class GLIndexBuffer : public IIndexBuffer
{
	unsigned int ibo_id = 0;

public:
	GLIndexBuffer();
	~GLIndexBuffer();
	void SetData(size_t size, void *data, bool staticdata) override;
	void Bind();
};

