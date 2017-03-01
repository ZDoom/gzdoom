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
#include "poly_drawers.h"

void ScreenTriangle::SetupNormal(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipright = args->clipright;
	int clipbottom = args->clipbottom;
	
	int stencilPitch = args->stencilPitch;
	uint8_t * RESTRICT stencilValues = args->stencilValues;
	uint32_t * RESTRICT stencilMasks = args->stencilMasks;
	uint8_t stencilTestValue = args->stencilTestValue;
	
	TriFullSpan * RESTRICT span = thread->FullSpans;
	TriPartialBlock * RESTRICT partial = thread->PartialBlocks;
	
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
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
	{
		thread->NumFullSpans = 0;
		thread->NumPartialBlocks = 0;
		return;
	}
	
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
	
	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((miny / q) - core) % num_cores) % num_cores;
	miny += core_skip * q;

	thread->StartX = minx;
	thread->StartY = miny;
	span->Length = 0;

	// Loop through blocks
	for (int y = miny; y < maxy; y += q * num_cores)
	{
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
			
			// Stencil test the whole block, if possible
			int block = x / 8 + y / 8 * stencilPitch;
			uint8_t *stencilBlock = &stencilValues[block * 64];
			uint32_t *stencilBlockMask = &stencilMasks[block];
			bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
			bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;

			// Skip block when outside an edge
			if (a == 0 || b == 0 || c == 0 || skipBlock)
			{
				if (span->Length != 0)
				{
					span++;
					span->Length = 0;
				}
				continue;
			}

			// Accept whole block when totally covered
			if (a == 0xf && b == 0xf && c == 0xf && x + q <= clipright && y + q <= clipbottom && blockIsSingleStencil)
			{
				if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
			else // Partially covered block
			{
				x0 = x << 4;
				x1 = (x + q - 1) << 4;
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				uint32_t mask0 = 0;
				uint32_t mask1 = 0;

				for (int iy = 0; iy < 4; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask0 <<= 1;
						mask0 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				for (int iy = 4; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask1 <<= 1;
						mask1 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				if (mask0 != 0xffffffff || mask1 != 0xffffffff)
				{
					if (span->Length > 0)
					{
						span++;
						span->Length = 0;
					}

					if (mask0 == 0 && mask1 == 0)
						continue;

					partial->X = x;
					partial->Y = y;
					partial->Mask0 = mask0;
					partial->Mask1 = mask1;
					partial++;
				}
				else if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
		}
		
		if (span->Length != 0)
		{
			span++;
			span->Length = 0;
		}
	}
	
	thread->NumFullSpans = (int)(span - thread->FullSpans);
	thread->NumPartialBlocks = (int)(partial - thread->PartialBlocks);
}

void ScreenTriangle::SetupSubsector(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipright = args->clipright;
	int clipbottom = args->clipbottom;

	int stencilPitch = args->stencilPitch;
	uint8_t * RESTRICT stencilValues = args->stencilValues;
	uint32_t * RESTRICT stencilMasks = args->stencilMasks;
	uint8_t stencilTestValue = args->stencilTestValue;

	uint32_t * RESTRICT subsectorGBuffer = args->subsectorGBuffer;
	uint32_t subsectorDepth = args->uniforms->subsectorDepth;
	int32_t pitch = args->pitch;

	TriFullSpan * RESTRICT span = thread->FullSpans;
	TriPartialBlock * RESTRICT partial = thread->PartialBlocks;

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
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
	{
		thread->NumFullSpans = 0;
		thread->NumPartialBlocks = 0;
		return;
	}

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

	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((miny / q) - core) % num_cores) % num_cores;
	miny += core_skip * q;

	thread->StartX = minx;
	thread->StartY = miny;
	span->Length = 0;

	// Loop through blocks
	for (int y = miny; y < maxy; y += q * num_cores)
	{
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

			// Stencil test the whole block, if possible
			int block = x / 8 + y / 8 * stencilPitch;
			uint8_t *stencilBlock = &stencilValues[block * 64];
			uint32_t *stencilBlockMask = &stencilMasks[block];
			bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
			bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) < stencilTestValue;

			// Skip block when outside an edge
			if (a == 0 || b == 0 || c == 0 || skipBlock)
			{
				if (span->Length != 0)
				{
					span++;
					span->Length = 0;
				}
				continue;
			}

			// Accept whole block when totally covered
			if (a == 0xf && b == 0xf && c == 0xf && x + q <= clipright && y + q <= clipbottom && blockIsSingleStencil)
			{
				// Totally covered block still needs a subsector coverage test:

				uint32_t *subsector = subsectorGBuffer + x + y * pitch;

				uint32_t mask0 = 0;
				uint32_t mask1 = 0;

				for (int iy = 0; iy < 4; iy++)
				{
					for (int ix = 0; ix < q; ix++)
					{
						bool covered = subsector[ix] >= subsectorDepth;
						mask0 <<= 1;
						mask0 |= (uint32_t)covered;
					}
					subsector += pitch;
				}

				for (int iy = 4; iy < q; iy++)
				{
					for (int ix = 0; ix < q; ix++)
					{
						bool covered = subsector[ix] >= subsectorDepth;
						mask1 <<= 1;
						mask1 |= (uint32_t)covered;
					}
					subsector += pitch;
				}

				if (mask0 != 0xffffffff || mask1 != 0xffffffff)
				{
					if (span->Length > 0)
					{
						span++;
						span->Length = 0;
					}

					if (mask0 == 0 && mask1 == 0)
						continue;

					partial->X = x;
					partial->Y = y;
					partial->Mask0 = mask0;
					partial->Mask1 = mask1;
					partial++;
				}
				else if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
			else // Partially covered block
			{
				x0 = x << 4;
				x1 = (x + q - 1) << 4;
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				uint32_t *subsector = subsectorGBuffer + x + y * pitch;

				uint32_t mask0 = 0;
				uint32_t mask1 = 0;

				for (int iy = 0; iy < 4; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] >= stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest && subsector[ix] >= subsectorDepth);
						mask0 <<= 1;
						mask0 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
					subsector += pitch;
				}

				for (int iy = 4; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] >= stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest && subsector[ix] >= subsectorDepth);
						mask1 <<= 1;
						mask1 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
					subsector += pitch;
				}

				if (mask0 != 0xffffffff || mask1 != 0xffffffff)
				{
					if (span->Length > 0)
					{
						span++;
						span->Length = 0;
					}

					if (mask0 == 0 && mask1 == 0)
						continue;

					partial->X = x;
					partial->Y = y;
					partial->Mask0 = mask0;
					partial->Mask1 = mask1;
					partial++;
				}
				else if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
		}

		if (span->Length != 0)
		{
			span++;
			span->Length = 0;
		}
	}

	thread->NumFullSpans = (int)(span - thread->FullSpans);
	thread->NumPartialBlocks = (int)(partial - thread->PartialBlocks);
}

void ScreenTriangle::StencilWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	uint8_t * RESTRICT stencilValues = args->stencilValues;
	uint32_t * RESTRICT stencilMasks = args->stencilMasks;
	uint32_t stencilWriteValue = args->stencilWriteValue;
	uint32_t stencilPitch = args->stencilPitch;

	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;

	for (int i = 0; i < numSpans; i++)
	{
		const auto &span = fullSpans[i];
		
		int block = span.X / 8 + span.Y / 8 * stencilPitch;
		uint8_t *stencilBlock = &stencilValues[block * 64];
		uint32_t *stencilBlockMask = &stencilMasks[block];
		
		int width = span.Length;
		for (int x = 0; x < width; x++)
			stencilBlockMask[x] = 0xffffff00 | stencilWriteValue;
	}
	
	for (int i = 0; i < numBlocks; i++)
	{
		const auto &block = partialBlocks[i];
		
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		
		int sblock = block.X / 8 + block.Y / 8 * stencilPitch;
		uint8_t *stencilBlock = &stencilValues[sblock * 64];
		uint32_t *stencilBlockMask = &stencilMasks[sblock];
		
		bool isSingleValue = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
		if (isSingleValue)
		{
			uint8_t value = (*stencilBlockMask) & 0xff;
			for (int v = 0; v < 64; v++)
				stencilBlock[v] = value;
			*stencilBlockMask = 0;
		}
		
		int count = 0;
		for (int v = 0; v < 32; v++)
		{
			if ((mask0 & (1 << 31)) || stencilBlock[v] == stencilWriteValue)
			{
				stencilBlock[v] = stencilWriteValue;
				count++;
			}
			mask0 <<= 1;
		}
		for (int v = 32; v < 64; v++)
		{
			if ((mask1 & (1 << 31)) || stencilBlock[v] == stencilWriteValue)
			{
				stencilBlock[v] = stencilWriteValue;
				count++;
			}
			mask1 <<= 1;
		}
		
		if (count == 64)
			*stencilBlockMask = 0xffffff00 | stencilWriteValue;
	}
}

void ScreenTriangle::SubsectorWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	uint32_t * RESTRICT subsectorGBuffer = args->subsectorGBuffer;
	uint32_t subsectorDepth = args->uniforms->subsectorDepth;
	int pitch = args->pitch;

	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;

	for (int i = 0; i < numSpans; i++)
	{
		const auto &span = fullSpans[i];

		uint32_t *subsector = subsectorGBuffer + span.X + span.Y * pitch;
		int width = span.Length * 8;
		int height = 8;
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
				subsector[x] = subsectorDepth;
			subsector += pitch;
		}
	}
	
	for (int i = 0; i < numBlocks; i++)
	{
		const auto &block = partialBlocks[i];

		uint32_t *subsector = subsectorGBuffer + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
					subsector[x] = subsectorDepth;
				mask0 <<= 1;
			}
			subsector += pitch;
		}
		for (int y = 4; y < 8; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
					subsector[x] = subsectorDepth;
				mask1 <<= 1;
			}
			subsector += pitch;
		}
	}
}

#if 0
float ScreenTriangle::FindGradientX(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
{
	float top = (c1 - c2) * (y0 - y2) - (c0 - c2) * (y1 - y2);
	float bottom = (x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2);
	return top / bottom;
}

float ScreenTriangle::FindGradientY(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2)
{
	float top = (c1 - c2) * (x0 - x2) - (c0 - c2) * (x1 - x2);
	float bottom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
	return top / bottom;
}

void ScreenTriangle::Draw(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	int numSpans = thread->NumFullSpans;
	auto fullSpans = thread->FullSpans;
	int numBlocks = thread->NumPartialBlocks;
	auto partialBlocks = thread->PartialBlocks;
	int startX = thread->StartX;
	int startY = thread->StartY;

	// Calculate gradients
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	ScreenTriangleStepVariables gradientX;
	ScreenTriangleStepVariables gradientY;
	ScreenTriangleStepVariables start;
	gradientX.W = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	gradientY.W = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	start.W = v1.w + gradientX.W * (startX - v1.x) + gradientY.W * (startY - v1.y);
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradientX.Varying[i] = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradientY.Varying[i] = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		start.Varying[i] = v1.varying[i] * v1.w + gradientX.Varying[i] * (startX - v1.x) + gradientY.Varying[i] * (startY - v1.y);
	}

	const uint32_t * RESTRICT texPixels = (const uint32_t *)args->texturePixels;
	uint32_t texWidth = args->textureWidth;
	uint32_t texHeight = args->textureHeight;

	uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
	uint32_t * RESTRICT subsectorGBuffer = (uint32_t*)args->subsectorGBuffer;
	int pitch = args->pitch;

	uint32_t subsectorDepth = args->uniforms->subsectorDepth;

	uint32_t light = args->uniforms->light;
	float shade = (64.0f - (light * 255 / 256 + 12.0f) * 32.0f / 128.0f) / 32.0f;
	float globVis = 1706.0f;

	for (int i = 0; i < numSpans; i++)
	{
		const auto &span = fullSpans[i];

		uint32_t *dest = destOrg + span.X + span.Y * pitch;
		uint32_t *subsector = subsectorGBuffer + span.X + span.Y * pitch;
		int width = span.Length;
		int height = 8;

		ScreenTriangleStepVariables blockPosY;
		blockPosY.W = start.W + gradientX.W * (span.X - startX) + gradientY.W * (span.Y - startY);
		for (int j = 0; j < TriVertex::NumVarying; j++)
			blockPosY.Varying[j] = start.Varying[j] + gradientX.Varying[j] * (span.X - startX) + gradientY.Varying[j] * (span.Y - startY);

		for (int y = 0; y < height; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);
			int lightpos = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);

			for (int x = 0; x < width; x++)
			{
				blockPosX.W += gradientX.W * 8;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosX.Varying[j] += gradientX.Varying[j] * 8;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t varyingStep[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
				{
					int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
					varyingStep[j] = (nextPos - varyingPos[j]) / 8;
				}

				int lightnext = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);
				int lightstep = (lightnext - lightpos) / 8;

				for (int ix = 0; ix < 8; ix++)
				{
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];

					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = r * lightpos / 256;
					g = g * lightpos / 256;
					b = b * lightpos / 256;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;

					dest[x * 8 + ix] = fg;
					subsector[x * 8 + ix] = subsectorDepth;

					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] += varyingStep[j];
					lightpos += lightstep;
				}
			}
		
			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
			subsector += pitch;
		}
	}
	
	for (int i = 0; i < numBlocks; i++)
	{
		const auto &block = partialBlocks[i];

		ScreenTriangleStepVariables blockPosY;
		blockPosY.W = start.W + gradientX.W * (block.X - startX) + gradientY.W * (block.Y - startY);
		for (int j = 0; j < TriVertex::NumVarying; j++)
			blockPosY.Varying[j] = start.Varying[j] + gradientX.Varying[j] * (block.X - startX) + gradientY.Varying[j] * (block.Y - startY);

		uint32_t *dest = destOrg + block.X + block.Y * pitch;
		uint32_t *subsector = subsectorGBuffer + block.X + block.Y * pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);

			blockPosX.W += gradientX.W * 8;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosX.Varying[j] += gradientX.Varying[j] * 8;

			rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingStep[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
			{
				int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
				varyingStep[j] = (nextPos - varyingPos[j]) / 8;
			}

			int lightnext = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);
			int lightstep = (lightnext - lightpos) / 8;

			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1 << 31))
				{
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];

					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = r * lightpos / 256;
					g = g * lightpos / 256;
					b = b * lightpos / 256;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;

					dest[x] = fg;
					subsector[x] = subsectorDepth;
				}
				mask0 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
			subsector += pitch;
		}
		for (int y = 4; y < 8; y++)
		{
			ScreenTriangleStepVariables blockPosX = blockPosY;

			float rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = (int32_t)(blockPosX.Varying[j] * rcpW);

			int lightpos = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);

			blockPosX.W += gradientX.W * 8;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosX.Varying[j] += gradientX.Varying[j] * 8;

			rcpW = 0x01000000 / blockPosX.W;
			int32_t varyingStep[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
			{
				int32_t nextPos = (int32_t)(blockPosX.Varying[j] * rcpW);
				varyingStep[j] = (nextPos - varyingPos[j]) / 8;
			}

			int lightnext = 256 - (int)(clamp(shade - MIN(24.0f, globVis * blockPosX.W) / 32.0f, 0.0f, 31.0f / 32.0f) * 256.0f);
			int lightstep = (lightnext - lightpos) / 8;

			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1 << 31))
				{
					int texelX = ((((uint32_t)varyingPos[0] << 8) >> 16) * texWidth) >> 16;
					int texelY = ((((uint32_t)varyingPos[1] << 8) >> 16) * texHeight) >> 16;
					uint32_t fg = texPixels[texelX * texHeight + texelY];

					uint32_t r = RPART(fg);
					uint32_t g = GPART(fg);
					uint32_t b = BPART(fg);
					r = r * lightpos / 256;
					g = g * lightpos / 256;
					b = b * lightpos / 256;
					fg = 0xff000000 | (r << 16) | (g << 8) | b;

					dest[x] = fg;
					subsector[x] = subsectorDepth;
				}
				mask1 <<= 1;

				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] += varyingStep[j];
				lightpos += lightstep;
			}

			blockPosY.W += gradientY.W;
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosY.Varying[j] += gradientY.Varying[j];

			dest += pitch;
			subsector += pitch;
		}
	}
}
#endif
