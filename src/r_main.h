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


#ifndef __R_MAIN_H__
#define __R_MAIN_H__

#include "r_utility.h"
#include "d_player.h"
#include "v_palette.h"
#include "r_data/colormaps.h"


typedef BYTE lighttable_t;	// This could be wider for >8 bit display.

//
// POV related.
//
extern bool				bRenderingToCanvas;
extern double			ViewCos;
extern double			ViewSin;
extern fixed_t			viewingrangerecip;
extern double			FocalLengthX, FocalLengthY;
extern double			InvZtoScale;

extern double			WallTMapScale2;

extern int				viewwindowx;
extern int				viewwindowy;

extern double			CenterX;
extern double			CenterY;
extern double			YaspectMul;
extern double			IYaspectMul;

extern FDynamicColormap*basecolormap;	// [RH] Colormap for sector currently being drawn

extern int				linecount;
extern int				loopcount;

extern bool				r_dontmaplines;

//
// Lighting.
//
// [RH] This has changed significantly from Doom, which used lookup
// tables based on 1/z for walls and z for flats and only recognized
// 16 discrete light levels. The terminology I use is borrowed from Build.
//

// The size of a single colormap, in bits
#define COLORMAPSHIFT			8

// Convert a light level into an unbounded colormap index (shade). Result is
// fixed point. Why the +12? I wish I knew, but experimentation indicates it
// is necessary in order to best reproduce Doom's original lighting.
#define LIGHT2SHADE(l)			((NUMCOLORMAPS*2*FRACUNIT)-(((l)+12)*(FRACUNIT*NUMCOLORMAPS/128)))

// MAXLIGHTSCALE from original DOOM, divided by 2.
#define MAXLIGHTVIS				(24.0)

// Convert a shade and visibility to a clamped colormap index.
// Result is not fixed point.
// Change R_CalcTiltedLighting() when this changes.
#define GETPALOOKUP(vis,shade)	(clamp<int> (((shade)-FLOAT2FIXED(MIN(MAXLIGHTVIS,double(vis))))>>FRACBITS, 0, NUMCOLORMAPS-1))

// Calculate the light multiplier for dc_light/ds_light
// This is used instead of GETPALOOKUP when ds_colormap/dc_colormap is set to the base colormap
// Returns a value between 0 and 1 in fixed point
#define LIGHTSCALE(vis,shade) FLOAT2FIXED(clamp((FIXED2DBL(shade) - (MIN(MAXLIGHTVIS,double(vis)))) / NUMCOLORMAPS, 0.0, (NUMCOLORMAPS-1)/(double)NUMCOLORMAPS))

// Converts fixedlightlev into a shade value
#define FIXEDLIGHT2SHADE(lightlev) (((lightlev) >> COLORMAPSHIFT) << FRACBITS)

struct ShadeConstants
{
	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	bool simple_shade;
};

// calculates the light constant passed to the shade_pal_index function
inline uint32_t calc_light_multiplier(dsfixed_t light)
{
	return 256 - (light >> (FRACBITS - 8));
}

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

// Calculates a ARGB8 color for the given palette index, light multiplier and dynamic colormap
FORCEINLINE uint32_t shade_pal_index(uint32_t index, uint32_t light, const ShadeConstants &constants)
{
	const PalEntry &color = GPalette.BaseColors[index];
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
	return 0xff000000 | (red << 16) | (green << 8) | blue;
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
		shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	__m128i fade = _mm_set_epi16( \
		shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
	__m128i fade_amount_hi = _mm_mullo_epi16(fade, _mm_subs_epu16(_mm_set1_epi16(256), mlight_hi)); \
	__m128i fade_amount_lo = fade_amount_hi; \
	__m128i inv_desaturate = _mm_set1_epi16(256 - shade_constants.desaturate); \

// Calculate constants for a complex shade with different light levels for each pixel
#define SSE_SHADE_INIT4(light3, light2, light1, light0, shade_constants) \
	__m128i mlight_hi = _mm_set_epi16(256, light1, light1, light1, 256, light0, light0, light0); \
	__m128i mlight_lo = _mm_set_epi16(256, light3, light3, light3, 256, light2, light2, light2); \
	__m128i color = _mm_set_epi16( \
		shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, \
		shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue); \
	__m128i fade = _mm_set_epi16( \
		shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, \
		shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue); \
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

extern bool				r_swtruecolor;

extern double			GlobVis;

void R_SetVisibility(double visibility);
double R_GetVisibility();

extern double			r_BaseVisibility;
extern double			r_WallVisibility;
extern double			r_FloorVisibility;
extern float			r_TiltVisibility;
extern double			r_SpriteVisibility;

extern int				r_actualextralight;
extern bool				foggy;
extern int				fixedlightlev;
extern FColormap*		fixedcolormap;
extern FSpecialColormap*realfixedcolormap;


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void 			(*colfunc) (void);
extern void 			(*basecolfunc) (void);
extern void 			(*fuzzcolfunc) (void);
extern void				(*transcolfunc) (void);
// No shadow effects on floors.
extern void 			(*spanfunc) (void);

// [RH] Function pointers for the horizontal column drawers.
extern void (*hcolfunc_pre) (void);
extern void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
extern void (*hcolfunc_post4) (int sx, int yl, int yh);


void R_InitTextureMapping ();


//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderActorView (AActor *actor, bool dontmaplines = false);
void R_SetupBuffer ();

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines = false);

// [RH] Initialize multires stuff for renderer
void R_MultiresInit (void);


extern int stacked_extralight;
extern double stacked_visibility;
extern DVector3 stacked_viewpos;
extern DAngle stacked_angle;

extern void R_CopyStackedViewParameters();


#endif // __R_MAIN_H__
