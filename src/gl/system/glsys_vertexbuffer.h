#pragma once

#include "hwrenderer/data/vertexbuffer.h"

#ifdef _MSC_VER
// silence bogus warning C4250: 'GLVertexBuffer': inherits 'GLBuffer::GLBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250) 
#endif

class GLBuffer : virtual public IBuffer
{
	const int mUseType;
	unsigned int mBufferId;
protected:
	int mAllocationSize = 0;
	bool mPersistent = false;
	bool nomap = true;

	GLBuffer(int usetype);
	~GLBuffer();
	void SetData(size_t size, void *data, bool staticdata) override;
	void Map() override;
	void Unmap() override;
	void *Lock(unsigned int size) override;
	void Unlock() override;
public:
	void Bind();
};


class GLVertexBuffer : public IVertexBuffer, public GLBuffer
{
	// If this could use the modern (since GL 4.3) binding system, things would be simpler... :(
	struct GLVertexBufferAttribute
	{
		int bindingpoint;
		int format;
		int size;
		int offset;
	};

	int mNumBindingPoints;
	GLVertexBufferAttribute mAttributeInfo[VATTR_MAX] = {};	// Thanks to OpenGL's state system this needs to contain info about every attribute that may ever be in use throughout the entire renderer.
	size_t mStride = 0;

public:
	GLVertexBuffer() : GLBuffer(GL_ARRAY_BUFFER) {}
	void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) override;
	void Bind(int *offsets);
};

class GLIndexBuffer : public IIndexBuffer, public GLBuffer
{
public:
	GLIndexBuffer() : GLBuffer(GL_ELEMENT_ARRAY_BUFFER) {}
};

