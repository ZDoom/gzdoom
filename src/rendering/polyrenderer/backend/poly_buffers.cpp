
#include "poly_buffers.h"
#include "poly_framebuffer.h"
#include "poly_renderstate.h"
#include "doomerrors.h"

PolyBuffer *PolyBuffer::First = nullptr;

PolyBuffer::PolyBuffer()
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

PolyBuffer::~PolyBuffer()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;
}

void PolyBuffer::ResetAll()
{
	for (PolyBuffer *cur = PolyBuffer::First; cur; cur = cur->Next)
		cur->Reset();
}

void PolyBuffer::Reset()
{
}

void PolyBuffer::SetData(size_t size, const void *data, bool staticdata)
{
	mData.resize(size);
	map = mData.data();
	if (data)
		memcpy(map, data, size);
	buffersize = size;
}

void PolyBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
	memcpy(static_cast<uint8_t*>(map) + offset, data, size);
}

void PolyBuffer::Resize(size_t newsize)
{
	mData.resize(newsize);
	buffersize = newsize;
	map = mData.data();
}

void PolyBuffer::Map()
{
}

void PolyBuffer::Unmap()
{
}

void *PolyBuffer::Lock(unsigned int size)
{
	return map;
}

void PolyBuffer::Unlock()
{
}

/////////////////////////////////////////////////////////////////////////////

void PolyVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	for (int j = 0; j < numAttributes; j++)
	{
		mOffsets[attrs[j].location] = attrs[j].offset;
	}
	mStride = stride;
}

void PolyVertexBuffer::CopyVertices(TriVertex *dst, int count, int index)
{
	size_t stride = mStride;
	size_t offsetVertex = mOffsets[VATTR_VERTEX];
	size_t offsetTexcoord = mOffsets[VATTR_TEXCOORD];
	uint8_t *vertex = static_cast<uint8_t*>(map) + stride * index;

	for (int i = 0; i < count; i++)
	{
		dst[i].x = *reinterpret_cast<float*>(vertex + offsetVertex);
		dst[i].y = *reinterpret_cast<float*>(vertex + offsetVertex + 4);
		dst[i].z = *reinterpret_cast<float*>(vertex + offsetVertex + 8);
		dst[i].w = 1.0f;
		dst[i].v = *reinterpret_cast<float*>(vertex + offsetTexcoord);
		dst[i].u = *reinterpret_cast<float*>(vertex + offsetTexcoord + 4);
		vertex += stride;
	}
}

void PolyVertexBuffer::CopyIndexed(TriVertex *dst, uint32_t *elements, int count, int index)
{
	size_t stride = mStride;
	size_t offsetVertex = mOffsets[VATTR_VERTEX];
	size_t offsetTexcoord = mOffsets[VATTR_TEXCOORD];
	uint8_t *vertices = static_cast<uint8_t*>(map);

	elements += index;
	for (int i = 0; i < count; i++)
	{
		uint8_t *vertex = vertices + stride * elements[i];
		dst[i].x = *reinterpret_cast<float*>(vertex + offsetVertex);
		dst[i].y = *reinterpret_cast<float*>(vertex + offsetVertex + 4);
		dst[i].z = *reinterpret_cast<float*>(vertex + offsetVertex + 8);
		dst[i].w = 1.0f;
		dst[i].v = *reinterpret_cast<float*>(vertex + offsetTexcoord);
		dst[i].u = *reinterpret_cast<float*>(vertex + offsetTexcoord + 4);
	}
}

/////////////////////////////////////////////////////////////////////////////

void PolyDataBuffer::BindRange(size_t start, size_t length)
{
	GetPolyFrameBuffer()->GetRenderState()->Bind(this, (uint32_t)start, (uint32_t)length);
}

void PolyDataBuffer::BindBase()
{
	GetPolyFrameBuffer()->GetRenderState()->Bind(this, 0, (uint32_t)buffersize);
}
