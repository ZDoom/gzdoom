#pragma once

#include "hwrenderer/data/buffers.h"

#ifdef _MSC_VER
// silence bogus warning C4250: 'GLVertexBuffer': inherits 'GLBuffer::GLBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250) 
#endif

namespace OpenGLRenderer
{

class GLBuffer : virtual public IBuffer
{
protected:
	const int mUseType;
	unsigned int mBufferId;
	int mAllocationSize = 0;
	bool mPersistent = false;
	bool nomap = true;

	GLBuffer(int usetype);
	~GLBuffer();
	void SetData(size_t size, const void *data, bool staticdata) override;
	void SetSubData(size_t offset, size_t size, const void *data) override;
	void Map() override;
	void Unmap() override;
	void Resize(size_t newsize) override;
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

class GLDataBuffer : public IDataBuffer, public GLBuffer
{
	int mBindingPoint;
public:
	GLDataBuffer(int bindingpoint, bool is_ssbo) : GLBuffer(is_ssbo? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER), mBindingPoint(bindingpoint) {}
	void BindRange(size_t start, size_t length) override;
	void BindBase() override;
};

}