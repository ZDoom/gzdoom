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
#include "poly_thread.h"
#include "screen_triangle.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

PolyTriangleThreadData::PolyTriangleThreadData(int32_t core, int32_t num_cores, int32_t numa_node, int32_t num_numa_nodes, int numa_start_y, int numa_end_y)
	: core(core), num_cores(num_cores), numa_node(numa_node), num_numa_nodes(num_numa_nodes), numa_start_y(numa_start_y), numa_end_y(numa_end_y)
{
}

void PolyTriangleThreadData::ClearDepth(float value)
{
	int width = depthstencil->Width();
	int height = depthstencil->Height();
	float *data = depthstencil->DepthValues();

	int skip = skipped_by_thread(0);
	int count = count_for_thread(0, height);

	data += skip * width;
	for (int i = 0; i < count; i++)
	{
		for (int x = 0; x < width; x++)
			data[x] = value;
		data += num_cores * width;
	}
}

void PolyTriangleThreadData::ClearStencil(uint8_t value)
{
	int width = depthstencil->Width();
	int height = depthstencil->Height();
	uint8_t *data = depthstencil->StencilValues();

	int skip = skipped_by_thread(0);
	int count = count_for_thread(0, height);

	data += skip * width;
	for (int i = 0; i < count; i++)
	{
		memset(data, value, width);
		data += num_cores * width;
	}
}

void PolyTriangleThreadData::SetViewport(int x, int y, int width, int height, uint8_t *new_dest, int new_dest_width, int new_dest_height, int new_dest_pitch, bool new_dest_bgra, PolyDepthStencil *new_depthstencil, bool new_topdown)
{
	viewport_x = x;
	viewport_y = y;
	viewport_width = width;
	viewport_height = height;
	dest = new_dest;
	dest_width = new_dest_width;
	dest_height = new_dest_height;
	dest_pitch = new_dest_pitch;
	dest_bgra = new_dest_bgra;
	depthstencil = new_depthstencil;
	topdown = new_topdown;
	UpdateClip();
}

void PolyTriangleThreadData::SetScissor(int x, int y, int w, int h)
{
	scissor.left = x;
	scissor.right = x + w;
	scissor.top = y;
	scissor.bottom = y + h;
	UpdateClip();
}

void PolyTriangleThreadData::UpdateClip()
{
	clip.left = MAX(MAX(viewport_x, scissor.left), 0);
	clip.top = MAX(MAX(viewport_y, scissor.top), 0);
	clip.right = MIN(MIN(viewport_x + viewport_width, scissor.right), dest_width);
	clip.bottom = MIN(MIN(viewport_y + viewport_height, scissor.bottom), dest_height);
}

void PolyTriangleThreadData::PushStreamData(const StreamData &data, const PolyPushConstants &constants)
{
	mainVertexShader.Data = data;
	mainVertexShader.uClipSplit = constants.uClipSplit;

	PushConstants = &constants;

	AlphaThreshold = clamp((int)(PushConstants->uAlphaThreshold * 255.0f + 0.5f), 0, 255) << 24;

	numPolyLights = 0;
	if (constants.uLightIndex >= 0)
	{
		const FVector4 &lightRange = lights[constants.uLightIndex];
		static_assert(sizeof(FVector4) == 16, "sizeof(FVector4) is not 16 bytes");
		if (lightRange.Y > lightRange.X)
		{
			int start = constants.uLightIndex + 1;
			int modulatedStart = static_cast<int>(lightRange.X) + start;
			int modulatedEnd = static_cast<int>(lightRange.Y) + start;
			for (int i = modulatedStart; i < modulatedEnd; i += 4)
			{
				if (numPolyLights == maxPolyLights)
					break;

				auto &lightpos = lights[i];
				auto &lightcolor = lights[i + 1];
				//auto &lightspot1 = lights[i + 2];
				//auto &lightspot2 = lights[i + 3];
				uint32_t r = (int)clamp(lightcolor.X * 255.0f, 0.0f, 255.0f);
				uint32_t g = (int)clamp(lightcolor.Y * 255.0f, 0.0f, 255.0f);
				uint32_t b = (int)clamp(lightcolor.Z * 255.0f, 0.0f, 255.0f);

				auto& polylight = polyLights[numPolyLights++];
				polylight.x = lightpos.X;
				polylight.y = lightpos.Y;
				polylight.z = lightpos.Z;
				polylight.radius = 256.0f / lightpos.W;
				polylight.color = (r << 16) | (g << 8) | b;
				if (lightcolor.W < 0.0f)
					polylight.radius = -polylight.radius;
			}
		}
	}
}

void PolyTriangleThreadData::PushMatrices(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix)
{
	mainVertexShader.ModelMatrix = modelMatrix;
	mainVertexShader.NormalModelMatrix = normalModelMatrix;
	mainVertexShader.TextureMatrix = textureMatrix;
}

void PolyTriangleThreadData::SetViewpointUniforms(const HWViewpointUniforms *uniforms)
{
	mainVertexShader.Viewpoint = uniforms;
}

void PolyTriangleThreadData::SetDepthClamp(bool on)
{
}

void PolyTriangleThreadData::SetDepthMask(bool on)
{
	WriteDepth = on;
}

void PolyTriangleThreadData::SetDepthFunc(int func)
{
	if (func == DF_LEqual || func == DF_Less)
	{
		DepthTest = true;
	}
	else // if (func == DF_Always)
	{
		DepthTest = false;
	}
}

void PolyTriangleThreadData::SetDepthRange(float min, float max)
{
	DepthRangeStart = min;
	DepthRangeScale = max - min;
}

void PolyTriangleThreadData::SetDepthBias(float depthBiasConstantFactor, float depthBiasSlopeFactor)
{
	depthbias = (float)(depthBiasConstantFactor / 2500.0);
}

void PolyTriangleThreadData::SetColorMask(bool r, bool g, bool b, bool a)
{
	WriteColor = r;
}

void PolyTriangleThreadData::SetStencil(int stencilRef, int op)
{
	StencilTestValue = stencilRef;
	if (op == SOP_Increment)
	{
		StencilWriteValue = MIN(stencilRef + 1, (int)255);
	}
	else if (op == SOP_Decrement)
	{
		StencilWriteValue = MAX(stencilRef - 1, (int)0);
	}
	else // SOP_Keep
	{
		StencilWriteValue = stencilRef;
	}

	WriteStencil = StencilTest && (StencilTestValue != StencilWriteValue);
}

void PolyTriangleThreadData::SetCulling(int mode)
{
	SetTwoSided(mode == Cull_None);
	SetCullCCW(mode == Cull_CCW);
}

void PolyTriangleThreadData::EnableStencil(bool on)
{
	StencilTest = on;
	WriteStencil = on && (StencilTestValue != StencilWriteValue);
}

void PolyTriangleThreadData::SetRenderStyle(FRenderStyle style)
{
	RenderStyle = style;
}

void PolyTriangleThreadData::SetShader(int specialEffect, int effectState, bool alphaTest, bool colormapShader)
{
	SpecialEffect = specialEffect;
	EffectState = effectState;
	AlphaTest = alphaTest;
	ColormapShader = colormapShader;
}

void PolyTriangleThreadData::SetTexture(int unit, const void *pixels, int width, int height, bool bgra)
{
	textures[unit].pixels = pixels;
	textures[unit].width = width;
	textures[unit].height = height;
	textures[unit].bgra = bgra;
}

void PolyTriangleThreadData::DrawIndexed(int index, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	elements += index;

	ShadedTriVertex vertbuffer[3];
	ShadedTriVertex *vert[3] = { &vertbuffer[0], &vertbuffer[1], &vertbuffer[2] };
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				*vert[j] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, ccw);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		*vert[0] = ShadeVertex(*(elements++));
		*vert[1] = ShadeVertex(*(elements++));
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, ccw);
			std::swap(vert[1], vert[2]);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleStrip)
	{
		bool toggleccw = ccw;
		*vert[0] = ShadeVertex(*(elements++));
		*vert[1] = ShadeVertex(*(elements++));
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, toggleccw);
			ShadedTriVertex *vtmp = vert[0];
			vert[0] = vert[1];
			vert[1] = vert[2];
			vert[2] = vtmp;
			toggleccw = !toggleccw;
		}
	}
	else if (drawmode == PolyDrawMode::Lines)
	{
		for (int i = 0; i < vcount / 2; i++)
		{
			*vert[0] = ShadeVertex(*(elements++));
			*vert[1] = ShadeVertex(*(elements++));
			DrawShadedLine(vert);
		}
	}
	else if (drawmode == PolyDrawMode::Points)
	{
		for (int i = 0; i < vcount; i++)
		{
			*vert[0] = ShadeVertex(*(elements++));
			DrawShadedPoint(vert);
		}
	}
}

void PolyTriangleThreadData::Draw(int index, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	int vinput = index;

	ShadedTriVertex vertbuffer[3];
	ShadedTriVertex *vert[3] = { &vertbuffer[0], &vertbuffer[1], &vertbuffer[2] };
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				*vert[j] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, ccw);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		*vert[0] = ShadeVertex(vinput++);
		*vert[1] = ShadeVertex(vinput++);
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, ccw);
			std::swap(vert[1], vert[2]);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleStrip)
	{
		bool toggleccw = ccw;
		*vert[0] = ShadeVertex(vinput++);
		*vert[1] = ShadeVertex(vinput++);
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, toggleccw);
			ShadedTriVertex *vtmp = vert[0];
			vert[0] = vert[1];
			vert[1] = vert[2];
			vert[2] = vtmp;
			toggleccw = !toggleccw;
		}
	}
	else if (drawmode == PolyDrawMode::Lines)
	{
		for (int i = 0; i < vcount / 2; i++)
		{
			*vert[0] = ShadeVertex(vinput++);
			*vert[1] = ShadeVertex(vinput++);
			DrawShadedLine(vert);
		}
	}
	else if (drawmode == PolyDrawMode::Points)
	{
		for (int i = 0; i < vcount; i++)
		{
			*vert[0] = ShadeVertex(vinput++);
			DrawShadedPoint(vert);
		}
	}
}

ShadedTriVertex PolyTriangleThreadData::ShadeVertex(int index)
{
	inputAssembly->Load(this, vertices, frame0, frame1, index);
	mainVertexShader.SIMPLE = (SpecialEffect == EFF_BURN) || (SpecialEffect == EFF_STENCIL);
	mainVertexShader.SPHEREMAP = (SpecialEffect == EFF_SPHEREMAP);
	mainVertexShader.main();
	return mainVertexShader;
}

bool PolyTriangleThreadData::IsDegenerate(const ShadedTriVertex *const* vert)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = vert[1]->gl_Position.X - vert[0]->gl_Position.X;
	float ay = vert[1]->gl_Position.Y - vert[0]->gl_Position.Y;
	float az = vert[1]->gl_Position.W - vert[0]->gl_Position.W;
	float bx = vert[2]->gl_Position.X - vert[0]->gl_Position.X;
	float by = vert[2]->gl_Position.Y - vert[0]->gl_Position.Y;
	float bz = vert[2]->gl_Position.W - vert[0]->gl_Position.W;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-8f;
}

bool PolyTriangleThreadData::IsFrontfacing(TriDrawTriangleArgs *args)
{
	float a =
		args->v1->x * args->v2->y - args->v2->x * args->v1->y +
		args->v2->x * args->v3->y - args->v3->x * args->v2->y +
		args->v3->x * args->v1->y - args->v1->x * args->v3->y;
	return a <= 0.0f;
}

void PolyTriangleThreadData::DrawShadedPoint(const ShadedTriVertex *const* vertex)
{
}

void PolyTriangleThreadData::DrawShadedLine(const ShadedTriVertex *const* vert)
{
	static const int numclipdistances = 9;
	float clipdistance[numclipdistances * 2];
	float *clipd = clipdistance;
	for (int i = 0; i < 2; i++)
	{
		const auto &v = *vert[i];
		clipd[0] = v.gl_Position.X + v.gl_Position.W;
		clipd[1] = v.gl_Position.W - v.gl_Position.X;
		clipd[2] = v.gl_Position.Y + v.gl_Position.W;
		clipd[3] = v.gl_Position.W - v.gl_Position.Y;
		clipd[4] = v.gl_Position.Z + v.gl_Position.W;
		clipd[5] = v.gl_Position.W - v.gl_Position.Z;
		clipd[6] = v.gl_ClipDistance[0];
		clipd[7] = v.gl_ClipDistance[1];
		clipd[8] = v.gl_ClipDistance[2];
		clipd += numclipdistances;
	}

	float t1 = 0.0f;
	float t2 = 1.0f;
	for (int p = 0; p < numclipdistances; p++)
	{
		float clipdistance1 = clipdistance[0 * numclipdistances + p];
		float clipdistance2 = clipdistance[1 * numclipdistances + p];
		if (clipdistance1 < 0.0f) t1 = MAX(-clipdistance1 / (clipdistance2 - clipdistance1), t1);
		if (clipdistance2 < 0.0f) t2 = MIN(1.0f + clipdistance2 / (clipdistance1 - clipdistance2), t2);
		if (t1 >= t2)
			return;
	}

	float weights[] = { 1.0f - t1, t1, 1.0f - t2, t2 };

	ScreenTriVertex clippedvert[2];
	for (int i = 0; i < 2; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(ScreenTriVertex));
		for (int w = 0; w < 2; w++)
		{
			float weight = weights[i * 2 + w];
			v.x += vert[w]->gl_Position.X * weight;
			v.y += vert[w]->gl_Position.Y * weight;
			v.z += vert[w]->gl_Position.Z * weight;
			v.w += vert[w]->gl_Position.W * weight;
		}

		// Calculate normalized device coordinates:
		v.w = 1.0f / v.w;
		v.x *= v.w;
		v.y *= v.w;
		v.z *= v.w;

		// Apply viewport scale to get screen coordinates:
		v.x = viewport_x + viewport_width * (1.0f + v.x) * 0.5f;
		if (topdown)
			v.y = viewport_y + viewport_height * (1.0f - v.y) * 0.5f;
		else
			v.y = viewport_y + viewport_height * (1.0f + v.y) * 0.5f;
	}

	uint32_t vColorA = (int)(vert[0]->vColor.W * 255.0f + 0.5f);
	uint32_t vColorR = (int)(vert[0]->vColor.X * 255.0f + 0.5f);
	uint32_t vColorG = (int)(vert[0]->vColor.Y * 255.0f + 0.5f);
	uint32_t vColorB = (int)(vert[0]->vColor.Z * 255.0f + 0.5f);
	uint32_t color = MAKEARGB(vColorA, vColorR, vColorG, vColorB);

	// Slow and naive implementation. Hopefully fast enough..

	float x1 = clippedvert[0].x;
	float y1 = clippedvert[0].y;
	float x2 = clippedvert[1].x;
	float y2 = clippedvert[1].y;
	float dx = x2 - x1;
	float dy = y2 - y1;
	float step = (abs(dx) >= abs(dy)) ? abs(dx) : abs(dy);
	dx /= step;
	dy /= step;
	float x = x1;
	float y = y1;
	int istep = (int)step;
	int pixelsize = dest_bgra ? 4 : 1;
	for (int i = 0; i <= istep; i++)
	{
		int scrx = (int)x;
		int scry = (int)y;
		if (scrx >= clip.left && scrx < clip.right && scry >= clip.top && scry < clip.bottom && !line_skipped_by_thread(scry))
		{
			uint8_t *destpixel = dest + (scrx + scry * dest_width) * pixelsize;
			if (pixelsize == 4)
			{
				*reinterpret_cast<uint32_t*>(destpixel) = color;
			}
			else
			{
				*destpixel = color;
			}
		}
		x += dx;
		y += dy;
	}
}

void PolyTriangleThreadData::DrawShadedTriangle(const ShadedTriVertex *const* vert, bool ccw)
{
	// Reject triangle if degenerate
	if (IsDegenerate(vert))
		return;

	// Cull, clip and generate additional vertices as needed
	ScreenTriVertex clippedvert[max_additional_vertices];
	int numclipvert = ClipEdge(vert);

	// Convert barycentric weights to actual vertices
	for (int i = 0; i < numclipvert; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(ScreenTriVertex));
		for (int w = 0; w < 3; w++)
		{
			float weight = weights[i * 3 + w];
			v.x += vert[w]->gl_Position.X * weight;
			v.y += vert[w]->gl_Position.Y * weight;
			v.z += vert[w]->gl_Position.Z * weight;
			v.w += vert[w]->gl_Position.W * weight;
			v.u += vert[w]->vTexCoord.X * weight;
			v.v += vert[w]->vTexCoord.Y * weight;
			v.worldX += vert[w]->pixelpos.X * weight;
			v.worldY += vert[w]->pixelpos.Y * weight;
			v.worldZ += vert[w]->pixelpos.Z * weight;
			v.a += vert[w]->vColor.W * weight;
			v.r += vert[w]->vColor.X * weight;
			v.g += vert[w]->vColor.Y * weight;
			v.b += vert[w]->vColor.Z * weight;
			v.gradientdistZ += vert[w]->gradientdist.Z * weight;
		}
	}

#ifdef NO_SSE
	// Map to 2D viewport:
	for (int j = 0; j < numclipvert; j++)
	{
		auto &v = clippedvert[j];

		// Calculate normalized device coordinates:
		v.w = 1.0f / v.w;
		v.x *= v.w;
		v.y *= v.w;
		v.z *= v.w;

		// Apply viewport scale to get screen coordinates:
		v.x = viewport_x + viewport_width * (1.0f + v.x) * 0.5f;
		if (topdown)
			v.y = viewport_y + viewport_height * (1.0f - v.y) * 0.5f;
		else
			v.y = viewport_y + viewport_height * (1.0f + v.y) * 0.5f;
		}
#else
	// Map to 2D viewport:
	__m128 mviewport_x = _mm_set1_ps((float)viewport_x);
	__m128 mviewport_y = _mm_set1_ps((float)viewport_y);
	__m128 mviewport_halfwidth = _mm_set1_ps(viewport_width * 0.5f);
	__m128 mviewport_halfheight = _mm_set1_ps(viewport_height * 0.5f);
	__m128 mone = _mm_set1_ps(1.0f);
	int sse_length = (numclipvert + 3) / 4 * 4;
	for (int j = 0; j < sse_length; j += 4)
	{
		__m128 vx = _mm_loadu_ps(&clippedvert[j].x);
		__m128 vy = _mm_loadu_ps(&clippedvert[j + 1].x);
		__m128 vz = _mm_loadu_ps(&clippedvert[j + 2].x);
		__m128 vw = _mm_loadu_ps(&clippedvert[j + 3].x);
		_MM_TRANSPOSE4_PS(vx, vy, vz, vw);

		// Calculate normalized device coordinates:
		vw = _mm_div_ps(mone, vw);
		vx = _mm_mul_ps(vx, vw);
		vy = _mm_mul_ps(vy, vw);
		vz = _mm_mul_ps(vz, vw);

		// Apply viewport scale to get screen coordinates:
		vx = _mm_add_ps(mviewport_x, _mm_mul_ps(mviewport_halfwidth, _mm_add_ps(mone, vx)));
		if (topdown)
			vy = _mm_add_ps(mviewport_y, _mm_mul_ps(mviewport_halfheight, _mm_sub_ps(mone, vy)));
		else
			vy = _mm_add_ps(mviewport_y, _mm_mul_ps(mviewport_halfheight, _mm_add_ps(mone, vy)));

		_MM_TRANSPOSE4_PS(vx, vy, vz, vw);
		_mm_storeu_ps(&clippedvert[j].x, vx);
		_mm_storeu_ps(&clippedvert[j + 1].x, vy);
		_mm_storeu_ps(&clippedvert[j + 2].x, vz);
		_mm_storeu_ps(&clippedvert[j + 3].x, vw);
	}
#endif

	if (!topdown) ccw = !ccw;

	TriDrawTriangleArgs args;

	if (twosided && numclipvert > 2)
	{
		args.v1 = &clippedvert[0];
		args.v2 = &clippedvert[1];
		args.v3 = &clippedvert[2];
		ccw = !IsFrontfacing(&args);
	}

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert - 1; i > 1; i--)
		{
			args.v1 = &clippedvert[numclipvert - 1];
			args.v2 = &clippedvert[i - 1];
			args.v3 = &clippedvert[i - 2];
			if (IsFrontfacing(&args) == ccw && args.CalculateGradients())
			{
				ScreenTriangle::Draw(&args, this);
			}
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args.v1 = &clippedvert[0];
			args.v2 = &clippedvert[i - 1];
			args.v3 = &clippedvert[i];
			if (IsFrontfacing(&args) != ccw && args.CalculateGradients())
			{
				ScreenTriangle::Draw(&args, this);
			}
		}
	}
}

int PolyTriangleThreadData::ClipEdge(const ShadedTriVertex *const* verts)
{
	// use barycentric weights for clipped vertices
	weights = weightsbuffer;
	for (int i = 0; i < 3; i++)
	{
		weights[i * 3 + 0] = 0.0f;
		weights[i * 3 + 1] = 0.0f;
		weights[i * 3 + 2] = 0.0f;
		weights[i * 3 + i] = 1.0f;
	}

	// Clip and cull so that the following is true for all vertices:
	// -v.w <= v.x <= v.w
	// -v.w <= v.y <= v.w
	// -v.w <= v.z <= v.w
	
	// halfspace clip distances
	static const int numclipdistances = 9;
#ifdef NO_SSE
	float clipdistance[numclipdistances * 3];
	bool needsclipping = false;
	float *clipd = clipdistance;
	for (int i = 0; i < 3; i++)
	{
		const auto &v = *verts[i];
		clipd[0] = v.gl_Position.X + v.gl_Position.W;
		clipd[1] = v.gl_Position.W - v.gl_Position.X;
		clipd[2] = v.gl_Position.Y + v.gl_Position.W;
		clipd[3] = v.gl_Position.W - v.gl_Position.Y;
		clipd[4] = v.gl_Position.Z + v.gl_Position.W;
		clipd[5] = v.gl_Position.W - v.gl_Position.Z;
		clipd[6] = v.gl_ClipDistance[0];
		clipd[7] = v.gl_ClipDistance[1];
		clipd[8] = v.gl_ClipDistance[2];
		for (int j = 0; j < 9; j++)
			needsclipping = needsclipping || clipd[i];
		clipd += numclipdistances;
	}

	// If all halfspace clip distances are positive then the entire triangle is visible. Skip the expensive clipping step.
	if (!needsclipping)
	{
		return 3;
	}
#else
	__m128 mx = _mm_loadu_ps(&verts[0]->gl_Position.X);
	__m128 my = _mm_loadu_ps(&verts[1]->gl_Position.X);
	__m128 mz = _mm_loadu_ps(&verts[2]->gl_Position.X);
	__m128 mw = _mm_setzero_ps();
	_MM_TRANSPOSE4_PS(mx, my, mz, mw);
	__m128 clipd0 = _mm_add_ps(mx, mw);
	__m128 clipd1 = _mm_sub_ps(mw, mx);
	__m128 clipd2 = _mm_add_ps(my, mw);
	__m128 clipd3 = _mm_sub_ps(mw, my);
	__m128 clipd4 = _mm_add_ps(mz, mw);
	__m128 clipd5 = _mm_sub_ps(mw, mz);
	__m128 clipd6 = _mm_setr_ps(verts[0]->gl_ClipDistance[0], verts[1]->gl_ClipDistance[0], verts[2]->gl_ClipDistance[0], 0.0f);
	__m128 clipd7 = _mm_setr_ps(verts[0]->gl_ClipDistance[1], verts[1]->gl_ClipDistance[1], verts[2]->gl_ClipDistance[1], 0.0f);
	__m128 clipd8 = _mm_setr_ps(verts[0]->gl_ClipDistance[2], verts[1]->gl_ClipDistance[2], verts[2]->gl_ClipDistance[2], 0.0f);
	__m128 mneedsclipping = _mm_cmplt_ps(clipd0, _mm_setzero_ps());
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd1, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd2, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd3, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd4, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd5, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd6, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd7, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd8, _mm_setzero_ps()));
	if (_mm_movemask_ps(mneedsclipping) == 0)
	{
		return 3;
	}
	float clipdistance[numclipdistances * 4];
	_mm_storeu_ps(clipdistance, clipd0);
	_mm_storeu_ps(clipdistance + 4, clipd1);
	_mm_storeu_ps(clipdistance + 8, clipd2);
	_mm_storeu_ps(clipdistance + 12, clipd3);
	_mm_storeu_ps(clipdistance + 16, clipd4);
	_mm_storeu_ps(clipdistance + 20, clipd5);
	_mm_storeu_ps(clipdistance + 24, clipd6);
	_mm_storeu_ps(clipdistance + 28, clipd7);
	_mm_storeu_ps(clipdistance + 32, clipd8);
#endif

	// Clip against each halfspace
	float *input = weights;
	float *output = weights + max_additional_vertices * 3;
	int inputverts = 3;
	for (int p = 0; p < numclipdistances; p++)
	{
		// Clip each edge
		int outputverts = 0;
		for (int i = 0; i < inputverts; i++)
		{
			int j = (i + 1) % inputverts;
#ifdef NO_SSE
			float clipdistance1 =
				clipdistance[0 * numclipdistances + p] * input[i * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[i * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 * numclipdistances + p] * input[j * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[j * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[j * 3 + 2];
#else
			float clipdistance1 =
				clipdistance[0 + p * 4] * input[i * 3 + 0] +
				clipdistance[1 + p * 4] * input[i * 3 + 1] +
				clipdistance[2 + p * 4] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 + p * 4] * input[j * 3 + 0] +
				clipdistance[1 + p * 4] * input[j * 3 + 1] +
				clipdistance[2 + p * 4] * input[j * 3 + 2];
#endif

			// Clip halfspace
			if ((clipdistance1 >= 0.0f || clipdistance2 >= 0.0f) && outputverts + 1 < max_additional_vertices)
			{
				float t1 = (clipdistance1 < 0.0f) ? MAX(-clipdistance1 / (clipdistance2 - clipdistance1), 0.0f) : 0.0f;
				float t2 = (clipdistance2 < 0.0f) ? MIN(1.0f + clipdistance2 / (clipdistance1 - clipdistance2), 1.0f) : 1.0f;

				// add t1 vertex
				for (int k = 0; k < 3; k++)
					output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t1) + input[j * 3 + k] * t1;
				outputverts++;
			
				if (t2 != 1.0f && t2 > t1)
				{
					// add t2 vertex
					for (int k = 0; k < 3; k++)
						output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t2) + input[j * 3 + k] * t2;
					outputverts++;
				}
			}
		}
		std::swap(input, output);
		inputverts = outputverts;
		if (inputverts == 0)
			break;
	}

	weights = input;
	return inputverts;
}

PolyTriangleThreadData *PolyTriangleThreadData::Get(DrawerThread *thread)
{
	if (!thread->poly)
		thread->poly = std::make_shared<PolyTriangleThreadData>(thread->core, thread->num_cores, thread->numa_node, thread->num_numa_nodes, thread->numa_start_y, thread->numa_end_y);
	return thread->poly.get();
}
