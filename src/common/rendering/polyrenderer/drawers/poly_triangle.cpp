/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stddef.h>
#include "templates.h"

#include "filesystem.h"
#include "v_video.h"
#include "model.h"
#include "poly_triangle.h"
#include "poly_thread.h"
#include "screen_triangle.h"

/////////////////////////////////////////////////////////////////////////////

class PolyDrawerCommand : public DrawerCommand
{
public:
};

class PolySetDepthClampCommand : public PolyDrawerCommand
{
public:
	PolySetDepthClampCommand(bool on) : on(on) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetDepthClamp(on); }

private:
	bool on;
};

class PolySetDepthMaskCommand : public PolyDrawerCommand
{
public:
	PolySetDepthMaskCommand(bool on) : on(on) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetDepthMask(on); }

private:
	bool on;
};

class PolySetDepthFuncCommand : public PolyDrawerCommand
{
public:
	PolySetDepthFuncCommand(int func) : func(func) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetDepthFunc(func); }

private:
	int func;
};

class PolySetDepthRangeCommand : public PolyDrawerCommand
{
public:
	PolySetDepthRangeCommand(float min, float max) : min(min), max(max) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetDepthRange(min, max); }

private:
	float min;
	float max;
};

class PolySetDepthBiasCommand : public PolyDrawerCommand
{
public:
	PolySetDepthBiasCommand(float depthBiasConstantFactor, float depthBiasSlopeFactor) : depthBiasConstantFactor(depthBiasConstantFactor), depthBiasSlopeFactor(depthBiasSlopeFactor) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetDepthBias(depthBiasConstantFactor, depthBiasSlopeFactor); }

private:
	float depthBiasConstantFactor;
	float depthBiasSlopeFactor;
};

class PolySetColorMaskCommand : public PolyDrawerCommand
{
public:
	PolySetColorMaskCommand(bool r, bool g, bool b, bool a) : r(r), g(g), b(b), a(a) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetColorMask(r, g, b, a); }

private:
	bool r;
	bool g;
	bool b;
	bool a;
};

class PolySetStencilCommand : public PolyDrawerCommand
{
public:
	PolySetStencilCommand(int stencilRef, int op) : stencilRef(stencilRef), op(op) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetStencil(stencilRef, op); }

private:
	int stencilRef;
	int op;
};

class PolySetCullingCommand : public PolyDrawerCommand
{
public:
	PolySetCullingCommand(int mode) : mode(mode) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetCulling(mode); }

private:
	int mode;
};

class PolyEnableStencilCommand : public PolyDrawerCommand
{
public:
	PolyEnableStencilCommand(bool on) : on(on) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->EnableStencil(on); }

private:
	bool on;
};

class PolySetScissorCommand : public PolyDrawerCommand
{
public:
	PolySetScissorCommand(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetScissor(x, y, w, h); }

private:
	int x;
	int y;
	int w;
	int h;
};

class PolySetRenderStyleCommand : public PolyDrawerCommand
{
public:
	PolySetRenderStyleCommand(FRenderStyle style) : style(style) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetRenderStyle(style); }

private:
	FRenderStyle style;
};

class PolySetTextureCommand : public PolyDrawerCommand
{
public:
	PolySetTextureCommand(int unit, void* pixels, int width, int height, bool bgra) : unit(unit), pixels(pixels), width(width), height(height), bgra(bgra) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetTexture(unit, pixels, width, height, bgra); }

private:
	int unit;
	void* pixels;
	int width;
	int height;
	bool bgra;
};

class PolySetShaderCommand : public PolyDrawerCommand
{
public:
	PolySetShaderCommand(int specialEffect, int effectState, bool alphaTest, bool colormapShader) : specialEffect(specialEffect), effectState(effectState), alphaTest(alphaTest), colormapShader(colormapShader) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetShader(specialEffect, effectState, alphaTest, colormapShader); }

private:
	int specialEffect;
	int effectState;
	bool alphaTest;
	bool colormapShader;
};

class PolySetVertexBufferCommand : public PolyDrawerCommand
{
public:
	PolySetVertexBufferCommand(const void* vertices, int offset0, int offset1) : vertices(vertices), offset0(offset0), offset1(offset1){ }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetVertexBuffer(vertices, offset0, offset1); }

private:
	const void* vertices;
	// [GEC] Add offset params, necessary to project frames and model interpolation correctly
	int offset0; 
	int offset1;
};

class PolySetIndexBufferCommand : public PolyDrawerCommand
{
public:
	PolySetIndexBufferCommand(const void* indices) : indices(indices) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetIndexBuffer(indices); }

private:
	const void* indices;
};

class PolySetLightBufferCommand : public PolyDrawerCommand
{
public:
	PolySetLightBufferCommand(const void* lights) : lights(lights) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetLightBuffer(lights); }

private:
	const void* lights;
};

class PolySetInputAssemblyCommand : public PolyDrawerCommand
{
public:
	PolySetInputAssemblyCommand(PolyInputAssembly* input) : input(input) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetInputAssembly(input); }

private:
	PolyInputAssembly* input;
};

class PolyClearDepthCommand : public PolyDrawerCommand
{
public:
	PolyClearDepthCommand(float value) : value(value) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->ClearDepth(value); }

private:
	float value;
};

class PolyClearStencilCommand : public PolyDrawerCommand
{
public:
	PolyClearStencilCommand(uint8_t value) : value(value) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->ClearStencil(value); }

private:
	uint8_t value;
};

class PolySetViewportCommand : public PolyDrawerCommand
{
public:
	PolySetViewportCommand(int x, int y, int width, int height, uint8_t* dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra, PolyDepthStencil* depthstencil, bool topdown)
		: x(x), y(y), width(width), height(height), dest(dest), dest_width(dest_width), dest_height(dest_height), dest_pitch(dest_pitch), dest_bgra(dest_bgra), depthstencil(depthstencil), topdown(topdown) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetViewport(x, y, width, height, dest, dest_width, dest_height, dest_pitch, dest_bgra, depthstencil, topdown); }

private:
	int x;
	int y;
	int width;
	int height;
	uint8_t* dest;
	int dest_width;
	int dest_height;
	int dest_pitch;
	bool dest_bgra;
	PolyDepthStencil* depthstencil;
	bool topdown;
};

class PolySetViewpointUniformsCommand : public PolyDrawerCommand
{
public:
	PolySetViewpointUniformsCommand(const HWViewpointUniforms* uniforms) : uniforms(uniforms) {}
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->SetViewpointUniforms(uniforms); }

private:
	const HWViewpointUniforms* uniforms;
};

class PolyPushMatricesCommand : public PolyDrawerCommand
{
public:
	PolyPushMatricesCommand(const VSMatrix& modelMatrix, const VSMatrix& normalModelMatrix, const VSMatrix& textureMatrix)
		: modelMatrix(modelMatrix), normalModelMatrix(normalModelMatrix), textureMatrix(textureMatrix) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->PushMatrices(modelMatrix, normalModelMatrix, textureMatrix); }

private:
	VSMatrix modelMatrix;
	VSMatrix normalModelMatrix;
	VSMatrix textureMatrix;
};

class PolyPushStreamDataCommand : public PolyDrawerCommand
{
public:
	PolyPushStreamDataCommand(const StreamData& data, const PolyPushConstants& constants) : data(data), constants(constants) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->PushStreamData(data, constants); }

private:
	StreamData data;
	PolyPushConstants constants;
};

class PolyDrawCommand : public PolyDrawerCommand
{
public:
	PolyDrawCommand(int index, int count, PolyDrawMode mode) : index(index), count(count), mode(mode) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->Draw(index, count, mode); }

private:
	int index;
	int count;
	PolyDrawMode mode;
};

class PolyDrawIndexedCommand : public PolyDrawerCommand
{
public:
	PolyDrawIndexedCommand(int index, int count, PolyDrawMode mode) : index(index), count(count), mode(mode) { }
	void Execute(DrawerThread* thread) override { PolyTriangleThreadData::Get(thread)->DrawIndexed(index, count, mode); }

private:
	int index;
	int count;
	PolyDrawMode mode;
};

/////////////////////////////////////////////////////////////////////////////

PolyCommandBuffer::PolyCommandBuffer(RenderMemory* frameMemory)
{
	mQueue = std::make_shared<DrawerCommandQueue>(frameMemory);
}

void PolyCommandBuffer::SetViewport(int x, int y, int width, int height, DCanvas *canvas, PolyDepthStencil *depthstencil, bool topdown)
{
	uint8_t *dest = (uint8_t*)canvas->GetPixels();
	int dest_width = canvas->GetWidth();
	int dest_height = canvas->GetHeight();
	int dest_pitch = canvas->GetPitch();
	bool dest_bgra = canvas->IsBgra();

	mQueue->Push<PolySetViewportCommand>(x, y, width, height, dest, dest_width, dest_height, dest_pitch, dest_bgra, depthstencil, topdown);
}

void PolyCommandBuffer::SetInputAssembly(PolyInputAssembly *input)
{
	mQueue->Push<PolySetInputAssemblyCommand>(input);
}

void PolyCommandBuffer::SetVertexBuffer(const void *vertices, int offset0, int offset1)
{
	mQueue->Push<PolySetVertexBufferCommand>(vertices, offset0, offset1);
}

void PolyCommandBuffer::SetIndexBuffer(const void *elements)
{
	mQueue->Push<PolySetIndexBufferCommand>(elements);
}

void PolyCommandBuffer::SetLightBuffer(const void *lights)
{
	mQueue->Push<PolySetLightBufferCommand>(lights);
}

void PolyCommandBuffer::SetDepthClamp(bool on)
{
	mQueue->Push<PolySetDepthClampCommand>(on);
}

void PolyCommandBuffer::SetDepthMask(bool on)
{
	mQueue->Push<PolySetDepthMaskCommand>(on);
}

void PolyCommandBuffer::SetDepthFunc(int func)
{
	mQueue->Push<PolySetDepthFuncCommand>(func);
}

void PolyCommandBuffer::SetDepthRange(float min, float max)
{
	mQueue->Push<PolySetDepthRangeCommand>(min, max);
}

void PolyCommandBuffer::SetDepthBias(float depthBiasConstantFactor, float depthBiasSlopeFactor)
{
	mQueue->Push<PolySetDepthBiasCommand>(depthBiasConstantFactor, depthBiasSlopeFactor);
}

void PolyCommandBuffer::SetColorMask(bool r, bool g, bool b, bool a)
{
	mQueue->Push<PolySetColorMaskCommand>(r, g, b, a);
}

void PolyCommandBuffer::SetStencil(int stencilRef, int op)
{
	mQueue->Push<PolySetStencilCommand>(stencilRef, op);
}

void PolyCommandBuffer::SetCulling(int mode)
{
	mQueue->Push<PolySetCullingCommand>(mode);
}

void PolyCommandBuffer::EnableStencil(bool on)
{
	mQueue->Push<PolyEnableStencilCommand>(on);
}

void PolyCommandBuffer::SetScissor(int x, int y, int w, int h)
{
	mQueue->Push<PolySetScissorCommand>(x, y, w, h);
}

void PolyCommandBuffer::SetRenderStyle(FRenderStyle style)
{
	mQueue->Push<PolySetRenderStyleCommand>(style);
}

void PolyCommandBuffer::SetTexture(int unit, void *pixels, int width, int height, bool bgra)
{
	mQueue->Push<PolySetTextureCommand>(unit, pixels, width, height, bgra);
}

void PolyCommandBuffer::SetShader(int specialEffect, int effectState, bool alphaTest, bool colormapShader)
{
	mQueue->Push<PolySetShaderCommand>(specialEffect, effectState, alphaTest, colormapShader);
}

void PolyCommandBuffer::PushStreamData(const StreamData &data, const PolyPushConstants &constants)
{
	mQueue->Push<PolyPushStreamDataCommand>(data, constants);
}

void PolyCommandBuffer::PushMatrices(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix)
{
	mQueue->Push<PolyPushMatricesCommand>(modelMatrix, normalModelMatrix, textureMatrix);
}

void PolyCommandBuffer::SetViewpointUniforms(const HWViewpointUniforms *uniforms)
{
	mQueue->Push<PolySetViewpointUniformsCommand>(uniforms);
}

void PolyCommandBuffer::ClearDepth(float value)
{
	mQueue->Push<PolyClearDepthCommand>(value);
}

void PolyCommandBuffer::ClearStencil(uint8_t value)
{
	mQueue->Push<PolyClearStencilCommand>(value);
}

void PolyCommandBuffer::Draw(int index, int vcount, PolyDrawMode mode)
{
	mQueue->Push<PolyDrawCommand>(index, vcount, mode);
}

void PolyCommandBuffer::DrawIndexed(int index, int count, PolyDrawMode mode)
{
	mQueue->Push<PolyDrawIndexedCommand>(index, count, mode);
}

void PolyCommandBuffer::Submit()
{
	DrawerThreads::Execute(mQueue);
}
