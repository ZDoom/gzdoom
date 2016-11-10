/*
**  Triangle drawers
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
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_poly_triangle.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

void PolyTriangleDrawer::draw(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, FTexture *texture, int stenciltestvalue)
{
	if (r_swtruecolor)
		queue_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, (const uint8_t*)texture->GetPixelsBgra(), texture->GetWidth(), texture->GetHeight(), 0, stenciltestvalue, stenciltestvalue);
	else
		draw_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, texture->GetPixels(), texture->GetWidth(), texture->GetHeight(), 0, stenciltestvalue, stenciltestvalue, nullptr, &ScreenPolyTriangleDrawer::draw);
}

void PolyTriangleDrawer::fill(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, int solidcolor, int stenciltestvalue)
{
	if (r_swtruecolor)
		queue_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, nullptr, 0, 0, solidcolor, stenciltestvalue, stenciltestvalue);
	else
		draw_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, nullptr, 0, 0, solidcolor, stenciltestvalue, stenciltestvalue, nullptr, &ScreenPolyTriangleDrawer::fill);
}

void PolyTriangleDrawer::stencil(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, int stenciltestvalue, int stencilwritevalue)
{
	if (r_swtruecolor)
		queue_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, nullptr, 0, 0, 0xbeef, stenciltestvalue, stencilwritevalue);
	else
		draw_arrays(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, nullptr, 0, 0, 0, stenciltestvalue, stencilwritevalue, nullptr, &ScreenPolyTriangleDrawer::stencil);
}

void PolyTriangleDrawer::queue_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue)
{
	if (clipright < clipleft || clipleft < 0 || clipright > MAXWIDTH || clipbottom < cliptop || cliptop < 0 || clipbottom > MAXHEIGHT)
		return;

	DrawerCommandQueue::QueueCommand<DrawPolyTrianglesCommand>(uniforms, vinput, vcount, mode, ccw, clipleft, clipright, cliptop, clipbottom, texturePixels, textureWidth, textureHeight, solidcolor, stenciltestvalue, stencilwritevalue);
}

void PolyTriangleDrawer::draw_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue, DrawerThread *thread, void(*drawfunc)(const ScreenPolyTriangleDrawerArgs *, DrawerThread *))
{
	if (vcount < 3)
		return;

	ScreenPolyTriangleDrawerArgs args;
	args.dest = dc_destorg;
	args.pitch = dc_pitch;
	args.clipleft = clipleft;
	args.clipright = clipright;
	args.cliptop = cliptop;
	args.clipbottom = clipbottom;
	args.texturePixels = texturePixels;
	args.textureWidth = textureWidth;
	args.textureHeight = textureHeight;
	args.solidcolor = solidcolor;
	args.uniforms = &uniforms;
	args.stencilTestValue = stenciltestvalue;
	args.stencilWriteValue = stencilwritevalue;
	args.stencilPitch = PolyStencilBuffer::Instance()->BlockWidth();
	args.stencilValues = PolyStencilBuffer::Instance()->Values();
	args.stencilMasks = PolyStencilBuffer::Instance()->Masks();

	TriVertex vert[3];
	if (mode == TriangleDrawMode::Normal)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = shade_vertex(uniforms, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
		}
	}
	else if (mode == TriangleDrawMode::Fan)
	{
		vert[0] = shade_vertex(uniforms, *(vinput++));
		vert[1] = shade_vertex(uniforms, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(uniforms, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
			vert[1] = vert[2];
		}
	}
	else // TriangleDrawMode::Strip
	{
		vert[0] = shade_vertex(uniforms, *(vinput++));
		vert[1] = shade_vertex(uniforms, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(uniforms, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
			vert[0] = vert[1];
			vert[1] = vert[2];
			ccw = !ccw;
		}
	}
}

TriVertex PolyTriangleDrawer::shade_vertex(const TriUniforms &uniforms, TriVertex v)
{
	// Apply transform to get clip coordinates:
	return uniforms.objectToClip * v;
}

void PolyTriangleDrawer::draw_shaded_triangle(const TriVertex *vert, bool ccw, ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread, void(*drawfunc)(const ScreenPolyTriangleDrawerArgs *, DrawerThread *))
{
	// Cull, clip and generate additional vertices as needed
	TriVertex clippedvert[max_additional_vertices];
	int numclipvert;
	clipedge(vert, clippedvert, numclipvert);

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
		v.x = viewwidth * (1.0f + v.x) * 0.5f;
		v.y = viewheight * (1.0f - v.y) * 0.5f;
	}

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert; i > 1; i--)
		{
			args->v1 = &clippedvert[numclipvert - 1];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i - 2];
			drawfunc(args, thread);
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args->v1 = &clippedvert[0];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i];
			drawfunc(args, thread);
		}
	}
}

bool PolyTriangleDrawer::cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2)
{
	if (clipdistance1 < 0.0f && clipdistance2 < 0.0f)
		return true;

	if (clipdistance1 < 0.0f)
		t1 = MAX(-clipdistance1 / (clipdistance2 - clipdistance1), 0.0f);
	else
		t1 = 0.0f;

	if (clipdistance2 < 0.0f)
		t2 = MIN(1.0f + clipdistance2 / (clipdistance1 - clipdistance2), 1.0f);
	else
		t2 = 1.0f;

	return false;
}

void PolyTriangleDrawer::clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert)
{
	// Clip and cull so that the following is true for all vertices:
	// -v.w <= v.x <= v.w
	// -v.w <= v.y <= v.w
	// -v.w <= v.z <= v.w
	
	// use barycentric weights while clipping vertices
	float weights[max_additional_vertices * 3 * 2];
	for (int i = 0; i < 3; i++)
	{
		weights[i * 3 + 0] = 0.0f;
		weights[i * 3 + 1] = 0.0f;
		weights[i * 3 + 2] = 0.0f;
		weights[i * 3 + i] = 1.0f;
	}
	
	// halfspace clip distances
	float clipdistance[6 * 3];
	for (int i = 0; i < 3; i++)
	{
		const auto &v = verts[i];
		clipdistance[i * 6 + 0] = v.x + v.w;
		clipdistance[i * 6 + 1] = v.w - v.x;
		clipdistance[i * 6 + 2] = v.y + v.w;
		clipdistance[i * 6 + 3] = v.w - v.y;
		clipdistance[i * 6 + 4] = v.z + v.w;
		clipdistance[i * 6 + 5] = v.w - v.z;
	}
	
	// Clip against each halfspace
	float *input = weights;
	float *output = weights + max_additional_vertices * 3;
	int inputverts = 3;
	int outputverts = 0;
	for (int p = 0; p < 6; p++)
	{
		// Clip each edge
		outputverts = 0;
		for (int i = 0; i < inputverts; i++)
		{
			int j = (i + 1) % inputverts;
			float clipdistance1 =
				clipdistance[0 * 6 + p] * input[i * 3 + 0] +
				clipdistance[1 * 6 + p] * input[i * 3 + 1] +
				clipdistance[2 * 6 + p] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 * 6 + p] * input[j * 3 + 0] +
				clipdistance[1 * 6 + p] * input[j * 3 + 1] +
				clipdistance[2 * 6 + p] * input[j * 3 + 2];
				
			float t1, t2;
			if (!cullhalfspace(clipdistance1, clipdistance2, t1, t2) && outputverts + 1 < max_additional_vertices)
			{
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
		std::swap(inputverts, outputverts);
		if (inputverts == 0)
			break;
	}
	
	// Convert barycentric weights to actual vertices
	numclipvert = inputverts;
	for (int i = 0; i < numclipvert; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(TriVertex));
		for (int w = 0; w < 3; w++)
		{
			float weight = input[i * 3 + w];
			v.x += verts[w].x * weight;
			v.y += verts[w].y * weight;
			v.z += verts[w].z * weight;
			v.w += verts[w].w * weight;
			for (int iv = 0; iv < TriVertex::NumVarying; iv++)
				v.varying[iv] += verts[w].varying[iv] * weight;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ScreenPolyTriangleDrawer::draw(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread)
{
	uint8_t *dest = args->dest;
	int pitch = args->pitch;
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipleft = args->clipleft;
	int clipright = args->clipright;
	int cliptop = args->cliptop;
	int clipbottom = args->clipbottom;
	const uint8_t *texturePixels = args->texturePixels;
	int textureWidth = args->textureWidth;
	int textureHeight = args->textureHeight;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, clipleft);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, cliptop);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	dest += miny * pitch;

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Gradients
	float gradWX = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	float gradWY = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	float startW = v1.w + gradWX * (minx - v1.x) + gradWY * (miny - v1.y);
	float gradVaryingX[TriVertex::NumVarying], gradVaryingY[TriVertex::NumVarying], startVarying[TriVertex::NumVarying];
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradVaryingX[i] = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradVaryingY[i] = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		startVarying[i] = v1.varying[i] * v1.w + gradVaryingX[i] * (minx - v1.x) + gradVaryingY[i] * (miny - v1.y);
	}

	// Loop through blocks
	for (int y = miny; y < maxy; y += q, dest += q * pitch)
	{
		// Is this row of blocks done by this thread?
		if (thread && thread->skipped_by_thread(y / q)) continue;

		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0) continue;

			// Check if block needs clipping
			bool clipneeded = clipleft > x || clipright < (x + q) || cliptop > y || clipbottom < (y + q);

			// Calculate varying variables for affine block
			float offx0 = (x - minx) + 0.5f;
			float offy0 = (y - miny) + 0.5f;
			float offx1 = offx0 + q;
			float offy1 = offy0 + q;
			float rcpWTL = 1.0f / (startW + offx0 * gradWX + offy0 * gradWY);
			float rcpWTR = 1.0f / (startW + offx1 * gradWX + offy0 * gradWY);
			float rcpWBL = 1.0f / (startW + offx0 * gradWX + offy1 * gradWY);
			float rcpWBR = 1.0f / (startW + offx1 * gradWX + offy1 * gradWY);
			float varyingTL[TriVertex::NumVarying];
			float varyingTR[TriVertex::NumVarying];
			float varyingBL[TriVertex::NumVarying];
			float varyingBR[TriVertex::NumVarying];
			for (int i = 0; i < TriVertex::NumVarying; i++)
			{
				varyingTL[i] = (startVarying[i] + offx0 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTL;
				varyingTR[i] = (startVarying[i] + offx1 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTR;
				varyingBL[i] = ((startVarying[i] + offx0 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBL - varyingTL[i]) * (1.0f / q);
				varyingBR[i] = ((startVarying[i] + offx1 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBR - varyingTR[i]) * (1.0f / q);
			}

			uint8_t *buffer = dest;

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF && !clipneeded)
			{
				for (int iy = 0; iy < q; iy++)
				{
					uint32_t varying[TriVertex::NumVarying], varyingStep[TriVertex::NumVarying];
					for (int i = 0; i < TriVertex::NumVarying; i++)
					{
						float pos = varyingTL[i] + varyingBL[i] * iy;
						float step = (varyingTR[i] + varyingBR[i] * iy - pos) * (1.0f / q);

						varying[i] = (uint32_t)((pos - floor(pos)) * 0x100000000LL);
						varyingStep[i] = (uint32_t)(step * 0x100000000LL);
					}

					for (int ix = x; ix < x + q; ix++)
					{
						uint32_t ufrac = varying[0];
						uint32_t vfrac = varying[1];

						uint32_t upos = ((ufrac >> 16) * textureWidth) >> 16;
						uint32_t vpos = ((vfrac >> 16) * textureHeight) >> 16;
						uint32_t uvoffset = upos * textureHeight + vpos;

						if (texturePixels[uvoffset] != 0)
							buffer[ix] = texturePixels[uvoffset];

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] += varyingStep[i];
					}

					buffer += pitch;
				}
			}
			else // Partially covered block
			{
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (int iy = 0; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					uint32_t varying[TriVertex::NumVarying], varyingStep[TriVertex::NumVarying];
					for (int i = 0; i < TriVertex::NumVarying; i++)
					{
						float pos = varyingTL[i] + varyingBL[i] * iy;
						float step = (varyingTR[i] + varyingBR[i] * iy - pos) * (1.0f / q);

						varying[i] = (uint32_t)((pos - floor(pos)) * 0x100000000LL);
						varyingStep[i] = (uint32_t)(step * 0x100000000LL);
					}

					for (int ix = x; ix < x + q; ix++)
					{
						bool visible = ix >= clipleft && ix < clipright && (cliptop <= y + iy) && (clipbottom > y + iy);

						if (CX1 > 0 && CX2 > 0 && CX3 > 0 && visible)
						{
							uint32_t ufrac = varying[0];
							uint32_t vfrac = varying[1];

							uint32_t upos = ((ufrac >> 16) * textureWidth) >> 16;
							uint32_t vpos = ((vfrac >> 16) * textureHeight) >> 16;
							uint32_t uvoffset = upos * textureHeight + vpos;

							if (texturePixels[uvoffset] != 0)
								buffer[ix] = texturePixels[uvoffset];
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] += varyingStep[i];

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;

					buffer += pitch;
				}
			}
		}
	}
}

void ScreenPolyTriangleDrawer::fill(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread)
{
	uint8_t *dest = args->dest;
	int pitch = args->pitch;
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipleft = args->clipleft;
	int clipright = args->clipright;
	int cliptop = args->cliptop;
	int clipbottom = args->clipbottom;
	int solidcolor = args->solidcolor;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, clipleft);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, cliptop);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	dest += miny * pitch;

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Loop through blocks
	for (int y = miny; y < maxy; y += q, dest += q * pitch)
	{
		// Is this row of blocks done by this thread?
		if (thread && thread->skipped_by_thread(y / q)) continue;

		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0) continue;

			// Check if block needs clipping
			bool clipneeded = clipleft > x || clipright < (x + q) || cliptop > y || clipbottom < (y + q);

			uint8_t *buffer = dest;

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF && !clipneeded)
			{
				for (int iy = 0; iy < q; iy++)
				{
					for (int ix = x; ix < x + q; ix++)
					{
						buffer[ix] = solidcolor;
					}

					buffer += pitch;
				}
			}
			else // Partially covered block
			{
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (int iy = 0; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = x; ix < x + q; ix++)
					{
						bool visible = ix >= clipleft && ix < clipright && (cliptop <= y + iy) && (clipbottom > y + iy);

						if (CX1 > 0 && CX2 > 0 && CX3 > 0 && visible)
						{
							buffer[ix] = solidcolor;
						}

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;

					buffer += pitch;
				}
			}
		}
	}
}

void ScreenPolyTriangleDrawer::stencil(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread)
{
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipleft = args->clipleft;
	int clipright = args->clipright;
	int cliptop = args->cliptop;
	int clipbottom = args->clipbottom;
	int solidcolor = args->solidcolor;
	uint8_t *stencilValues = args->stencilValues;
	uint32_t *stencilMasks = args->stencilMasks;
	int stencilPitch = args->stencilPitch;
	uint8_t stencilTestValue = args->stencilTestValue;
	uint8_t stencilWriteValue = args->stencilWriteValue;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, clipleft);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, cliptop);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Loop through blocks
	for (int y = miny; y < maxy; y += q)
	{
		// Is this row of blocks done by this thread?
		if (thread && thread->skipped_by_thread(y / q)) continue;

		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0) continue;

			// Check if block needs clipping
			bool clipneeded = clipleft > x || clipright < (x + q) || cliptop > y || clipbottom < (y + q);

			PolyStencilBlock stencil(x / 8 + y / 8 * stencilPitch, stencilValues, stencilMasks);

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF && !clipneeded && stencil.IsSingleValue())
			{
				// Reject whole block if the stencil test fails
				if (stencil.Get(0, 0) != stencilTestValue)
					continue;
				stencil.Clear(stencilWriteValue);
			}
			else // Partially covered block
			{
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (int iy = 0; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool visible = (ix + x >= clipleft) && (ix + x < clipright) && (cliptop <= y + iy) && (clipbottom > y + iy);

						if (CX1 > 0 && CX2 > 0 && CX3 > 0 && visible && stencil.Get(ix, iy) == stencilTestValue)
						{
							stencil.Set(ix, iy, stencilWriteValue);
						}

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}
			}
		}
	}
}

void ScreenPolyTriangleDrawer::draw32(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread)
{
	uint32_t *dest = (uint32_t *)args->dest;
	int pitch = args->pitch;
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipleft = args->clipleft;
	int clipright = args->clipright;
	int cliptop = args->cliptop;
	int clipbottom = args->clipbottom;
	const uint32_t *texturePixels = (const uint32_t *)args->texturePixels;
	int textureWidth = args->textureWidth;
	int textureHeight = args->textureHeight;
	uint32_t light = args->uniforms->light;
	uint8_t *stencilValues = args->stencilValues;
	uint32_t *stencilMasks = args->stencilMasks;
	int stencilPitch = args->stencilPitch;
	uint8_t stencilTestValue = args->stencilTestValue;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, clipleft);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, cliptop);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	dest += miny * pitch;

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Gradients
	float gradWX = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	float gradWY = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	float startW = v1.w + gradWX * (minx - v1.x) + gradWY * (miny - v1.y);
	float gradVaryingX[TriVertex::NumVarying], gradVaryingY[TriVertex::NumVarying], startVarying[TriVertex::NumVarying];
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradVaryingX[i] = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradVaryingY[i] = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		startVarying[i] = v1.varying[i] * v1.w + gradVaryingX[i] * (minx - v1.x) + gradVaryingY[i] * (miny - v1.y);
	}

	// Loop through blocks
	for (int y = miny; y < maxy; y += q, dest += q * pitch)
	{
		// Is this row of blocks done by this thread?
		if (thread->skipped_by_thread(y / q)) continue;

		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0) continue;

			// Check if block needs clipping
			bool clipneeded = clipleft > x || clipright < (x + q) || cliptop > y || clipbottom < (y + q);

			// Calculate varying variables for affine block
			float offx0 = (x - minx) + 0.5f;
			float offy0 = (y - miny) + 0.5f;
			float offx1 = offx0 + q;
			float offy1 = offy0 + q;
			float rcpWTL = 1.0f / (startW + offx0 * gradWX + offy0 * gradWY);
			float rcpWTR = 1.0f / (startW + offx1 * gradWX + offy0 * gradWY);
			float rcpWBL = 1.0f / (startW + offx0 * gradWX + offy1 * gradWY);
			float rcpWBR = 1.0f / (startW + offx1 * gradWX + offy1 * gradWY);
			float varyingTL[TriVertex::NumVarying];
			float varyingTR[TriVertex::NumVarying];
			float varyingBL[TriVertex::NumVarying];
			float varyingBR[TriVertex::NumVarying];
			for (int i = 0; i < TriVertex::NumVarying; i++)
			{
				varyingTL[i] = (startVarying[i] + offx0 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTL;
				varyingTR[i] = (startVarying[i] + offx1 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTR;
				varyingBL[i] = ((startVarying[i] + offx0 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBL - varyingTL[i]) * (1.0f / q);
				varyingBR[i] = ((startVarying[i] + offx1 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBR - varyingTR[i]) * (1.0f / q);
			}

			float globVis = 1706.0f;
			float vis = globVis / rcpWTL;
			float shade = 64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f;
			float lightscale = clamp((shade - MIN(24.0f, vis)) / 32.0f, 0.0f, 31.0f / 32.0f);
			int diminishedlight = (int)clamp((1.0f - lightscale) * 256.0f + 0.5f, 0.0f, 256.0f);

#if !defined(NO_SSE)
			__m128i mlight = _mm_set1_epi16(diminishedlight);
#endif

			uint32_t *buffer = dest;

			PolyStencilBlock stencil(x / 8 + y / 8 * stencilPitch, stencilValues, stencilMasks);

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF && !clipneeded && stencil.IsSingleValue())
			{
				// Reject whole block if the stencil test fails
				if (stencil.Get(0, 0) != stencilTestValue)
					continue;

				for (int iy = 0; iy < q; iy++)
				{
					uint32_t varying[TriVertex::NumVarying], varyingStep[TriVertex::NumVarying];
					for (int i = 0; i < TriVertex::NumVarying; i++)
					{
						float pos = varyingTL[i] + varyingBL[i] * iy;
						float step = (varyingTR[i] + varyingBR[i] * iy - pos) * (1.0f / q);

						varying[i] = (uint32_t)((pos - floor(pos)) * 0x100000000LL);
						varyingStep[i] = (uint32_t)(step * 0x100000000LL);
					}

#if NO_SSE
					for (int ix = x; ix < x + q; ix++)
					{
						uint32_t ufrac = varying[0];
						uint32_t vfrac = varying[1];

						uint32_t upos = ((ufrac >> 16) * textureWidth) >> 16;
						uint32_t vpos = ((vfrac >> 16) * textureHeight) >> 16;
						uint32_t uvoffset = upos * textureHeight + vpos;

						uint32_t fg = texturePixels[uvoffset];
						uint32_t fg_red = (RPART(fg) * diminishedlight) >> 8;
						uint32_t fg_green = (GPART(fg) * diminishedlight) >> 8;
						uint32_t fg_blue = (BPART(fg) * diminishedlight) >> 8;
						uint32_t fg_alpha = APART(fg);

						if (fg_alpha > 127)
							buffer[ix] = 0xff000000 | (fg_red << 16) | (fg_green << 8) | fg_blue;

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] += varyingStep[i];
					}
#else
					for (int sse = 0; sse < q / 4; sse++)
					{
						uint32_t fg[4];
						for (int ix = 0; ix < 4; ix++)
						{
							uint32_t ufrac = varying[0];
							uint32_t vfrac = varying[1];
							uint32_t upos = ((ufrac >> 16) * textureWidth) >> 16;
							uint32_t vpos = ((vfrac >> 16) * textureHeight) >> 16;
							uint32_t uvoffset = upos * textureHeight + vpos;
							fg[ix] = texturePixels[uvoffset];
							for (int i = 0; i < TriVertex::NumVarying; i++)
								varying[i] += varyingStep[i];
						}

						__m128i mfg = _mm_loadu_si128((const __m128i*)fg);
						__m128i mfg0 = _mm_unpacklo_epi8(mfg, _mm_setzero_si128());
						__m128i mfg1 = _mm_unpackhi_epi8(mfg, _mm_setzero_si128());
						__m128i mout0 = _mm_srli_epi16(_mm_mullo_epi16(mfg0, mlight), 8);
						__m128i mout1 = _mm_srli_epi16(_mm_mullo_epi16(mfg1, mlight), 8);
						__m128i mout = _mm_packus_epi16(mout0, mout1);
						__m128i mmask0 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(mfg0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
						__m128i mmask1 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(mfg1, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
						__m128i mmask = _mm_cmplt_epi8(_mm_packus_epi16(mmask0, mmask1), _mm_setzero_si128());
						_mm_maskmoveu_si128(mout, mmask, (char*)(&buffer[x + sse * 4]));
					}
#endif

					buffer += pitch;
				}
			}
			else // Partially covered block
			{
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (int iy = 0; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					uint32_t varying[TriVertex::NumVarying], varyingStep[TriVertex::NumVarying];
					for (int i = 0; i < TriVertex::NumVarying; i++)
					{
						float pos = varyingTL[i] + varyingBL[i] * iy;
						float step = (varyingTR[i] + varyingBR[i] * iy - pos) * (1.0f / q);

						varying[i] = (uint32_t)((pos - floor(pos)) * 0x100000000LL);
						varyingStep[i] = (uint32_t)(step * 0x100000000LL);
					}

					for (int ix = 0; ix < q; ix++)
					{
						bool visible = (ix + x >= clipleft) && (ix + x < clipright) && (cliptop <= y + iy) && (clipbottom > y + iy);

						if (CX1 > 0 && CX2 > 0 && CX3 > 0 && visible && stencil.Get(ix, iy) == stencilTestValue)
						{
							uint32_t ufrac = varying[0];
							uint32_t vfrac = varying[1];

							uint32_t upos = ((ufrac >> 16) * textureWidth) >> 16;
							uint32_t vpos = ((vfrac >> 16) * textureHeight) >> 16;
							uint32_t uvoffset = upos * textureHeight + vpos;

							uint32_t fg = texturePixels[uvoffset];
							uint32_t fg_red = (RPART(fg) * diminishedlight) >> 8;
							uint32_t fg_green = (GPART(fg) * diminishedlight) >> 8;
							uint32_t fg_blue = (BPART(fg) * diminishedlight) >> 8;
							uint32_t fg_alpha = APART(fg);

							if (fg_alpha > 127)
								buffer[ix + x] = 0xff000000 | (fg_red << 16) | (fg_green << 8) | fg_blue;
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] += varyingStep[i];

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;

					buffer += pitch;
				}
			}
		}
	}
}

void ScreenPolyTriangleDrawer::fill32(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread)
{
	uint32_t *dest = (uint32_t *)args->dest;
	int pitch = args->pitch;
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipleft = args->clipleft;
	int clipright = args->clipright;
	int cliptop = args->cliptop;
	int clipbottom = args->clipbottom;
	int solidcolor = args->solidcolor;
	uint8_t *stencilValues = args->stencilValues;
	uint32_t *stencilMasks = args->stencilMasks;
	int stencilPitch = args->stencilPitch;
	uint8_t stencilTestValue = args->stencilTestValue;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, clipleft);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, cliptop);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	dest += miny * pitch;

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Loop through blocks
	for (int y = miny; y < maxy; y += q, dest += q * pitch)
	{
		// Is this row of blocks done by this thread?
		if (thread->skipped_by_thread(y / q)) continue;

		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0) continue;

			// Check if block needs clipping
			bool clipneeded = clipleft > x || clipright < (x + q) || cliptop > y || clipbottom < (y + q);

			uint32_t *buffer = dest;

			PolyStencilBlock stencil(x / 8 + y / 8 * stencilPitch, stencilValues, stencilMasks);

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF && !clipneeded && stencil.IsSingleValue())
			{
				// Reject whole block if the stencil test fails
				if (stencil.Get(0, 0) != stencilTestValue)
					continue;

				for (int iy = 0; iy < q; iy++)
				{
					for (int ix = x; ix < x + q; ix++)
					{
						buffer[ix] = solidcolor;
					}

					buffer += pitch;
				}
			}
			else // Partially covered block
			{
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (int iy = 0; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool visible = (ix + x >= clipleft) && (ix + x < clipright) && (cliptop <= y + iy) && (clipbottom > y + iy);

						if (CX1 > 0 && CX2 > 0 && CX3 > 0 && visible && stencil.Get(ix, iy) == stencilTestValue)
						{
							buffer[ix + x] = solidcolor;
						}

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;

					buffer += pitch;
				}
			}
		}
	}
}

float ScreenPolyTriangleDrawer::gradx(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
{
	float top = (c1 - c2) * (y0 - y2) - (c0 - c2) * (y1 - y2);
	float bottom = (x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2);
	return top / bottom;
}

float ScreenPolyTriangleDrawer::grady(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
{
	float top = (c1 - c2) * (x0 - x2) - (c0 - c2) * (x1 - x2);
	float bottom = -((x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2));
	return top / bottom;
}

/////////////////////////////////////////////////////////////////////////////

DrawPolyTrianglesCommand::DrawPolyTrianglesCommand(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue)
	: uniforms(uniforms), vinput(vinput), vcount(vcount), mode(mode), ccw(ccw), clipleft(clipleft), clipright(clipright), cliptop(cliptop), clipbottom(clipbottom), texturePixels(texturePixels), textureWidth(textureWidth), textureHeight(textureHeight), solidcolor(solidcolor), stenciltestvalue(stenciltestvalue), stencilwritevalue(stencilwritevalue)
{
}

void DrawPolyTrianglesCommand::Execute(DrawerThread *thread)
{
	PolyTriangleDrawer::draw_arrays(
		uniforms, vinput, vcount, mode, ccw,
		clipleft, clipright, cliptop, clipbottom,
		texturePixels, textureWidth, textureHeight, solidcolor,
		stenciltestvalue, stencilwritevalue,
		thread, texturePixels ? ScreenPolyTriangleDrawer::draw32 : solidcolor != 0xbeef ? ScreenPolyTriangleDrawer::fill32 : ScreenPolyTriangleDrawer::stencil);
}

FString DrawPolyTrianglesCommand::DebugInfo()
{
	return "DrawPolyTriangles";
}
