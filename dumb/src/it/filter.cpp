#if defined(_DEBUG) && defined(_MSC_VER)
#include <crtdbg.h>
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__amd64__)
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <xmmintrin.h>
#define HAVE_SSE_VERSION 1
#endif
#include <math.h>

#include "dumb.h"

extern "C" {
#include "internal/it.h"
}

#ifdef _M_IX86
enum {
	CPU_HAVE_3DNOW		= 1 << 0,
	CPU_HAVE_3DNOW_EX	= 1 << 1,
	CPU_HAVE_SSE		= 1 << 2,
	CPU_HAVE_SSE2		= 1 << 3,
	CPU_HAVE_SSE3		= 1 << 4,
};

static bool query_cpu_feature_set(/*unsigned p_value*/) {
	__try {
		/*if (p_value & (CPU_HAVE_SSE | CPU_HAVE_SSE2 | CPU_HAVE_SSE2))*/ {
			int buffer[4];
			__cpuid(buffer,1);
			/*if (p_value & CPU_HAVE_SSE)*/ {
				if ((buffer[3]&(1<<25)) == 0) return false;
			}
			/*if (p_value & CPU_HAVE_SSE2) {
				if ((buffer[3]&(1<<26)) == 0) return false;
			}
			if (p_value & CPU_HAVE_SSE3) {
				if ((buffer[2]&(1<<0)) == 0) return false;
			}*/
		}

#ifdef _M_IX86
		/*if (p_value & (CPU_HAVE_3DNOW_EX | CPU_HAVE_3DNOW)) {
			int buffer_amd[4];
			__cpuid(buffer_amd,0x80000000);
			if ((unsigned)buffer_amd[0] < 0x80000001) return false;
			__cpuid(buffer_amd,0x80000001);
			
			if (p_value & CPU_HAVE_3DNOW) {
				if ((buffer_amd[3]&(1<<22)) == 0) return false;
			}
			if (p_value & CPU_HAVE_3DNOW_EX) {
				if ((buffer_amd[3]&(1<<30)) == 0) return false;
			}
		}*/
#endif
		return true;
	} __except(1) {
		return false;
	}
}

static const bool g_have_sse = query_cpu_feature_set(/*CPU_HAVE_SSE*/);
#elif defined(_M_X64) || defined(__amd64__)

enum {g_have_sse2 = true, g_have_sse = true, g_have_3dnow = false};

#else

enum {g_have_sse2 = false, g_have_sse = false, g_have_3dnow = false};

#endif

static void it_filter_int(DUMB_CLICK_REMOVER *cr, IT_FILTER_STATE *state, sample_t *dst, int32 pos, sample_t *src, int32 size, int step, int sampfreq, int cutoff, int resonance);
static void it_filter_sse(DUMB_CLICK_REMOVER *cr, IT_FILTER_STATE *state, sample_t *dst, int32 pos, sample_t *src, int32 size, int step, int sampfreq, int cutoff, int resonance);

extern "C" void it_filter(DUMB_CLICK_REMOVER *cr, IT_FILTER_STATE *state, sample_t *dst, int32 pos, sample_t *src, int32 size, int step, int sampfreq, int cutoff, int resonance)
{
	if ( g_have_sse ) it_filter_sse(cr, state, dst, pos, src, size, step, sampfreq, cutoff, resonance);
	else it_filter_int(cr, state, dst, pos, src, size, step, sampfreq, cutoff, resonance);
}

#define LOG10 2.30258509299
#define MULSCA(a, b) ((int)((LONG_LONG)((a) << 4) * (b) >> 32))
#define SCALEB 12

static void it_filter_int(DUMB_CLICK_REMOVER *cr, IT_FILTER_STATE *state, sample_t *dst, int32 pos, sample_t *src, int32 size, int step, int sampfreq, int cutoff, int resonance)
{
	//profiler( filter );

	sample_t currsample = state->currsample;
	sample_t prevsample = state->prevsample;

	float a, b, c;

	int32 datasize;

	{
		float inv_angle = (float)(sampfreq * pow(0.5, 0.25 + cutoff*(1.0/(24<<IT_ENVELOPE_SHIFT))) * (1.0/(2*3.14159265358979323846*110.0)));
		float loss = (float)exp(resonance*(-LOG10*1.2/128.0));
		float d, e;
#if 0
		loss *= 2; // This is the mistake most players seem to make!
#endif

#if 1
		d = (1.0f - loss) / inv_angle;
		if (d > 2.0f) d = 2.0f;
		d = (loss - d) * inv_angle;
		e = inv_angle * inv_angle;
		a = 1.0f / (1.0f + d + e);
		c = -e * a;
		b = 1.0f - a - c;
#else
		a = 1.0f / (inv_angle*inv_angle + inv_angle*loss + loss);
		c = -(inv_angle*inv_angle) * a;
		b = 1.0f - a - c;
#endif
	}

	dst += pos * step;
	datasize = size * step;

#define INT_FILTERS
#ifdef INT_FILTERS
	{
		int ai = (int)(a * (1 << (16+SCALEB)));
		int bi = (int)(b * (1 << (16+SCALEB)));
		int ci = (int)(c * (1 << (16+SCALEB)));
		int i;

		if (cr) {
			sample_t startstep = MULSCA(src[0], ai) + MULSCA(currsample, bi) + MULSCA(prevsample, ci);
			dumb_record_click(cr, pos, startstep);
		}

		for (i = 0; i < datasize; i += step) {
			{
				sample_t newsample = MULSCA(src[i], ai) + MULSCA(currsample, bi) + MULSCA(prevsample, ci);
				prevsample = currsample;
				currsample = newsample;
			}
			dst[i] += currsample;
		}

		if (cr) {
			sample_t endstep = MULSCA(src[datasize], ai) + MULSCA(currsample, bi) + MULSCA(prevsample, ci);
			dumb_record_click(cr, pos + size, -endstep);
		}
	}
#else
	if (cr) {
		float startstep = src[0]*a + currsample*b + prevsample*c;
		dumb_record_click(cr, pos, (sample_t)startstep);
	}

	{
		int i = size % 3;
		while (i > 0) {
			{
				float newsample = *src*a + currsample*b + prevsample*c;
				src += step;
				prevsample = currsample;
				currsample = newsample;
			}
			*dst += (sample_t)currsample;
			dst += step;
			i--;
		}
		i = size / 3;
		while (i > 0) {
			float newsample;
			/* Gotta love unrolled loops! */
			*dst += (sample_t)(newsample = *src*a + currsample*b + prevsample*c);
			src += step; dst += step;
			*dst += (sample_t)(prevsample = *src*a + newsample*b + currsample*c);
			src += step; dst += step;
			*dst += (sample_t)(currsample = *src*a + prevsample*b + newsample*c);
			src += step; dst += step;
			i--;
		}
	}

	if (cr) {
		float endstep = src[datasize]*a + currsample*b + prevsample*c;
		dumb_record_click(cr, pos + size, -(sample_t)endstep);
	}
#endif

	state->currsample = currsample;
	state->prevsample = prevsample;
}

#if HAVE_SSE_VERSION
static void it_filter_sse(DUMB_CLICK_REMOVER *cr, IT_FILTER_STATE *state, sample_t *dst, int32 pos, sample_t *src, int32 size, int step, int sampfreq, int cutoff, int resonance)
{
	__m128 data, impulse;
	__m128 temp1, temp2;

	sample_t currsample = state->currsample;
	sample_t prevsample = state->prevsample;

	float imp[4];

	//profiler( filter_sse ); On ClawHammer Athlon64 3200+, ~12000 cycles, ~500 for that x87 setup code (as opposed to ~25500 for the original integer code)

	int32 datasize;

	{
		float inv_angle = (float)(sampfreq * pow(0.5, 0.25 + cutoff*(1.0/(24<<IT_ENVELOPE_SHIFT))) * (1.0/(2*3.14159265358979323846*110.0)));
		float loss = (float)exp(resonance*(-LOG10*1.2/128.0));
		float d, e;
#if 0
		loss *= 2; // This is the mistake most players seem to make!
#endif

#if 1
		d = (1.0f - loss) / inv_angle;
		if (d > 2.0f) d = 2.0f;
		d = (loss - d) * inv_angle;
		e = inv_angle * inv_angle;
		imp[0] = 1.0f / (1.0f + d + e);
		imp[2] = -e * imp[0];
		imp[1] = 1.0f - imp[0] - imp[2];
#else
		imp[0] = 1.0f / (inv_angle*inv_angle + inv_angle*loss + loss);
		imp[2] = -(inv_angle*inv_angle) * imp[0];
		imp[1] = 1.0f - imp[0] - imp[2];
#endif
		imp[3] = 0;
	}

	dst += pos * step;
	datasize = size * step;

	{
		int ai, bi, ci, i;

		if (cr) {
			sample_t startstep;
			ai = (int)(imp[0] * (1 << (16+SCALEB)));
			bi = (int)(imp[1] * (1 << (16+SCALEB)));
			ci = (int)(imp[2] * (1 << (16+SCALEB)));
			startstep = MULSCA(src[0], ai) + MULSCA(currsample, bi) + MULSCA(prevsample, ci);
			dumb_record_click(cr, pos, startstep);
		}

		data = _mm_cvtsi32_ss( data, prevsample );
		data = _mm_shuffle_ps( data, data, _MM_SHUFFLE(0, 0, 0, 0) );
		data = _mm_cvtsi32_ss( data, currsample );
		impulse = _mm_loadu_ps( (const float *) &imp );
		temp1 = _mm_shuffle_ps( data, data, _MM_SHUFFLE(0, 1, 0, 0) );

		for (i = 0; i < datasize; i += step) {
			data = _mm_cvtsi32_ss( temp1, src [i] );
			temp1 = _mm_mul_ps( data, impulse );
			temp2 = _mm_shuffle_ps( temp1, temp1, _MM_SHUFFLE(0, 0, 3, 2) );
			temp1 = _mm_add_ps( temp1, temp2 );
			temp2 = _mm_shuffle_ps( temp1, temp1, _MM_SHUFFLE(0, 0, 0, 1) );
			temp1 = _mm_add_ps( temp1, temp2 );
			temp1 = _mm_shuffle_ps( temp1, data, _MM_SHUFFLE(0, 1, 0, 0) );
			dst [i] += _mm_cvtss_si32( temp1 );
		}

		currsample = _mm_cvtss_si32( temp1 );
		temp1 = _mm_shuffle_ps( temp1, temp1, _MM_SHUFFLE(0, 0, 0, 2) );
		prevsample = _mm_cvtss_si32( temp1 );

#ifndef _M_X64
		_mm_empty();
#endif

		if (cr) {
			sample_t endstep = MULSCA(src[datasize], ai) + MULSCA(currsample, bi) + MULSCA(prevsample, ci);
			dumb_record_click(cr, pos + size, -endstep);
		}
	}

	state->currsample = currsample;
	state->prevsample = prevsample;
}
#endif
