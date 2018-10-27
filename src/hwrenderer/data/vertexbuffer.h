#pragma once

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

class IVertexBuffer
{
protected:
	size_t buffersize = 0;
	void *map = nullptr;
	bool nomap = true;
public:
	virtual ~IVertexBuffer() {}
	virtual void SetData(size_t size, void *data, bool staticdata = true) = 0;
	virtual void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) = 0;
	virtual void *Lock(unsigned int size) = 0;
	virtual void Unlock() = 0;
	virtual void Map() {}		// Only needed by old OpenGL but this needs to be in the interface.
	virtual void Unmap() {}
	void *Memory() { assert(map); return map; }
};


class IIndexBuffer
{
protected:
	// Element size is fixed to 4, thanks to OpenGL requiring this info to be coded into the glDrawElements call.
	// This mostly prohibits a more flexible buffer setup but GZDoom doesn't use any other format anyway.
	// Ob Vulkam, element size is a buffer property and of no concern to the drawing functions (as it should be.)
	size_t buffersize = 0;
public:
	virtual ~IIndexBuffer() {}
	virtual void SetData(size_t size, void *data, bool staticdata = true) = 0;
	virtual void *Lock(unsigned int size) = 0;
	virtual void Unlock() = 0;
};

