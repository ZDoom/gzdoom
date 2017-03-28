/*
**  Projected triangle drawer
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
class PolyDrawArgs;

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
	uint32_t *temp;

	// Triangle working data:
	TriFullSpan *FullSpans;
	TriPartialBlock *PartialBlocks;
	uint32_t NumFullSpans;
	uint32_t NumPartialBlocks;
	int32_t StartX;
	int32_t StartY;
};

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v) : x(x), y(y), z(z), w(w), u(u), v(v) { }

	float x, y, z, w;
	float u, v;
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
	uint8_t *stencilValues;
	uint32_t *stencilMasks;
	int32_t stencilPitch;
	uint32_t *subsectorGBuffer;
	const PolyDrawArgs *uniforms;
};

class RectDrawArgs;

enum class TriBlendMode
{
	TextureOpaque,
	TextureMasked,
	TextureAdd,
	TextureSub,
	TextureRevSub,
	TextureAddSrcColor,
	TranslatedOpaque,
	TranslatedMasked,
	TranslatedAdd,
	TranslatedSub,
	TranslatedRevSub,
	TranslatedAddSrcColor,
	Shaded,
	AddShaded,
	Stencil,
	AddStencil,
	FillOpaque,
	FillAdd,
	FillSub,
	FillRevSub,
	FillAddSrcColor,
	Skycap
};

class ScreenTriangle
{
public:
	static void SetupNormal(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void SetupSubsector(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void StencilWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void SubsectorWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread);

	static void(*TriDrawers8[])(const TriDrawTriangleArgs *, WorkerThreadData *);
	static void(*TriDrawers32[])(const TriDrawTriangleArgs *, WorkerThreadData *);
	static void(*RectDrawers8[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *);
	static void(*RectDrawers32[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *);
};

struct ScreenTriangleStepVariables
{
	float W, U, V;
};

namespace TriScreenDrawerModes
{
	enum class BlendModes { Opaque, Masked, AddClamp, SubClamp, RevSubClamp, AddSrcColorOneMinusSrcColor, Shaded, AddClampShaded };
	struct OpaqueBlend { static const int Mode = (int)BlendModes::Opaque; };
	struct MaskedBlend { static const int Mode = (int)BlendModes::Masked; };
	struct AddClampBlend { static const int Mode = (int)BlendModes::AddClamp; };
	struct SubClampBlend { static const int Mode = (int)BlendModes::SubClamp; };
	struct RevSubClampBlend { static const int Mode = (int)BlendModes::RevSubClamp; };
	struct AddSrcColorBlend { static const int Mode = (int)BlendModes::AddSrcColorOneMinusSrcColor; };
	struct ShadedBlend { static const int Mode = (int)BlendModes::Shaded; };
	struct AddClampShadedBlend { static const int Mode = (int)BlendModes::AddClampShaded; };

	enum class FilterModes { Nearest, Linear };
	struct NearestFilter { static const int Mode = (int)FilterModes::Nearest; };
	struct LinearFilter { static const int Mode = (int)FilterModes::Linear; };

	enum class ShadeMode { Simple, Advanced };
	struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
	struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };

	enum class Samplers { Texture, Fill, Shaded, Stencil, Translated, Skycap };
	struct TextureSampler { static const int Mode = (int)Samplers::Texture; };
	struct FillSampler { static const int Mode = (int)Samplers::Fill; };
	struct ShadedSampler { static const int Mode = (int)Samplers::Shaded; };
	struct StencilSampler { static const int Mode = (int)Samplers::Stencil; };
	struct TranslatedSampler { static const int Mode = (int)Samplers::Translated; };
	struct SkycapSampler { static const int Mode = (int)Samplers::Skycap; };
}
