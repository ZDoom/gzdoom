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

#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/drawers/r_thread.h"
#include "polyrenderer/drawers/screen_triangle.h"
#include "polyrenderer/math/gpu_types.h"
#include "polyrenderer/drawers/poly_buffer.h"
#include "polyrenderer/drawers/poly_draw_args.h"
#include "polyrenderer/drawers/poly_vertex_shader.h"

class DCanvas;
class PolyDrawerCommand;
class PolyInputAssembly;
class PolyPipeline;
struct PolyPushConstants;

class PolyTriangleDrawer
{
public:
	static void ResizeBuffers(DCanvas *canvas);
	static void ClearDepth(const DrawerCommandQueuePtr &queue, float value);
	static void ClearStencil(const DrawerCommandQueuePtr &queue, uint8_t value);
	static void SetViewport(const DrawerCommandQueuePtr &queue, int x, int y, int width, int height, DCanvas *canvas);
	static void SetInputAssembly(const DrawerCommandQueuePtr &queue, PolyInputAssembly *input);
	static void SetVertexBuffer(const DrawerCommandQueuePtr &queue, const void *vertices);
	static void SetIndexBuffer(const DrawerCommandQueuePtr &queue, const void *elements);
	static void SetViewpointUniforms(const DrawerCommandQueuePtr &queue, const HWViewpointUniforms *uniforms);
	static void SetDepthClamp(const DrawerCommandQueuePtr &queue, bool on);
	static void SetDepthMask(const DrawerCommandQueuePtr &queue, bool on);
	static void SetDepthFunc(const DrawerCommandQueuePtr &queue, int func);
	static void SetDepthRange(const DrawerCommandQueuePtr &queue, float min, float max);
	static void SetDepthBias(const DrawerCommandQueuePtr &queue, float depthBiasConstantFactor, float depthBiasSlopeFactor);
	static void SetColorMask(const DrawerCommandQueuePtr &queue, bool r, bool g, bool b, bool a);
	static void SetStencil(const DrawerCommandQueuePtr &queue, int stencilRef, int op);
	static void SetCulling(const DrawerCommandQueuePtr &queue, int mode);
	static void EnableClipDistance(const DrawerCommandQueuePtr &queue, int num, bool state);
	static void EnableStencil(const DrawerCommandQueuePtr &queue, bool on);
	static void SetScissor(const DrawerCommandQueuePtr &queue, int x, int y, int w, int h);
	static void EnableDepthTest(const DrawerCommandQueuePtr &queue, bool on);
	static void SetRenderStyle(const DrawerCommandQueuePtr &queue, FRenderStyle style);
	static void SetTexture(const DrawerCommandQueuePtr &queue, void *pixels, int width, int height);
	static void SetShader(const DrawerCommandQueuePtr &queue, int specialEffect, int effectState, bool alphaTest);
	static void PushStreamData(const DrawerCommandQueuePtr &queue, const StreamData &data, const PolyPushConstants &constants);
	static void PushMatrices(const DrawerCommandQueuePtr &queue, const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix);
	static void Draw(const DrawerCommandQueuePtr &queue, int index, int vcount, PolyDrawMode mode = PolyDrawMode::Triangles);
	static void DrawIndexed(const DrawerCommandQueuePtr &queue, int index, int count, PolyDrawMode mode = PolyDrawMode::Triangles);
	static bool IsBgra();

	// Old softpoly/swrenderer interface
	static void SetCullCCW(const DrawerCommandQueuePtr &queue, bool ccw);
	static void SetTwoSided(const DrawerCommandQueuePtr &queue, bool twosided);
	static void SetWeaponScene(const DrawerCommandQueuePtr &queue, bool enable);
	static void SetModelVertexShader(const DrawerCommandQueuePtr &queue, int frame1, int frame2, float interpolationFactor);
	static void SetTransform(const DrawerCommandQueuePtr &queue, const Mat4f *objectToClip, const Mat4f *objectToWorld);
	static void PushDrawArgs(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args);
};

struct PolyPushConstants
{
	int uTextureMode;
	float uAlphaThreshold;
	Vec2f uClipSplit;

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

class PolySWInputAssembly : public PolyInputAssembly
{
public:
	void Load(PolyTriangleThreadData *thread, const void *vertices, int index) override;
};

class PolyTriangleThreadData
{
public:
	PolyTriangleThreadData(int32_t core, int32_t num_cores, int32_t numa_node, int32_t num_numa_nodes, int numa_start_y, int numa_end_y)
		: core(core), num_cores(num_cores), numa_node(numa_node), num_numa_nodes(num_numa_nodes), numa_start_y(numa_start_y), numa_end_y(numa_end_y)
	{
		swVertexShader.drawargs = &drawargs;
	}

	void ClearDepth(float value);
	void ClearStencil(uint8_t value);
	void SetViewport(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra);

	void SetTransform(const Mat4f *objectToClip, const Mat4f *objectToWorld);
	void SetCullCCW(bool value) { ccw = value; }
	void SetTwoSided(bool value) { twosided = value; }
	void SetWeaponScene(bool value) { depthbias = value ? -1.0f : 0.0f; }
	void SetModelVertexShader(int frame1, int frame2, float interpolationFactor) { modelFrame1 = frame1; modelFrame2 = frame2; swVertexShader.modelInterpolationFactor = interpolationFactor; }

	void SetInputAssembly(PolyInputAssembly *input) { inputAssembly = input; }
	void SetVertexBuffer(const void *data) { vertices = data; }
	void SetIndexBuffer(const void *data) { elements = (const unsigned int *)data; }
	void SetViewpointUniforms(const HWViewpointUniforms *uniforms);
	void SetDepthClamp(bool on);
	void SetDepthMask(bool on);
	void SetDepthFunc(int func);
	void SetDepthRange(float min, float max);
	void SetDepthBias(float depthBiasConstantFactor, float depthBiasSlopeFactor);
	void SetColorMask(bool r, bool g, bool b, bool a);
	void SetStencil(int stencilRef, int op);
	void SetCulling(int mode);
	void EnableClipDistance(int num, bool state);
	void EnableStencil(bool on);
	void SetScissor(int x, int y, int w, int h);
	void EnableDepthTest(bool on);
	void SetRenderStyle(FRenderStyle style);
	void SetTexture(void *pixels, int width, int height);
	void SetShader(int specialEffect, int effectState, bool alphaTest);

	void UpdateClip();

	void PushDrawArgs(const PolyDrawArgs &args);
	void PushStreamData(const StreamData &data, const PolyPushConstants &constants);
	void PushMatrices(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix);

	void DrawIndexed(int index, int count, PolyDrawMode mode);
	void Draw(int index, int vcount, PolyDrawMode mode);

	int32_t core;
	int32_t num_cores;
	int32_t numa_node;
	int32_t num_numa_nodes;

	int numa_start_y;
	int numa_end_y;

	bool line_skipped_by_thread(int line)
	{
		return line < numa_start_y || line >= numa_end_y || line % num_cores != core;
	}

	int skipped_by_thread(int first_line)
	{
		int clip_first_line = MAX(first_line, numa_start_y);
		int core_skip = (num_cores - (clip_first_line - core) % num_cores) % num_cores;
		return clip_first_line + core_skip - first_line;
	}

	int count_for_thread(int first_line, int count)
	{
		count = MIN(count, numa_end_y - first_line);
		int c = (count - skipped_by_thread(first_line) + num_cores - 1) / num_cores;
		return MAX(c, 0);
	}

	// Varyings
	float worldposX[MAXWIDTH];
	float worldposY[MAXWIDTH];
	float worldposZ[MAXWIDTH];
	uint32_t texel[MAXWIDTH];
	int32_t texelV[MAXWIDTH];
	uint16_t lightarray[MAXWIDTH];
	uint32_t dynlights[MAXWIDTH];
	float depthvalues[MAXWIDTH];

	static PolyTriangleThreadData *Get(DrawerThread *thread);

	int dest_pitch = 0;
	int dest_width = 0;
	int dest_height = 0;
	bool dest_bgra = false;
	uint8_t *dest = nullptr;
	float depthbias = 0.0f;

	int viewport_y = 0;

	struct ClipRect
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	} clip, scissor;

	FRenderStyle RenderStyle;
	int SpecialEffect = EFF_NONE;
	int EffectState = 0;
	bool AlphaTest = false;

	PolyDrawArgs drawargs;

	const void *vertices = nullptr;
	const unsigned int *elements = nullptr;

	PolyMainVertexShader mainVertexShader;

	int modelFrame1 = -1;
	int modelFrame2 = -1;
	PolySWVertexShader swVertexShader;

private:
	ShadedTriVertex ShadeVertex(int index);
	void DrawShadedTriangle(const ShadedTriVertex *const* vertices, bool ccw, TriDrawTriangleArgs *args);
	static bool IsDegenerate(const ShadedTriVertex *const* vertices);
	static bool IsFrontfacing(TriDrawTriangleArgs *args);

	int ClipEdge(const ShadedTriVertex *const* verts);

	int viewport_x = 0;
	int viewport_width = 0;
	int viewport_height = 0;
	bool ccw = true;
	bool twosided = true;
	PolyInputAssembly *inputAssembly = nullptr;

	enum { max_additional_vertices = 16 };
	float weightsbuffer[max_additional_vertices * 3 * 2];
	float *weights = nullptr;
};

class PolyDrawerCommand : public DrawerCommand
{
public:
};

class PolySetDepthClampCommand : public PolyDrawerCommand
{
public:
	PolySetDepthClampCommand(bool on) : on(on) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetDepthClamp(on); }

private:
	bool on;
};

class PolySetDepthMaskCommand : public PolyDrawerCommand
{
public:
	PolySetDepthMaskCommand(bool on) : on(on) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetDepthMask(on); }

private:
	bool on;
};

class PolySetDepthFuncCommand : public PolyDrawerCommand
{
public:
	PolySetDepthFuncCommand(int func) : func(func) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetDepthFunc(func); }

private:
	int func;
};

class PolySetDepthRangeCommand : public PolyDrawerCommand
{
public:
	PolySetDepthRangeCommand(float min, float max) : min(min), max(max) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetDepthRange(min, max); }

private:
	float min;
	float max;
};

class PolySetDepthBiasCommand : public PolyDrawerCommand
{
public:
	PolySetDepthBiasCommand(float depthBiasConstantFactor, float depthBiasSlopeFactor) : depthBiasConstantFactor(depthBiasConstantFactor), depthBiasSlopeFactor(depthBiasSlopeFactor) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetDepthBias(depthBiasConstantFactor, depthBiasSlopeFactor); }

private:
	float depthBiasConstantFactor;
	float depthBiasSlopeFactor;
};

class PolySetColorMaskCommand : public PolyDrawerCommand
{
public:
	PolySetColorMaskCommand(bool r, bool g, bool b, bool a) : r(r), g(g), b(b), a(a) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetColorMask(r, g, b, a); }

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
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetStencil(stencilRef, op); }

private:
	int stencilRef;
	int op;
};

class PolySetCullingCommand : public PolyDrawerCommand
{
public:
	PolySetCullingCommand(int mode) : mode(mode) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetCulling(mode); }

private:
	int mode;
};

class PolyEnableClipDistanceCommand : public PolyDrawerCommand
{
public:
	PolyEnableClipDistanceCommand(int num, bool state) : num(num), state(state) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->EnableClipDistance(num, state); }

private:
	int num;
	bool state;
};

class PolyEnableStencilCommand : public PolyDrawerCommand
{
public:
	PolyEnableStencilCommand(bool on) : on(on) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->EnableStencil(on); }

private:
	bool on;
};

class PolySetScissorCommand : public PolyDrawerCommand
{
public:
	PolySetScissorCommand(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetScissor(x, y, w, h); }

private:
	int x;
	int y;
	int w;
	int h;
};

class PolyEnableDepthTestCommand : public PolyDrawerCommand
{
public:
	PolyEnableDepthTestCommand(bool on) : on(on) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->EnableDepthTest(on); }

private:
	bool on;
};

class PolySetRenderStyleCommand : public PolyDrawerCommand
{
public:
	PolySetRenderStyleCommand(FRenderStyle style) : style(style) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetRenderStyle(style); }

private:
	FRenderStyle style;
};

class PolySetTextureCommand : public PolyDrawerCommand
{
public:
	PolySetTextureCommand(void *pixels, int width, int height) : pixels(pixels), width(width), height(height) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetTexture(pixels, width, height); }

private:
	void *pixels;
	int width;
	int height;
};

class PolySetShaderCommand : public PolyDrawerCommand
{
public:
	PolySetShaderCommand(int specialEffect, int effectState, bool alphaTest) : specialEffect(specialEffect), effectState(effectState), alphaTest(alphaTest) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetShader(specialEffect, effectState, alphaTest); }

private:
	int specialEffect;
	int effectState;
	bool alphaTest;
};

class PolySetVertexBufferCommand : public PolyDrawerCommand
{
public:
	PolySetVertexBufferCommand(const void *vertices) : vertices(vertices) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetVertexBuffer(vertices); }

private:
	const void *vertices;
};

class PolySetIndexBufferCommand : public PolyDrawerCommand
{
public:
	PolySetIndexBufferCommand(const void *indices) : indices(indices) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetIndexBuffer(indices); }

private:
	const void *indices;
};

class PolySetInputAssemblyCommand : public PolyDrawerCommand
{
public:
	PolySetInputAssemblyCommand(PolyInputAssembly *input) : input(input) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetInputAssembly(input); }

private:
	PolyInputAssembly *input;
};

class PolySetTransformCommand : public PolyDrawerCommand
{
public:
	PolySetTransformCommand(const Mat4f *objectToClip, const Mat4f *objectToWorld) : objectToClip(objectToClip), objectToWorld(objectToWorld) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetTransform(objectToClip, objectToWorld); }

private:
	const Mat4f *objectToClip;
	const Mat4f *objectToWorld;
};

class PolySetCullCCWCommand : public PolyDrawerCommand
{
public:
	PolySetCullCCWCommand(bool ccw) : ccw(ccw) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetCullCCW(ccw); }

private:
	bool ccw;
};

class PolySetTwoSidedCommand : public PolyDrawerCommand
{
public:
	PolySetTwoSidedCommand(bool twosided) : twosided(twosided) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetTwoSided(twosided); }

private:
	bool twosided;
};

class PolySetWeaponSceneCommand : public PolyDrawerCommand
{
public:
	PolySetWeaponSceneCommand(bool value) : value(value) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetWeaponScene(value); }

private:
	bool value;
};

class PolySetModelVertexShaderCommand : public PolyDrawerCommand
{
public:
	PolySetModelVertexShaderCommand(int frame1, int frame2, float interpolationFactor) : frame1(frame1), frame2(frame2), interpolationFactor(interpolationFactor) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetModelVertexShader(frame1, frame2, interpolationFactor); }

private:
	int frame1;
	int frame2;
	float interpolationFactor;
};

class PolyClearDepthCommand : public PolyDrawerCommand
{
public:
	PolyClearDepthCommand(float value) : value(value) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->ClearDepth(value); }

private:
	float value;
};

class PolyClearStencilCommand : public PolyDrawerCommand
{
public:
	PolyClearStencilCommand(uint8_t value) : value(value) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->ClearStencil(value); }

private:
	uint8_t value;
};

class PolySetViewportCommand : public PolyDrawerCommand
{
public:
	PolySetViewportCommand(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra)
		: x(x), y(y), width(width), height(height), dest(dest), dest_width(dest_width), dest_height(dest_height), dest_pitch(dest_pitch), dest_bgra(dest_bgra) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetViewport(x, y, width, height, dest, dest_width, dest_height, dest_pitch, dest_bgra); }

private:
	int x;
	int y;
	int width;
	int height;
	uint8_t *dest;
	int dest_width;
	int dest_height;
	int dest_pitch;
	bool dest_bgra;
};

class PolySetViewpointUniformsCommand : public PolyDrawerCommand
{
public:
	PolySetViewpointUniformsCommand(const HWViewpointUniforms *uniforms) : uniforms(uniforms) {}
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->SetViewpointUniforms(uniforms); }

private:
	const HWViewpointUniforms *uniforms;
};

class PolyPushMatricesCommand : public PolyDrawerCommand
{
public:
	PolyPushMatricesCommand(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix)
		: modelMatrix(modelMatrix), normalModelMatrix(normalModelMatrix), textureMatrix(textureMatrix) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->PushMatrices(modelMatrix, normalModelMatrix, textureMatrix); }

private:
	VSMatrix modelMatrix;
	VSMatrix normalModelMatrix;
	VSMatrix textureMatrix;
};

class PolyPushStreamDataCommand : public PolyDrawerCommand
{
public:
	PolyPushStreamDataCommand(const StreamData &data, const PolyPushConstants &constants) : data(data), constants(constants) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->PushStreamData(data, constants); }

private:
	StreamData data;
	PolyPushConstants constants;
};

class PolyPushDrawArgsCommand : public PolyDrawerCommand
{
public:
	PolyPushDrawArgsCommand(const PolyDrawArgs &args) : args(args) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->PushDrawArgs(args); }

private:
	PolyDrawArgs args;
};

class PolyDrawCommand : public PolyDrawerCommand
{
public:
	PolyDrawCommand(int index, int count, PolyDrawMode mode) : index(index), count(count), mode(mode) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->Draw(index, count, mode); }

private:
	int index;
	int count;
	PolyDrawMode mode;
};

class PolyDrawIndexedCommand : public PolyDrawerCommand
{
public:
	PolyDrawIndexedCommand(int index, int count, PolyDrawMode mode) : index(index), count(count), mode(mode) { }
	void Execute(DrawerThread *thread) override { PolyTriangleThreadData::Get(thread)->DrawIndexed(index, count, mode); }

private:
	int index;
	int count;
	PolyDrawMode mode;
};
