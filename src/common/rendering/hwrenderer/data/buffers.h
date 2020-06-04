#pragma once

#include <stddef.h>
#include <assert.h>

class FRenderState;

// The low level code needs to know which attributes exist.
// OpenGL needs to change the state of all of them per buffer binding.
// VAOs are mostly useless for this because they lump buffer and binding state together which the model code does not want.
enum
{
	VATTR_VERTEX,
	VATTR_TEXCOORD,
	VATTR_COLOR,
	VATTR_VERTEX2,
	VATTR_NORMAL,
	VATTR_NORMAL2,
	
	VATTR_MAX
};

enum EVertexAttributeFormat
{
	VFmt_Float4,
	VFmt_Float3,
	VFmt_Float2,
	VFmt_Float,
	VFmt_Byte4,
	VFmt_Packed_A2R10G10B10,
};

struct FVertexBufferAttribute
{
	int binding;
	int location;
	int format;
	int offset;
};

class IBuffer
{
protected:
	size_t buffersize = 0;
	void *map = nullptr;
public:
	IBuffer() = default;
	IBuffer(const IBuffer &) = delete;
	IBuffer &operator=(const IBuffer &) = delete;
	virtual ~IBuffer() = default;

	virtual void SetData(size_t size, const void *data, bool staticdata = true) = 0;
	virtual void SetSubData(size_t offset, size_t size, const void *data) = 0;
	virtual void *Lock(unsigned int size) = 0;
	virtual void Unlock() = 0;
	virtual void Resize(size_t newsize) = 0;
	virtual void Map() {}		// Only needed by old OpenGL but this needs to be in the interface.
	virtual void Unmap() {}
	void *Memory() { assert(map); return map; }
	size_t Size() { return buffersize; }
};

class IVertexBuffer : virtual public IBuffer
{
public:
	virtual void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) = 0;
};

// This merely exists to have a dedicated type for index buffers to inherit from.
class IIndexBuffer : virtual public IBuffer
{
	// Element size is fixed to 4, thanks to OpenGL requiring this info to be coded into the glDrawElements call.
	// This mostly prohibits a more flexible buffer setup but GZDoom doesn't use any other format anyway.
	// Ob Vulkam, element size is a buffer property and of no concern to the drawing functions (as it should be.)
};

class IDataBuffer : virtual public IBuffer
{
	// Can be either uniform or shader storage buffer, depending on its needs.
public:
	virtual void BindRange(FRenderState *state, size_t start, size_t length) = 0;

};
