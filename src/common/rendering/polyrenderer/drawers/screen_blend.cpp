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

#include "screen_blend.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

static const int shiftTable[] = {
	0, 0, 0, 0, // STYLEALPHA_Zero
	0, 0, 0, 0, // STYLEALPHA_One
	24, 24, 24, 24, // STYLEALPHA_Src
	24, 24, 24, 24, // STYLEALPHA_InvSrc
	24, 16, 8, 0, // STYLEALPHA_SrcCol
	24, 16, 8, 0, // STYLEALPHA_InvSrcCol
	24, 16, 8, 0, // STYLEALPHA_DstCol
	24, 16, 8, 0 // STYLEALPHA_InvDstCol
};

#if 1 //#ifndef USE_AVX2
template<typename OptT>
void BlendColor(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	FRenderStyle style = thread->RenderStyle;

	bool invsrc = style.SrcAlpha & 1;
	bool invdst = style.DestAlpha & 1;

	const int* shiftsrc = shiftTable + (style.SrcAlpha << 2);
	const int* shiftdst = shiftTable + (style.DestAlpha << 2);

	uint32_t* dest = (uint32_t*)thread->dest;
	uint32_t* line = dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int srcSelect = style.SrcAlpha <= STYLEALPHA_One ? 0 : (style.SrcAlpha >= STYLEALPHA_DstCol ? 1 : 2);
	int dstSelect = style.DestAlpha <= STYLEALPHA_One ? 0 : (style.DestAlpha >= STYLEALPHA_DstCol ? 1 : 2);

	uint32_t inputs[3];
	inputs[0] = 0;

	for (int x = x0; x < x1; x++)
	{
		inputs[1] = line[x];
		inputs[2] = fragcolor[x];

		uint32_t srcinput = inputs[srcSelect];
		uint32_t dstinput = inputs[dstSelect];

		uint32_t out[4];
		for (int i = 0; i < 4; i++)
		{
			// Grab component for scale factors
			int32_t src = (srcinput >> shiftsrc[i]) & 0xff;
			int32_t dst = (dstinput >> shiftdst[i]) & 0xff;

			// Inverse if needed
			if (invsrc) src = 0xff - src;
			if (invdst) dst = 0xff - dst;

			// Rescale 0-255 to 0-256
			src = src + (src >> 7);
			dst = dst + (dst >> 7);

			// Multiply with input
			src = src * ((inputs[2] >> (24 - (i << 3))) & 0xff);
			dst = dst * ((inputs[1] >> (24 - (i << 3))) & 0xff);

			// Apply blend operator
			int32_t val;
			if (OptT::Flags & SWBLEND_Sub)
			{
				val = src - dst;
			}
			else if (OptT::Flags & SWBLEND_RevSub)
			{
				val = dst - src;
			}
			else
			{
				val = src + dst;
			}
			out[i] = clamp((val + 127) >> 8, 0, 255);
		}

		line[x] = MAKEARGB(out[0], out[1], out[2], out[3]);
	}
}
#else
template<typename OptT>
void BlendColor(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	FRenderStyle style = thread->RenderStyle;

	bool invsrc = style.SrcAlpha & 1;
	bool invdst = style.DestAlpha & 1;

	__m128i shiftsrc = _mm_loadu_si128((const __m128i*)(shiftTable + (style.SrcAlpha << 2)));
	__m128i shiftdst = _mm_loadu_si128((const __m128i*)(shiftTable + (style.DestAlpha << 2)));

	uint32_t* dest = (uint32_t*)thread->dest;
	uint32_t* line = dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int srcSelect = style.SrcAlpha <= STYLEALPHA_One ? 0 : (style.SrcAlpha >= STYLEALPHA_DstCol ? 1 : 2);
	int dstSelect = style.DestAlpha <= STYLEALPHA_One ? 0 : (style.DestAlpha >= STYLEALPHA_DstCol ? 1 : 2);

	uint32_t inputs[3];
	inputs[0] = 0;

	__m128i shiftmul = _mm_set_epi32(24, 16, 8, 0);

	for (int x = x0; x < x1; x++)
	{
		inputs[1] = line[x];
		inputs[2] = fragcolor[x];

		__m128i srcinput = _mm_set1_epi32(inputs[srcSelect]);
		__m128i dstinput = _mm_set1_epi32(inputs[dstSelect]);

		// Grab component for scale factors
		__m128i src = _mm_and_si128(_mm_srlv_epi32(srcinput, shiftsrc), _mm_set1_epi32(0xff));
		__m128i dst = _mm_and_si128(_mm_srlv_epi32(dstinput, shiftdst), _mm_set1_epi32(0xff));

		// Inverse if needed
		if (invsrc) src = _mm_sub_epi32(_mm_set1_epi32(0xff), src);
		if (invdst) dst = _mm_sub_epi32(_mm_set1_epi32(0xff), dst);

		// Rescale 0-255 to 0-256
		src = _mm_add_epi32(src, _mm_srli_epi32(src, 7));
		dst = _mm_add_epi32(dst, _mm_srli_epi32(dst, 7));

		// Multiply with input
		__m128i mulsrc = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(inputs[2]), shiftmul), _mm_set1_epi32(0xff));
		__m128i muldst = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(inputs[1]), shiftmul), _mm_set1_epi32(0xff));
		__m128i mulresult = _mm_mullo_epi16(_mm_packs_epi32(src, dst), _mm_packs_epi32(mulsrc, muldst));
		src = _mm_unpacklo_epi16(mulresult, _mm_setzero_si128());
		dst = _mm_unpackhi_epi16(mulresult, _mm_setzero_si128());

		// Apply blend operator
		__m128i val;
		if (OptT::Flags & SWBLEND_Sub)
		{
			val = _mm_sub_epi32(src, dst);
		}
		else if (OptT::Flags & SWBLEND_RevSub)
		{
			val = _mm_sub_epi32(dst, src);
		}
		else
		{
			val = _mm_add_epi32(src, dst);
		}

		__m128i out = _mm_srli_epi32(_mm_add_epi32(val, _mm_set1_epi32(127)), 8);
		out = _mm_packs_epi32(out, out);
		out = _mm_packus_epi16(out, out);
		line[x] = _mm_cvtsi128_si32(out);
	}
}
#endif

#ifdef NO_SSE
void BlendColorOpaque(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* dest = (uint32_t*)thread->dest;
	uint32_t* line = dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	memcpy(line + x0, fragcolor + x0, (x1 - x0) * sizeof(uint32_t));
}
#else
void BlendColorOpaque(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* dest = (uint32_t*)thread->dest;
	uint32_t* line = dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int ssecount = ((x1 - x0) & ~3);
	int sseend = x0 + ssecount;

	for (int x = x0; x < sseend; x += 4)
	{
		__m128i v = _mm_loadu_si128((__m128i*) & fragcolor[x]);
		_mm_storeu_si128((__m128i*) & line[x], v);
	}

	for (int x = sseend; x < x1; x++)
	{
		line[x] = fragcolor[x];
	}
}
#endif

void BlendColorAdd_Src_InvSrc(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)&line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)&fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));
		__m128i dstscale = _mm_sub_epi16(_mm_set1_epi16(256), srcscale);

		__m128i out = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_mullo_epi16(dst, dstscale)), _mm_set1_epi16(127)), 8);
		_mm_storel_epi64((__m128i*)&line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale = APART(src);
		srcscale += srcscale >> 7;
		uint32_t dstscale = 256 - srcscale;

		uint32_t a = ((APART(src) * srcscale + APART(dst) * dstscale) + 127) >> 8;
		uint32_t r = ((RPART(src) * srcscale + RPART(dst) * dstscale) + 127) >> 8;
		uint32_t g = ((GPART(src) * srcscale + GPART(dst) * dstscale) + 127) >> 8;
		uint32_t b = ((BPART(src) * srcscale + BPART(dst) * dstscale) + 127) >> 8;

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorAdd_SrcCol_InvSrcCol(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = src;
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));
		__m128i dstscale = _mm_sub_epi16(_mm_set1_epi16(256), srcscale);

		__m128i out = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_mullo_epi16(dst, dstscale)), _mm_set1_epi16(127)), 8);
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale_a = APART(src);
		uint32_t srcscale_r = RPART(src);
		uint32_t srcscale_g = GPART(src);
		uint32_t srcscale_b = BPART(src);
		srcscale_a += srcscale_a >> 7;
		srcscale_r += srcscale_r >> 7;
		srcscale_g += srcscale_g >> 7;
		srcscale_b += srcscale_b >> 7;
		uint32_t dstscale_a = 256 - srcscale_a;
		uint32_t dstscale_r = 256 - srcscale_r;
		uint32_t dstscale_g = 256 - srcscale_g;
		uint32_t dstscale_b = 256 - srcscale_b;

		uint32_t a = ((APART(src) * srcscale_a + APART(dst) * dstscale_a) + 127) >> 8;
		uint32_t r = ((RPART(src) * srcscale_r + RPART(dst) * dstscale_r) + 127) >> 8;
		uint32_t g = ((GPART(src) * srcscale_g + GPART(dst) * dstscale_g) + 127) >> 8;
		uint32_t b = ((BPART(src) * srcscale_b + BPART(dst) * dstscale_b) + 127) >> 8;

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorAdd_Src_One(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));

		__m128i out = _mm_add_epi16(_mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_set1_epi16(127)), 8), dst);
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale = APART(src);
		srcscale += srcscale >> 7;

		uint32_t a = min<int32_t>((((APART(src) * srcscale) + 127) >> 8) + APART(dst), 255);
		uint32_t r = min<int32_t>((((RPART(src) * srcscale) + 127) >> 8) + RPART(dst), 255);
		uint32_t g = min<int32_t>((((GPART(src) * srcscale) + 127) >> 8) + GPART(dst), 255);
		uint32_t b = min<int32_t>((((BPART(src) * srcscale) + 127) >> 8) + BPART(dst), 255);

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorAdd_SrcCol_One(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = src;
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));

		__m128i out = _mm_add_epi16(_mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_set1_epi16(127)), 8), dst);
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale_a = APART(src);
		uint32_t srcscale_r = RPART(src);
		uint32_t srcscale_g = GPART(src);
		uint32_t srcscale_b = BPART(src);
		srcscale_a += srcscale_a >> 7;
		srcscale_r += srcscale_r >> 7;
		srcscale_g += srcscale_g >> 7;
		srcscale_b += srcscale_b >> 7;

		uint32_t a = min<int32_t>((((APART(src) * srcscale_a) + 127) >> 8) + APART(dst), 255);
		uint32_t r = min<int32_t>((((RPART(src) * srcscale_r) + 127) >> 8) + RPART(dst), 255);
		uint32_t g = min<int32_t>((((GPART(src) * srcscale_g) + 127) >> 8) + GPART(dst), 255);
		uint32_t b = min<int32_t>((((BPART(src) * srcscale_b) + 127) >> 8) + BPART(dst), 255);

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorAdd_DstCol_Zero(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = dst;
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));

		__m128i out = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_set1_epi16(127)), 8);
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale_a = APART(dst);
		uint32_t srcscale_r = RPART(dst);
		uint32_t srcscale_g = GPART(dst);
		uint32_t srcscale_b = BPART(dst);
		srcscale_a += srcscale_a >> 7;
		srcscale_r += srcscale_r >> 7;
		srcscale_g += srcscale_g >> 7;
		srcscale_b += srcscale_b >> 7;

		uint32_t a = (((APART(src) * srcscale_a) + 127) >> 8);
		uint32_t r = (((RPART(src) * srcscale_r) + 127) >> 8);
		uint32_t g = (((GPART(src) * srcscale_g) + 127) >> 8);
		uint32_t b = (((BPART(src) * srcscale_b) + 127) >> 8);

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorAdd_InvDstCol_Zero(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = _mm_sub_epi16(_mm_set1_epi16(255), dst);
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));

		__m128i out = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_set1_epi16(127)), 8);
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale_a = 255 - APART(dst);
		uint32_t srcscale_r = 255 - RPART(dst);
		uint32_t srcscale_g = 255 - GPART(dst);
		uint32_t srcscale_b = 255 - BPART(dst);
		srcscale_a += srcscale_a >> 7;
		srcscale_r += srcscale_r >> 7;
		srcscale_g += srcscale_g >> 7;
		srcscale_b += srcscale_b >> 7;

		uint32_t a = (((APART(src) * srcscale_a) + 127) >> 8);
		uint32_t r = (((RPART(src) * srcscale_r) + 127) >> 8);
		uint32_t g = (((GPART(src) * srcscale_g) + 127) >> 8);
		uint32_t b = (((BPART(src) * srcscale_b) + 127) >> 8);

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorRevSub_Src_One(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;
	uint32_t* fragcolor = thread->scanline.FragColor;

	int sseend = x0;

#ifndef NO_SSE
	int ssecount = ((x1 - x0) & ~1);
	sseend = x0 + ssecount;
	for (int x = x0; x < sseend; x += 2)
	{
		__m128i dst = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & line[x]), _mm_setzero_si128());
		__m128i src = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*) & fragcolor[x]), _mm_setzero_si128());

		__m128i srcscale = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		srcscale = _mm_add_epi16(srcscale, _mm_srli_epi16(srcscale, 7));

		__m128i out = _mm_sub_epi16(dst, _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(src, srcscale), _mm_set1_epi16(127)), 8));
		_mm_storel_epi64((__m128i*) & line[x], _mm_packus_epi16(out, out));
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];
		uint32_t src = fragcolor[x];

		uint32_t srcscale = APART(src);
		srcscale += srcscale >> 7;

		uint32_t a = max<int32_t>(APART(dst) - (((APART(src) * srcscale) + 127) >> 8), 0);
		uint32_t r = max<int32_t>(RPART(dst) - (((RPART(src) * srcscale) + 127) >> 8), 0);
		uint32_t g = max<int32_t>(GPART(dst) - (((GPART(src) * srcscale) + 127) >> 8), 0);
		uint32_t b = max<int32_t>(BPART(dst) - (((BPART(src) * srcscale) + 127) >> 8), 0);

		line[x] = MAKEARGB(a, r, g, b);
	}
}

void BlendColorColormap(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* line = (uint32_t*)thread->dest + y * (ptrdiff_t)thread->dest_pitch;

	// [GEC] I leave the default floating values.
	float startR = thread->mainVertexShader.Data.uObjectColor.r;
	float startG = thread->mainVertexShader.Data.uObjectColor.g;
	float startB = thread->mainVertexShader.Data.uObjectColor.b;
	float rangeR = thread->mainVertexShader.Data.uAddColor.r - startR;
	float rangeG = thread->mainVertexShader.Data.uAddColor.g - startG;
	float rangeB = thread->mainVertexShader.Data.uAddColor.b - startB;

	int sseend = x0;
	for (int x = sseend; x < x1; x++)
	{
		uint32_t dst = line[x];

		uint32_t a = APART(dst);
		uint32_t r = RPART(dst);
		uint32_t g = GPART(dst);
		uint32_t b = BPART(dst);

		uint32_t gray = (r * 77 + g * 143 + b * 37) >> 8;
		gray += (gray >> 7); // gray*=256/255

		// [GEC] I use the same method as in shaders using floating values.
		// This avoids errors in the invulneravility colormap in Doom and Heretic.
		float fgray = (float)(gray / 255.f);
		float fr = (startR + (fgray * rangeR)) * 2;
		float fg = (startG + (fgray * rangeG)) * 2;
		float fb = (startB + (fgray * rangeB)) * 2;

		fr = clamp<float>(fr, 0.0f, 1.0f);
		fg = clamp<float>(fg, 0.0f, 1.0f);
		fb = clamp<float>(fb, 0.0f, 1.0f);

		r = (uint32_t)(fr * 255.f);
		g = (uint32_t)(fg * 255.f);
		b = (uint32_t)(fb * 255.f);

		line[x] = MAKEARGB(a, (uint8_t)r, (uint8_t)g, (uint8_t)b);
	}
}

void SelectWriteColorFunc(PolyTriangleThreadData* thread)
{
	FRenderStyle style = thread->RenderStyle;
	if (thread->ColormapShader)
	{
		thread->WriteColorFunc = &BlendColorColormap;
	}
	else if (style.BlendOp == STYLEOP_Add)
	{
		if (style.SrcAlpha == STYLEALPHA_One && style.DestAlpha == STYLEALPHA_Zero)
		{
			thread->WriteColorFunc = &BlendColorOpaque;
		}
		else if (style.SrcAlpha == STYLEALPHA_Src && style.DestAlpha == STYLEALPHA_InvSrc)
		{
			thread->WriteColorFunc = &BlendColorAdd_Src_InvSrc;
		}
		else if (style.SrcAlpha == STYLEALPHA_SrcCol && style.DestAlpha == STYLEALPHA_InvSrcCol)
		{
			thread->WriteColorFunc = &BlendColorAdd_SrcCol_InvSrcCol;
		}
		else if (style.SrcAlpha == STYLEALPHA_Src && style.DestAlpha == STYLEALPHA_One)
		{
			thread->WriteColorFunc = &BlendColorAdd_Src_One;
		}
		else if (style.SrcAlpha == STYLEALPHA_SrcCol && style.DestAlpha == STYLEALPHA_One)
		{
			thread->WriteColorFunc = &BlendColorAdd_SrcCol_One;
		}
		else if (style.SrcAlpha == STYLEALPHA_DstCol && style.DestAlpha == STYLEALPHA_Zero)
		{
			thread->WriteColorFunc = &BlendColorAdd_DstCol_Zero;
		}
		else if (style.SrcAlpha == STYLEALPHA_InvDstCol && style.DestAlpha == STYLEALPHA_Zero)
		{
			thread->WriteColorFunc = &BlendColorAdd_InvDstCol_Zero;
		}
		else
		{
			thread->WriteColorFunc = &BlendColor<BlendColorOpt_Add>;
		}
	}
	else if (style.BlendOp == STYLEOP_Sub)
	{
		thread->WriteColorFunc = &BlendColor<BlendColorOpt_Sub>;
	}
	else // if (style.BlendOp == STYLEOP_RevSub)
	{
		if (style.SrcAlpha == STYLEALPHA_Src && style.DestAlpha == STYLEALPHA_One)
		{
			thread->WriteColorFunc = &BlendColorRevSub_Src_One;
		}
		else
		{
			thread->WriteColorFunc = &BlendColor<BlendColorOpt_RevSub>;
		}
	}
}
