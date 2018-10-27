#pragma once

// The low level code needs to know which attributes exist.
// OpenGL needs to change the state of all of them per buffer binding.
// VAOs are mostly useless for this because they lump buffer and binding state together which the model code does not want.
enum
{
	VATTR_VERTEX_BIT,
	VATTR_TEXCOORD_BIT,
	VATTR_COLOR_BIT,
	VATTR_VERTEX2_BIT,
	VATTR_NORMAL_BIT,
	VATTR_NORMAL2_BIT,
	
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
	virtual void SetFormat(int numBindingPoints, int numAttributes, size_t stride, FVertexBufferAttribute *attrs) = 0;
	virtual void Map() {}		// Only needed by old OpenGL but this needs to be in the interface.
	virtual void Unmap() {}
	void *Memory() { assert(map); return map; }
};

