/*
**  Drawer commands for the RT family of drawers
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

#pragma once

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"
#include "swrenderer/viewport/r_skydrawer.h"
#include "swrenderer/viewport/r_spandrawer.h"
#include "swrenderer/viewport/r_walldrawer.h"
#include "swrenderer/viewport/r_spritedrawer.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

struct FSpecialColormap;

EXTERN_CVAR(Bool, r_mipmap)
EXTERN_CVAR(Float, r_lod_bias)

namespace swrenderer
{
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

	// Force the compiler to use a calling convention that works for vector types
	#if defined(_MSC_VER)
	#define VECTORCALL __vectorcall
	#else
	#define VECTORCALL
	#endif

	class DrawFuzzColumnRGBACommand : public DrawerCommand
	{
		int _x;
		int _yl;
		int _yh;
		uint8_t * RESTRICT _destorg;
		int _pitch;
		int _fuzzpos;
		int _fuzzviewheight;

	public:
		DrawFuzzColumnRGBACommand(const SpriteDrawerArgs &drawerargs);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawScaledFuzzColumnRGBACommand : public DrawerCommand
	{
		int _x;
		int _yl;
		int _yh;
		uint8_t * RESTRICT _destorg;
		int _pitch;
		int _fuzzpos;
		int _fuzzviewheight;

	public:
		DrawScaledFuzzColumnRGBACommand(const SpriteDrawerArgs &drawerargs);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class FillSpanRGBACommand : public DrawerCommand
	{
		int _x1;
		int _x2;
		int _y;
		uint8_t * RESTRICT _dest;
		fixed_t _light;
		int _color;

	public:
		FillSpanRGBACommand(const SpanDrawerArgs &drawerargs);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawFogBoundaryLineRGBACommand : public DrawerCommand
	{
		int _y;
		int _x;
		int _x2;
		uint8_t * RESTRICT _line;
		fixed_t _light;
		ShadeConstants _shade_constants;

	public:
		DrawFogBoundaryLineRGBACommand(const SpanDrawerArgs &drawerargs);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawTiltedSpanRGBACommand : public DrawerCommand
	{
		int _x1;
		int _x2;
		int _y;
		uint8_t * RESTRICT _dest;
		fixed_t _light;
		ShadeConstants _shade_constants;
		FVector3 _plane_sz;
		FVector3 _plane_su;
		FVector3 _plane_sv;
		bool _plane_shade;
		int _planeshade;
		float _planelightfloat;
		fixed_t _pviewx;
		fixed_t _pviewy;
		int _xbits;
		int _ybits;
		const uint32_t * RESTRICT _source;
		RenderViewport *viewport;

	public:
		DrawTiltedSpanRGBACommand(const SpanDrawerArgs &drawerargs, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawColoredSpanRGBACommand : public DrawerCommand
	{
		int _y;
		int _x1;
		int _x2;
		uint8_t * RESTRICT _dest;
		fixed_t _light;
		int _color;

	public:
		DrawColoredSpanRGBACommand(const SpanDrawerArgs &drawerargs);

		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

#if 0
	class ApplySpecialColormapRGBACommand : public DrawerCommand
	{
		uint8_t *buffer;
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
		FString DebugInfo() override { return "ApplySpecialColormapRGBACommand"; }
	};
#endif

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

	class DrawParticleColumnRGBACommand : public DrawerCommand
	{
	public:
		DrawParticleColumnRGBACommand(uint32_t *dest, int dest_y, int pitch, int count, uint32_t fg, uint32_t alpha, uint32_t fracposx);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;

	private:
		uint32_t *_dest;
		int _dest_y;
		int _pitch;
		int _count;
		uint32_t _fg;
		uint32_t _alpha;
		uint32_t _fracposx;
	};

	/////////////////////////////////////////////////////////////////////////////

	class DrawVoxelBlocksRGBACommand : public DrawerCommand
	{
	public:
		DrawVoxelBlocksRGBACommand(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;

	private:
		SpriteDrawerArgs args;
		const VoxelBlock *blocks;
		int blockcount;
	};

	/////////////////////////////////////////////////////////////////////////////

	class SWTruecolorDrawers : public SWPixelFormatDrawers
	{
	public:
		using SWPixelFormatDrawers::SWPixelFormatDrawers;
		
		void DrawWallColumn(const WallDrawerArgs &args) override;
		void DrawWallMaskedColumn(const WallDrawerArgs &args) override;
		void DrawWallAddColumn(const WallDrawerArgs &args) override;
		void DrawWallAddClampColumn(const WallDrawerArgs &args) override;
		void DrawWallSubClampColumn(const WallDrawerArgs &args) override;
		void DrawWallRevSubClampColumn(const WallDrawerArgs &args) override;
		void DrawSingleSkyColumn(const SkyDrawerArgs &args) override;
		void DrawDoubleSkyColumn(const SkyDrawerArgs &args) override;
		void DrawColumn(const SpriteDrawerArgs &args) override;
		void FillColumn(const SpriteDrawerArgs &args) override;
		void FillAddColumn(const SpriteDrawerArgs &args) override;
		void FillAddClampColumn(const SpriteDrawerArgs &args) override;
		void FillSubClampColumn(const SpriteDrawerArgs &args) override;
		void FillRevSubClampColumn(const SpriteDrawerArgs &args) override;
		void DrawFuzzColumn(const SpriteDrawerArgs &args) override;
		void DrawAddColumn(const SpriteDrawerArgs &args) override;
		void DrawTranslatedColumn(const SpriteDrawerArgs &args) override;
		void DrawTranslatedAddColumn(const SpriteDrawerArgs &args) override;
		void DrawShadedColumn(const SpriteDrawerArgs &args) override;
		void DrawAddClampShadedColumn(const SpriteDrawerArgs &args) override;
		void DrawAddClampColumn(const SpriteDrawerArgs &args) override;
		void DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args) override;
		void DrawSubClampColumn(const SpriteDrawerArgs &args) override;
		void DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args) override;
		void DrawRevSubClampColumn(const SpriteDrawerArgs &args) override;
		void DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args) override;
		void DrawVoxelBlocks(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount) override;
		void DrawSpan(const SpanDrawerArgs &args) override;
		void DrawSpanMasked(const SpanDrawerArgs &args) override;
		void DrawSpanTranslucent(const SpanDrawerArgs &args) override;
		void DrawSpanMaskedTranslucent(const SpanDrawerArgs &args) override;
		void DrawSpanAddClamp(const SpanDrawerArgs &args) override;
		void DrawSpanMaskedAddClamp(const SpanDrawerArgs &args) override;
		void FillSpan(const SpanDrawerArgs &args) override { Queue->Push<FillSpanRGBACommand>(args); }

		void DrawTiltedSpan(const SpanDrawerArgs &args, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) override
		{
			Queue->Push<DrawTiltedSpanRGBACommand>(args, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy);
		}

		void DrawColoredSpan(const SpanDrawerArgs &args) override { Queue->Push<DrawColoredSpanRGBACommand>(args); }
		void DrawFogBoundaryLine(const SpanDrawerArgs &args) override { Queue->Push<DrawFogBoundaryLineRGBACommand>(args); }
	};

	/////////////////////////////////////////////////////////////////////////////
	// Pixel shading inline functions:

	class LightBgra
	{
	public:
		// calculates the light constant passed to the shade_pal_index function
		FORCEINLINE static uint32_t calc_light_multiplier(uint32_t light)
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

	struct BgraColor
	{
		uint32_t b, g, r, a;
		BgraColor() { }
		BgraColor(uint32_t c) : b(BPART(c)), g(GPART(c)), r(RPART(c)), a(APART(c)) { }
		BgraColor &operator=(uint32_t c) { b = BPART(c); g = GPART(c); r = RPART(c); a = APART(c); return *this; }
		operator uint32_t() const { return MAKEARGB(a, r, g, b); }
	};
}
