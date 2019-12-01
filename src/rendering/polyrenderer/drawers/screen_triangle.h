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
#include "r_data/renderstyle.h"
#include "rendering/swrenderer/drawers/r_draw.h"

class FString;
class PolyTriangleThreadData;

struct ScreenTriVertex
{
	float x, y, z, w;
	float u, v;
	float worldX, worldY, worldZ;
};

struct ScreenTriangleStepVariables
{
	float W, U, V;
	float WorldX, WorldY, WorldZ, Padding; // Padding so it can be loaded directly into a XMM register
};

struct TriDrawTriangleArgs
{
	ScreenTriVertex *v1;
	ScreenTriVertex *v2;
	ScreenTriVertex *v3;
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

enum class TriBlendMode
{
	Opaque,
	Skycap,
	FogBoundary,
	SrcColor,
	Fill,
	FillTranslucent,
	Normal,
	AlphaTest,
	Fuzzy,
	Stencil,
	Translucent,
	Add,
	Shaded,
	TranslucentStencil,
	Shadow,
	Subtract,
	AddStencil,
	AddShaded,
	OpaqueTranslated,
	SrcColorTranslated,
	NormalTranslated,
	StencilTranslated,
	TranslucentTranslated,
	AddTranslated,
	ShadedTranslated,
	TranslucentStencilTranslated,
	ShadowTranslated,
	SubtractTranslated,
	AddStencilTranslated,
	AddShadedTranslated
};

class ScreenTriangle
{
public:
	static void Draw(const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread);

private:
	static void(*TestSpanOpts[])(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread);
};

namespace TriScreenDrawerModes
{
	enum SWTestSpan
	{
		SWTRI_DepthTest = 1,
		SWTRI_StencilTest = 2
	};

	struct TestSpanOpt0 { static const int Flags = 0; };
	struct TestSpanOpt1 { static const int Flags = 1; };
	struct TestSpanOpt2 { static const int Flags = 2; };
	struct TestSpanOpt3 { static const int Flags = 3; };
}
