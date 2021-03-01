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

#include "poly_triangle.h"

struct PolyLight
{
	uint32_t color;
	float x, y, z;
	float radius;
};

class PolyTriangleThreadData
{
public:
	PolyTriangleThreadData(int32_t core, int32_t num_cores, int32_t numa_node, int32_t num_numa_nodes, int numa_start_y, int numa_end_y);

	void ClearDepth(float value);
	void ClearStencil(uint8_t value);
	void SetViewport(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra, PolyDepthStencil *depthstencil, bool topdown);

	void SetCullCCW(bool value) { ccw = value; }
	void SetTwoSided(bool value) { twosided = value; }

	void SetInputAssembly(PolyInputAssembly *input) { inputAssembly = input; }
	void SetVertexBuffer(const void *data, int offset0, int offset1) { vertices = data; frame0 = offset0; frame1 = offset1;} //[GEC] Save frame params
	void SetIndexBuffer(const void *data) { elements = (const unsigned int *)data; }
	void SetLightBuffer(const void *data) { lights = (const FVector4 *)data; }
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
	void SetTexture(int unit, const void *pixels, int width, int height, bool bgra);
	void SetShader(int specialEffect, int effectState, bool alphaTest, bool colormapShader);

	void UpdateClip();

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

	struct Scanline
	{
		float W[MAXWIDTH];
		uint16_t U[MAXWIDTH];
		uint16_t V[MAXWIDTH];
		float WorldX[MAXWIDTH];
		float WorldY[MAXWIDTH];
		float WorldZ[MAXWIDTH];
		uint8_t vColorA[MAXWIDTH];
		uint8_t vColorR[MAXWIDTH];
		uint8_t vColorG[MAXWIDTH];
		uint8_t vColorB[MAXWIDTH];
		float GradientdistZ[MAXWIDTH];
		uint32_t FragColor[MAXWIDTH];
		uint32_t lightarray[MAXWIDTH];
		uint8_t discard[MAXWIDTH];
	} scanline;

	static PolyTriangleThreadData *Get(DrawerThread *thread);

	int dest_pitch = 0;
	int dest_width = 0;
	int dest_height = 0;
	bool dest_bgra = false;
	uint8_t *dest = nullptr;
	PolyDepthStencil *depthstencil = nullptr;
	bool topdown = true;

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
	bool ColormapShader = false;
	uint32_t AlphaThreshold = 0x7f000000;
	const PolyPushConstants* PushConstants = nullptr;

	// [GEC] Add frame params, necessary to project frames and model interpolation correctly
	int frame0 = 0;
	int frame1 = 0;

	const void *vertices = nullptr;
	const unsigned int *elements = nullptr;
	const FVector4 *lights = nullptr;

	enum { maxPolyLights = 16 };
	PolyLight polyLights[maxPolyLights];
	int numPolyLights = 0;

	PolyMainVertexShader mainVertexShader;

	struct TextureUnit
	{
		const void* pixels = nullptr;
		int width = 0;
		int height = 0;
		bool bgra = true;
	} textures[16];

	bool DepthTest = false;
	bool StencilTest = true;
	bool WriteStencil = true;
	bool WriteColor = true;
	bool WriteDepth = true;
	uint8_t StencilTestValue = 0;
	uint8_t StencilWriteValue = 0;
	float DepthRangeStart = 0.0f;
	float DepthRangeScale = 1.0f;

	void (*FragmentShader)(int x0, int x1, PolyTriangleThreadData* thread) = nullptr;
	void (*WriteColorFunc)(int y, int x0, int x1, PolyTriangleThreadData* thread) = nullptr;

private:
	ShadedTriVertex ShadeVertex(int index);
	void DrawShadedPoint(const ShadedTriVertex *const* vertex);
	void DrawShadedLine(const ShadedTriVertex *const* vertices);
	void DrawShadedTriangle(const ShadedTriVertex *const* vertices, bool ccw);
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
