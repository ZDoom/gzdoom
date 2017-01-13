/*
**  LLVM code generated drawers
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

#include <cstdint>
#include <vector>

class FString;

struct TriFullSpan
{
	uint16_t X;
	uint16_t Y;
	uint32_t Length;
};

struct TriPartialBlock
{
	uint16_t X;
	uint16_t Y;
	uint32_t Mask0;
	uint32_t Mask1;
};

struct WorkerThreadData
{
	int32_t core;
	int32_t num_cores;
	int32_t pass_start_y;
	int32_t pass_end_y;
	uint32_t *temp;

	// Triangle working data:
	TriFullSpan *FullSpans;
	TriPartialBlock *PartialBlocks;
	uint32_t NumFullSpans;
	uint32_t NumPartialBlocks;
	int32_t StartX;
	int32_t StartY;
};

struct TriLight
{
	uint32_t color;
	float x, y, z;
	float radius;
};

struct DrawWallArgs
{
	uint32_t *dest;
	const uint32_t *source[4];
	const uint32_t *source2[4];
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t texturefrac[4];
	uint32_t texturefracx[4];
	uint32_t iscale[4];
	uint32_t textureheight[4];
	uint32_t light[4];
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};

	float z, step_z;
	TriLight *dynlights;
	uint32_t num_dynlights;

	FString ToString();
};

struct DrawSpanArgs
{
	uint32_t *destorg;
	const uint32_t *source;
	int32_t destpitch;
	int32_t xfrac;
	int32_t yfrac;
	int32_t xstep;
	int32_t ystep;
	int32_t x1;
	int32_t x2;
	int32_t y;
	int32_t xbits;
	int32_t ybits;
	uint32_t light;
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};

	float viewpos_x, step_viewpos_x;
	TriLight *dynlights;
	uint32_t num_dynlights;

	FString ToString();
};

struct DrawColumnArgs
{
	uint32_t *dest;
	const uint8_t *source;
	const uint8_t *source2;
	uint8_t *colormap;
	uint8_t *translation;
	const uint32_t *basecolors;
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t iscale;
	uint32_t texturefracx;
	uint32_t textureheight;
	uint32_t texturefrac;
	uint32_t light;
	uint32_t color;
	uint32_t srccolor;
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};

	FString ToString();
};

struct DrawSkyArgs
{
	uint32_t *dest;
	const uint32_t *source0[4];
	const uint32_t *source1[4];
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t texturefrac[4];
	uint32_t iscale[4];
	uint32_t textureheight0;
	uint32_t textureheight1;
	uint32_t top_color;
	uint32_t bottom_color;

	FString ToString();
};

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v) : x(x), y(y), z(z), w(w) { varying[0] = u; varying[1] = v; }

	enum { NumVarying = 2 };
	float x, y, z, w;
	float varying[NumVarying];
};

struct TriUniforms
{
	uint32_t light;
	uint32_t subsectorDepth;
	uint32_t color;
	uint32_t srcalpha;
	uint32_t destalpha;
	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	float globvis;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2,
		fixed_light = 4
	};
};

struct TriDrawTriangleArgs
{
	uint8_t *dest;
	int32_t pitch;
	TriVertex *v1;
	TriVertex *v2;
	TriVertex *v3;
	int32_t clipleft;
	int32_t clipright;
	int32_t cliptop;
	int32_t clipbottom;
	const uint8_t *texturePixels;
	uint32_t textureWidth;
	uint32_t textureHeight;
	const uint8_t *translation;
	const TriUniforms *uniforms;
	uint8_t *stencilValues;
	uint32_t *stencilMasks;
	int32_t stencilPitch;
	uint8_t stencilTestValue;
	uint8_t stencilWriteValue;
	uint32_t *subsectorGBuffer;
	const uint8_t *colormaps;
	const uint8_t *RGB256k;
	const uint8_t *BaseColors;
};

enum class TriBlendMode
{
	Copy,           // blend_copy(shade(fg))
	AlphaBlend,     // blend_alpha_blend(shade(fg), bg)
	AddSolid,       // blend_add(shade(fg), bg, srcalpha, destalpha)
	Add,            // blend_add(shade(fg), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	Sub,            // blend_sub(shade(fg), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	RevSub,         // blend_revsub(shade(fg), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	Stencil,        // blend_stencil(shade(color), fg.a, bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	Shaded,         // blend_stencil(shade(color), fg.index, bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	TranslateCopy,  // blend_copy(shade(translate(fg)))
	TranslateAlphaBlend, // blend_alpha_blend(shade(translate(fg)), bg)
	TranslateAdd,   // blend_add(shade(translate(fg)), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	TranslateSub,   // blend_sub(shade(translate(fg)), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	TranslateRevSub,// blend_revsub(shade(translate(fg)), bg, srcalpha, calc_blend_bgalpha(fg, destalpha))
	AddSrcColorOneMinusSrcColor, // glBlendMode(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR) used by GZDoom's fullbright additive sprites
	Skycap          // Fade to sky color when the V texture coordinate go beyond the [-1, 1] range
};

inline int NumTriBlendModes() { return (int)TriBlendMode::Skycap + 1; }

class Drawers
{
public:
	static Drawers *Instance();

	void(*DrawColumn)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnShaded)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnTlatedAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAddClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRevSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumn)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnAddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnRevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;

	void(*DrawSpan)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMasked)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanTranslucent)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMaskedTranslucent)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanAddClamp)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMaskedAddClamp)(const DrawSpanArgs *) = nullptr;

	void(*vlinec1)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*mvlinec1)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_add)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_addclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_subclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_revsubclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;

	void(*DrawSky1)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawDoubleSky1)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;

	std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> TriDraw8;
	std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> TriDraw32;
	std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> TriFill8;
	std::vector<void(*)(const TriDrawTriangleArgs *, WorkerThreadData *)> TriFill32;
	
private:
	Drawers();
};
