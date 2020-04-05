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

#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#include "screen_blend.h"
#include "screen_scanline_setup.h"
#include "screen_shader.h"
#include "x86.h"
#include <cmath>

static void WriteDepth(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	size_t pitch = thread->depthstencil->Width();
	float* line = thread->depthstencil->DepthValues() + pitch * y;
	float* w = thread->scanline.W;
	for (int x = x0; x < x1; x++)
	{
		line[x] = w[x];
	}
}

static void WriteStencil(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	size_t pitch = thread->depthstencil->Width();
	uint8_t* line = thread->depthstencil->StencilValues() + pitch * y;
	uint8_t value = thread->StencilWriteValue;
	for (int x = x0; x < x1; x++)
	{
		line[x] = value;
	}
}

static void DrawSpan(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	WriteVaryings(y, x0, x1, args, thread);

	thread->FragmentShader(x0, x1, thread);

	if (!thread->AlphaTest)
	{
		if (thread->WriteColor)
			thread->WriteColorFunc(y, x0, x1, thread);
		if (thread->WriteDepth)
			WriteDepth(y, x0, x1, thread);
		if (thread->WriteStencil)
			WriteStencil(y, x0, x1, thread);
	}
	else
	{
		uint8_t* discard = thread->scanline.discard;
		while (x0 < x1)
		{
			int xstart = x0;
			while (!discard[x0] && x0 < x1) x0++;

			if (xstart < x0)
			{
				if (thread->WriteColor)
					thread->WriteColorFunc(y, xstart, x0, thread);
				if (thread->WriteDepth)
					WriteDepth(y, xstart, x0, thread);
				if (thread->WriteStencil)
					WriteStencil(y, xstart, x0, thread);
			}

			while (discard[x0] && x0 < x1) x0++;
		}
	}
}

template<typename OptT>
static void TestSpan(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	WriteW(y, x0, x1, args, thread);

	if ((OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_StencilTest))
	{
		size_t pitch = thread->depthstencil->Width();

		uint8_t* stencilbuffer;
		uint8_t* stencilLine;
		uint8_t stencilTestValue;
		if (OptT::Flags & SWTRI_StencilTest)
		{
			stencilbuffer = thread->depthstencil->StencilValues();
			stencilLine = stencilbuffer + pitch * y;
			stencilTestValue = thread->StencilTestValue;
		}

		float* zbuffer;
		float* zbufferLine;
		float* w;
		float depthbias;
		if (OptT::Flags & SWTRI_DepthTest)
		{
			zbuffer = thread->depthstencil->DepthValues();
			zbufferLine = zbuffer + pitch * y;
			w = thread->scanline.W;
			depthbias = thread->depthbias;
		}

		int x = x0;
		int xend = x1;
		while (x < xend)
		{
			int xstart = x;

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while (zbufferLine[x] >= w[x] + depthbias && stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] >= w[x] + depthbias && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else
			{
				x = xend;
			}

			if (x > xstart)
			{
				DrawSpan(y, xstart, x, args, thread);
			}

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while ((zbufferLine[x] < w[x] + depthbias || stencilLine[x] != stencilTestValue) && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] < w[x] + depthbias && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] != stencilTestValue && x < xend)
					x++;
			}
		}
	}
	else
	{
		DrawSpan(y, x0, x1, args, thread);
	}
}

static void SortVertices(const TriDrawTriangleArgs* args, ScreenTriVertex** sortedVertices)
{
	sortedVertices[0] = args->v1;
	sortedVertices[1] = args->v2;
	sortedVertices[2] = args->v3;

	if (sortedVertices[1]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[1]);
	if (sortedVertices[2]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[2]);
	if (sortedVertices[2]->y < sortedVertices[1]->y)
		std::swap(sortedVertices[1], sortedVertices[2]);
}

void ScreenTriangle::Draw(const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	// Sort vertices by Y position
	ScreenTriVertex* sortedVertices[3];
	SortVertices(args, sortedVertices);

	int clipleft = thread->clip.left;
	int cliptop = MAX(thread->clip.top, thread->numa_start_y);
	int clipright = thread->clip.right;
	int clipbottom = MIN(thread->clip.bottom, thread->numa_end_y);

	int topY = (int)(sortedVertices[0]->y + 0.5f);
	int midY = (int)(sortedVertices[1]->y + 0.5f);
	int bottomY = (int)(sortedVertices[2]->y + 0.5f);

	topY = MAX(topY, cliptop);
	midY = MIN(midY, clipbottom);
	bottomY = MIN(bottomY, clipbottom);

	if (topY >= bottomY)
		return;

	SelectFragmentShader(thread);
	SelectWriteColorFunc(thread);

	void(*testfunc)(int y, int x0, int x1, const TriDrawTriangleArgs * args, PolyTriangleThreadData * thread);

	int opt = 0;
	if (thread->DepthTest) opt |= SWTRI_DepthTest;
	if (thread->StencilTest) opt |= SWTRI_StencilTest;
	testfunc = ScreenTriangle::TestSpanOpts[opt];

	topY += thread->skipped_by_thread(topY);
	int num_cores = thread->num_cores;

	// Find start/end X positions for each line covered by the triangle:

	int y = topY;

	float longDX = sortedVertices[2]->x - sortedVertices[0]->x;
	float longDY = sortedVertices[2]->y - sortedVertices[0]->y;
	float longStep = longDX / longDY;
	float longPos = sortedVertices[0]->x + longStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
	longStep *= num_cores;

	if (y < midY)
	{
		float shortDX = sortedVertices[1]->x - sortedVertices[0]->x;
		float shortDY = sortedVertices[1]->y - sortedVertices[0]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[0]->x + shortStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < midY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			testfunc(y, x0, x1, args, thread);

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}

	if (y < bottomY)
	{
		float shortDX = sortedVertices[2]->x - sortedVertices[1]->x;
		float shortDY = sortedVertices[2]->y - sortedVertices[1]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[1]->x + shortStep * (y + 0.5f - sortedVertices[1]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < bottomY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			testfunc(y, x0, x1, args, thread);

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}
}

void(*ScreenTriangle::TestSpanOpts[])(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread) =
{
	&TestSpan<TestSpanOpt0>,
	&TestSpan<TestSpanOpt1>,
	&TestSpan<TestSpanOpt2>,
	&TestSpan<TestSpanOpt3>
};
