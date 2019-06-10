
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

	auto fb = GetPolyFrameBuffer();
	if (fb && !mData.empty())
		fb->FrameDeleteList.Buffers.push_back(std::move(mData));
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
	VertexFormat = GetPolyFrameBuffer()->GetRenderState()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs);
}

/////////////////////////////////////////////////////////////////////////////

void PolyVertexInputAssembly::Load(PolyTriangleThreadData *thread, const void *vertices, int index)
{
	const uint8_t *vertex = static_cast<const uint8_t*>(vertices) + mStride * index;
	const float *attrVertex = reinterpret_cast<const float*>(vertex + mOffsets[VATTR_VERTEX]);
	const float *attrTexcoord = reinterpret_cast<const float*>(vertex + mOffsets[VATTR_TEXCOORD]);
	const uint8_t *attrColor = reinterpret_cast<const uint8_t*>(vertex + mOffsets[VATTR_COLOR]);

	thread->mainVertexShader.aPosition = { attrVertex[0], attrVertex[1], attrVertex[2], 1.0f };
	thread->mainVertexShader.aTexCoord = { attrTexcoord[0], attrTexcoord[1] };

	if (UseVertexData)
	{
		uint32_t r = attrColor[0];
		uint32_t g = attrColor[1];
		uint32_t b = attrColor[2];
		uint32_t a = attrColor[3];
		thread->mainVertexShader.aColor = MAKEARGB(a, r, g, b);
	}
	else
	{
		const auto &c = thread->mainVertexShader.Data.uVertexColor;
		thread->mainVertexShader.aColor = MAKEARGB(
			static_cast<uint32_t>(c.W * 255.0f + 0.5f),
			static_cast<uint32_t>(c.X * 255.0f + 0.5f),
			static_cast<uint32_t>(c.Y * 255.0f + 0.5f),
			static_cast<uint32_t>(c.Z * 255.0f + 0.5f)
		);
	}
}

/////////////////////////////////////////////////////////////////////////////

void PolyDataBuffer::BindRange(FRenderState *state, size_t start, size_t length)
{
	static_cast<PolyRenderState*>(state)->Bind(this, (uint32_t)start, (uint32_t)length);
}
