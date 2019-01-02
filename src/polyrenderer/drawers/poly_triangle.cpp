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
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "r_data/models/models.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "polyrenderer/poly_renderer.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#include "x86.h"

static bool isBgraRenderTarget = false;

void PolyTriangleDrawer::ResizeBuffers(DCanvas *canvas)
{
	PolyStencilBuffer::Instance()->Resize(canvas->GetWidth(), canvas->GetHeight());
	PolyZBuffer::Instance()->Resize(canvas->GetPitch(), canvas->GetHeight());
}

bool PolyTriangleDrawer::IsBgra()
{
	return isBgraRenderTarget;
}

void PolyTriangleDrawer::ClearStencil(const DrawerCommandQueuePtr &queue, uint8_t value)
{
	queue->Push<PolyClearStencilCommand>(value);
}

void PolyTriangleDrawer::SetViewport(const DrawerCommandQueuePtr &queue, int x, int y, int width, int height, DCanvas *canvas)
{
	uint8_t *dest = (uint8_t*)canvas->GetPixels();
	int dest_width = canvas->GetWidth();
	int dest_height = canvas->GetHeight();
	int dest_pitch = canvas->GetPitch();
	bool dest_bgra = canvas->IsBgra();
	isBgraRenderTarget = dest_bgra;

	int offsetx = clamp(x, 0, dest_width);
	int pixelsize = dest_bgra ? 4 : 1;

	int viewport_x = x - offsetx;
	int viewport_y = y;
	int viewport_width = width;
	int viewport_height = height;

	dest += offsetx * pixelsize;
	dest_width = clamp(viewport_x + viewport_width, 0, dest_width - offsetx);
	dest_height = clamp(viewport_y + viewport_height, 0, dest_height);

	queue->Push<PolySetViewportCommand>(viewport_x, viewport_y, viewport_width, viewport_height, dest, dest_width, dest_height, dest_pitch, dest_bgra);
}

void PolyTriangleDrawer::SetTransform(const DrawerCommandQueuePtr &queue, const Mat4f *objectToClip, const Mat4f *objectToWorld)
{
	queue->Push<PolySetTransformCommand>(objectToClip, objectToWorld);
}

void PolyTriangleDrawer::SetCullCCW(const DrawerCommandQueuePtr &queue, bool ccw)
{
	queue->Push<PolySetCullCCWCommand>(ccw);
}

void PolyTriangleDrawer::SetTwoSided(const DrawerCommandQueuePtr &queue, bool twosided)
{
	queue->Push<PolySetTwoSidedCommand>(twosided);
}

void PolyTriangleDrawer::SetWeaponScene(const DrawerCommandQueuePtr &queue, bool enable)
{
	queue->Push<PolySetWeaponSceneCommand>(enable);
}

void PolyTriangleDrawer::SetModelVertexShader(const DrawerCommandQueuePtr &queue, int frame1, int frame2, float interpolationFactor)
{
	queue->Push<PolySetModelVertexShaderCommand>(frame1, frame2, interpolationFactor);
}

void PolyTriangleDrawer::DrawArray(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args, const void *vertices, int vcount, PolyDrawMode mode)
{
	queue->Push<DrawPolyTrianglesCommand>(args, vertices, nullptr, vcount, mode);
}

void PolyTriangleDrawer::DrawElements(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args, const void *vertices, const unsigned int *elements, int count, PolyDrawMode mode)
{
	queue->Push<DrawPolyTrianglesCommand>(args, vertices, elements, count, mode);
}

/////////////////////////////////////////////////////////////////////////////

void PolyTriangleThreadData::ClearStencil(uint8_t value)
{
	auto buffer = PolyStencilBuffer::Instance();
	int width = buffer->Width();
	int height = buffer->Height();
	uint8_t *data = buffer->Values();

	int skip = skipped_by_thread(0);
	int count = count_for_thread(0, height);

	data += skip * width;
	for (int i = 0; i < count; i++)
	{
		memset(data, value, width);
		data += num_cores * width;
	}
}

void PolyTriangleThreadData::SetViewport(int x, int y, int width, int height, uint8_t *new_dest, int new_dest_width, int new_dest_height, int new_dest_pitch, bool new_dest_bgra)
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
	ccw = true;
	weaponScene = false;
}

void PolyTriangleThreadData::SetTransform(const Mat4f *newObjectToClip, const Mat4f *newObjectToWorld)
{
	objectToClip = newObjectToClip;
	objectToWorld = newObjectToWorld;
}

void PolyTriangleThreadData::DrawElements(const PolyDrawArgs &drawargs, const void *vertices, const unsigned int *elements, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	TriDrawTriangleArgs args;
	args.uniforms = &drawargs;

	ShadedTriVertex vert[3];
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = ShadeVertex(drawargs, vertices, *(elements++));
			DrawShadedTriangle(vert, ccw, &args);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		vert[0] = ShadeVertex(drawargs, vertices, *(elements++));
		vert[1] = ShadeVertex(drawargs, vertices, *(elements++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = ShadeVertex(drawargs, vertices, *(elements++));
			DrawShadedTriangle(vert, ccw, &args);
			vert[1] = vert[2];
		}
	}
	else // TriangleDrawMode::TriangleStrip
	{
		bool toggleccw = ccw;
		vert[0] = ShadeVertex(drawargs, vertices, *(elements++));
		vert[1] = ShadeVertex(drawargs, vertices, *(elements++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = ShadeVertex(drawargs, vertices, *(elements++));
			DrawShadedTriangle(vert, toggleccw, &args);
			vert[0] = vert[1];
			vert[1] = vert[2];
			toggleccw = !toggleccw;
		}
	}
}

void PolyTriangleThreadData::DrawArray(const PolyDrawArgs &drawargs, const void *vertices, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	TriDrawTriangleArgs args;
	args.uniforms = &drawargs;

	int vinput = 0;

	ShadedTriVertex vert[3];
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = ShadeVertex(drawargs, vertices, vinput++);
			DrawShadedTriangle(vert, ccw, &args);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		vert[0] = ShadeVertex(drawargs, vertices, vinput++);
		vert[1] = ShadeVertex(drawargs, vertices, vinput++);
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = ShadeVertex(drawargs, vertices, vinput++);
			DrawShadedTriangle(vert, ccw, &args);
			vert[1] = vert[2];
		}
	}
	else // TriangleDrawMode::TriangleStrip
	{
		bool toggleccw = ccw;
		vert[0] = ShadeVertex(drawargs, vertices, vinput++);
		vert[1] = ShadeVertex(drawargs, vertices, vinput++);
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = ShadeVertex(drawargs, vertices, vinput++);
			DrawShadedTriangle(vert, toggleccw, &args);
			vert[0] = vert[1];
			vert[1] = vert[2];
			toggleccw = !toggleccw;
		}
	}
}

ShadedTriVertex PolyTriangleThreadData::ShadeVertex(const PolyDrawArgs &drawargs, const void *vertices, int index)
{
	ShadedTriVertex sv;
	Vec4f objpos;

	if (modelFrame1 == -1)
	{
		const TriVertex &v = static_cast<const TriVertex*>(vertices)[index];
		objpos = Vec4f(v.x, v.y, v.z, v.w);
		sv.u = v.u;
		sv.v = v.v;
	}
	else if (modelFrame1 == modelFrame2 || modelInterpolationFactor == 0.f)
	{
		const FModelVertex &v = static_cast<const FModelVertex*>(vertices)[modelFrame1 + index];
		objpos = Vec4f(v.x, v.y, v.z, 1.0f);
		sv.u = v.u;
		sv.v = v.v;
	}
	else
	{
		const FModelVertex &v1 = static_cast<const FModelVertex*>(vertices)[modelFrame1 + index];
		const FModelVertex &v2 = static_cast<const FModelVertex*>(vertices)[modelFrame2 + index];

		float frac = modelInterpolationFactor;
		float inv_frac = 1.0f - frac;

		objpos = Vec4f(v1.x * inv_frac + v2.x * frac, v1.y * inv_frac + v2.y * frac, v1.z * inv_frac + v2.z * frac, 1.0f);
		sv.u = v1.u;
		sv.v = v1.v;
	}

	// Apply transform to get clip coordinates:
	Vec4f clippos = (*objectToClip) * objpos;

	sv.x = clippos.X;
	sv.y = clippos.Y;
	sv.z = clippos.Z;
	sv.w = clippos.W;

	if (!objectToWorld) // Identity matrix
	{
		sv.worldX = objpos.X;
		sv.worldY = objpos.Y;
		sv.worldZ = objpos.Z;
	}
	else
	{
		Vec4f worldpos = (*objectToWorld) * objpos;
		sv.worldX = worldpos.X;
		sv.worldY = worldpos.Y;
		sv.worldZ = worldpos.Z;
	}

	// Calculate gl_ClipDistance[i]
	for (int i = 0; i < 3; i++)
	{
		const auto &clipPlane = drawargs.ClipPlane(i);
		sv.clipDistance[i] = objpos.X * clipPlane.A + objpos.Y * clipPlane.B + objpos.Z * clipPlane.C + objpos.W * clipPlane.D;
	}

	return sv;
}

bool PolyTriangleThreadData::IsDegenerate(const ShadedTriVertex *vert)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = vert[1].x - vert[0].x;
	float ay = vert[1].y - vert[0].y;
	float az = vert[1].w - vert[0].w;
	float bx = vert[2].x - vert[0].x;
	float by = vert[2].y - vert[0].y;
	float bz = vert[2].w - vert[0].w;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

bool PolyTriangleThreadData::IsFrontfacing(TriDrawTriangleArgs *args)
{
	float a =
		args->v1->x * args->v2->y - args->v2->x * args->v1->y +
		args->v2->x * args->v3->y - args->v3->x * args->v2->y +
		args->v3->x * args->v1->y - args->v1->x * args->v3->y;
	return a <= 0.0f;
}

void PolyTriangleThreadData::DrawShadedTriangle(const ShadedTriVertex *vert, bool ccw, TriDrawTriangleArgs *args)
{
	// Reject triangle if degenerate
	if (IsDegenerate(vert))
		return;

	// Cull, clip and generate additional vertices as needed
	ShadedTriVertex clippedvert[max_additional_vertices];
	int numclipvert = ClipEdge(vert, clippedvert);

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
		v.y = viewport_y + viewport_height * (1.0f - v.y) * 0.5f;
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
		vy = _mm_add_ps(mviewport_y, _mm_mul_ps(mviewport_halfheight, _mm_sub_ps(mone, vy)));

		_MM_TRANSPOSE4_PS(vx, vy, vz, vw);
		_mm_storeu_ps(&clippedvert[j].x, vx);
		_mm_storeu_ps(&clippedvert[j + 1].x, vy);
		_mm_storeu_ps(&clippedvert[j + 2].x, vz);
		_mm_storeu_ps(&clippedvert[j + 3].x, vw);
	}
#endif

	// Keep varyings in -128 to 128 range if possible
	// But don't do this for the skycap mode since the V texture coordinate is used for blending
	if (numclipvert > 0 && args->uniforms->BlendMode() != TriBlendMode::Skycap)
	{
		float newOriginU = floorf(clippedvert[0].u * 0.1f) * 10.0f;
		float newOriginV = floorf(clippedvert[0].v * 0.1f) * 10.0f;
		for (int i = 0; i < numclipvert; i++)
		{
			clippedvert[i].u -= newOriginU;
			clippedvert[i].v -= newOriginV;
		}
	}

	if (twosided && numclipvert > 2)
	{
		args->v1 = &clippedvert[0];
		args->v2 = &clippedvert[1];
		args->v3 = &clippedvert[2];
		ccw = !IsFrontfacing(args);
	}

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert - 1; i > 1; i--)
		{
			args->v1 = &clippedvert[numclipvert - 1];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i - 2];
			if (IsFrontfacing(args) == ccw && args->CalculateGradients())
			{
				ScreenTriangle::Draw(args, this);
			}
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args->v1 = &clippedvert[0];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i];
			if (IsFrontfacing(args) != ccw && args->CalculateGradients())
			{
				ScreenTriangle::Draw(args, this);
			}
		}
	}
}

int PolyTriangleThreadData::ClipEdge(const ShadedTriVertex *verts, ShadedTriVertex *clippedvert)
{
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
		const auto &v = verts[i];
		clipd[0] = v.x + v.w;
		clipd[1] = v.w - v.x;
		clipd[2] = v.y + v.w;
		clipd[3] = v.w - v.y;
		clipd[4] = v.z + v.w;
		clipd[5] = v.w - v.z;
		clipd[6] = v.clipDistance[0];
		clipd[7] = v.clipDistance[1];
		clipd[8] = v.clipDistance[2];
		for (int j = 0; j < 9; j++)
			needsclipping = needsclipping || clipd[i];
		clipd += numclipdistances;
	}

	// If all halfspace clip distances are positive then the entire triangle is visible. Skip the expensive clipping step.
	if (!needsclipping)
	{
		for (int i = 0; i < 3; i++)
		{
			memcpy(clippedvert + i, &verts[i], sizeof(ShadedTriVertex));
		}
		return 3;
	}
#else
	__m128 mx = _mm_loadu_ps(&verts[0].x);
	__m128 my = _mm_loadu_ps(&verts[1].x);
	__m128 mz = _mm_loadu_ps(&verts[2].x);
	__m128 mw = _mm_setzero_ps();
	_MM_TRANSPOSE4_PS(mx, my, mz, mw);
	__m128 clipd0 = _mm_add_ps(mx, mw);
	__m128 clipd1 = _mm_sub_ps(mw, mx);
	__m128 clipd2 = _mm_add_ps(my, mw);
	__m128 clipd3 = _mm_sub_ps(mw, my);
	__m128 clipd4 = _mm_add_ps(mz, mw);
	__m128 clipd5 = _mm_sub_ps(mw, mz);
	__m128 clipd6 = _mm_setr_ps(verts[0].clipDistance[0], verts[1].clipDistance[0], verts[2].clipDistance[0], 0.0f);
	__m128 clipd7 = _mm_setr_ps(verts[0].clipDistance[1], verts[1].clipDistance[1], verts[2].clipDistance[1], 0.0f);
	__m128 clipd8 = _mm_setr_ps(verts[0].clipDistance[2], verts[1].clipDistance[2], verts[2].clipDistance[2], 0.0f);
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
		for (int i = 0; i < 3; i++)
		{
			memcpy(clippedvert + i, &verts[i], sizeof(ShadedTriVertex));
		}
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

	// use barycentric weights while clipping vertices
	float weights[max_additional_vertices * 3 * 2];
	for (int i = 0; i < 3; i++)
	{
		weights[i * 3 + 0] = 0.0f;
		weights[i * 3 + 1] = 0.0f;
		weights[i * 3 + 2] = 0.0f;
		weights[i * 3 + i] = 1.0f;
	}

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
	
	// Convert barycentric weights to actual vertices
	for (int i = 0; i < inputverts; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(ShadedTriVertex));
		for (int w = 0; w < 3; w++)
		{
			float weight = input[i * 3 + w];
			v.x += verts[w].x * weight;
			v.y += verts[w].y * weight;
			v.z += verts[w].z * weight;
			v.w += verts[w].w * weight;
			v.u += verts[w].u * weight;
			v.v += verts[w].v * weight;
			v.worldX += verts[w].worldX * weight;
			v.worldY += verts[w].worldY * weight;
			v.worldZ += verts[w].worldZ * weight;
		}
	}
	return inputverts;
}

PolyTriangleThreadData *PolyTriangleThreadData::Get(DrawerThread *thread)
{
	if (!thread->poly)
		thread->poly = std::make_shared<PolyTriangleThreadData>(thread->core, thread->num_cores, thread->numa_node, thread->num_numa_nodes, thread->numa_start_y, thread->numa_end_y);
	return thread->poly.get();
}

/////////////////////////////////////////////////////////////////////////////

PolySetTransformCommand::PolySetTransformCommand(const Mat4f *objectToClip, const Mat4f *objectToWorld) : objectToClip(objectToClip), objectToWorld(objectToWorld)
{
}

void PolySetTransformCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetTransform(objectToClip, objectToWorld);
}

/////////////////////////////////////////////////////////////////////////////

PolySetCullCCWCommand::PolySetCullCCWCommand(bool ccw) : ccw(ccw)
{
}

void PolySetCullCCWCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetCullCCW(ccw);
}

/////////////////////////////////////////////////////////////////////////////

PolySetTwoSidedCommand::PolySetTwoSidedCommand(bool twosided) : twosided(twosided)
{
}

void PolySetTwoSidedCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetTwoSided(twosided);
}

/////////////////////////////////////////////////////////////////////////////

PolySetWeaponSceneCommand::PolySetWeaponSceneCommand(bool value) : value(value)
{
}

void PolySetWeaponSceneCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetWeaponScene(value);
}

/////////////////////////////////////////////////////////////////////////////

PolySetModelVertexShaderCommand::PolySetModelVertexShaderCommand(int frame1, int frame2, float interpolationFactor) : frame1(frame1), frame2(frame2), interpolationFactor(interpolationFactor)
{
}

void PolySetModelVertexShaderCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetModelVertexShader(frame1, frame2, interpolationFactor);
}

/////////////////////////////////////////////////////////////////////////////

PolyClearStencilCommand::PolyClearStencilCommand(uint8_t value) : value(value)
{
}

void PolyClearStencilCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->ClearStencil(value);
}

/////////////////////////////////////////////////////////////////////////////

PolySetViewportCommand::PolySetViewportCommand(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra)
	: x(x), y(y), width(width), height(height), dest(dest), dest_width(dest_width), dest_height(dest_height), dest_pitch(dest_pitch), dest_bgra(dest_bgra)
{
}

void PolySetViewportCommand::Execute(DrawerThread *thread)
{
	PolyTriangleThreadData::Get(thread)->SetViewport(x, y, width, height, dest, dest_width, dest_height, dest_pitch, dest_bgra);
}

/////////////////////////////////////////////////////////////////////////////

DrawPolyTrianglesCommand::DrawPolyTrianglesCommand(const PolyDrawArgs &args, const void *vertices, const unsigned int *elements, int count, PolyDrawMode mode) : args(args), vertices(vertices), elements(elements), count(count), mode(mode)
{
}

void DrawPolyTrianglesCommand::Execute(DrawerThread *thread)
{
	if (!elements)
		PolyTriangleThreadData::Get(thread)->DrawArray(args, vertices, count, mode);
	else
		PolyTriangleThreadData::Get(thread)->DrawElements(args, vertices, elements, count, mode);
}

/////////////////////////////////////////////////////////////////////////////

void DrawRectCommand::Execute(DrawerThread *thread)
{
	auto renderTarget = PolyRenderer::Instance()->RenderTarget;
	const void *destOrg = renderTarget->GetPixels();
	int destWidth = renderTarget->GetWidth();
	int destHeight = renderTarget->GetHeight();
	int destPitch = renderTarget->GetPitch();
	int blendmode = (int)args.BlendMode();
	if (renderTarget->IsBgra())
		ScreenTriangle::RectDrawers32[blendmode](destOrg, destWidth, destHeight, destPitch, &args, PolyTriangleThreadData::Get(thread));
	else
		ScreenTriangle::RectDrawers8[blendmode](destOrg, destWidth, destHeight, destPitch, &args, PolyTriangleThreadData::Get(thread));
}
