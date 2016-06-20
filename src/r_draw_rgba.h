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

// Manages queueing up commands and executing them on worker threads
class DrawerCommandQueue
{
	enum { memorypool_size = 4 * 1024 * 1024 };
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
			if (!ptr)
				return;
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

/////////////////////////////////////////////////////////////////////////////
// Pixel shading macros and inline functions:

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

// calculates the light constant passed to the shade_pal_index function
FORCEINLINE uint32_t calc_light_multiplier(dsfixed_t light)
{
	return 256 - (light >> (FRACBITS - 8));
}

// Calculates a ARGB8 color for the given palette index and light multiplier
FORCEINLINE uint32_t shade_pal_index_simple(uint32_t index, uint32_t light)
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

FORCEINLINE uint32_t shade_bgra_simple(uint32_t color, uint32_t light)
{
	uint32_t red = (color >> 16) & 0xff;
	uint32_t green = (color >> 8) & 0xff;
	uint32_t blue = color & 0xff;

	red = red * light / 256;
	green = green * light / 256;
	blue = blue * light / 256;

	return 0xff000000 | (red << 16) | (green << 8) | blue;
}

// Calculates a ARGB8 color for the given palette index, light multiplier and dynamic colormap
FORCEINLINE uint32_t shade_pal_index(uint32_t index, uint32_t light, const ShadeConstants &constants)
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

FORCEINLINE uint32_t shade_bgra(uint32_t color, uint32_t light, const ShadeConstants &constants)
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

FORCEINLINE uint32_t alpha_blend(uint32_t fg, uint32_t bg)
{
	uint32_t fg_alpha = fg >> 24;
	uint32_t fg_red = (fg >> 16) & 0xff;
	uint32_t fg_green = (fg >> 8) & 0xff;
	uint32_t fg_blue = fg & 0xff;

	uint32_t alpha = fg_alpha + (fg_alpha >> 7); // 255 -> 256
	uint32_t inv_alpha = 256 - alpha;

	uint32_t bg_red = (bg >> 16) & 0xff;
	uint32_t bg_green = (bg >> 8) & 0xff;
	uint32_t bg_blue = bg & 0xff;

	uint32_t red = clamp<uint32_t>(fg_red + (bg_red * inv_alpha) / 256, 0, 255);
	uint32_t green = clamp<uint32_t>(fg_green + (bg_green * inv_alpha) / 256, 0, 255);
	uint32_t blue = clamp<uint32_t>(fg_blue + (bg_blue * inv_alpha) / 256, 0, 255);

	return 0xff000000 | (red << 16) | (green << 8) | blue;
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
	__m128i m255 = _mm_set1_epi16(255); \
	__m128i inv_alpha_hi = _mm_sub_epi16(m255, _mm_shufflehi_epi16(_mm_shufflelo_epi16(fg_hi, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3))); \
	__m128i inv_alpha_lo = _mm_sub_epi16(m255, _mm_shufflehi_epi16(_mm_shufflelo_epi16(fg_lo, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3))); \
	inv_alpha_hi = _mm_add_epi16(inv_alpha_hi, _mm_srli_epi16(inv_alpha_hi, 7)); \
	inv_alpha_lo = _mm_add_epi16(inv_alpha_lo, _mm_srli_epi16(inv_alpha_lo, 7)); \
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
	return 256 - alpha; // (dest_alpha * (256 - alpha)) >> 8;
}

#define VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha) \
	__m128i msrc_alpha = _mm_set1_epi16(src_alpha); \
	__m128i mdest_alpha = _mm_set1_epi16(dest_alpha);

// Calculates the final alpha values to be used when combined with the source texture alpha channel
#define VEC_CALC_BLEND_ALPHA(fg) \
	__m128i fg_alpha_hi, fg_alpha_lo, bg_alpha_hi, bg_alpha_lo; { \
		__m128i alpha_hi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(_mm_unpackhi_epi8(fg, _mm_setzero_si128()), _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)); \
		__m128i alpha_lo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(_mm_unpacklo_epi8(fg, _mm_setzero_si128()), _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)); \
		alpha_hi = _mm_add_epi16(alpha_hi, _mm_srli_epi16(alpha_hi, 7)); \
		alpha_lo = _mm_add_epi16(alpha_lo, _mm_srli_epi16(alpha_lo, 7)); \
		bg_alpha_hi = _mm_sub_epi16(_mm_set1_epi16(256), alpha_hi); /* _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(256), alpha_hi), mdest_alpha), 8);*/ \
		bg_alpha_lo = _mm_sub_epi16(_mm_set1_epi16(256), alpha_lo); /* _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(256), alpha_lo), mdest_alpha), 8);*/ \
		fg_alpha_hi = msrc_alpha; \
		fg_alpha_lo = msrc_alpha; \
	}

// Calculate constants for a simple shade
#define SSE_SHADE_SIMPLE_INIT(light) \
	__m128i mlight_hi = _mm_set_epi16(256, light, light, light, 256, light, light, light); \
	__m128i mlight_lo = mlight_hi;

// Calculate constants for a simple shade with different light levels for each pixel
#define SSE_SHADE_SIMPLE_INIT4(light3, light2, light1, light0) \
	__m128i mlight_hi = _mm_set_epi16(256, light1, light1, light1, 256, light0, light0, light0); \
	__m128i mlight_lo = _mm_set_epi16(256, light3, light3, light3, 256, light2, light2, light2);

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
	__m128i mlight_hi = _mm_set_epi16(256, light, light, light, 256, light, light, light); \
	__m128i mlight_lo = mlight_hi; \
	__m128i color = _mm_set_epi16( \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	__m128i fade = _mm_set_epi16( \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
	__m128i fade_amount_hi = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_hi)); \
	__m128i fade_amount_lo = fade_amount_hi; \
	__m128i inv_desaturate = _mm_set1_epi16(256 - shade_constants.desaturate); \

// Calculate constants for a complex shade with different light levels for each pixel
#define SSE_SHADE_INIT4(light3, light2, light1, light0, shade_constants) \
	__m128i mlight_hi = _mm_set_epi16(256, light1, light1, light1, 256, light0, light0, light0); \
	__m128i mlight_lo = _mm_set_epi16(256, light3, light3, light3, 256, light2, light2, light2); \
	__m128i color = _mm_set_epi16( \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		256, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	__m128i fade = _mm_set_epi16( \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		0, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
	__m128i fade_amount_hi = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_hi)); \
	__m128i fade_amount_lo = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_lo)); \
	__m128i inv_desaturate = _mm_set1_epi16(256 - shade_constants.desaturate); \

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
