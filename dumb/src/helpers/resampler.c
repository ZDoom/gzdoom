#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#if (defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__amd64__))
#include <xmmintrin.h>
#define RESAMPLER_SSE
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_CPU_ARM || TARGET_CPU_ARM64
#include <arm_neon.h>
#define RESAMPLER_NEON
#endif
#endif

#ifdef _MSC_VER
#define ALIGNED     _declspec(align(16))
#else
#define ALIGNED     __attribute__((aligned(16)))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "internal/resampler.h"

enum { RESAMPLER_SHIFT = 10 };
enum { RESAMPLER_SHIFT_EXTRA = 8 };
enum { RESAMPLER_RESOLUTION = 1 << RESAMPLER_SHIFT };
enum { RESAMPLER_RESOLUTION_EXTRA = 1 << (RESAMPLER_SHIFT + RESAMPLER_SHIFT_EXTRA) };
enum { SINC_WIDTH = 16 };
enum { SINC_SAMPLES = RESAMPLER_RESOLUTION * SINC_WIDTH };
enum { CUBIC_SAMPLES = RESAMPLER_RESOLUTION * 4 };

typedef union bigint
{
	unsigned long long quad;
#ifndef __BIG_ENDIAN__
	struct { unsigned int lo, hi; };
#else
	struct { unsigned int hi, lo; };
#endif
} bigint;

// What works well on 32-bit can make for extra work on 64-bit
#if defined(_M_X64) || defined(__amd64__) || TARGET_CPU_ARM64
#define CLEAR_HI(p)		(p.quad &= 0xffffffffu)
#define ADD_HI(a,p)		(a += p.quad >> 32)
#define PHASE_REDUCE(p)	(int)(p.quad >> (32 - RESAMPLER_SHIFT))
#else
#define CLEAR_HI(p)		p.hi = 0
#define ADD_HI(a,p)		a += p.hi
// Should be equivalent to (int)(p.quad >> (32 - RESAMPLER_SHIFT)),
// since the high part should get zeroed after every sample.
#define PHASE_REDUCE(p)	(p.lo >> (32 - RESAMPLER_SHIFT))
#endif

static const float RESAMPLER_BLEP_CUTOFF = 0.90f;
static const float RESAMPLER_BLAM_CUTOFF = 0.93f;
static const float RESAMPLER_SINC_CUTOFF = 0.999f;

ALIGNED static float cubic_lut[CUBIC_SAMPLES];

static float sinc_lut[SINC_SAMPLES + 1];
static float window_lut[SINC_SAMPLES + 1];

enum { resampler_buffer_size = SINC_WIDTH * 4 };

static int fEqual(const double b, const double a)
{
    return fabs(a - b) < 1.0e-6;
}

static double sinc(double x)
{
    return fEqual(x, 0.0) ? 1.0 : sin(x * M_PI) / (x * M_PI);
}

#ifdef RESAMPLER_SSE
#ifdef _MSC_VER
#include <intrin.h>
#elif defined(__clang__) || defined(__GNUC__)
static inline void
__cpuid(int *data, int selector)
{
#if defined(__PIC__) && defined(__i386__)
    asm("xchgl %%ebx, %%esi; cpuid; xchgl %%ebx, %%esi"
        : "=a" (data[0]),
        "=S" (data[1]),
        "=c" (data[2]),
        "=d" (data[3])
        : "0" (selector));
#elif defined(__PIC__) && defined(__amd64__)
    asm("xchg{q} {%%}rbx, %q1; cpuid; xchg{q} {%%}rbx, %q1"
        : "=a" (data[0]),
        "=&r" (data[1]),
        "=c" (data[2]),
        "=d" (data[3])
        : "0" (selector));
#else
    asm("cpuid"
        : "=a" (data[0]),
        "=b" (data[1]),
        "=c" (data[2]),
        "=d" (data[3])
        : "0" (selector));
#endif
}
#else
#define __cpuid(a,b) memset((a), 0, sizeof(int) * 4)
#endif

static int query_cpu_feature_sse() {
    int buffer[4];
    __cpuid(buffer,1);
    if ((buffer[3]&(1<<25)) == 0) return 0;
    return 1;
}

static int resampler_has_sse = 0;
#endif

void resampler_init(void)
{
    unsigned i;
    double dx = (float)(SINC_WIDTH) / SINC_SAMPLES, x = 0.0;
    for (i = 0; i < SINC_SAMPLES + 1; ++i, x += dx)
    {
        double y = x / SINC_WIDTH;
#if 0
        // Blackman
        float window = 0.42659 - 0.49656 * cos(M_PI + M_PI * y) + 0.076849 * cos(2.0 * M_PI * y);
#elif 1
        // Nuttal 3 term
        double window = 0.40897 + 0.5 * cos(M_PI * y) + 0.09103 * cos(2.0 * M_PI * y);
#elif 0
        // C.R.Helmrich's 2 term window
        float window = 0.79445 * cos(0.5 * M_PI * y) + 0.20555 * cos(1.5 * M_PI * y);
#elif 0
        // Lanczos
        float window = sinc(y);
#endif
        sinc_lut[i] = (float)(fabs(x) < SINC_WIDTH ? sinc(x) : 0.0);
        window_lut[i] = (float)window;
    }
    dx = 1.0 / RESAMPLER_RESOLUTION;
    x = 0.0;
    for (i = 0; i < RESAMPLER_RESOLUTION; ++i, x += dx)
    {
        cubic_lut[i*4]   = (float)(-0.5 * x * x * x +       x * x - 0.5 * x);
        cubic_lut[i*4+1] = (float)( 1.5 * x * x * x - 2.5 * x * x           + 1.0);
        cubic_lut[i*4+2] = (float)(-1.5 * x * x * x + 2.0 * x * x + 0.5 * x);
        cubic_lut[i*4+3] = (float)( 0.5 * x * x * x - 0.5 * x * x);
    }
#ifdef RESAMPLER_SSE
    resampler_has_sse = query_cpu_feature_sse();
#endif
}

typedef struct resampler
{
    int write_pos, write_filled;
    int read_pos, read_filled;
    bigint phase;
    bigint phase_inc;
    bigint inv_phase;
    bigint inv_phase_inc;
    unsigned char quality;
    signed char delay_added;
    signed char delay_removed;
    double last_amp;
    double accumulator;
    float buffer_in[resampler_buffer_size * 2];
    float buffer_out[resampler_buffer_size + SINC_WIDTH * 2 - 1];
} resampler;

void * resampler_create(void)
{
    resampler * r = ( resampler * ) malloc( sizeof(resampler) );
    if ( !r ) return 0;

    r->write_pos = SINC_WIDTH - 1;
    r->write_filled = 0;
    r->read_pos = 0;
    r->read_filled = 0;
    r->phase.quad = 0;
    r->phase_inc.quad = 0;
    r->inv_phase.quad = 0;
    r->inv_phase_inc.quad = 0;
    r->quality = RESAMPLER_QUALITY_MAX;
    r->delay_added = -1;
    r->delay_removed = -1;
    r->last_amp = 0;
    r->accumulator = 0;
    memset( r->buffer_in, 0, sizeof(r->buffer_in) );
    memset( r->buffer_out, 0, sizeof(r->buffer_out) );

    return r;
}

void resampler_delete(void * _r)
{
    free( _r );
}

void * resampler_dup(const void * _r)
{
    void * r_out = malloc( sizeof(resampler) );
    if ( !r_out ) return 0;

    resampler_dup_inplace(r_out, _r);

    return r_out;
}

void resampler_dup_inplace(void *_d, const void *_s)
{
    const resampler * r_in = ( const resampler * ) _s;
    resampler * r_out = ( resampler * ) _d;

    r_out->write_pos = r_in->write_pos;
    r_out->write_filled = r_in->write_filled;
    r_out->read_pos = r_in->read_pos;
    r_out->read_filled = r_in->read_filled;
    r_out->phase = r_in->phase;
    r_out->phase_inc = r_in->phase_inc;
    r_out->inv_phase = r_in->inv_phase;
    r_out->inv_phase_inc = r_in->inv_phase_inc;
    r_out->quality = r_in->quality;
    r_out->delay_added = r_in->delay_added;
    r_out->delay_removed = r_in->delay_removed;
    r_out->last_amp = r_in->last_amp;
    r_out->accumulator = r_in->accumulator;
    memcpy( r_out->buffer_in, r_in->buffer_in, sizeof(r_in->buffer_in) );
    memcpy( r_out->buffer_out, r_in->buffer_out, sizeof(r_in->buffer_out) );
}

void resampler_set_quality(void *_r, int quality)
{
    resampler * r = ( resampler * ) _r;
    if (quality < RESAMPLER_QUALITY_MIN)
        quality = RESAMPLER_QUALITY_MIN;
    else if (quality > RESAMPLER_QUALITY_MAX)
        quality = RESAMPLER_QUALITY_MAX;
    if ( r->quality != quality )
    {
        if ( quality == RESAMPLER_QUALITY_BLEP || r->quality == RESAMPLER_QUALITY_BLEP ||
             quality == RESAMPLER_QUALITY_BLAM || r->quality == RESAMPLER_QUALITY_BLAM )
        {
            r->read_pos = 0;
            r->read_filled = 0;
            r->last_amp = 0;
            r->accumulator = 0;
            memset( r->buffer_out, 0, sizeof(r->buffer_out) );
        }
        r->delay_added = -1;
        r->delay_removed = -1;
    }
    r->quality = (unsigned char)quality;
}

int resampler_get_free_count(void *_r)
{
    resampler * r = ( resampler * ) _r;
    return resampler_buffer_size - r->write_filled;
}

static int resampler_min_filled(resampler *r)
{
    switch (r->quality)
    {
    default:
    case RESAMPLER_QUALITY_ZOH:
    case RESAMPLER_QUALITY_BLEP:
        return 1;
            
    case RESAMPLER_QUALITY_LINEAR:
    case RESAMPLER_QUALITY_BLAM:
        return 2;
            
    case RESAMPLER_QUALITY_CUBIC:
        return 4;
            
    case RESAMPLER_QUALITY_SINC:
        return SINC_WIDTH * 2;
    }
}

static int resampler_input_delay(resampler *r)
{
    switch (r->quality)
    {
    default:
    case RESAMPLER_QUALITY_ZOH:
    case RESAMPLER_QUALITY_BLEP:
    case RESAMPLER_QUALITY_LINEAR:
    case RESAMPLER_QUALITY_BLAM:
        return 0;
            
    case RESAMPLER_QUALITY_CUBIC:
        return 1;
            
    case RESAMPLER_QUALITY_SINC:
        return SINC_WIDTH - 1;
    }
}

static int resampler_output_delay(resampler *r)
{
    switch (r->quality)
    {
    default:
    case RESAMPLER_QUALITY_ZOH:
    case RESAMPLER_QUALITY_LINEAR:
    case RESAMPLER_QUALITY_CUBIC:
    case RESAMPLER_QUALITY_SINC:
        return 0;
            
    case RESAMPLER_QUALITY_BLEP:
    case RESAMPLER_QUALITY_BLAM:
        return SINC_WIDTH - 1;
    }
}

int resampler_ready(void *_r)
{
    resampler * r = ( resampler * ) _r;
    return r->write_filled > resampler_min_filled(r);
}

void resampler_clear(void *_r)
{
    resampler * r = ( resampler * ) _r;
    r->write_pos = SINC_WIDTH - 1;
    r->write_filled = 0;
    r->read_pos = 0;
    r->read_filled = 0;
    r->phase.quad = 0;
    r->delay_added = -1;
    r->delay_removed = -1;
    memset(r->buffer_in, 0, (SINC_WIDTH - 1) * sizeof(r->buffer_in[0]));
    memset(r->buffer_in + resampler_buffer_size, 0, (SINC_WIDTH - 1) * sizeof(r->buffer_in[0]));
    if (r->quality == RESAMPLER_QUALITY_BLEP || r->quality == RESAMPLER_QUALITY_BLAM)
    {
        r->inv_phase.quad = 0;
        r->last_amp = 0;
        r->accumulator = 0;
        memset(r->buffer_out, 0, sizeof(r->buffer_out));
    }
}

void resampler_set_rate(void *_r, double new_factor)
{
    resampler * r = ( resampler * ) _r;
    r->phase_inc.quad = (long long)(new_factor * 0x100000000ll);
    new_factor = 1.0 / new_factor;
    r->inv_phase_inc.quad = (long long)(new_factor * 0x100000000ll);
}

void resampler_write_sample(void *_r, short s)
{
    resampler * r = ( resampler * ) _r;

    if ( r->delay_added < 0 )
    {
        r->delay_added = 0;
        r->write_filled = resampler_input_delay( r );
    }
    
    if ( r->write_filled < resampler_buffer_size )
    {
        float s32 = s;
        s32 *= 256.0;

        r->buffer_in[ r->write_pos ] = s32;
        r->buffer_in[ r->write_pos + resampler_buffer_size ] = s32;

        ++r->write_filled;

        r->write_pos = ( r->write_pos + 1 ) % resampler_buffer_size;
    }
}

void resampler_write_sample_fixed(void *_r, int s, unsigned char depth)
{
    resampler * r = ( resampler * ) _r;
    
    if ( r->delay_added < 0 )
    {
        r->delay_added = 0;
        r->write_filled = resampler_input_delay( r );
    }
    
    if ( r->write_filled < resampler_buffer_size )
    {
        double s32 = s;
        s32 /= (double)(1 << (depth - 1));
        
        r->buffer_in[ r->write_pos ] = (float)s32;
        r->buffer_in[ r->write_pos + resampler_buffer_size ] = (float)s32;
        
        ++r->write_filled;
        
        r->write_pos = ( r->write_pos + 1 ) % resampler_buffer_size;
    }
}

static int resampler_run_zoh(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 1;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        do
        {
            if ( out >= out_end )
                break;

            *out++ = *in;
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}

#ifndef RESAMPLER_NEON
static int resampler_run_blep(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 1;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        double last_amp = r->last_amp;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;

        const int step = (int)(RESAMPLER_BLEP_CUTOFF * RESAMPLER_RESOLUTION);
        const int window_step = RESAMPLER_RESOLUTION;
        
        do
        {
            double sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;
            
            sample = *in++ - last_amp;
            
            if (sample)
            {
                double kernel[SINC_WIDTH * 2], kernel_sum = 0.0f;
                int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernel[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
                for (i = 0; i < SINC_WIDTH * 2; ++i)
                    out[i] += (float)(sample * kernel[i]);
            }
            
            inv_phase.quad += inv_phase_inc.quad;
            
            ADD_HI(out, inv_phase);
            
            CLEAR_HI(inv_phase);
        }
        while ( in < in_end );
        
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_SSE
static int resampler_run_blep_sse(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 1;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        double last_amp = r->last_amp;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;

        const int step = (int)(RESAMPLER_BLEP_CUTOFF * RESAMPLER_RESOLUTION);
        const int window_step = RESAMPLER_RESOLUTION;
        
        do
        {
            double sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;
            
            sample = *in++ - last_amp;
            
            if (sample)
            {
                float kernel_sum = 0.0f;
                __m128 kernel[SINC_WIDTH / 2];
                __m128 temp1, temp2;
                __m128 samplex;
                float *kernelf = (float*)(&kernel);
                int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
				samplex = _mm_set1_ps( (float)sample );
                for (i = 0; i < SINC_WIDTH / 2; ++i)
                {
                    temp1 = _mm_load_ps( (const float *)( kernel + i ) );
                    temp1 = _mm_mul_ps( temp1, samplex );
                    temp2 = _mm_loadu_ps( (const float *) out + i * 4 );
                    temp1 = _mm_add_ps( temp1, temp2 );
                    _mm_storeu_ps( (float *) out + i * 4, temp1 );
                }
            }
            
            inv_phase.quad += inv_phase_inc.quad;
            
            ADD_HI(out, inv_phase);
            
            CLEAR_HI(inv_phase);
        }
        while ( in < in_end );
        
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_NEON
static int resampler_run_blep(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 1;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        float last_amp = r->last_amp;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;

        const int step = RESAMPLER_BLEP_CUTOFF * RESAMPLER_RESOLUTION;
        const int window_step = RESAMPLER_RESOLUTION;
        
        do
        {
            float sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;
            
            sample = *in++ - last_amp;
            
            if (sample)
            {
                float kernel_sum = 0.0f;
                float32x4_t kernel[SINC_WIDTH / 2];
                float32x4_t temp1, temp2;
                float32x4_t samplex;
                float *kernelf = (float*)(&kernel);
                int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
                samplex = vdupq_n_f32(sample);
                for (i = 0; i < SINC_WIDTH / 2; ++i)
                {
                    temp1 = vld1q_f32( (const float32_t *)( kernel + i ) );
                    temp2 = vld1q_f32( (const float32_t *) out + i * 4 );
                    temp2 = vmlaq_f32( temp2, temp1, samplex );
                    vst1q_f32( (float32_t *) out + i * 4, temp2 );
                }
            }
            
            inv_phase.quad += inv_phase_inc.quad;
            
            ADD_HI(out, inv_phase);
            
            CLEAR_HI(inv_phase);
        }
        while ( in < in_end );
        
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

static int resampler_run_linear(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;

        do
        {
            if ( out >= out_end )
                break;
         
            *out++ = (float)(in[0] + (in[1] - in[0]) * phase.lo * (1.f / 0x100000000ll));
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}

#ifndef RESAMPLER_NEON
static int resampler_run_blam(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        double last_amp = r->last_amp;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;

        const int step = (int)(RESAMPLER_BLAM_CUTOFF * RESAMPLER_RESOLUTION);
        const int window_step = RESAMPLER_RESOLUTION;

        do
        {
            double sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;
            
            sample = in[0];
            if (phase_inc.quad < 0x100000000ll)
                sample += (in[1] - in[0]) * phase.quad * (1.f / 0x100000000ll);
            sample -= last_amp;
            
            if (sample)
            {
                double kernel[SINC_WIDTH * 2], kernel_sum = 0.0f;
                int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernel[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
                for (i = 0; i < SINC_WIDTH * 2; ++i)
                    out[i] += (float)(sample * kernel[i]);
            }
            
            if (inv_phase_inc.quad < 0x100000000ll)
            {
                ++in;
                inv_phase.quad += inv_phase_inc.quad;
                ADD_HI(out, inv_phase);
                CLEAR_HI(inv_phase);
            }
            else
            {
                phase.quad += phase_inc.quad;
                ++out;
                ADD_HI(in, phase);
                CLEAR_HI(phase);
            }
        }
        while ( in < in_end );
        
        r->phase = phase;
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_SSE
static int resampler_run_blam_sse(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        double last_amp = r->last_amp;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;

        const int step = (int)(RESAMPLER_BLAM_CUTOFF * RESAMPLER_RESOLUTION);
        const int window_step = RESAMPLER_RESOLUTION;

        do
        {
            double sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;

            sample = in[0];
            if (phase_inc.quad < 0x100000000ll)
            {
                sample += (in[1] - in[0]) * phase.quad * (1.f / 0x100000000ll);
            }
            sample -= last_amp;
            
            if (sample)
            {
                float kernel_sum = 0.0f;
                __m128 kernel[SINC_WIDTH / 2];
                __m128 temp1, temp2;
                __m128 samplex;
                float *kernelf = (float*)(&kernel);
				int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
                samplex = _mm_set1_ps( (float)sample );
                for (i = 0; i < SINC_WIDTH / 2; ++i)
                {
                    temp1 = _mm_load_ps( (const float *)( kernel + i ) );
                    temp1 = _mm_mul_ps( temp1, samplex );
                    temp2 = _mm_loadu_ps( (const float *) out + i * 4 );
                    temp1 = _mm_add_ps( temp1, temp2 );
                    _mm_storeu_ps( (float *) out + i * 4, temp1 );
                }
            }
            
            if (inv_phase_inc.quad < 0x100000000ll)
            {
                ++in;
                inv_phase.quad += inv_phase_inc.quad;
                ADD_HI(out, inv_phase);
                CLEAR_HI(inv_phase);
            }
            else
            {
                phase.quad += phase_inc.quad;
                ++out;
                
                if (phase.quad >= 0x100000000ll)
                {
                    ++in;
                    CLEAR_HI(phase);
                }
            }
        }
        while ( in < in_end );

        r->phase = phase;
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_NEON
static int resampler_run_blam(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        float last_amp = r->last_amp;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        bigint inv_phase = r->inv_phase;
        bigint inv_phase_inc = r->inv_phase_inc;
        
        const int step = RESAMPLER_BLAM_CUTOFF * RESAMPLER_RESOLUTION;
        const int window_step = RESAMPLER_RESOLUTION;

        do
        {
            float sample;
            
            if ( out + SINC_WIDTH * 2 > out_end )
                break;
            
            sample = in[0];
            if (phase_inc.quad < 0x100000000ll)
                sample += (in[1] - in[0]) * phase;
            sample -= last_amp;
            
            if (sample)
            {
                float kernel_sum = 0.0;
                float32x4_t kernel[SINC_WIDTH / 2];
                float32x4_t temp1, temp2;
                float32x4_t samplex;
                float *kernelf = (float*)(&kernel);
                int phase_reduced = PHASE_REDUCE(inv_phase);
                int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
                int i = SINC_WIDTH;

                for (; i >= -SINC_WIDTH + 1; --i)
                {
                    int pos = i * step;
                    int window_pos = i * window_step;
                    kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
                }
                last_amp += sample;
                sample /= kernel_sum;
                samplex = vdupq_n_f32(sample);
                for (i = 0; i < SINC_WIDTH / 2; ++i)
                {
                    temp1 = vld1q_f32( (const float32_t *)( kernel + i ) );
                    temp2 = vld1q_f32( (const float32_t *) out + i * 4 );
                    temp2 = vmlaq_f32( temp2, temp1, samplex );
                    vst1q_f32( (float32_t *) out + i * 4, temp2 );
                }
            }

            if (inv_phase_inc.quad < 0x100000000ll)
            {
                ++in;
                inv_phase.quad += inv_phase_inc.quad;
                ADD_HI(out, inv_phase);
                CLEAR_HI(inv_phase);
            }
            else
            {
                phase.quad += phase_inc.quad;
                ++out;
                
                if (phase.quad >= 0x100000000ll)
                {
                    ++in;
                    CLEAR_HI(phase);
                }
            }
        }
        while ( in < in_end );
        
        r->phase = phase;
        r->inv_phase = inv_phase;
        r->last_amp = last_amp;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifndef RESAMPLER_NEON
static int resampler_run_cubic(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 4;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;

        do
        {
            float * kernel;
            int i;
            float sample;
            
            if ( out >= out_end )
                break;
            
            kernel = cubic_lut + PHASE_REDUCE(phase) * 4;
            
            for (sample = 0, i = 0; i < 4; ++i)
                sample += in[i] * kernel[i];
            *out++ = sample;
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_SSE
static int resampler_run_cubic_sse(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 4;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;

        do
        {
            __m128 temp1, temp2;
            __m128 samplex = _mm_setzero_ps();
            
            if ( out >= out_end )
                break;
            
            temp1 = _mm_loadu_ps( (const float *)( in ) );
            temp2 = _mm_load_ps( (const float *)( cubic_lut + PHASE_REDUCE(phase) * 4 ) );
            temp1 = _mm_mul_ps( temp1, temp2 );
            samplex = _mm_add_ps( samplex, temp1 );
            temp1 = _mm_movehl_ps( temp1, samplex );
            samplex = _mm_add_ps( samplex, temp1 );
            temp1 = samplex;
            temp1 = _mm_shuffle_ps( temp1, samplex, _MM_SHUFFLE(0, 0, 0, 1) );
            samplex = _mm_add_ps( samplex, temp1 );
            _mm_store_ss( out, samplex );
            ++out;
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_NEON
static int resampler_run_cubic(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= 4;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        
        do
        {
            float32x4_t temp1, temp2;
            float32x2_t half;
            
            if ( out >= out_end )
                break;
            
            temp1 = vld1q_f32( (const float32_t *)( in ) );
            temp2 = vld1q_f32( (const float32_t *)( cubic_lut + PHASE_REDUCE(phase) * 4 ) );
            temp1 = vmulq_f32( temp1, temp2 );
            half = vadd_f32(vget_high_f32(temp1), vget_low_f32(temp1));
            *out++ = vget_lane_f32(vpadd_f32(half, half), 0);
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase)
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifndef RESAMPLER_NEON
static int resampler_run_sinc(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= SINC_WIDTH * 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;

        int step = phase_inc.quad > 0x100000000ll ?
			(int)(RESAMPLER_RESOLUTION / (phase_inc.quad * (1.f / 0x100000000ll)) * RESAMPLER_SINC_CUTOFF) :
			(int)(RESAMPLER_RESOLUTION * RESAMPLER_SINC_CUTOFF);
        int window_step = RESAMPLER_RESOLUTION;

        do
        {
            double kernel[SINC_WIDTH * 2], kernel_sum = 0.0;
            int i = SINC_WIDTH;
            int phase_reduced = PHASE_REDUCE(phase);
            int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
            float sample;

            if ( out >= out_end )
                break;

            for (; i >= -SINC_WIDTH + 1; --i)
            {
                int pos = i * step;
                int window_pos = i * window_step;
                kernel_sum += kernel[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
            }
            for (sample = 0, i = 0; i < SINC_WIDTH * 2; ++i)
                sample += (float)(in[i] * kernel[i]);
            *out++ = (float)(sample / kernel_sum);

            phase.quad += phase_inc.quad;

            ADD_HI(in, phase);

            CLEAR_HI(phase);
        }
        while ( in < in_end );

        r->phase = phase;
        *out_ = out;

        used = (int)(in - in_);

        r->write_filled -= used;
    }

    return used;
}
#endif

#ifdef RESAMPLER_SSE
static int resampler_run_sinc_sse(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= SINC_WIDTH * 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;

        int step = phase_inc.quad > 0x100000000ll ?
			(int)(RESAMPLER_RESOLUTION / (phase_inc.quad * (1.f / 0x100000000ll)) * RESAMPLER_SINC_CUTOFF) :
			(int)(RESAMPLER_RESOLUTION * RESAMPLER_SINC_CUTOFF);
        int window_step = RESAMPLER_RESOLUTION;
        
        do
        {
            // accumulate in extended precision
            float kernel_sum = 0.0;
            __m128 kernel[SINC_WIDTH / 2];
            __m128 temp1, temp2;
            __m128 samplex = _mm_setzero_ps();
            float *kernelf = (float*)(&kernel);
            int i = SINC_WIDTH;
            int phase_reduced = PHASE_REDUCE(phase);
            int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
            
            if ( out >= out_end )
                break;
            
            for (; i >= -SINC_WIDTH + 1; --i)
            {
                int pos = i * step;
                int window_pos = i * window_step;
                kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
            }
            for (i = 0; i < SINC_WIDTH / 2; ++i)
            {
                temp1 = _mm_loadu_ps( (const float *)( in + i * 4 ) );
                temp2 = _mm_load_ps( (const float *)( kernel + i ) );
                temp1 = _mm_mul_ps( temp1, temp2 );
                samplex = _mm_add_ps( samplex, temp1 );
            }
            kernel_sum = 1.0f / kernel_sum;
            temp1 = _mm_movehl_ps( temp1, samplex );
            samplex = _mm_add_ps( samplex, temp1 );
            temp1 = samplex;
            temp1 = _mm_shuffle_ps( temp1, samplex, _MM_SHUFFLE(0, 0, 0, 1) );
            samplex = _mm_add_ps( samplex, temp1 );
            temp1 = _mm_set_ss( kernel_sum );
            samplex = _mm_mul_ps( samplex, temp1 );
            _mm_store_ss( out, samplex );
            ++out;
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

#ifdef RESAMPLER_NEON
static int resampler_run_sinc(resampler * r, float ** out_, float * out_end)
{
    int in_size = r->write_filled;
    float const* in_ = r->buffer_in + resampler_buffer_size + r->write_pos - r->write_filled;
    int used = 0;
    in_size -= SINC_WIDTH * 2;
    if ( in_size > 0 )
    {
        float* out = *out_;
        float const* in = in_;
        float const* const in_end = in + in_size;
        bigint phase = r->phase;
        bigint phase_inc = r->phase_inc;
        
        int step = phase_inc.quad > 0x100000000ll ?
			(int)(RESAMPLER_RESOLUTION / (phase_inc.quad * (1.f / 0x100000000ll)) * RESAMPLER_SINC_CUTOFF) :
			(int)(RESAMPLER_RESOLUTION * RESAMPLER_SINC_CUTOFF);
        int window_step = RESAMPLER_RESOLUTION;
        
        do
        {
            // accumulate in extended precision
            float kernel_sum = 0.0;
            float32x4_t kernel[SINC_WIDTH / 2];
            float32x4_t temp1, temp2;
            float32x4_t samplex = {0};
            float32x2_t half;
            float *kernelf = (float*)(&kernel);
            int i = SINC_WIDTH;
            int phase_reduced = PHASE_REDUCE(phase);
            int phase_adj = phase_reduced * step / RESAMPLER_RESOLUTION;
            
            if ( out >= out_end )
                break;
            
            for (; i >= -SINC_WIDTH + 1; --i)
            {
                int pos = i * step;
                int window_pos = i * window_step;
                kernel_sum += kernelf[i + SINC_WIDTH - 1] = sinc_lut[abs(phase_adj - pos)] * window_lut[abs(phase_reduced - window_pos)];
            }
            for (i = 0; i < SINC_WIDTH / 2; ++i)
            {
                temp1 = vld1q_f32( (const float32_t *)( in + i * 4 ) );
                temp2 = vld1q_f32( (const float32_t *)( kernel + i ) );
                samplex = vmlaq_f32( samplex, temp1, temp2 );
            }
            kernel_sum = 1.0 / kernel_sum;
            samplex = vmulq_f32(samplex, vmovq_n_f32(kernel_sum));
            half = vadd_f32(vget_high_f32(samplex), vget_low_f32(samplex));
            *out++ = vget_lane_f32(vpadd_f32(half, half), 0);
            
            phase.quad += phase_inc.quad;
            
            ADD_HI(in, phase);
            
            CLEAR_HI(phase);
        }
        while ( in < in_end );
        
        r->phase = phase;
        *out_ = out;
        
        used = (int)(in - in_);
        
        r->write_filled -= used;
    }
    
    return used;
}
#endif

static void resampler_fill(resampler * r)
{
    int min_filled = resampler_min_filled(r);
    int quality = r->quality;
    while ( r->write_filled > min_filled &&
            r->read_filled < resampler_buffer_size )
    {
        int write_pos = ( r->read_pos + r->read_filled ) % resampler_buffer_size;
        int write_size = resampler_buffer_size - write_pos;
        float * out = r->buffer_out + write_pos;
        if ( write_size > ( resampler_buffer_size - r->read_filled ) )
            write_size = resampler_buffer_size - r->read_filled;
        switch (quality)
        {
        case RESAMPLER_QUALITY_ZOH:
            resampler_run_zoh( r, &out, out + write_size );
            break;
                
        case RESAMPLER_QUALITY_BLEP:
        {
            int used;
            int write_extra = 0;
            if ( write_pos >= r->read_pos )
                write_extra = r->read_pos;
            if ( write_extra > SINC_WIDTH * 2 - 1 )
                write_extra = SINC_WIDTH * 2 - 1;
            memcpy( r->buffer_out + resampler_buffer_size, r->buffer_out, write_extra * sizeof(r->buffer_out[0]) );
#ifdef RESAMPLER_SSE
            if ( resampler_has_sse )
                used = resampler_run_blep_sse( r, &out, out + write_size + write_extra );
            else
#endif
                used = resampler_run_blep( r, &out, out + write_size + write_extra );
            memcpy( r->buffer_out, r->buffer_out + resampler_buffer_size, write_extra * sizeof(r->buffer_out[0]) );
            if (!used)
                return;
            break;
        }
                
        case RESAMPLER_QUALITY_LINEAR:
            resampler_run_linear( r, &out, out + write_size );
            break;
                
        case RESAMPLER_QUALITY_BLAM:
        {
            float * out_ = out;
            int write_extra = 0;
            if ( write_pos >= r->read_pos )
                write_extra = r->read_pos;
            if ( write_extra > SINC_WIDTH * 2 - 1 )
                write_extra = SINC_WIDTH * 2 - 1;
            memcpy( r->buffer_out + resampler_buffer_size, r->buffer_out, write_extra * sizeof(r->buffer_out[0]) );
#ifdef RESAMPLER_SSE
            if ( resampler_has_sse )
                resampler_run_blam_sse( r, &out, out + write_size + write_extra );
            else
#endif
                resampler_run_blam( r, &out, out + write_size + write_extra );
            memcpy( r->buffer_out, r->buffer_out + resampler_buffer_size, write_extra * sizeof(r->buffer_out[0]) );
            if ( out == out_ )
                return;
            break;
        }

        case RESAMPLER_QUALITY_CUBIC:
#ifdef RESAMPLER_SSE
            if ( resampler_has_sse )
                resampler_run_cubic_sse( r, &out, out + write_size );
            else
#endif
                resampler_run_cubic( r, &out, out + write_size );
            break;
                
        case RESAMPLER_QUALITY_SINC:
#ifdef RESAMPLER_SSE
            if ( resampler_has_sse )
                resampler_run_sinc_sse( r, &out, out + write_size );
            else
#endif
                resampler_run_sinc( r, &out, out + write_size );
            break;
        }
        r->read_filled += (int)(out - r->buffer_out - write_pos);
    }
}

static void resampler_fill_and_remove_delay(resampler * r)
{
    resampler_fill( r );
    if ( r->delay_removed < 0 )
    {
        int delay = resampler_output_delay( r );
        r->delay_removed = 0;
        while ( delay-- )
            resampler_remove_sample( r, 1 );
    }
}

int resampler_get_sample_count(void *_r)
{
    resampler * r = ( resampler * ) _r;
    if ( r->read_filled < 1 && ((r->quality != RESAMPLER_QUALITY_BLEP && r->quality != RESAMPLER_QUALITY_BLAM) || r->inv_phase_inc.quad))
        resampler_fill_and_remove_delay( r );
    return r->read_filled;
}

int resampler_get_sample(void *_r)
{
    resampler * r = ( resampler * ) _r;
    if ( r->read_filled < 1 && r->phase_inc.quad)
        resampler_fill_and_remove_delay( r );
    if ( r->read_filled < 1 )
        return 0;
    if ( r->quality == RESAMPLER_QUALITY_BLEP || r->quality == RESAMPLER_QUALITY_BLAM )
        return (int)(r->buffer_out[ r->read_pos ] + r->accumulator);
    else
        return (int)r->buffer_out[ r->read_pos ];
}

float resampler_get_sample_float(void *_r)
{
    resampler * r = ( resampler * ) _r;
    if ( r->read_filled < 1 && r->phase_inc.quad)
        resampler_fill_and_remove_delay( r );
    if ( r->read_filled < 1 )
        return 0;
    if ( r->quality == RESAMPLER_QUALITY_BLEP || r->quality == RESAMPLER_QUALITY_BLAM )
        return (float)(r->buffer_out[ r->read_pos ] + r->accumulator);
    else
        return r->buffer_out[ r->read_pos ];
}

void resampler_remove_sample(void *_r, int decay)
{
    resampler * r = ( resampler * ) _r;
    if ( r->read_filled > 0 )
    {
        if ( r->quality == RESAMPLER_QUALITY_BLEP || r->quality == RESAMPLER_QUALITY_BLAM )
        {
            r->accumulator += r->buffer_out[ r->read_pos ];
            r->buffer_out[ r->read_pos ] = 0;
            if (decay)
            {
                r->accumulator -= r->accumulator * (1.0f / 8192.0f);
                if (fabs(r->accumulator) < 1e-20f)
                    r->accumulator = 0;
            }
        }
        --r->read_filled;
        r->read_pos = ( r->read_pos + 1 ) % resampler_buffer_size;
    }
}
