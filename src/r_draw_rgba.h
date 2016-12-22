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
#include "r_drawers.h"

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

	#define DECLARE_DRAW_COMMAND(name, func, base) \
	class name##LLVMCommand : public base \
	{ \
	public: \
		using base::base; \
		void Execute(DrawerThread *thread) override \
		{ \
			WorkerThreadData d = ThreadData(thread); \
			Drawers::Instance()->func(&args, &d); \
		} \
	};

	class DrawSpanLLVMCommand : public DrawerCommand
	{
	public:
		DrawSpanLLVMCommand();

		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;

	protected:
		DrawSpanArgs args;

	private:
		inline static bool sampler_setup(const uint32_t * &source, int &xbits, int &ybits, bool mipmapped);
	};

	class DrawSpanMaskedLLVMCommand : public DrawSpanLLVMCommand
	{
	public:
		void Execute(DrawerThread *thread) override;
	};

	class DrawSpanTranslucentLLVMCommand : public DrawSpanLLVMCommand
	{
	public:
		void Execute(DrawerThread *thread) override;
	};

	class DrawSpanMaskedTranslucentLLVMCommand : public DrawSpanLLVMCommand
	{
	public:
		void Execute(DrawerThread *thread) override;
	};

	class DrawSpanAddClampLLVMCommand : public DrawSpanLLVMCommand
	{
	public:
		void Execute(DrawerThread *thread) override;
	};

	class DrawSpanMaskedAddClampLLVMCommand : public DrawSpanLLVMCommand
	{
	public:
		void Execute(DrawerThread *thread) override;
	};

	class DrawWall4LLVMCommand : public DrawerCommand
	{
	protected:
		DrawWallArgs args;

		WorkerThreadData ThreadData(DrawerThread *thread);

	public:
		DrawWall4LLVMCommand();

		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawWall1LLVMCommand : public DrawerCommand
	{
	protected:
		DrawWallArgs args;

		WorkerThreadData ThreadData(DrawerThread *thread);

	public:
		DrawWall1LLVMCommand();

		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawColumnLLVMCommand : public DrawerCommand
	{
	protected:
		DrawColumnArgs args;

		WorkerThreadData ThreadData(DrawerThread *thread);
		FString DebugInfo() override;

	public:
		DrawColumnLLVMCommand();

		void Execute(DrawerThread *thread) override;
	};

	class DrawSkyLLVMCommand : public DrawerCommand
	{
	protected:
		DrawSkyArgs args;

		WorkerThreadData ThreadData(DrawerThread *thread);

	public:
		DrawSkyLLVMCommand(uint32_t solid_top, uint32_t solid_bottom);
		FString DebugInfo() override;
	};

	DECLARE_DRAW_COMMAND(DrawWallMasked4, mvlinec4, DrawWall4LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallAdd4, tmvline4_add, DrawWall4LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallAddClamp4, tmvline4_addclamp, DrawWall4LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallSubClamp4, tmvline4_subclamp, DrawWall4LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallRevSubClamp4, tmvline4_revsubclamp, DrawWall4LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallMasked1, mvlinec1, DrawWall1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallAdd1, tmvline1_add, DrawWall1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallAddClamp1, tmvline1_addclamp, DrawWall1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallSubClamp1, tmvline1_subclamp, DrawWall1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawWallRevSubClamp1, tmvline1_revsubclamp, DrawWall1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnAdd, DrawColumnAdd, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnTranslated, DrawColumnTranslated, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnTlatedAdd, DrawColumnTlatedAdd, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnShaded, DrawColumnShaded, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnAddClamp, DrawColumnAddClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnAddClampTranslated, DrawColumnAddClampTranslated, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnSubClamp, DrawColumnSubClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnSubClampTranslated, DrawColumnSubClampTranslated, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRevSubClamp, DrawColumnRevSubClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRevSubClampTranslated, DrawColumnRevSubClampTranslated, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(FillColumn, FillColumn, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(FillColumnAdd, FillColumnAdd, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(FillColumnAddClamp, FillColumnAddClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(FillColumnSubClamp, FillColumnSubClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(FillColumnRevSubClamp, FillColumnRevSubClamp, DrawColumnLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawSingleSky1, DrawSky1, DrawSkyLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawSingleSky4, DrawSky4, DrawSkyLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawDoubleSky1, DrawDoubleSky1, DrawSkyLLVMCommand);
	DECLARE_DRAW_COMMAND(DrawDoubleSky4, DrawDoubleSky4, DrawSkyLLVMCommand);

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
		DrawFuzzColumnRGBACommand();
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class FillSpanRGBACommand : public DrawerCommand
	{
		int _x1;
		int _x2;
		int _y;
		uint8_t * RESTRICT _destorg;
		fixed_t _light;
		int _color;

	public:
		FillSpanRGBACommand();
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawSlabRGBACommand : public DrawerCommand
	{
		int _dx;
		fixed_t _v;
		int _dy;
		fixed_t _vi;
		const uint8_t *_voxelptr;
		uint32_t *_p;
		ShadeConstants _shade_constants;
		const uint8_t *_colormap;
		fixed_t _light;
		int _pitch;
		int _start_y;

	public:
		DrawSlabRGBACommand(int dx, fixed_t v, int dy, fixed_t vi, const uint8_t *vptr, uint8_t *p, ShadeConstants shade_constants, const uint8_t *colormap, fixed_t light);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawFogBoundaryLineRGBACommand : public DrawerCommand
	{
		int _y;
		int _x;
		int _x2;
		uint8_t * RESTRICT _destorg;
		fixed_t _light;
		ShadeConstants _shade_constants;

	public:
		DrawFogBoundaryLineRGBACommand(int y, int x, int x2);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawTiltedSpanRGBACommand : public DrawerCommand
	{
		int _x1;
		int _x2;
		int _y;
		uint8_t * RESTRICT _destorg;
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

	public:
		DrawTiltedSpanRGBACommand(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class DrawColoredSpanRGBACommand : public DrawerCommand
	{
		int _y;
		int _x1;
		int _x2;
		uint8_t * RESTRICT _destorg;
		fixed_t _light;
		int _color;

	public:
		DrawColoredSpanRGBACommand(int y, int x1, int x2);

		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class FillTransColumnRGBACommand : public DrawerCommand
	{
		int _x;
		int _y1;
		int _y2;
		int _color;
		int _a;
		uint8_t * RESTRICT _destorg;
		int _pitch;
		fixed_t _light;

	public:
		FillTransColumnRGBACommand(int x, int y1, int y2, int color, int a);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

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

	class DrawColumnRt1LLVMCommand : public DrawerCommand
	{
	protected:
		DrawColumnArgs args;
		WorkerThreadData ThreadData(DrawerThread *thread);

	public:
		DrawColumnRt1LLVMCommand(int hx, int sx, int yl, int yh);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	DECLARE_DRAW_COMMAND(DrawColumnRt1Copy, DrawColumnRt1Copy, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1Add, DrawColumnRt1Add, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1Shaded, DrawColumnRt1Shaded, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1AddClamp, DrawColumnRt1AddClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1SubClamp, DrawColumnRt1SubClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1RevSubClamp, DrawColumnRt1RevSubClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1Translated, DrawColumnRt1Translated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1TlatedAdd, DrawColumnRt1TlatedAdd, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1AddClampTranslated, DrawColumnRt1AddClampTranslated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1SubClampTranslated, DrawColumnRt1SubClampTranslated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt1RevSubClampTranslated, DrawColumnRt1RevSubClampTranslated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4, DrawColumnRt4, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4Copy, DrawColumnRt4Copy, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4Add, DrawColumnRt4Add, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4Shaded, DrawColumnRt4Shaded, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4AddClamp, DrawColumnRt4AddClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4SubClamp, DrawColumnRt4SubClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4RevSubClamp, DrawColumnRt4RevSubClamp, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4Translated, DrawColumnRt4Translated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4TlatedAdd, DrawColumnRt4TlatedAdd, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4AddClampTranslated, DrawColumnRt4AddClampTranslated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4SubClampTranslated, DrawColumnRt4SubClampTranslated, DrawColumnRt1LLVMCommand);
	DECLARE_DRAW_COMMAND(DrawColumnRt4RevSubClampTranslated, DrawColumnRt4RevSubClampTranslated, DrawColumnRt1LLVMCommand);

	/////////////////////////////////////////////////////////////////////////////

	class RtInitColsRGBACommand : public DrawerCommand
	{
		BYTE * RESTRICT buff;

	public:
		RtInitColsRGBACommand(BYTE *buff);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	template<typename InputPixelType>
	class DrawColumnHorizRGBACommand : public DrawerCommand
	{
		int _count;
		fixed_t _iscale;
		fixed_t _texturefrac;
		const InputPixelType * RESTRICT _source;
		int _x;
		int _yl;
		int _yh;

	public:
		DrawColumnHorizRGBACommand();
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	class FillColumnHorizRGBACommand : public DrawerCommand
	{
		int _x;
		int _yl;
		int _yh;
		int _count;
		uint32_t _color;

	public:
		FillColumnHorizRGBACommand();
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;
	};

	/////////////////////////////////////////////////////////////////////////////
	// Pixel shading inline functions:

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
}
