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

#pragma once

#include <cstdint>
#include <vector>

class FString;
class PolyDrawArgs;

struct WorkerThreadData
{
	int32_t core;
	int32_t num_cores;

	// The number of lines to skip to reach the first line to be rendered by this thread
	int skipped_by_thread(int first_line)
	{
		int core_skip = (num_cores - (first_line - core) % num_cores) % num_cores;
		return core_skip;
	}
};

struct ShadedTriVertex
{
	float x, y, z, w;
	float u, v;
	float clipDistance[3];
	float worldX, worldY, worldZ;
};

struct ScreenTriangleStepVariables
{
	float W, U, V;
	float WorldX, WorldY, WorldZ, Padding; // Padding so it can be loaded directly into a XMM register
};

struct TriDrawTriangleArgs
{
	uint8_t *dest;
	int32_t pitch;
	ShadedTriVertex *v1;
	ShadedTriVertex *v2;
	ShadedTriVertex *v3;
	int32_t clipright;
	int32_t clipbottom;
	uint8_t *stencilValues;
	uint32_t *stencilMasks;
	int32_t stencilPitch;
	float *zbuffer;
	const PolyDrawArgs *uniforms;
	bool destBgra;
	ScreenTriangleStepVariables gradientX;
	ScreenTriangleStepVariables gradientY;

	bool CalculateGradients()
	{
		float bottomX = (v2->x - v3->x) * (v1->y - v3->y) - (v1->x - v3->x) * (v2->y - v3->y);
		float bottomY = (v1->x - v3->x) * (v2->y - v3->y) - (v2->x - v3->x) * (v1->y - v3->y);
		if ((bottomX >= -FLT_EPSILON && bottomX <= FLT_EPSILON) || (bottomY >= -FLT_EPSILON && bottomY <= FLT_EPSILON))
			return false;

		gradientX.W = FindGradientX(bottomX, 1.0f, 1.0f, 1.0f);
		gradientX.U = FindGradientX(bottomX, v1->u, v2->u, v3->u);
		gradientX.V = FindGradientX(bottomX, v1->v, v2->v, v3->v);
		gradientX.WorldX = FindGradientX(bottomX, v1->worldX, v2->worldX, v3->worldX);
		gradientX.WorldY = FindGradientX(bottomX, v1->worldY, v2->worldY, v3->worldY);
		gradientX.WorldZ = FindGradientX(bottomX, v1->worldZ, v2->worldZ, v3->worldZ);

		gradientY.W = FindGradientY(bottomY, 1.0f, 1.0f, 1.0f);
		gradientY.U = FindGradientY(bottomY, v1->u, v2->u, v3->u);
		gradientY.V = FindGradientY(bottomY, v1->v, v2->v, v3->v);
		gradientY.WorldX = FindGradientY(bottomY, v1->worldX, v2->worldX, v3->worldX);
		gradientY.WorldY = FindGradientY(bottomY, v1->worldY, v2->worldY, v3->worldY);
		gradientY.WorldZ = FindGradientY(bottomY, v1->worldZ, v2->worldZ, v3->worldZ);

		return true;
	}

private:
	float FindGradientX(float bottomX, float c0, float c1, float c2)
	{
		c0 *= v1->w;
		c1 *= v2->w;
		c2 *= v3->w;
		return ((c1 - c2) * (v1->y - v3->y) - (c0 - c2) * (v2->y - v3->y)) / bottomX;
	}

	float FindGradientY(float bottomY, float c0, float c1, float c2)
	{
		c0 *= v1->w;
		c1 *= v2->w;
		c2 *= v3->w;
		return ((c1 - c2) * (v1->x - v3->x) - (c0 - c2) * (v2->x - v3->x)) / bottomY;
	}
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
	Skycap,
	Fuzz,
	FogBoundary
};

class ScreenTriangle
{
public:
	static void Draw(const TriDrawTriangleArgs *args, WorkerThreadData *thread);

	static void(*TriDrawers8[])(int, int, uint32_t, uint32_t, const TriDrawTriangleArgs *);
	static void(*TriDrawers32[])(int, int, uint32_t, uint32_t, const TriDrawTriangleArgs *);
	static void(*RectDrawers8[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *);
	static void(*RectDrawers32[])(const void *, int, int, int, const RectDrawArgs *, WorkerThreadData *);

	static int FuzzStart;
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

	enum class ShadeMode { None, Simple, Advanced };
	struct NoShade { static const int Mode = (int)ShadeMode::None; };
	struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
	struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };

	enum class Samplers { Texture, Fill, Shaded, Stencil, Translated, Skycap, Fuzz, FogBoundary };
	struct TextureSampler { static const int Mode = (int)Samplers::Texture; };
	struct FillSampler { static const int Mode = (int)Samplers::Fill; };
	struct ShadedSampler { static const int Mode = (int)Samplers::Shaded; };
	struct StencilSampler { static const int Mode = (int)Samplers::Stencil; };
	struct TranslatedSampler { static const int Mode = (int)Samplers::Translated; };
	struct SkycapSampler { static const int Mode = (int)Samplers::Skycap; };
	struct FuzzSampler { static const int Mode = (int)Samplers::Fuzz; };
	struct FogBoundarySampler { static const int Mode = (int)Samplers::FogBoundary; };

	static const int fuzzcolormap[FUZZTABLE] =
	{
		 6, 11,  6, 11,  6,  6, 11,  6,  6, 11, 
		 6,  6,  6, 11,  6,  6,  6, 11, 15, 18, 
		21,  6, 11, 15,  6,  6,  6,  6, 11,  6, 
		11,  6,  6, 11, 15,  6,  6, 11, 15, 18, 
		21,  6,  6,  6,  6, 11,  6,  6, 11,  6, 
	};
}
