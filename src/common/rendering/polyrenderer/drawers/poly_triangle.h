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

#pragma once

//#include "swrenderer/drawers/r_draw.h"
#include "r_thread.h"
#include "polyrenderer/drawers/screen_triangle.h"
#include "polyrenderer/drawers/poly_vertex_shader.h"

class DCanvas;
class RenderMemory;
class PolyDrawerCommand;
class PolyInputAssembly;
class PolyDepthStencil;
struct PolyPushConstants;

enum class PolyDrawMode
{
	Points,
	Lines,
	Triangles,
	TriangleFan,
	TriangleStrip
};

class PolyCommandBuffer
{
public:
	PolyCommandBuffer(RenderMemory* frameMemory);

	void SetViewport(int x, int y, int width, int height, DCanvas *canvas, PolyDepthStencil *depthStencil, bool topdown);
	void SetInputAssembly(PolyInputAssembly *input);
	void SetVertexBuffer(const void *vertices);
	void SetIndexBuffer(const void *elements);
	void SetLightBuffer(const void *lights);
	void SetViewpointUniforms(const HWViewpointUniforms *uniforms);
	void SetDepthClamp(bool on);
	void SetDepthMask(bool on);
	void SetDepthFunc(int func);
	void SetDepthRange(float min, float max);
	void SetDepthBias(float depthBiasConstantFactor, float depthBiasSlopeFactor);
	void SetColorMask(bool r, bool g, bool b, bool a);
	void SetStencil(int stencilRef, int op);
	void SetCulling(int mode);
	void EnableStencil(bool on);
	void SetScissor(int x, int y, int w, int h);
	void SetRenderStyle(FRenderStyle style);
	void SetTexture(int unit, void *pixels, int width, int height, bool bgra);
	void SetShader(int specialEffect, int effectState, bool alphaTest, bool colormapShader);
	void PushStreamData(const StreamData &data, const PolyPushConstants &constants);
	void PushMatrices(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix);
	void ClearDepth(float value);
	void ClearStencil(uint8_t value);
	void Draw(int index, int vcount, PolyDrawMode mode = PolyDrawMode::Triangles);
	void DrawIndexed(int index, int count, PolyDrawMode mode = PolyDrawMode::Triangles);
	void Submit();

private:
	std::shared_ptr<DrawerCommandQueue> mQueue;
};

class PolyDepthStencil
{
public:
	PolyDepthStencil(int width, int height) : width(width), height(height), depthbuffer(width * height), stencilbuffer(width * height) { }

	int Width() const { return width; }
	int Height() const { return height; }
	float *DepthValues() { return depthbuffer.data(); }
	uint8_t *StencilValues() { return stencilbuffer.data(); }

private:
	int width;
	int height;
	std::vector<float> depthbuffer;
	std::vector<uint8_t> stencilbuffer;
};

struct PolyPushConstants
{
	int uTextureMode;
	float uAlphaThreshold;
	FVector2 uClipSplit;

	// Lighting + Fog
	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;
	int uFogEnabled;

	// dynamic lights
	int uLightIndex;
};

class PolyInputAssembly
{
public:
	virtual void Load(PolyTriangleThreadData *thread, const void *vertices, int index) = 0;
};
