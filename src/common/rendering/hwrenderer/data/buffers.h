#pragma once

#include <stddef.h>
#include <assert.h>

class FRenderState;

#ifdef __ANDROID__
#define HW_MAX_PIPELINE_BUFFERS 4
#define HW_BLOCK_SSBO 1
#else
// On desktop this is only useful fpr letting the GPU run in parallel with the playsim and for that 2 buffers are enough.
#define HW_MAX_PIPELINE_BUFFERS 2
#endif

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
	VATTR_LIGHTMAP,	
	VATTR_BONEWEIGHT,
	VATTR_BONESELECTOR,
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
	VFmt_Byte4_UInt
};

struct FVertexBufferAttribute
{
	int binding;
	int location;
	int format;
	int offset;
};

enum class BufferUsageType
{
	Static,     // initial data is not null, staticdata is true
	Stream,     // initial data is not null, staticdata is false
	Persistent, // initial data is null, staticdata is false
	Mappable    // initial data is null, staticdata is true
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

	virtual void SetData(size_t size, const void *data, BufferUsageType type) = 0;
	virtual void SetSubData(size_t offset, size_t size, const void *data) = 0;
	virtual void *Lock(unsigned int size) = 0;
	virtual void Unlock() = 0;
	virtual void Resize(size_t newsize) = 0;

	virtual void Upload(size_t start, size_t size) {} // For unmappable buffers

	virtual void Map() {}		// Only needed by old OpenGL but this needs to be in the interface.
	virtual void Unmap() {}
	void *Memory() { return map; }
	size_t Size() { return buffersize; }
	virtual void GPUDropSync() {}
	virtual void GPUWaitSync() {}
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
