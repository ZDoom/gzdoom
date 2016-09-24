// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAW_RGBA__
#define __R_DRAW_RGBA__

#include "r_draw.h"
#include "v_palette.h"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#ifndef NO_SSE
#include <immintrin.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Drawer functions:

void rt_initcols_rgba(BYTE *buffer);
void rt_span_coverage_rgba(int x, int start, int stop);

void rt_copy1col_rgba(int hx, int sx, int yl, int yh);
void rt_copy4cols_rgba(int sx, int yl, int yh);
void rt_shaded1col_rgba(int hx, int sx, int yl, int yh);
void rt_shaded4cols_rgba(int sx, int yl, int yh);
void rt_map1col_rgba(int hx, int sx, int yl, int yh);
void rt_add1col_rgba(int hx, int sx, int yl, int yh);
void rt_addclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_subclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_revsubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlate1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlateadd1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlateaddclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlatesubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_tlaterevsubclamp1col_rgba(int hx, int sx, int yl, int yh);
void rt_map4cols_rgba(int sx, int yl, int yh);
void rt_add4cols_rgba(int sx, int yl, int yh);
void rt_addclamp4cols_rgba(int sx, int yl, int yh);
void rt_subclamp4cols_rgba(int sx, int yl, int yh);
void rt_revsubclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlate4cols_rgba(int sx, int yl, int yh);
void rt_tlateadd4cols_rgba(int sx, int yl, int yh);
void rt_tlateaddclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlatesubclamp4cols_rgba(int sx, int yl, int yh);
void rt_tlaterevsubclamp4cols_rgba(int sx, int yl, int yh);

void R_DrawColumnHoriz_rgba();
void R_DrawColumn_rgba();
void R_DrawFuzzColumn_rgba();
void R_DrawTranslatedColumn_rgba();
void R_DrawShadedColumn_rgba();

void R_FillColumn_rgba();
void R_FillAddColumn_rgba();
void R_FillAddClampColumn_rgba();
void R_FillSubClampColumn_rgba();
void R_FillRevSubClampColumn_rgba();
void R_DrawAddColumn_rgba();
void R_DrawTlatedAddColumn_rgba();
void R_DrawAddClampColumn_rgba();
void R_DrawAddClampTranslatedColumn_rgba();
void R_DrawSubClampColumn_rgba();
void R_DrawSubClampTranslatedColumn_rgba();
void R_DrawRevSubClampColumn_rgba();
void R_DrawRevSubClampTranslatedColumn_rgba();

void R_DrawSpan_rgba(void);
void R_DrawSpanMasked_rgba(void);
void R_DrawSpanTranslucent_rgba();
void R_DrawSpanMaskedTranslucent_rgba();
void R_DrawSpanAddClamp_rgba();
void R_DrawSpanMaskedAddClamp_rgba();
void R_FillSpan_rgba();

void R_DrawTiltedSpan_rgba(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);
void R_DrawColoredSpan_rgba(int y, int x1, int x2);

void R_SetupDrawSlab_rgba(FSWColormap *base_colormap, float light, int shade);
void R_DrawSlab_rgba(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p);

void R_DrawFogBoundary_rgba(int x1, int x2, short *uclip, short *dclip);

DWORD vlinec1_rgba();
void vlinec4_rgba();
DWORD mvlinec1_rgba();
void mvlinec4_rgba();
fixed_t tmvline1_add_rgba();
void tmvline4_add_rgba();
fixed_t tmvline1_addclamp_rgba();
void tmvline4_addclamp_rgba();
fixed_t tmvline1_subclamp_rgba();
void tmvline4_subclamp_rgba();
fixed_t tmvline1_revsubclamp_rgba();
void tmvline4_revsubclamp_rgba();

void R_FillColumnHoriz_rgba();
void R_FillSpan_rgba();

/////////////////////////////////////////////////////////////////////////////
// Multithreaded rendering infrastructure:

// Redirect drawer commands to worker threads
void R_BeginDrawerCommands();

// Wait until all drawers finished executing
void R_EndDrawerCommands();

struct FSpecialColormap;
class DrawerCommandQueue;

// Worker data for each thread executing drawer commands
class DrawerThread
{
public:
	std::thread thread;

	// Thread line index of this thread
	int core = 0;

	// Number of active threads
	int num_cores = 1;

	// Range of rows processed this pass
	int pass_start_y = 0;
	int pass_end_y = MAXHEIGHT;

	uint32_t dc_temp_rgbabuff_rgba[MAXHEIGHT * 4];
	uint32_t *dc_temp_rgba;

	// Checks if a line is rendered by this thread
	bool line_skipped_by_thread(int line)
	{
		return line < pass_start_y || line >= pass_end_y || line % num_cores != core;
	}

	// The number of lines to skip to reach the first line to be rendered by this thread
	int skipped_by_thread(int first_line)
	{
		int pass_skip = MAX(pass_start_y - first_line, 0);
		int core_skip = (num_cores - (first_line + pass_skip - core) % num_cores) % num_cores;
		return pass_skip + core_skip;
	}

	// The number of lines to be rendered by this thread
	int count_for_thread(int first_line, int count)
	{
		int lines_until_pass_end = MAX(pass_end_y - first_line, 0);
		count = MIN(count, lines_until_pass_end);
		int c = (count - skipped_by_thread(first_line) + num_cores - 1) / num_cores;
		return MAX(c, 0);
	}

	// Calculate the dest address for the first line to be rendered by this thread
	uint32_t *dest_for_thread(int first_line, int pitch, uint32_t *dest)
	{
		return dest + skipped_by_thread(first_line) * pitch;
	}
};

// Task to be executed by each worker thread
class DrawerCommand
{
protected:
	int _dest_y;

public:
	DrawerCommand()
	{
		_dest_y = static_cast<int>((dc_dest - dc_destorg) / (dc_pitch * 4));
	}

	virtual void Execute(DrawerThread *thread) = 0;
};

EXTERN_CVAR(Bool, r_multithreaded)
EXTERN_CVAR(Bool, r_mipmap)

// Manages queueing up commands and executing them on worker threads
class DrawerCommandQueue
{
	enum { memorypool_size = 16 * 1024 * 1024 };
	char memorypool[memorypool_size];
	size_t memorypool_pos = 0;

	std::vector<DrawerCommand *> commands;

	std::vector<DrawerThread> threads;

	std::mutex start_mutex;
	std::condition_variable start_condition;
	std::vector<DrawerCommand *> active_commands;
	bool shutdown_flag = false;
	int run_id = 0;

	std::mutex end_mutex;
	std::condition_variable end_condition;
	size_t finished_threads = 0;

	int threaded_render = 0;
	DrawerThread single_core_thread;
	int num_passes = 1;
	int rows_in_pass = MAXHEIGHT;

	void StartThreads();
	void StopThreads();
	void Finish();

	static DrawerCommandQueue *Instance();

	DrawerCommandQueue();
	~DrawerCommandQueue();

public:
	// Allocate memory valid for the duration of a command execution
	static void* AllocMemory(size_t size);

	// Queue command to be executed by drawer worker threads
	template<typename T, typename... Types>
	static void QueueCommand(Types &&... args)
	{
		auto queue = Instance();
		if (queue->threaded_render == 0 || !r_multithreaded)
		{
			T command(std::forward<Types>(args)...);
			command.Execute(&queue->single_core_thread);
		}
		else
		{
			void *ptr = AllocMemory(sizeof(T));
			if (!ptr) // Out of memory - render what we got
			{
				queue->Finish();
				ptr = AllocMemory(sizeof(T));
				if (!ptr)
					return;
			}
			T *command = new (ptr)T(std::forward<Types>(args)...);
			queue->commands.push_back(command);
		}
	}

	// Redirects all drawing commands to worker threads until End is called
	// Begin/End blocks can be nested.
	static void Begin();

	// End redirection and wait until all worker threads finished executing
	static void End();

	// Waits until all worker threads finished executing
	static void WaitForWorkers();
};

/////////////////////////////////////////////////////////////////////////////
// Drawer commands:

class ApplySpecialColormapRGBACommand : public DrawerCommand
{
	BYTE *buffer;
	int pitch;
	int width;
	int height;
	int start_red;
	int start_green;
	int start_blue;
	int end_red;
	int end_green;
	int end_blue;

public:
	ApplySpecialColormapRGBACommand(FSpecialColormap *colormap, DFrameBuffer *screen);
	void Execute(DrawerThread *thread) override;
};

template<typename CommandType, typename BlendMode>
class DrawerBlendCommand : public CommandType
{
public:
	void Execute(DrawerThread *thread) override
	{
		typename CommandType::LoopIterator loop(this, thread);
		if (!loop) return;
		BlendMode blend(*this, loop);
		do
		{
			blend.Blend(*this, loop);
		} while (loop.next());
	}
};

/////////////////////////////////////////////////////////////////////////////
// Pixel shading inline functions:

// Give the compiler a strong hint we want these functions inlined:
#ifndef FORCEINLINE
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __attribute__((always_inline)) inline
#else
#define FORCEINLINE inline
#endif
#endif

// Promise compiler we have no aliasing of this pointer
#ifndef RESTRICT
#if defined(_MSC_VER)
#define RESTRICT __restrict
#elif defined(__GNUC__)
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif
#endif

class LightBgra
{
public:
	// calculates the light constant passed to the shade_pal_index function
	FORCEINLINE static uint32_t calc_light_multiplier(dsfixed_t light)
	{
		return 256 - (light >> (FRACBITS - 8));
	}

	// Calculates a ARGB8 color for the given palette index and light multiplier
	FORCEINLINE static uint32_t shade_pal_index_simple(uint32_t index, uint32_t light)
	{
		const PalEntry &color = GPalette.BaseColors[index];
		uint32_t red = color.r;
		uint32_t green = color.g;
		uint32_t blue = color.b;

		red = red * light / 256;
		green = green * light / 256;
		blue = blue * light / 256;

		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	// Calculates a ARGB8 color for the given palette index, light multiplier and dynamic colormap
	FORCEINLINE static uint32_t shade_pal_index(uint32_t index, uint32_t light, const ShadeConstants &constants)
	{
		const PalEntry &color = GPalette.BaseColors[index];
		uint32_t alpha = color.d & 0xff000000;
		uint32_t red = color.r;
		uint32_t green = color.g;
		uint32_t blue = color.b;
		if (constants.simple_shade)
		{
			red = red * light / 256;
			green = green * light / 256;
			blue = blue * light / 256;
		}
		else
		{
			uint32_t inv_light = 256 - light;
			uint32_t inv_desaturate = 256 - constants.desaturate;

			uint32_t intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

			red = (red * inv_desaturate + intensity) / 256;
			green = (green * inv_desaturate + intensity) / 256;
			blue = (blue * inv_desaturate + intensity) / 256;

			red = (constants.fade_red * inv_light + red * light) / 256;
			green = (constants.fade_green * inv_light + green * light) / 256;
			blue = (constants.fade_blue * inv_light + blue * light) / 256;

			red = (red * constants.light_red) / 256;
			green = (green * constants.light_green) / 256;
			blue = (blue * constants.light_blue) / 256;
		}
		return alpha | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t shade_bgra_simple(uint32_t color, uint32_t light)
	{
		uint32_t red = RPART(color) * light / 256;
		uint32_t green = GPART(color) * light / 256;
		uint32_t blue = BPART(color) * light / 256;
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t shade_bgra(uint32_t color, uint32_t light, const ShadeConstants &constants)
	{
		uint32_t alpha = color & 0xff000000;
		uint32_t red = (color >> 16) & 0xff;
		uint32_t green = (color >> 8) & 0xff;
		uint32_t blue = color & 0xff;
		if (constants.simple_shade)
		{
			red = red * light / 256;
			green = green * light / 256;
			blue = blue * light / 256;
		}
		else
		{
			uint32_t inv_light = 256 - light;
			uint32_t inv_desaturate = 256 - constants.desaturate;

			uint32_t intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

			red = (red * inv_desaturate + intensity) / 256;
			green = (green * inv_desaturate + intensity) / 256;
			blue = (blue * inv_desaturate + intensity) / 256;

			red = (constants.fade_red * inv_light + red * light) / 256;
			green = (constants.fade_green * inv_light + green * light) / 256;
			blue = (constants.fade_blue * inv_light + blue * light) / 256;

			red = (red * constants.light_red) / 256;
			green = (green * constants.light_green) / 256;
			blue = (blue * constants.light_blue) / 256;
		}
		return alpha | (red << 16) | (green << 8) | blue;
	}
};

class BlendBgra
{
public:
	FORCEINLINE static uint32_t copy(uint32_t fg)
	{
		return fg;
	}

	FORCEINLINE static uint32_t add(uint32_t fg, uint32_t bg, uint32_t srcalpha, uint32_t destalpha)
	{
		uint32_t red = MIN<uint32_t>((RPART(fg) * srcalpha + RPART(bg) * destalpha) >> 8, 255);
		uint32_t green = MIN<uint32_t>((GPART(fg) * srcalpha + GPART(bg) * destalpha) >> 8, 255);
		uint32_t blue = MIN<uint32_t>((BPART(fg) * srcalpha + BPART(bg) * destalpha) >> 8, 255);
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t sub(uint32_t fg, uint32_t bg, uint32_t srcalpha, uint32_t destalpha)
	{
		uint32_t red = clamp<uint32_t>((0x10000 - RPART(fg) * srcalpha + RPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		uint32_t green = clamp<uint32_t>((0x10000 - GPART(fg) * srcalpha + GPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		uint32_t blue = clamp<uint32_t>((0x10000 - BPART(fg) * srcalpha + BPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t revsub(uint32_t fg, uint32_t bg, uint32_t srcalpha, uint32_t destalpha)
	{
		uint32_t red = clamp<uint32_t>((0x10000 + RPART(fg) * srcalpha - RPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		uint32_t green = clamp<uint32_t>((0x10000 + GPART(fg) * srcalpha - GPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		uint32_t blue = clamp<uint32_t>((0x10000 + BPART(fg) * srcalpha - BPART(bg) * destalpha) >> 8, 256, 256 + 255) - 256;
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t alpha_blend(uint32_t fg, uint32_t bg)
	{
		uint32_t alpha = APART(fg) + (APART(fg) >> 7); // 255 -> 256
		uint32_t inv_alpha = 256 - alpha;
		uint32_t red = MIN<uint32_t>(RPART(fg) * alpha + (RPART(bg) * inv_alpha) / 256, 255);
		uint32_t green = MIN<uint32_t>(GPART(fg) * alpha + (GPART(bg) * inv_alpha) / 256, 255);
		uint32_t blue = MIN<uint32_t>(BPART(fg) * alpha + (BPART(bg) * inv_alpha) / 256, 255);
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}
};

class SampleBgra
{
public:
	inline static bool span_sampler_setup(const uint32_t * RESTRICT &source, int &xbits, int &ybits, fixed_t xstep, fixed_t ystep, bool mipmapped)
	{
		// Is this a magfilter or minfilter?
		fixed_t xmagnitude = abs(xstep) >> (32 - xbits - FRACBITS);
		fixed_t ymagnitude = abs(ystep) >> (32 - ybits - FRACBITS);
		fixed_t magnitude = (xmagnitude + ymagnitude) * 2 + (1 << (FRACBITS - 1));
		bool magnifying = (magnitude >> FRACBITS == 0);

		if (r_mipmap && mipmapped)
		{
			int level = magnitude >> (FRACBITS + 1);
			while (level != 0)
			{
				if (xbits <= 2 || ybits <= 2)
					break;

				source += (1 << (xbits)) * (1 << (ybits));
				xbits -= 1;
				ybits -= 1;
				level >>= 1;
			}
		}

		return (magnifying && r_magfilter) || (!magnifying && r_minfilter);
	}

	FORCEINLINE static uint32_t sample_bilinear(const uint32_t *col0, const uint32_t *col1, uint32_t texturefracx, uint32_t texturefracy, uint32_t one, uint32_t height)
	{
		uint32_t frac_y0 = (texturefracy >> FRACBITS) * height;
		uint32_t frac_y1 = ((texturefracy + one) >> FRACBITS) * height;
		uint32_t y0 = frac_y0 >> FRACBITS;
		uint32_t y1 = frac_y1 >> FRACBITS;

		uint32_t p00 = col0[y0];
		uint32_t p01 = col0[y1];
		uint32_t p10 = col1[y0];
		uint32_t p11 = col1[y1];

		uint32_t inv_b = texturefracx;
		uint32_t inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
		uint32_t a = 16 - inv_a;
		uint32_t b = 16 - inv_b;

		uint32_t red = (RPART(p00) * a * b + RPART(p01) * inv_a * b + RPART(p10) * a * inv_b + RPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t green = (GPART(p00) * a * b + GPART(p01) * inv_a * b + GPART(p10) * a * inv_b + GPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t blue = (BPART(p00) * a * b + BPART(p01) * inv_a * b + BPART(p10) * a * inv_b + BPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t alpha = (APART(p00) * a * b + APART(p01) * inv_a * b + APART(p10) * a * inv_b + APART(p11) * inv_a * inv_b + 127) >> 8;

		return (alpha << 24) | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t sample_bilinear(const uint32_t *texture, dsfixed_t xfrac, dsfixed_t yfrac, int xbits, int ybits)
	{
		int xshift = (32 - xbits);
		int yshift = (32 - ybits);
		int xmask = (1 << xshift) - 1;
		int ymask = (1 << yshift) - 1;
		uint32_t x = xfrac >> xbits;
		uint32_t y = yfrac >> ybits;

		uint32_t p00 = texture[(y & ymask) + ((x & xmask) << yshift)];
		uint32_t p01 = texture[((y + 1) & ymask) + ((x & xmask) << yshift)];
		uint32_t p10 = texture[(y & ymask) + (((x + 1) & xmask) << yshift)];
		uint32_t p11 = texture[((y + 1) & ymask) + (((x + 1) & xmask) << yshift)];

		uint32_t inv_b = (xfrac >> (xbits - 4)) & 15;
		uint32_t inv_a = (yfrac >> (ybits - 4)) & 15;
		uint32_t a = 16 - inv_a;
		uint32_t b = 16 - inv_b;

		uint32_t red = (RPART(p00) * a * b + RPART(p01) * inv_a * b + RPART(p10) * a * inv_b + RPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t green = (GPART(p00) * a * b + GPART(p01) * inv_a * b + GPART(p10) * a * inv_b + GPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t blue = (BPART(p00) * a * b + BPART(p01) * inv_a * b + BPART(p10) * a * inv_b + BPART(p11) * inv_a * inv_b + 127) >> 8;
		uint32_t alpha = (APART(p00) * a * b + APART(p01) * inv_a * b + APART(p10) * a * inv_b + APART(p11) * inv_a * inv_b + 127) >> 8;

		return (alpha << 24) | (red << 16) | (green << 8) | blue;
	}

#ifndef NO_SSE
	static __m128i samplertable[256 * 2];
#endif
};

/////////////////////////////////////////////////////////////////////////////
// SSE/AVX shading macros:

#define AVX2_SAMPLE_BILINEAR4_COLUMN_INIT(col0, col1, one, height, texturefracx) \
	const uint32_t *baseptr = col0[0]; \
	__m128i coloffsets0 = _mm_setr_epi32(col0[0] - baseptr, col0[1] - baseptr, col0[2] - baseptr, col0[3] - baseptr); \
	__m128i coloffsets1 = _mm_setr_epi32(col1[0] - baseptr, col1[1] - baseptr, col1[2] - baseptr, col1[3] - baseptr); \
	__m128i mone = _mm_loadu_si128((const __m128i*)one); \
	__m128i m127 = _mm_set1_epi16(127); \
	__m128i m16 = _mm_set1_epi32(16); \
	__m128i m15 = _mm_set1_epi32(15); \
	__m128i mheight = _mm_loadu_si128((const __m128i*)height); \
	__m128i mtexturefracx = _mm_loadu_si128((const __m128i*)texturefracx);

#define AVX2_SAMPLE_BILINEAR4_COLUMN(fg, texturefracy) { \
	__m128i mtexturefracy = _mm_loadu_si128((const __m128i*)texturefracy); \
	__m128i multmp0 = _mm_srli_epi32(mtexturefracy, FRACBITS); \
	__m128i multmp1 = _mm_srli_epi32(_mm_add_epi32(mtexturefracy, mone), FRACBITS); \
	__m128i frac_y0 = _mm_or_si128(_mm_mul_epu32(multmp0, mheight), _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(multmp0, 4), _mm_srli_si128(mheight, 4)), 4)); \
	__m128i frac_y1 = _mm_or_si128(_mm_mul_epu32(multmp1, mheight), _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(multmp1, 4), _mm_srli_si128(mheight, 4)), 4)); \
	__m128i y0 = _mm_srli_epi32(frac_y0, FRACBITS); \
	__m128i y1 = _mm_srli_epi32(frac_y1, FRACBITS); \
	__m128i inv_b = mtexturefracx; \
	__m128i inv_a = _mm_and_si128(_mm_srli_epi32(frac_y1, FRACBITS - 4), m15); \
	__m128i a = _mm_sub_epi32(m16, inv_a); \
	__m128i b = _mm_sub_epi32(m16, inv_b); \
	__m128i ab = _mm_mullo_epi16(a, b); \
	__m128i invab = _mm_mullo_epi16(inv_a, b); \
	__m128i ainvb = _mm_mullo_epi16(a, inv_b); \
	__m128i invainvb = _mm_mullo_epi16(inv_a, inv_b); \
	__m128i ab_lo = _mm_shuffle_epi32(ab, _MM_SHUFFLE(1, 1, 0, 0)); \
	__m128i ab_hi = _mm_shuffle_epi32(ab, _MM_SHUFFLE(3, 3, 2, 2)); \
	__m128i invab_lo = _mm_shuffle_epi32(invab, _MM_SHUFFLE(1, 1, 0, 0)); \
	__m128i invab_hi = _mm_shuffle_epi32(invab, _MM_SHUFFLE(3, 3, 2, 2)); \
	__m128i ainvb_lo = _mm_shuffle_epi32(ainvb, _MM_SHUFFLE(1, 1, 0, 0)); \
	__m128i ainvb_hi = _mm_shuffle_epi32(ainvb, _MM_SHUFFLE(3, 3, 2, 2)); \
	__m128i invainvb_lo = _mm_shuffle_epi32(invainvb, _MM_SHUFFLE(1, 1, 0, 0)); \
	__m128i invainvb_hi = _mm_shuffle_epi32(invainvb, _MM_SHUFFLE(3, 3, 2, 2)); \
	ab_lo = _mm_or_si128(ab_lo, _mm_slli_epi32(ab_lo, 16)); \
	ab_hi = _mm_or_si128(ab_hi, _mm_slli_epi32(ab_hi, 16)); \
	invab_lo = _mm_or_si128(invab_lo, _mm_slli_epi32(invab_lo, 16)); \
	invab_hi = _mm_or_si128(invab_hi, _mm_slli_epi32(invab_hi, 16)); \
	ainvb_lo = _mm_or_si128(ainvb_lo, _mm_slli_epi32(ainvb_lo, 16)); \
	ainvb_hi = _mm_or_si128(ainvb_hi, _mm_slli_epi32(ainvb_hi, 16)); \
	invainvb_lo = _mm_or_si128(invainvb_lo, _mm_slli_epi32(invainvb_lo, 16)); \
	invainvb_hi = _mm_or_si128(invainvb_hi, _mm_slli_epi32(invainvb_hi, 16)); \
	__m128i p00 = _mm_i32gather_epi32((const int *)baseptr, _mm_add_epi32(y0, coloffsets0), 4); \
	__m128i p01 = _mm_i32gather_epi32((const int *)baseptr, _mm_add_epi32(y1, coloffsets0), 4); \
	__m128i p10 = _mm_i32gather_epi32((const int *)baseptr, _mm_add_epi32(y0, coloffsets1), 4); \
	__m128i p11 = _mm_i32gather_epi32((const int *)baseptr, _mm_add_epi32(y1, coloffsets1), 4); \
	__m128i p00_lo = _mm_mullo_epi16(_mm_unpacklo_epi8(p00, _mm_setzero_si128()), ab_lo); \
	__m128i p01_lo = _mm_mullo_epi16(_mm_unpacklo_epi8(p01, _mm_setzero_si128()), invab_lo); \
	__m128i p10_lo = _mm_mullo_epi16(_mm_unpacklo_epi8(p10, _mm_setzero_si128()), ainvb_lo); \
	__m128i p11_lo = _mm_mullo_epi16(_mm_unpacklo_epi8(p11, _mm_setzero_si128()), invainvb_lo); \
	__m128i p00_hi = _mm_mullo_epi16(_mm_unpackhi_epi8(p00, _mm_setzero_si128()), ab_hi); \
	__m128i p01_hi = _mm_mullo_epi16(_mm_unpackhi_epi8(p01, _mm_setzero_si128()), invab_hi); \
	__m128i p10_hi = _mm_mullo_epi16(_mm_unpackhi_epi8(p10, _mm_setzero_si128()), ainvb_hi); \
	__m128i p11_hi = _mm_mullo_epi16(_mm_unpackhi_epi8(p11, _mm_setzero_si128()), invainvb_hi); \
	__m128i fg_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_adds_epu16(p00_lo, p01_lo), _mm_adds_epu16(p10_lo, p11_lo)), m127), 8); \
	__m128i fg_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_adds_epu16(p00_hi, p01_hi), _mm_adds_epu16(p10_hi, p11_hi)), m127), 8); \
	fg = _mm_packus_epi16(fg_lo, fg_hi); \
}

#define VEC_SAMPLE_BILINEAR4_COLUMN(fg, col0, col1, texturefracx, texturefracy, one, height) { \
	__m128i m127 = _mm_set1_epi16(127); \
	fg = _mm_setzero_si128(); \
	for (int i = 0; i < 4; i++) \
	{ \
		uint32_t frac_y0 = (texturefracy[i] >> FRACBITS) * height[i]; \
		uint32_t frac_y1 = ((texturefracy[i] + one[i]) >> FRACBITS) * height[i]; \
		uint32_t y0 = (frac_y0 >> FRACBITS); \
		uint32_t y1 = (frac_y1 >> FRACBITS); \
		 \
		uint32_t inv_b = texturefracx[i]; \
		uint32_t inv_a = (frac_y1 >> (FRACBITS - 4)) & 15; \
		 \
		__m128i ab_invab = _mm_load_si128(SampleBgra::samplertable + inv_b * 32 + inv_a * 2); \
		__m128i ainvb_invainvb = _mm_load_si128(SampleBgra::samplertable + inv_b * 32 + inv_a * 2 + 1); \
		 \
		__m128i gather = _mm_set_epi32(col1[i][y1], col1[i][y0], col0[i][y1], col0[i][y0]); \
		__m128i p0 = _mm_unpacklo_epi8(gather, _mm_setzero_si128()); \
		__m128i p1 = _mm_unpackhi_epi8(gather, _mm_setzero_si128()); \
		 \
		__m128i tmp = _mm_adds_epu16(_mm_mullo_epi16(p0, ab_invab), _mm_mullo_epi16(p1, ainvb_invainvb)); \
		__m128i color = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_srli_si128(tmp, 8), tmp), m127), 8); \
		 \
		fg = _mm_or_si128(_mm_srli_si128(fg, 4), _mm_slli_si128(_mm_packus_epi16(color, _mm_setzero_si128()), 12)); \
	} \
}

#define VEC_SAMPLE_MIP_NEAREST4_COLUMN(fg, col0, col1, mipfrac, texturefracy, height0, height1) { \
	uint32_t y0[4], y1[4]; \
	for (int i = 0; i < 4; i++) \
	{ \
		 y0[i] = (texturefracy[i] >> FRACBITS) * height0[i]; \
		 y1[i] = (texturefracy[i] >> FRACBITS) * height1[i]; \
	} \
	__m128i p0 = _mm_set_epi32(col0[y0[3]], col0[y0[2]], col0[y0[1]], col0[y0[0]]); \
	__m128i p1 = _mm_set_epi32(col1[y1[3]], col1[y1[2]], col1[y1[1]], col1[y1[0]]); \
	__m128i t = _mm_loadu_si128((const __m128i*)mipfrac); \
	__m128i inv_t = _mm_sub_epi32(_mm_set1_epi32(256), mipfrac); \
	__m128i p0_lo = _mm_unpacklo_epi8(p0, _mm_setzero_si128()); \
	__m128i p0_hi = _mm_unpackhi_epi8(p0, _mm_setzero_si128()); \
	__m128i p1_lo = _mm_unpacklo_epi8(p1, _mm_setzero_si128()); \
	__m128i p1_hi = _mm_unpackhi_epi8(p1, _mm_setzero_si128()); \
	__m128i fg_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(p0_lo, t), _mm_mullo_epi16(p1_lo, inv_t)), 8); \
	__m128i fg_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(p0_hi, t), _mm_mullo_epi16(p1_hi, inv_t)), 8); \
	fg = _mm_packus_epi16(fg_lo, fg_hi); \
}

#define VEC_SAMPLE_BILINEAR4_SPAN(fg, texture, xfrac, yfrac, xstep, ystep, xbits, ybits) { \
	int xshift = (32 - xbits); \
	int yshift = (32 - ybits); \
	int xmask = (1 << xshift) - 1; \
	int ymask = (1 << yshift) - 1; \
	 \
	__m128i m127 = _mm_set1_epi16(127); \
	fg = _mm_setzero_si128(); \
	for (int i = 0; i < 4; i++) \
	{ \
		uint32_t x = xfrac >> xbits; \
		uint32_t y = yfrac >> ybits; \
		 \
		uint32_t p00 = texture[(y & ymask) + ((x & xmask) << yshift)]; \
		uint32_t p01 = texture[((y + 1) & ymask) + ((x & xmask) << yshift)]; \
		uint32_t p10 = texture[(y & ymask) + (((x + 1) & xmask) << yshift)]; \
		uint32_t p11 = texture[((y + 1) & ymask) + (((x + 1) & xmask) << yshift)]; \
		 \
		uint32_t inv_b = (xfrac >> (xbits - 4)) & 15; \
		uint32_t inv_a = (yfrac >> (ybits - 4)) & 15; \
		 \
		__m128i ab_invab = _mm_load_si128(SampleBgra::samplertable + inv_b * 32 + inv_a * 2); \
		__m128i ainvb_invainvb = _mm_load_si128(SampleBgra::samplertable + inv_b * 32 + inv_a * 2 + 1); \
		 \
		__m128i p0 = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, p01, p00), _mm_setzero_si128()); \
		__m128i p1 = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, p11, p10), _mm_setzero_si128()); \
		 \
		__m128i tmp = _mm_adds_epu16(_mm_mullo_epi16(p0, ab_invab), _mm_mullo_epi16(p1, ainvb_invainvb)); \
		__m128i color = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_srli_si128(tmp, 8), tmp), m127), 8); \
		 \
		fg = _mm_or_si128(_mm_srli_si128(fg, 4), _mm_slli_si128(_mm_packus_epi16(color, _mm_setzero_si128()), 12)); \
		 \
		xfrac += xstep; \
		yfrac += ystep; \
	} \
}

// Calculate constants for a simple shade with gamma correction
#define AVX_LINEAR_SHADE_SIMPLE_INIT(light) \
	__m256 mlight_hi = _mm256_set_ps(1.0f, light * (1.0f/256.0f), light * (1.0f/256.0f), light * (1.0f/256.0f), 1.0f, light * (1.0f/256.0f), light * (1.0f/256.0f), light * (1.0f/256.0f)); \
	mlight_hi = _mm256_mul_ps(mlight_hi, mlight_hi); \
	__m256 mlight_lo = mlight_hi; \
	__m256 mrcp_255 = _mm256_set1_ps(1.0f/255.0f); \
	__m256 m255 = _mm256_set1_ps(255.0f);

// Calculate constants for a simple shade with different light levels for each pixel and gamma correction
#define AVX_LINEAR_SHADE_SIMPLE_INIT4(light3, light2, light1, light0) \
	__m256 mlight_hi = _mm256_set_ps(1.0f, light1 * (1.0f/256.0f), light1 * (1.0f/256.0f), light1 * (1.0f/256.0f), 1.0f, light0 * (1.0f/256.0f), light0 * (1.0f/256.0f), light0 * (1.0f/256.0f)); \
	__m256 mlight_lo = _mm256_set_ps(1.0f, light3 * (1.0f/256.0f), light3 * (1.0f/256.0f), light3 * (1.0f/256.0f), 1.0f, light2 * (1.0f/256.0f), light2 * (1.0f/256.0f), light2 * (1.0f/256.0f)); \
	mlight_hi = _mm256_mul_ps(mlight_hi, mlight_hi); \
	mlight_lo = _mm256_mul_ps(mlight_lo, mlight_lo); \
	__m256 mrcp_255 = _mm256_set1_ps(1.0f/255.0f); \
	__m256 m255 = _mm256_set1_ps(255.0f);

// Simple shade 4 pixels with gamma correction
#define AVX_LINEAR_SHADE_SIMPLE(fg) { \
	__m256i fg_16 = _mm256_set_m128i(_mm_unpackhi_epi8(fg, _mm_setzero_si128()), _mm_unpacklo_epi8(fg, _mm_setzero_si128())); \
	__m256 fg_hi = _mm256_cvtepi32_ps(_mm256_unpackhi_epi16(fg_16, _mm256_setzero_si256())); \
	__m256 fg_lo = _mm256_cvtepi32_ps(_mm256_unpacklo_epi16(fg_16, _mm256_setzero_si256())); \
	fg_hi = _mm256_mul_ps(fg_hi, mrcp_255); \
	fg_hi = _mm256_mul_ps(fg_hi, fg_hi); \
	fg_hi = _mm256_mul_ps(fg_hi, mlight_hi); \
	fg_hi = _mm256_sqrt_ps(fg_hi); \
	fg_hi = _mm256_mul_ps(fg_hi, m255); \
	fg_lo = _mm256_mul_ps(fg_lo, mrcp_255); \
	fg_lo = _mm256_mul_ps(fg_lo, fg_lo); \
	fg_lo = _mm256_mul_ps(fg_lo, mlight_lo); \
	fg_lo = _mm256_sqrt_ps(fg_lo); \
	fg_lo = _mm256_mul_ps(fg_lo, m255); \
	fg_16 = _mm256_packus_epi32(_mm256_cvtps_epi32(fg_lo), _mm256_cvtps_epi32(fg_hi)); \
	fg = _mm_packus_epi16(_mm256_extractf128_si256(fg_16, 0), _mm256_extractf128_si256(fg_16, 1)); \
}

// Calculate constants for a complex shade with gamma correction
#define AVX_LINEAR_SHADE_INIT(light, shade_constants) \
	__m256 mlight_hi = _mm256_set_ps(1.0f, light * (1.0f/256.0f), light * (1.0f/256.0f), light * (1.0f/256.0f), 1.0f, light * (1.0f/256.0f), light * (1.0f/256.0f), light * (1.0f/256.0f)); \
	mlight_hi = _mm256_mul_ps(mlight_hi, mlight_hi); \
	__m256 mlight_lo = mlight_hi; \
	__m256 mrcp_255 = _mm256_set1_ps(1.0f/255.0f); \
	__m256 m255 = _mm256_set1_ps(255.0f); \
	__m256 color = _mm256_set_ps( \
		1.0f, shade_constants.light_red * (1.0f/256.0f), shade_constants.light_green * (1.0f/256.0f), shade_constants.light_blue * (1.0f/256.0f), \
		1.0f, shade_constants.light_red * (1.0f/256.0f), shade_constants.light_green * (1.0f/256.0f), shade_constants.light_blue * (1.0f/256.0f)); \
	__m256 fade = _mm256_set_ps( \
		0.0f, shade_constants.fade_red * (1.0f/256.0f), shade_constants.fade_green * (1.0f/256.0f), shade_constants.fade_blue * (1.0f/256.0f), \
		0.0f, shade_constants.fade_red * (1.0f/256.0f), shade_constants.fade_green * (1.0f/256.0f), shade_constants.fade_blue * (1.0f/256.0f)); \
	__m256 fade_amount_hi = _mm256_mul_ps(fade, _mm256_sub_ps(_mm256_set1_ps(1.0f), mlight_hi)); \
	__m256 fade_amount_lo = _mm256_mul_ps(fade, _mm256_sub_ps(_mm256_set1_ps(1.0f), mlight_lo)); \
	__m256 inv_desaturate = _mm256_set1_ps((256 - shade_constants.desaturate) * (1.0f/256.0f)); \
	__m128 ss_desaturate = _mm_set_ss(shade_constants.desaturate * (1.0f/256.0f)); \
	__m128 intensity_weight = _mm_set_ps(0.0f, 77.0f/256.0f, 143.0f/256.0f, 37.0f/256.0f);

// Calculate constants for a complex shade with different light levels for each pixel and gamma correction
#define AVX_LINEAR_SHADE_INIT4(light3, light2, light1, light0, shade_constants) \
	__m256 mlight_hi = _mm256_set_ps(1.0f, light1 * (1.0f/256.0f), light1 * (1.0f/256.0f), light1 * (1.0f/256.0f), 1.0f, light0 * (1.0f/256.0f), light0 * (1.0f/256.0f), light0 * (1.0f/256.0f)); \
	__m256 mlight_lo = _mm256_set_ps(1.0f, light3 * (1.0f/256.0f), light3 * (1.0f/256.0f), light3 * (1.0f/256.0f), 1.0f, light2 * (1.0f/256.0f), light2 * (1.0f/256.0f), light2 * (1.0f/256.0f)); \
	mlight_hi = _mm256_mul_ps(mlight_hi, mlight_hi); \
	mlight_lo = _mm256_mul_ps(mlight_lo, mlight_lo); \
	__m256 mrcp_255 = _mm256_set1_ps(1.0f/255.0f); \
	__m256 m255 = _mm256_set1_ps(255.0f); \
	__m256 color = _mm256_set_ps( \
		1.0f, shade_constants.light_red * (1.0f/256.0f), shade_constants.light_green * (1.0f/256.0f), shade_constants.light_blue * (1.0f/256.0f), \
		1.0f, shade_constants.light_red * (1.0f/256.0f), shade_constants.light_green * (1.0f/256.0f), shade_constants.light_blue * (1.0f/256.0f)); \
	__m256 fade = _mm256_set_ps( \
		0.0f, shade_constants.fade_red * (1.0f/256.0f), shade_constants.fade_green * (1.0f/256.0f), shade_constants.fade_blue * (1.0f/256.0f), \
		0.0f, shade_constants.fade_red * (1.0f/256.0f), shade_constants.fade_green * (1.0f/256.0f), shade_constants.fade_blue * (1.0f/256.0f)); \
	__m256 fade_amount_hi = _mm256_mul_ps(fade, _mm256_sub_ps(_mm256_set1_ps(1.0f), mlight_hi)); \
	__m256 fade_amount_lo = _mm256_mul_ps(fade, _mm256_sub_ps(_mm256_set1_ps(1.0f), mlight_lo)); \
	__m256 inv_desaturate = _mm256_set1_ps((256 - shade_constants.desaturate) * (1.0f/256.0f)); \
	__m128 ss_desaturate = _mm_set_ss(shade_constants.desaturate * (1.0f/256.0f)); \
	__m128 intensity_weight = _mm_set_ps(0.0f, 77.0f/256.0f, 143.0f/256.0f, 37.0f/256.0f);

// Complex shade 4 pixels with gamma correction
#define AVX_LINEAR_SHADE(fg, shade_constants) { \
	__m256i fg_16 = _mm256_set_m128i(_mm_unpackhi_epi8(fg, _mm_setzero_si128()), _mm_unpacklo_epi8(fg, _mm_setzero_si128())); \
	__m256 fg_hi = _mm256_cvtepi32_ps(_mm256_unpackhi_epi16(fg_16, _mm256_setzero_si256())); \
	__m256 fg_lo = _mm256_cvtepi32_ps(_mm256_unpacklo_epi16(fg_16, _mm256_setzero_si256())); \
	fg_hi = _mm256_mul_ps(fg_hi, mrcp_255); \
	fg_hi = _mm256_mul_ps(fg_hi, fg_hi); \
	fg_lo = _mm256_mul_ps(fg_lo, mrcp_255); \
	fg_lo = _mm256_mul_ps(fg_lo, fg_lo); \
	 \
	__m128 intensity_hi0 = _mm_mul_ps(_mm256_extractf128_ps(fg_hi, 0), intensity_weight); \
	__m128 intensity_hi1 = _mm_mul_ps(_mm256_extractf128_ps(fg_hi, 1), intensity_weight); \
	intensity_hi0 = _mm_mul_ss(_mm_add_ss(_mm_add_ss(intensity_hi0, _mm_shuffle_ps(intensity_hi0, intensity_hi0, _MM_SHUFFLE(1,1,1,1))), _mm_shuffle_ps(intensity_hi0, intensity_hi0, _MM_SHUFFLE(2,2,2,2))), ss_desaturate); \
	intensity_hi0 = _mm_shuffle_ps(intensity_hi0, intensity_hi0, _MM_SHUFFLE(0,0,0,0)); \
	intensity_hi1 = _mm_mul_ss(_mm_add_ss(_mm_add_ss(intensity_hi1, _mm_shuffle_ps(intensity_hi1, intensity_hi1, _MM_SHUFFLE(1,1,1,1))), _mm_shuffle_ps(intensity_hi1, intensity_hi1, _MM_SHUFFLE(2,2,2,2))), ss_desaturate); \
	intensity_hi1 = _mm_shuffle_ps(intensity_hi1, intensity_hi1, _MM_SHUFFLE(0,0,0,0)); \
	__m256 intensity_hi = _mm256_set_m128(intensity_hi1, intensity_hi0); \
	 \
	fg_hi = _mm256_add_ps(_mm256_mul_ps(fg_hi, inv_desaturate), intensity_hi); \
	fg_hi = _mm256_add_ps(_mm256_mul_ps(fg_hi, mlight_hi), fade_amount_hi); \
	fg_hi = _mm256_mul_ps(fg_hi, color); \
	 \
	__m128 intensity_lo0 = _mm_mul_ps(_mm256_extractf128_ps(fg_lo, 0), intensity_weight); \
	__m128 intensity_lo1 = _mm_mul_ps(_mm256_extractf128_ps(fg_lo, 1), intensity_weight); \
	intensity_lo0 = _mm_mul_ss(_mm_add_ss(_mm_add_ss(intensity_lo0, _mm_shuffle_ps(intensity_lo0, intensity_lo0, _MM_SHUFFLE(1,1,1,1))), _mm_shuffle_ps(intensity_lo0, intensity_lo0, _MM_SHUFFLE(2,2,2,2))), ss_desaturate); \
	intensity_lo0 = _mm_shuffle_ps(intensity_lo0, intensity_lo0, _MM_SHUFFLE(0,0,0,0)); \
	intensity_lo1 = _mm_mul_ss(_mm_add_ss(_mm_add_ss(intensity_lo1, _mm_shuffle_ps(intensity_lo1, intensity_lo1, _MM_SHUFFLE(1,1,1,1))), _mm_shuffle_ps(intensity_lo1, intensity_lo1, _MM_SHUFFLE(2,2,2,2))), ss_desaturate); \
	intensity_lo1 = _mm_shuffle_ps(intensity_lo1, intensity_lo1, _MM_SHUFFLE(0,0,0,0)); \
	__m256 intensity_lo = _mm256_set_m128(intensity_lo1, intensity_lo0); \
	 \
	fg_lo = _mm256_add_ps(_mm256_mul_ps(fg_lo, inv_desaturate), intensity_lo); \
	fg_lo = _mm256_add_ps(_mm256_mul_ps(fg_lo, mlight_lo), fade_amount_lo); \
	fg_lo = _mm256_mul_ps(fg_lo, color); \
	 \
	fg_hi = _mm256_sqrt_ps(fg_hi); \
	fg_hi = _mm256_mul_ps(fg_hi, m255); \
	fg_lo = _mm256_sqrt_ps(fg_lo); \
	fg_lo = _mm256_mul_ps(fg_lo, m255); \
	fg_16 = _mm256_packus_epi32(_mm256_cvtps_epi32(fg_lo), _mm256_cvtps_epi32(fg_hi)); \
	fg = _mm_packus_epi16(_mm256_extractf128_si256(fg_16, 0), _mm256_extractf128_si256(fg_16, 1)); \
}

/*
// Complex shade 8 pixels
#define AVX_SHADE(fg, shade_constants) { \
	__m256i fg_hi = _mm256_unpackhi_epi8(fg, _mm256_setzero_si256()); \
	__m256i fg_lo = _mm256_unpacklo_epi8(fg, _mm256_setzero_si256()); \
	 \
	__m256i intensity_hi = _mm256_mullo_epi16(fg_hi, _mm256_set_epi16(0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37)); \
	__m256i intensity_lo = _mm256_mullo_epi16(fg_lo, _mm256_set_epi16(0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37, 0, 77, 143, 37)); \
	__m256i intensity = _mm256_mullo_epi16(_mm256_srli_epi16(_mm256_hadd_epi16(_mm256_hadd_epi16(intensity_lo, intensity_hi), _mm256_setzero_si256()), 8), desaturate); \
	intensity = _mm256_unpacklo_epi16(intensity, intensity); \
	intensity_hi = _mm256_unpackhi_epi32(intensity, intensity); \
	intensity_lo = _mm256_unpacklo_epi32(intensity, intensity); \
	 \
	fg_hi = _mm256_srli_epi16(_mm256_adds_epu16(_mm256_mullo_epi16(fg_hi, inv_desaturate), intensity_hi), 8); \
	fg_hi = _mm256_srli_epi16(_mm256_adds_epu16(_mm256_mullo_epi16(fg_hi, mlight), fade_amount), 8); \
	fg_hi = _mm256_srli_epi16(_mm256_mullo_epi16(fg_hi, color), 8); \
	 \
	fg_lo = _mm256_srli_epi16(_mm256_adds_epu16(_mm256_mullo_epi16(fg_lo, inv_desaturate), intensity_lo), 8); \
	fg_lo = _mm256_srli_epi16(_mm256_adds_epu16(_mm256_mullo_epi16(fg_lo, mlight), fade_amount), 8); \
	fg_lo = _mm256_srli_epi16(_mm256_mullo_epi16(fg_lo, color), 8); \
	 \
	fg = _mm256_packus_epi16(fg_lo, fg_hi); \
}
*/

// Normal premultiplied alpha blend using the alpha from fg
#define VEC_ALPHA_BLEND(fg,bg) { \
	__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128()); \
	__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128()); \
	__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128()); \
	__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128()); \
	__m128i m256 = _mm_set1_epi16(256); \
	__m128i alpha_hi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(fg_hi, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3)); \
	__m128i alpha_lo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(fg_lo, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3)); \
	alpha_hi = _mm_add_epi16(alpha_hi, _mm_srli_epi16(alpha_hi, 7)); \
	alpha_lo = _mm_add_epi16(alpha_lo, _mm_srli_epi16(alpha_lo, 7)); \
	__m128i inv_alpha_hi = _mm_sub_epi16(m256, alpha_hi); \
	__m128i inv_alpha_lo = _mm_sub_epi16(m256, alpha_lo); \
	fg_hi = _mm_mullo_epi16(fg_hi, alpha_hi); \
	fg_hi = _mm_srli_epi16(fg_hi, 8); \
	fg_lo = _mm_mullo_epi16(fg_lo, alpha_lo); \
	fg_lo = _mm_srli_epi16(fg_lo, 8); \
	fg = _mm_packus_epi16(fg_lo, fg_hi); \
	bg_hi = _mm_mullo_epi16(bg_hi, inv_alpha_hi); \
	bg_hi = _mm_srli_epi16(bg_hi, 8); \
	bg_lo = _mm_mullo_epi16(bg_lo, inv_alpha_lo); \
	bg_lo = _mm_srli_epi16(bg_lo, 8); \
	bg = _mm_packus_epi16(bg_lo, bg_hi); \
	fg = _mm_adds_epu8(fg, bg); \
}

// Calculates the final alpha values to be used when combined with the source texture alpha channel
FORCEINLINE uint32_t calc_blend_bgalpha(uint32_t fg, uint32_t dest_alpha)
{
	uint32_t alpha = fg >> 24;
	alpha += alpha >> 7;
	uint32_t inv_alpha = 256 - alpha;
	return (dest_alpha * alpha + 256 * inv_alpha + 128) >> 8;
}

#define VEC_CALC_BLEND_ALPHA_VARS() __m128i msrc_alpha, mdest_alpha, m256, m255, m128;

#define VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha) \
	msrc_alpha = _mm_set1_epi16(src_alpha); \
	mdest_alpha = _mm_set1_epi16(dest_alpha * 255 / 256); \
	m256 = _mm_set1_epi16(256); \
	m255 = _mm_set1_epi16(255); \
	m128 = _mm_set1_epi16(128);

// Calculates the final alpha values to be used when combined with the source texture alpha channel
#define VEC_CALC_BLEND_ALPHA(fg) \
	__m128i fg_alpha_hi, fg_alpha_lo, bg_alpha_hi, bg_alpha_lo; { \
		__m128i alpha_hi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(_mm_unpackhi_epi8(fg, _mm_setzero_si128()), _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)); \
		__m128i alpha_lo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(_mm_unpacklo_epi8(fg, _mm_setzero_si128()), _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)); \
		alpha_hi = _mm_add_epi16(alpha_hi, _mm_srli_epi16(alpha_hi, 7)); \
		alpha_lo = _mm_add_epi16(alpha_lo, _mm_srli_epi16(alpha_lo, 7)); \
		bg_alpha_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_mullo_epi16(mdest_alpha, alpha_hi), _mm_mullo_epi16(m255, _mm_sub_epi16(m256, alpha_hi))), m128), 8); \
		bg_alpha_hi = _mm_add_epi16(bg_alpha_hi, _mm_srli_epi16(bg_alpha_hi, 7)); \
		bg_alpha_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_adds_epu16(_mm_mullo_epi16(mdest_alpha, alpha_lo), _mm_mullo_epi16(m255, _mm_sub_epi16(m256, alpha_lo))), m128), 8); \
		bg_alpha_lo = _mm_add_epi16(bg_alpha_lo, _mm_srli_epi16(bg_alpha_lo, 7)); \
		fg_alpha_hi = msrc_alpha; \
		fg_alpha_lo = msrc_alpha; \
	}

#define SSE_SHADE_VARS() __m128i mlight_hi, mlight_lo, color, fade, fade_amount_hi, fade_amount_lo, inv_desaturate;

// Calculate constants for a simple shade
#define SSE_SHADE_SIMPLE_INIT(light) \
	mlight_hi = _mm_set_epi16(256, light, light, light, 256, light, light, light); \
	mlight_lo = mlight_hi;

// Calculate constants for a simple shade with different light levels for each pixel
#define SSE_SHADE_SIMPLE_INIT4(light3, light2, light1, light0) \
	mlight_hi = _mm_set_epi16(256, light1, light1, light1, 256, light0, light0, light0); \
	mlight_lo = _mm_set_epi16(256, light3, light3, light3, 256, light2, light2, light2);

// Simple shade 4 pixels
#define SSE_SHADE_SIMPLE(fg) { \
	__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128()); \
	__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128()); \
	fg_hi = _mm_mullo_epi16(fg_hi, mlight_hi); \
	fg_hi = _mm_srli_epi16(fg_hi, 8); \
	fg_lo = _mm_mullo_epi16(fg_lo, mlight_lo); \
	fg_lo = _mm_srli_epi16(fg_lo, 8); \
	fg = _mm_packus_epi16(fg_lo, fg_hi); \
}

// Calculate constants for a complex shade
#define SSE_SHADE_INIT(light, shade_constants) \
	mlight_hi = _mm_set_epi16(256, light, light, light, 256, light, light, light); \
	mlight_lo = mlight_hi; \
	color = _mm_set_epi16( \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	fade = _mm_set_epi16( \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
	fade_amount_hi = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_hi)); \
	fade_amount_lo = fade_amount_hi; \
	inv_desaturate = _mm_set1_epi16(256 - shade_constants.desaturate); \

// Calculate constants for a complex shade with different light levels for each pixel
#define SSE_SHADE_INIT4(light3, light2, light1, light0, shade_constants) \
	mlight_hi = _mm_set_epi16(256, light1, light1, light1, 256, light0, light0, light0); \
	mlight_lo = _mm_set_epi16(256, light3, light3, light3, 256, light2, light2, light2); \
	color = _mm_set_epi16( \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	fade = _mm_set_epi16( \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
	fade_amount_hi = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_hi)); \
	fade_amount_lo = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_lo)); \
	inv_desaturate = _mm_set1_epi16(256 - shade_constants.desaturate); \

// Complex shade 4 pixels
#define SSE_SHADE(fg, shade_constants) { \
	__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128()); \
	__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128()); \
	 \
	__m128i intensity_hi = _mm_mullo_epi16(fg_hi, _mm_set_epi16(0, 77, 143, 37, 0, 77, 143, 37)); \
	uint16_t intensity_hi0 = ((_mm_extract_epi16(intensity_hi, 2) + _mm_extract_epi16(intensity_hi, 1) + _mm_extract_epi16(intensity_hi, 0)) >> 8) * shade_constants.desaturate; \
	uint16_t intensity_hi1 = ((_mm_extract_epi16(intensity_hi, 6) + _mm_extract_epi16(intensity_hi, 5) + _mm_extract_epi16(intensity_hi, 4)) >> 8) * shade_constants.desaturate; \
	intensity_hi = _mm_set_epi16(intensity_hi1, intensity_hi1, intensity_hi1, intensity_hi1, intensity_hi0, intensity_hi0, intensity_hi0, intensity_hi0); \
	 \
	fg_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, inv_desaturate), intensity_hi), 8); \
	fg_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mlight_hi), fade_amount_hi), 8); \
	fg_hi = _mm_srli_epi16(_mm_mullo_epi16(fg_hi, color), 8); \
	 \
	__m128i intensity_lo = _mm_mullo_epi16(fg_lo, _mm_set_epi16(0, 77, 143, 37, 0, 77, 143, 37)); \
	uint16_t intensity_lo0 = ((_mm_extract_epi16(intensity_lo, 2) + _mm_extract_epi16(intensity_lo, 1) + _mm_extract_epi16(intensity_lo, 0)) >> 8) * shade_constants.desaturate; \
	uint16_t intensity_lo1 = ((_mm_extract_epi16(intensity_lo, 6) + _mm_extract_epi16(intensity_lo, 5) + _mm_extract_epi16(intensity_lo, 4)) >> 8) * shade_constants.desaturate; \
	intensity_lo = _mm_set_epi16(intensity_lo1, intensity_lo1, intensity_lo1, intensity_lo1, intensity_lo0, intensity_lo0, intensity_lo0, intensity_lo0); \
	 \
	fg_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, inv_desaturate), intensity_lo), 8); \
	fg_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mlight_lo), fade_amount_lo), 8); \
	fg_lo = _mm_srli_epi16(_mm_mullo_epi16(fg_lo, color), 8); \
	 \
	fg = _mm_packus_epi16(fg_lo, fg_hi); \
}

#endif
