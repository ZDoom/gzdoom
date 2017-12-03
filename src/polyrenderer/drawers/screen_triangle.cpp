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
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#ifndef NO_SSE
#include "poly_drawer32_sse2.h"
#else
#include "poly_drawer32.h"
#endif
#include "poly_drawer8.h"
#include "x86.h"

class TriangleBlock
{
public:
	TriangleBlock(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	void Render();

private:
	void RenderSubdivide(int x0, int y0, int x1, int y1);

	enum class CoverageModes { Full, Partial };
	struct CoverageFull { static const int Mode = (int)CoverageModes::Full; };
	struct CoveragePartial { static const int Mode = (int)CoverageModes::Partial; };

	template<typename CoverageMode>
	void RenderBlock(int x0, int y0, int x1, int y1);

	const TriDrawTriangleArgs *args;
	WorkerThreadData *thread;

	// Block size, standard 8x8 (must be power of two)
	static const int q = 8;

	// Deltas
	int DX12, DX23, DX31;
	int DY12, DY23, DY31;

	// Fixed-point deltas
	int FDX12, FDX23, FDX31;
	int FDY12, FDY23, FDY31;

	// Half-edge constants
	int C1, C2, C3;

	// Stencil buffer
	int stencilPitch;
	uint8_t * RESTRICT stencilValues;
	uint32_t * RESTRICT stencilMasks;
	uint8_t stencilTestValue;
	uint32_t stencilWriteValue;

	// Viewport clipping
	int clipright;
	int clipbottom;

	// Depth buffer
	float * RESTRICT zbuffer;
	int32_t zbufferPitch;

	// Triangle bounding block
	int minx, miny;
	int maxx, maxy;

	// Active block
	int X, Y;
	uint32_t Mask0, Mask1;

#ifndef NO_SSE
	__m128i mFDY12Offset;
	__m128i mFDY23Offset;
	__m128i mFDY31Offset;
	__m128i mFDY12x4;
	__m128i mFDY23x4;
	__m128i mFDY31x4;
	__m128i mFDX12;
	__m128i mFDX23;
	__m128i mFDX31;
	__m128i mC1;
	__m128i mC2;
	__m128i mC3;
	__m128i mDX12;
	__m128i mDY12;
	__m128i mDX23;
	__m128i mDY23;
	__m128i mDX31;
	__m128i mDY31;
#endif

	enum class CoverageResult
	{
		full,
		partial,
		none
	};
	CoverageResult AreaCoverageTest(int x0, int y0, int x1, int y1);

	void CoverageTest();
	void StencilEqualTest();
	void StencilGreaterEqualTest();
	void DepthTest(const TriDrawTriangleArgs *args);
	void ClipTest();
	void StencilWrite();
	void DepthWrite(const TriDrawTriangleArgs *args);
};

TriangleBlock::TriangleBlock(const TriDrawTriangleArgs *args, WorkerThreadData *thread) : args(args), thread(thread)
{
	const ShadedTriVertex &v1 = *args->v1;
	const ShadedTriVertex &v2 = *args->v2;
	const ShadedTriVertex &v3 = *args->v3;

	clipright = args->clipright;
	clipbottom = args->clipbottom;

	stencilPitch = args->stencilPitch;
	stencilValues = args->stencilValues;
	stencilMasks = args->stencilMasks;
	stencilTestValue = args->uniforms->StencilTestValue();
	stencilWriteValue = args->uniforms->StencilWriteValue();

	zbuffer = args->zbuffer;
	zbufferPitch = args->stencilPitch;

	// 28.4 fixed-point coordinates
#ifdef NO_SSE
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);
#else
	int tempround[4 * 3];
	__m128 m16 = _mm_set1_ps(16.0f);
	__m128 mhalf = _mm_set1_ps(65536.5f);
	__m128i m65536 = _mm_set1_epi32(65536);
	_mm_storeu_si128((__m128i*)tempround, _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v1), m16), mhalf)), m65536));
	_mm_storeu_si128((__m128i*)(tempround + 4), _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v2), m16), mhalf)), m65536));
	_mm_storeu_si128((__m128i*)(tempround + 8), _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v3), m16), mhalf)), m65536));
	const int X1 = tempround[0];
	const int X2 = tempround[4];
	const int X3 = tempround[8];
	const int Y1 = tempround[1];
	const int Y2 = tempround[5];
	const int Y3 = tempround[9];
#endif

	// Deltas
	DX12 = X1 - X2;
	DX23 = X2 - X3;
	DX31 = X3 - X1;

	DY12 = Y1 - Y2;
	DY23 = Y2 - Y3;
	DY31 = Y3 - Y1;

	// Fixed-point deltas
	FDX12 = DX12 << 4;
	FDX23 = DX23 << 4;
	FDX31 = DX31 << 4;

	FDY12 = DY12 << 4;
	FDY23 = DY23 << 4;
	FDY31 = DY31 << 4;

	// Bounding rectangle
	minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
	{
		return;
	}

	// Start and end in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);
	maxx |= q - 1;
	maxy |= q - 1;

	// Half-edge constants
	C1 = DY12 * X1 - DX12 * Y1;
	C2 = DY23 * X2 - DX23 * Y2;
	C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

#ifndef NO_SSE
	mFDY12Offset = _mm_setr_epi32(0, FDY12, FDY12 * 2, FDY12 * 3);
	mFDY23Offset = _mm_setr_epi32(0, FDY23, FDY23 * 2, FDY23 * 3);
	mFDY31Offset = _mm_setr_epi32(0, FDY31, FDY31 * 2, FDY31 * 3);
	mFDY12x4 = _mm_set1_epi32(FDY12 * 4);
	mFDY23x4 = _mm_set1_epi32(FDY23 * 4);
	mFDY31x4 = _mm_set1_epi32(FDY31 * 4);
	mFDX12 = _mm_set1_epi32(FDX12);
	mFDX23 = _mm_set1_epi32(FDX23);
	mFDX31 = _mm_set1_epi32(FDX31);
	mC1 = _mm_set1_epi32(C1);
	mC2 = _mm_set1_epi32(C2);
	mC3 = _mm_set1_epi32(C3);
	mDX12 = _mm_set1_epi32(DX12);
	mDY12 = _mm_set1_epi32(DY12);
	mDX23 = _mm_set1_epi32(DX23);
	mDY23 = _mm_set1_epi32(DY23);
	mDX31 = _mm_set1_epi32(DX31);
	mDY31 = _mm_set1_epi32(DY31);
#endif
}

void TriangleBlock::Render()
{
	RenderSubdivide(minx / q, miny / q, (maxx + 1) / q, (maxy + 1) / q);
}

void TriangleBlock::RenderSubdivide(int x0, int y0, int x1, int y1)
{
	CoverageResult result = AreaCoverageTest(x0 * q, y0 * q, x1 * q, y1 * q);
	if (result == CoverageResult::full)
	{
		RenderBlock<CoverageFull>(x0 * q, y0 * q, x1 * q, y1 * q);
	}
	else if (result == CoverageResult::partial)
	{
		bool doneX = x1 - x0 <= 8;
		bool doneY = y1 - y0 <= 8;
		if (doneX && doneY)
		{
			RenderBlock<CoveragePartial>(x0 * q, y0 * q, x1 * q, y1 * q);
		}
		else
		{
			int midx = (x0 + x1) >> 1;
			int midy = (y0 + y1) >> 1;
			if (doneX)
			{
				RenderSubdivide(x0, y0, x1, midy);
				RenderSubdivide(x0, midy, x1, y1);
			}
			else if (doneY)
			{
				RenderSubdivide(x0, y0, midx, y1);
				RenderSubdivide(midx, y0, x1, y1);
			}
			else
			{
				RenderSubdivide(x0, y0, midx, midy);
				RenderSubdivide(midx, y0, x1, midy);
				RenderSubdivide(x0, midy, midx, y1);
				RenderSubdivide(midx, midy, x1, y1);
			}
		}
	}
}

template<typename CoverageModeT>
void TriangleBlock::RenderBlock(int x0, int y0, int x1, int y1)
{
	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((y0 / q) - core) % num_cores) % num_cores;
	int start_miny = y0 + core_skip * q;

	bool depthTest = args->uniforms->DepthTest();
	bool writeColor = args->uniforms->WriteColor();
	bool writeStencil = args->uniforms->WriteStencil();
	bool writeDepth = args->uniforms->WriteDepth();

	int bmode = (int)args->uniforms->BlendMode();
	auto drawFunc = args->destBgra ? ScreenTriangle::TriDrawers32[bmode] : ScreenTriangle::TriDrawers8[bmode];

	// Loop through blocks
	for (int y = start_miny; y < y1; y += q * num_cores)
	{
		for (int x = x0; x < x1; x += q)
		{
			X = x;
			Y = y;

			if (CoverageModeT::Mode == (int)CoverageModes::Full)
			{
				Mask0 = 0xffffffff;
				Mask1 = 0xffffffff;
			}
			else
			{
				CoverageTest();
				if (Mask0 == 0 && Mask1 == 0)
					continue;
			}

			ClipTest();
			if (Mask0 == 0 && Mask1 == 0)
				continue;

			// To do: make the stencil test use its own flag for comparison mode instead of abusing the depth test..
			if (!depthTest)
			{
				StencilEqualTest();
				if (Mask0 == 0 && Mask1 == 0)
					continue;
			}
			else
			{
				StencilGreaterEqualTest();
				if (Mask0 == 0 && Mask1 == 0)
					continue;

				DepthTest(args);
				if (Mask0 == 0 && Mask1 == 0)
					continue;
			}

			if (writeColor)
				drawFunc(X, Y, Mask0, Mask1, args);
			if (writeStencil)
				StencilWrite();
			if (writeDepth)
				DepthWrite(args);
		}
	}
}

#ifdef NO_SSE

void TriangleBlock::DepthTest(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y);

	uint32_t mask0 = 0;
	uint32_t mask1 = 0;

	for (int iy = 0; iy < 4; iy++)
	{
		float posXW = posYW;
		for (int ix = 0; ix < 8; ix++)
		{
			bool covered = *depth <= posXW;
			mask0 <<= 1;
			mask0 |= (uint32_t)covered;
			depth++;
			posXW += stepXW;
		}
		posYW += stepYW;
	}

	for (int iy = 0; iy < 4; iy++)
	{
		float posXW = posYW;
		for (int ix = 0; ix < 8; ix++)
		{
			bool covered = *depth <= posXW;
			mask1 <<= 1;
			mask1 |= (uint32_t)covered;
			depth++;
			posXW += stepXW;
		}
		posYW += stepYW;
	}

	Mask0 = Mask0 & mask0;
	Mask1 = Mask1 & mask1;
}

#else

void TriangleBlock::DepthTest(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y);

	__m128 mposYW = _mm_setr_ps(posYW, posYW + stepXW, posYW + stepXW + stepXW, posYW + stepXW + stepXW + stepXW);
	__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
	__m128 mstepYW = _mm_set1_ps(stepYW);

	uint32_t mask0 = 0;
	uint32_t mask1 = 0;

	for (int iy = 0; iy < 4; iy++)
	{
		__m128 mposXW = mposYW;
		for (int ix = 0; ix < 2; ix++)
		{
			__m128 covered = _mm_cmplt_ps(_mm_loadu_ps(depth), mposXW);
			mask0 <<= 4;
			mask0 |= _mm_movemask_ps(_mm_shuffle_ps(covered, covered, _MM_SHUFFLE(0, 1, 2, 3)));
			depth += 4;
			mposXW = _mm_add_ps(mposXW, mstepXW);
		}
		mposYW = _mm_add_ps(mposYW, mstepYW);
	}

	for (int iy = 0; iy < 4; iy++)
	{
		__m128 mposXW = mposYW;
		for (int ix = 0; ix < 2; ix++)
		{
			__m128 covered = _mm_cmplt_ps(_mm_loadu_ps(depth), mposXW);
			mask1 <<= 4;
			mask1 |= _mm_movemask_ps(_mm_shuffle_ps(covered, covered, _MM_SHUFFLE(0, 1, 2, 3)));
			depth += 4;
			mposXW = _mm_add_ps(mposXW, mstepXW);
		}
		mposYW = _mm_add_ps(mposYW, mstepYW);
	}

	Mask0 = Mask0 & mask0;
	Mask1 = Mask1 & mask1;
}

#endif

void TriangleBlock::ClipTest()
{
	static const uint32_t clipxmask[8] =
	{
		0,
		0x80808080,
		0xc0c0c0c0,
		0xe0e0e0e0,
		0xf0f0f0f0,
		0xf8f8f8f8,
		0xfcfcfcfc,
		0xfefefefe
	};

	static const uint32_t clipymask[8] =
	{
		0,
		0xff000000,
		0xffff0000,
		0xffffff00,
		0xffffffff,
		0xffffffff,
		0xffffffff,
		0xffffffff
	};

	uint32_t xmask = (X + 8 <= clipright) ? 0xffffffff : clipxmask[clipright - X];
	uint32_t ymask0 = (Y + 4 <= clipbottom) ? 0xffffffff : clipymask[clipbottom - Y];
	uint32_t ymask1 = (Y + 8 <= clipbottom) ? 0xffffffff : clipymask[clipbottom - Y - 4];

	Mask0 = Mask0 & xmask & ymask0;
	Mask1 = Mask1 & xmask & ymask1;
}

#ifdef NO_SSE

void TriangleBlock::StencilEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 4; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] == stencilTestValue;
				mask0 <<= 1;
				mask0 |= (uint32_t)passStencilTest;
			}
		}

		for (int iy = 4; iy < q; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] == stencilTestValue;
				mask1 <<= 1;
				mask1 |= (uint32_t)passStencilTest;
			}
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

#else

void TriangleBlock::StencilEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		__m128i mstencilTestValue = _mm_set1_epi16(stencilTestValue);
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mstencilBlock = _mm_loadu_si128((const __m128i *)stencilBlock);

			__m128i mstencilTest = _mm_cmpeq_epi16(_mm_unpacklo_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			__m128i mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			__m128i mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mstencilTest = _mm_cmpeq_epi16(_mm_unpackhi_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask0 <<= 16;
			mask0 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));

			stencilBlock += 16;
		}

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mstencilBlock = _mm_loadu_si128((const __m128i *)stencilBlock);

			__m128i mstencilTest = _mm_cmpeq_epi16(_mm_unpacklo_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			__m128i mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			__m128i mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mstencilTest = _mm_cmpeq_epi16(_mm_unpackhi_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask1 <<= 16;
			mask1 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));

			stencilBlock += 16;
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

#endif

void TriangleBlock::StencilGreaterEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) < stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 4; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] >= stencilTestValue;
				mask0 <<= 1;
				mask0 |= (uint32_t)passStencilTest;
			}
		}

		for (int iy = 4; iy < q; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] >= stencilTestValue;
				mask1 <<= 1;
				mask1 |= (uint32_t)passStencilTest;
			}
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

TriangleBlock::CoverageResult TriangleBlock::AreaCoverageTest(int x0, int y0, int x1, int y1)
{
	// Corners of block
	x0 = x0 << 4;
	x1 = (x1 - 1) << 4;
	y0 = y0 << 4;
	y1 = (y1 - 1) << 4;

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

	if (a == 0 || b == 0 || c == 0) // Skip block when outside an edge
	{
		return CoverageResult::none;
	}
	else if (a == 0xf && b == 0xf && c == 0xf) // Accept whole block when totally covered
	{
		return CoverageResult::full;
	}
	else // Partially covered block
	{
		return CoverageResult::partial;
	}
}

#ifdef NO_SSE

void TriangleBlock::CoverageTest()
{
	// Corners of block
	int x0 = X << 4;
	int x1 = (X + q - 1) << 4;
	int y0 = Y << 4;
	int y1 = (Y + q - 1) << 4;

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

	if (a == 0 || b == 0 || c == 0) // Skip block when outside an edge
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (a == 0xf && b == 0xf && c == 0xf) // Accept whole block when totally covered
	{
		Mask0 = 0xffffffff;
		Mask1 = 0xffffffff;
	}
	else // Partially covered block
	{
		x0 = X << 4;
		x1 = (X + q - 1) << 4;
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
				bool covered = CX1 > 0 && CX2 > 0 && CX3 > 0;
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
				bool covered = CX1 > 0 && CX2 > 0 && CX3 > 0;
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

		Mask0 = mask0;
		Mask1 = mask1;
	}
}

#else

void TriangleBlock::CoverageTest()
{
	// Corners of block
	int x0 = X << 4;
	int x1 = (X + q - 1) << 4;
	int y0 = Y << 4;
	int y1 = (Y + q - 1) << 4;

	__m128i mY = _mm_set_epi32(y0, y0, y1, y1);
	__m128i mX = _mm_set_epi32(x0, x0, x1, x1);

	// Evaluate half-space functions
	__m128i mCY1 = _mm_sub_epi32(
		_mm_add_epi32(mC1, _mm_shuffle_epi32(_mm_mul_epu32(mDX12, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY12, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mA = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());

	__m128i mCY2 = _mm_sub_epi32(
		_mm_add_epi32(mC2, _mm_shuffle_epi32(_mm_mul_epu32(mDX23, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY23, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mB = _mm_cmpgt_epi32(mCY2, _mm_setzero_si128());

	__m128i mCY3 = _mm_sub_epi32(
		_mm_add_epi32(mC3, _mm_shuffle_epi32(_mm_mul_epu32(mDX31, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY31, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mC = _mm_cmpgt_epi32(mCY3, _mm_setzero_si128());

	int abc = _mm_movemask_epi8(_mm_packs_epi16(_mm_packs_epi32(mA, mB), _mm_packs_epi32(mC, _mm_setzero_si128())));

	if ((abc & 0xf) == 0 || (abc & 0xf0) == 0 || (abc & 0xf00) == 0) // Skip block when outside an edge
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (abc == 0xfff) // Accept whole block when totally covered
	{
		Mask0 = 0xffffffff;
		Mask1 = 0xffffffff;
	}
	else // Partially covered block
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		mCY1 = _mm_sub_epi32(_mm_shuffle_epi32(mCY1, _MM_SHUFFLE(0, 0, 0, 0)), mFDY12Offset);
		mCY2 = _mm_sub_epi32(_mm_shuffle_epi32(mCY2, _MM_SHUFFLE(0, 0, 0, 0)), mFDY23Offset);
		mCY3 = _mm_sub_epi32(_mm_shuffle_epi32(mCY3, _MM_SHUFFLE(0, 0, 0, 0)), mFDY31Offset);
		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			__m128i mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask0 <<= 16;
			mask0 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));
		}

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			__m128i mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask1 <<= 16;
			mask1 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));
		}

		Mask0 = mask0;
		Mask1 = mask1;
	}
}

#endif

void TriangleBlock::StencilWrite()
{
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t &stencilBlockMask = stencilMasks[block];
	uint32_t writeValue = stencilWriteValue;

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		stencilBlockMask = 0xffffff00 | writeValue;
	}
	else
	{
		uint32_t mask0 = Mask0;
		uint32_t mask1 = Mask1;

		bool isSingleValue = (stencilBlockMask & 0xffffff00) == 0xffffff00;
		if (isSingleValue)
		{
			uint8_t value = stencilBlockMask & 0xff;
			for (int v = 0; v < 64; v++)
				stencilBlock[v] = value;
			stencilBlockMask = 0;
		}

		int count = 0;
		for (int v = 0; v < 32; v++)
		{
			if ((mask0 & (1 << 31)) || stencilBlock[v] == writeValue)
			{
				stencilBlock[v] = writeValue;
				count++;
			}
			mask0 <<= 1;
		}
		for (int v = 32; v < 64; v++)
		{
			if ((mask1 & (1 << 31)) || stencilBlock[v] == writeValue)
			{
				stencilBlock[v] = writeValue;
				count++;
			}
			mask1 <<= 1;
		}

		if (count == 64)
			stencilBlockMask = 0xffffff00 | writeValue;
	}
}

#ifdef NO_SSE

void TriangleBlock::DepthWrite(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y);

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		for (int iy = 0; iy < 8; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				*(depth++) = posXW;
				posXW += stepXW;
			}
			posYW += stepYW;
		}
	}
	else
	{
		uint32_t mask0 = Mask0;
		uint32_t mask1 = Mask1;

		for (int iy = 0; iy < 4; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				if (mask0 & (1 << 31))
					*depth = posXW;
				posXW += stepXW;
				mask0 <<= 1;
				depth++;
			}
			posYW += stepYW;
		}

		for (int iy = 0; iy < 4; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				if (mask1 & (1 << 31))
					*depth = posXW;
				posXW += stepXW;
				mask1 <<= 1;
				depth++;
			}
			posYW += stepYW;
		}
	}
}

#else

void TriangleBlock::DepthWrite(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y);

	__m128 mposYW = _mm_setr_ps(posYW, posYW + stepXW, posYW + stepXW + stepXW, posYW + stepXW + stepXW + stepXW);
	__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
	__m128 mstepYW = _mm_set1_ps(stepYW);

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		for (int iy = 0; iy < 8; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_storeu_ps(depth, mposXW); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_storeu_ps(depth, mposXW); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}
	}
	else
	{
		__m128i mxormask = _mm_set1_epi32(0xffffffff);
		__m128i topfour = _mm_setr_epi32(1 << 31, 1 << 30, 1 << 29, 1 << 28);

		__m128i mmask0 = _mm_set1_epi32(Mask0);
		__m128i mmask1 = _mm_set1_epi32(Mask1);

		for (int iy = 0; iy < 4; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask0, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask0 = _mm_slli_epi32(mmask0, 4); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask0, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask0 = _mm_slli_epi32(mmask0, 4); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}

		for (int iy = 0; iy < 4; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask1, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask1 = _mm_slli_epi32(mmask1, 4); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask1, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask1 = _mm_slli_epi32(mmask1, 4); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}
	}
}

#endif

EXTERN_CVAR(Bool, r_polyrenderer)

void ScreenTriangle::Draw(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	if (r_polyrenderer)
	{
		TriangleBlock block(args, thread);
		block.Render();
	}
	else
	{
		DrawSWRender(args, thread);
	}
}

static void SortVertices(const TriDrawTriangleArgs *args, ShadedTriVertex **sortedVertices)
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

void ScreenTriangle::DrawSWRender(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	// Sort vertices by Y position
	ShadedTriVertex *sortedVertices[3];
	SortVertices(args, sortedVertices);

	int clipright = args->clipright;
	int clipbottom = args->clipbottom;

	// Ranges that different triangles edges are active
	int topY = (int)(sortedVertices[0]->y + 0.5f);
	int midY = (int)(sortedVertices[1]->y + 0.5f);
	int bottomY = (int)(sortedVertices[2]->y + 0.5f);

	topY = MAX(topY, 0);
	midY = clamp(midY, 0, clipbottom);
	bottomY = MIN(bottomY, clipbottom);

	if (topY >= bottomY)
		return;

	// Find start/end X positions for each line covered by the triangle:

	int leftEdge[MAXHEIGHT];
	int rightEdge[MAXHEIGHT];

	float longDX = sortedVertices[2]->x - sortedVertices[0]->x;
	float longDY = sortedVertices[2]->y - sortedVertices[0]->y;
	float longStep = longDX / longDY;
	float longPos = sortedVertices[0]->x + longStep * (topY + 0.5f - sortedVertices[0]->y) + 0.5f;

	if (topY < midY)
	{
		float shortDX = sortedVertices[1]->x - sortedVertices[0]->x;
		float shortDY = sortedVertices[1]->y - sortedVertices[0]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[0]->x + shortStep * (topY + 0.5f - sortedVertices[0]->y) + 0.5f;

		for (int y = topY; y < midY; y++)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, 0, clipright);
			x1 = clamp(x1, 0, clipright);

			leftEdge[y] = x0;
			rightEdge[y] = x1;

			shortPos += shortStep;
			longPos += longStep;
		}
	}

	if (midY < bottomY)
	{
		float shortDX = sortedVertices[2]->x - sortedVertices[1]->x;
		float shortDY = sortedVertices[2]->y - sortedVertices[1]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[1]->x + shortStep * (midY + 0.5f - sortedVertices[1]->y) + 0.5f;

		for (int y = midY; y < bottomY; y++)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, 0, clipright);
			x1 = clamp(x1, 0, clipright);

			leftEdge[y] = x0;
			rightEdge[y] = x1;

			shortPos += shortStep;
			longPos += longStep;
		}
	}

	// Draw the triangle:

	auto drawfunc = (args->destBgra) ? DrawSpan32 : DrawSpan8;

	float stepXW = args->gradientX.W;
	float v1X = args->v1->x;
	float v1Y = args->v1->y;
	float v1W = args->v1->w;

	int num_cores = thread->num_cores;
	for (int y = topY + thread->skipped_by_thread(topY); y < bottomY; y += num_cores)
	{
		int x = leftEdge[y];
		int xend = rightEdge[y];

		float *zbufferLine = args->zbuffer + args->stencilPitch * 8 * y;

		float startX = x + (0.5f - v1X);
		float startY = y + (0.5f - v1Y);
		float posXW = v1W + stepXW * startX + args->gradientY.W * startY;

#ifndef NO_SSE
		__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
		__m128 mfirstStepXW = _mm_setr_ps(0.0f, stepXW, stepXW + stepXW, stepXW + stepXW + stepXW);
		while (x < xend)
		{
			int xstart = x;

			int xendsse = x + ((xend - x) & ~3);
			__m128 mposXW = _mm_add_ps(_mm_set1_ps(posXW), mfirstStepXW);
			while (_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(zbufferLine + x), mposXW)) == 15 && x < xendsse)
			{
				_mm_storeu_ps(zbufferLine + x, mposXW);
				mposXW = _mm_add_ps(mposXW, mstepXW);
				x += 4;
			}
			posXW = _mm_cvtss_f32(mposXW);

			while (zbufferLine[x] <= posXW && x < xend)
			{
				zbufferLine[x] = posXW;
				posXW += stepXW;
				x++;
			}

			if (x > xstart)
				drawfunc(y, xstart, x, args);

			xendsse = x + ((xend - x) & ~3);
			mposXW = _mm_add_ps(_mm_set1_ps(posXW), mfirstStepXW);
			while (_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(zbufferLine + x), mposXW)) == 0 && x < xendsse)
			{
				mposXW = _mm_add_ps(mposXW, mstepXW);
				x += 4;
			}
			posXW = _mm_cvtss_f32(mposXW);

			while (zbufferLine[x] > posXW && x < xend)
			{
				posXW += stepXW;
				x++;
			}
		}
#else
		while (x < xend)
		{
			int xstart = x;
			while (zbufferLine[x] <= posXW && x < xend)
			{
				zbufferLine[x] = posXW;
				posXW += stepXW;
				x++;
			}

			if (x > xstart)
				drawfunc(y, xstart, x, args);

			while (zbufferLine[x] > posXW && x < xend)
			{
				posXW += stepXW;
				x++;
			}
		}
#endif
	}
}

#ifndef NO_SSE

void ScreenTriangle::DrawSpan32(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	float v1X = args->v1->x;
	float v1Y = args->v1->y;
	float v1W = args->v1->w;
	float v1U = args->v1->u * v1W;
	float v1V = args->v1->v * v1W;
	float stepXW = args->gradientX.W;
	float stepXU = args->gradientX.U;
	float stepXV = args->gradientX.V;
	float startX = x0 + (0.5f - v1X);
	float startY = y + (0.5f - v1Y);
	float posXW = v1W + stepXW * startX + args->gradientY.W * startY;
	float posXU = v1U + stepXU * startX + args->gradientY.U * startY;
	float posXV = v1V + stepXV * startX + args->gradientY.V * startY;

	const uint32_t *texPixels = (const uint32_t*)args->uniforms->TexturePixels();
	int texWidth = args->uniforms->TextureWidth();
	int texHeight = args->uniforms->TextureHeight();

	bool is_fixed_light = args->uniforms->FixedLight();
	uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
	uint32_t light = args->uniforms->Light();
	float shade = 2.0f - (light + 12.0f) / 128.0f;
	float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
	light += light >> 7; // 255 -> 256

	uint32_t *dest = (uint32_t*)args->dest;
	uint32_t *destLine = dest + args->pitch * y;

	int x = x0;
	int sseEnd = x0 + ((x1 - x0) & ~3);
	while (x < sseEnd)
	{
		uint32_t fgcolor[2];
		int32_t lightshade[2];

		float rcpW = 0x01000000 / posXW;
		int32_t u = (int32_t)(posXU * rcpW);
		int32_t v = (int32_t)(posXV * rcpW);
		uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
		uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
		fgcolor[0] = texPixels[texelX * texHeight + texelY];

		fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * posXW), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
		lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);
		lightshade[0] = lightpos >> 8;

		posXW += stepXW;
		posXU += stepXU;
		posXV += stepXV;

		rcpW = 0x01000000 / posXW;
		u = (int32_t)(posXU * rcpW);
		v = (int32_t)(posXV * rcpW);
		texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
		texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
		fgcolor[1] = texPixels[texelX * texHeight + texelY];

		lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * posXW), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
		lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);
		lightshade[1] = lightpos >> 8;

		posXW += stepXW;
		posXU += stepXU;
		posXV += stepXV;

		__m128i mfgcolor = _mm_loadl_epi64((const __m128i*)fgcolor);
		mfgcolor = _mm_unpacklo_epi8(mfgcolor, _mm_setzero_si128());

		__m128i mlightshade = _mm_loadl_epi64((const __m128i*)lightshade);
		mlightshade = _mm_shuffle_epi32(mlightshade, _MM_SHUFFLE(1, 0, 1, 0));
		mlightshade = _mm_packs_epi32(mlightshade, mlightshade);

		__m128i mdestcolor = _mm_srli_epi16(_mm_mullo_epi16(mfgcolor, mlightshade), 8);
		mdestcolor = _mm_packus_epi16(mdestcolor, _mm_setzero_si128());
		mdestcolor = _mm_or_si128(mdestcolor, _mm_set1_epi32(0xff000000));

		_mm_storel_epi64((__m128i*)(destLine + x), mdestcolor);

		x += 2;
	}

	while (x < x1)
	{
		float rcpW = 0x01000000 / posXW;
		int32_t u = (int32_t)(posXU * rcpW);
		int32_t v = (int32_t)(posXV * rcpW);

		uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
		uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
		uint32_t fgcolor = texPixels[texelX * texHeight + texelY];

		uint32_t fgcolor_r = RPART(fgcolor);
		uint32_t fgcolor_g = GPART(fgcolor);
		uint32_t fgcolor_b = BPART(fgcolor);
		uint32_t fgcolor_a = APART(fgcolor);
		if (fgcolor_a > 127)
		{
			fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * posXW), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);
			int lightshade = lightpos >> 8;

			fgcolor_r = (fgcolor_r * lightshade) >> 8;
			fgcolor_g = (fgcolor_g * lightshade) >> 8;
			fgcolor_b = (fgcolor_b * lightshade) >> 8;

			destLine[x] = 0xff000000 | (fgcolor_r << 16) | (fgcolor_g << 8) | fgcolor_b;
		}

		posXW += stepXW;
		posXU += stepXU;
		posXV += stepXV;
		x++;
	}
}

#else

void ScreenTriangle::DrawSpan32(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	float v1X = args->v1->x;
	float v1Y = args->v1->y;
	float v1W = args->v1->w;
	float v1U = args->v1->u * v1W;
	float v1V = args->v1->v * v1W;
	float stepXW = args->gradientX.W;
	float stepXU = args->gradientX.U;
	float stepXV = args->gradientX.V;
	float startX = x0 + (0.5f - v1X);
	float startY = y + (0.5f - v1Y);
	float posXW = v1W + stepXW * startX + args->gradientY.W * startY;
	float posXU = v1U + stepXU * startX + args->gradientY.U * startY;
	float posXV = v1V + stepXV * startX + args->gradientY.V * startY;

	const uint32_t *texPixels = (const uint32_t*)args->uniforms->TexturePixels();
	int texWidth = args->uniforms->TextureWidth();
	int texHeight = args->uniforms->TextureHeight();

	bool is_fixed_light = args->uniforms->FixedLight();
	uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
	uint32_t light = args->uniforms->Light();
	float shade = 2.0f - (light + 12.0f) / 128.0f;
	float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
	light += light >> 7; // 255 -> 256

	uint32_t *dest = (uint32_t*)args->dest;
	uint32_t *destLine = dest + args->pitch * y;

	int x = x0;
	while (x < x1)
	{
		float rcpW = 0x01000000 / posXW;
		int32_t u = (int32_t)(posXU * rcpW);
		int32_t v = (int32_t)(posXV * rcpW);

		uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
		uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
		uint32_t fgcolor = texPixels[texelX * texHeight + texelY];

		uint32_t fgcolor_r = RPART(fgcolor);
		uint32_t fgcolor_g = GPART(fgcolor);
		uint32_t fgcolor_b = BPART(fgcolor);
		uint32_t fgcolor_a = APART(fgcolor);
		if (fgcolor_a > 127)
		{
			fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * posXW), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
			lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);
			int lightshade = lightpos >> 8;

			fgcolor_r = (fgcolor_r * lightshade) >> 8;
			fgcolor_g = (fgcolor_g * lightshade) >> 8;
			fgcolor_b = (fgcolor_b * lightshade) >> 8;

			destLine[x] = 0xff000000 | (fgcolor_r << 16) | (fgcolor_g << 8) | fgcolor_b;
		}

		posXW += stepXW;
		posXU += stepXU;
		posXV += stepXV;
		x++;
	}
}

#endif

void ScreenTriangle::DrawSpan8(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	float v1X = args->v1->x;
	float v1Y = args->v1->y;
	float v1W = args->v1->w;
	float v1U = args->v1->u * v1W;
	float v1V = args->v1->v * v1W;
	float stepXW = args->gradientX.W;
	float stepXU = args->gradientX.U;
	float stepXV = args->gradientX.V;
	float startX = x0 + (0.5f - v1X);
	float startY = y + (0.5f - v1Y);
	float posXW = v1W + stepXW * startX + args->gradientY.W * startY;
	float posXU = v1U + stepXU * startX + args->gradientY.U * startY;
	float posXV = v1V + stepXV * startX + args->gradientY.V * startY;

	auto colormaps = args->uniforms->BaseColormap();

	const uint8_t *texPixels = args->uniforms->TexturePixels();
	int texWidth = args->uniforms->TextureWidth();
	int texHeight = args->uniforms->TextureHeight();

	bool is_fixed_light = args->uniforms->FixedLight();
	uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
	uint32_t light = args->uniforms->Light();
	float shade = 2.0f - (light + 12.0f) / 128.0f;
	float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
	light += light >> 7; // 255 -> 256

	uint8_t *dest = (uint8_t*)args->dest;
	uint8_t *destLine = dest + args->pitch * y;

	int x = x0;
	while (x < x1)
	{
		float rcpW = 0x01000000 / posXW;
		int32_t u = (int32_t)(posXU * rcpW);
		int32_t v = (int32_t)(posXV * rcpW);

		uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
		uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
		uint8_t fgcolor = texPixels[texelX * texHeight + texelY];

		fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * posXW), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
		lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);
		int lightshade = lightpos >> 8;

		lightshade = ((256 - lightshade) * NUMCOLORMAPS) & 0xffffff00;
		uint8_t shadedfg = colormaps[lightshade + fgcolor];

		if (fgcolor != 0)
			destLine[x] = shadedfg;

		posXW += stepXW;
		posXU += stepXU;
		posXV += stepXV;
		x++;
	}
}

void(*ScreenTriangle::TriDrawers8[])(int, int, uint32_t, uint32_t, const TriDrawTriangleArgs *) =
{
	&TriScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureOpaque
	&TriScreenDrawer8<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureMasked
	&TriScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureAdd
	&TriScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureSub
	&TriScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureRevSub
	&TriScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureAddSrcColor
	&TriScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedOpaque
	&TriScreenDrawer8<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedMasked
	&TriScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedAdd
	&TriScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedSub
	&TriScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedRevSub
	&TriScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedAddSrcColor
	&TriScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,          // Shaded
	&TriScreenDrawer8<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,  // AddShaded
	&TriScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute,         // Stencil
	&TriScreenDrawer8<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute, // AddStencil
	&TriScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FillSampler>::Execute,            // FillOpaque
	&TriScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillAdd
	&TriScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillSub
	&TriScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillRevSub
	&TriScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillAddSrcColor
	&TriScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::SkycapSampler>::Execute,          // Skycap
	&TriScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::FuzzSampler>::Execute,            // Fuzz
	&TriScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FogBoundarySampler>::Execute,     // FogBoundary
};

void(*ScreenTriangle::TriDrawers32[])(int, int, uint32_t, uint32_t, const TriDrawTriangleArgs *) =
{
	&TriScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureOpaque
	&TriScreenDrawer32<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureMasked
	&TriScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureAdd
	&TriScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureSub
	&TriScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureRevSub
	&TriScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureAddSrcColor
	&TriScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedOpaque
	&TriScreenDrawer32<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedMasked
	&TriScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedAdd
	&TriScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedSub
	&TriScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedRevSub
	&TriScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedAddSrcColor
	&TriScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,          // Shaded
	&TriScreenDrawer32<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,  // AddShaded
	&TriScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute,         // Stencil
	&TriScreenDrawer32<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute, // AddStencil
	&TriScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FillSampler>::Execute,            // FillOpaque
	&TriScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillAdd
	&TriScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillSub
	&TriScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillRevSub
	&TriScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillAddSrcColor
	&TriScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::SkycapSampler>::Execute,          // Skycap
	&TriScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::FuzzSampler>::Execute,            // Fuzz
	&TriScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FogBoundarySampler>::Execute      // FogBoundary
};

void(*ScreenTriangle::RectDrawers8[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *) =
{
	&RectScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureOpaque
	&RectScreenDrawer8<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureMasked
	&RectScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureAdd
	&RectScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureSub
	&RectScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureRevSub
	&RectScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureAddSrcColor
	&RectScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedOpaque
	&RectScreenDrawer8<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedMasked
	&RectScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedAdd
	&RectScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedSub
	&RectScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedRevSub
	&RectScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedAddSrcColor
	&RectScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,          // Shaded
	&RectScreenDrawer8<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,  // AddShaded
	&RectScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute,         // Stencil
	&RectScreenDrawer8<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute, // AddStencil
	&RectScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FillSampler>::Execute,            // FillOpaque
	&RectScreenDrawer8<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillAdd
	&RectScreenDrawer8<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillSub
	&RectScreenDrawer8<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillRevSub
	&RectScreenDrawer8<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillAddSrcColor
	&RectScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::SkycapSampler>::Execute,          // Skycap
	&RectScreenDrawer8<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::FuzzSampler>::Execute,            // Fuzz
	&RectScreenDrawer8<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FogBoundarySampler>::Execute      // FogBoundary
};

void(*ScreenTriangle::RectDrawers32[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *) =
{
	&RectScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureOpaque
	&RectScreenDrawer32<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TextureSampler>::Execute,         // TextureMasked
	&RectScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureAdd
	&RectScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,       // TextureSub
	&RectScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureRevSub
	&RectScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TextureSampler>::Execute,    // TextureAddSrcColor
	&RectScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedOpaque
	&RectScreenDrawer32<TriScreenDrawerModes::MaskedBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,      // TranslatedMasked
	&RectScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedAdd
	&RectScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute,    // TranslatedSub
	&RectScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedRevSub
	&RectScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::TranslatedSampler>::Execute, // TranslatedAddSrcColor
	&RectScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,          // Shaded
	&RectScreenDrawer32<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::ShadedSampler>::Execute,  // AddShaded
	&RectScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute,         // Stencil
	&RectScreenDrawer32<TriScreenDrawerModes::AddClampShadedBlend, TriScreenDrawerModes::StencilSampler>::Execute, // AddStencil
	&RectScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FillSampler>::Execute,            // FillOpaque
	&RectScreenDrawer32<TriScreenDrawerModes::AddClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillAdd
	&RectScreenDrawer32<TriScreenDrawerModes::SubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,          // FillSub
	&RectScreenDrawer32<TriScreenDrawerModes::RevSubClampBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillRevSub
	&RectScreenDrawer32<TriScreenDrawerModes::AddSrcColorBlend, TriScreenDrawerModes::FillSampler>::Execute,       // FillAddSrcColor
	&RectScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::SkycapSampler>::Execute,          // Skycap
	&RectScreenDrawer32<TriScreenDrawerModes::ShadedBlend, TriScreenDrawerModes::FuzzSampler>::Execute,            // Fuzz
	&RectScreenDrawer32<TriScreenDrawerModes::OpaqueBlend, TriScreenDrawerModes::FogBoundarySampler>::Execute,     // FogBoundary
};

int ScreenTriangle::FuzzStart = 0;
