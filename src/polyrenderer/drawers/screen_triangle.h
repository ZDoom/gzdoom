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
class PolyTriangleThreadData;

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
	uint8_t *stencilbuffer;
	int stencilpitch;
	float *zbuffer;
	const PolyDrawArgs *uniforms;
	bool destBgra;
	ScreenTriangleStepVariables gradientX;
	ScreenTriangleStepVariables gradientY;
	float depthOffset;

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
	Opaque,
	Skycap,
	FogBoundary,
	SrcColor,
	Fill,
	Normal,
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
	static void Draw(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread);

	static void(*SpanDrawers8[])(int y, int x0, int x1, const TriDrawTriangleArgs *args);
	static void(*SpanDrawers32[])(int y, int x0, int x1, const TriDrawTriangleArgs *args);
	static void(*RectDrawers8[])(const void *, int, int, int, const RectDrawArgs *, PolyTriangleThreadData *);
	static void(*RectDrawers32[])(const void *, int, int, int, const RectDrawArgs *, PolyTriangleThreadData *);

	static int FuzzStart;
};

namespace TriScreenDrawerModes
{
	enum SWStyleFlags
	{
		SWSTYLEF_Translated = 1,
		SWSTYLEF_Skycap = 2,
		SWSTYLEF_FogBoundary = 4,
		SWSTYLEF_Fill = 8,
		SWSTYLEF_SrcColorOneMinusSrcColor = 16
	};

	struct StyleOpaque             { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_One, BlendDest = STYLEALPHA_Zero,   Flags = STYLEF_Alpha1, SWFlags = 0; };
	struct StyleSkycap             { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_One, BlendDest = STYLEALPHA_Zero,   Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_Skycap; };
	struct StyleFogBoundary        { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_One, BlendDest = STYLEALPHA_Zero,   Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_FogBoundary; };
	struct StyleSrcColor           { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_SrcColorOneMinusSrcColor; };
	struct StyleFill               { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_One, BlendDest = STYLEALPHA_Zero,   Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_Fill; };

	struct StyleNormal             { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1, SWFlags = 0; };
	struct StyleFuzzy              { static const int BlendOp = STYLEOP_Fuzz,   BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = 0, SWFlags = 0; };
	struct StyleStencil            { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1 | STYLEF_ColorIsFixed, SWFlags = 0; };
	struct StyleTranslucent        { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = 0, SWFlags = 0; };
	struct StyleAdd                { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = 0, SWFlags = 0; };
	struct StyleShaded             { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_RedIsAlpha | STYLEF_ColorIsFixed, SWFlags = 0; };
	struct StyleTranslucentStencil { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_ColorIsFixed, SWFlags = 0; };
	struct StyleShadow             { static const int BlendOp = STYLEOP_Shadow, BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = 0, SWFlags = 0; };
	struct StyleSubtract           { static const int BlendOp = STYLEOP_RevSub, BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = 0, SWFlags = 0; };
	struct StyleAddStencil         { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = STYLEF_ColorIsFixed, SWFlags = 0; };
	struct StyleAddShaded          { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = STYLEF_RedIsAlpha | STYLEF_ColorIsFixed, SWFlags = 0; };

	struct StyleOpaqueTranslated   { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_One, BlendDest = STYLEALPHA_Zero,   Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_Translated; };
	struct StyleSrcColorTranslated { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_Translated|SWSTYLEF_SrcColorOneMinusSrcColor; };
	struct StyleNormalTranslated   { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1, SWFlags = SWSTYLEF_Translated; };
	struct StyleStencilTranslated  { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_Alpha1 | STYLEF_ColorIsFixed, SWFlags = SWSTYLEF_Translated; };
	struct StyleTranslucentTranslated { static const int BlendOp = STYLEOP_Add, BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = 0, SWFlags = SWSTYLEF_Translated; };
	struct StyleAddTranslated      { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = 0, SWFlags = SWSTYLEF_Translated; };
	struct StyleShadedTranslated   { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_RedIsAlpha | STYLEF_ColorIsFixed, SWFlags = SWSTYLEF_Translated; };
	struct StyleTranslucentStencilTranslated { static const int BlendOp = STYLEOP_Add,    BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = STYLEF_ColorIsFixed, SWFlags = SWSTYLEF_Translated; };
	struct StyleShadowTranslated   { static const int BlendOp = STYLEOP_Shadow, BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_InvSrc, Flags = 0, SWFlags = SWSTYLEF_Translated; };
	struct StyleSubtractTranslated { static const int BlendOp = STYLEOP_RevSub, BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = 0, SWFlags = SWSTYLEF_Translated; };
	struct StyleAddStencilTranslated { static const int BlendOp = STYLEOP_Add,  BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = STYLEF_ColorIsFixed, SWFlags = SWSTYLEF_Translated; };
	struct StyleAddShadedTranslated { static const int BlendOp = STYLEOP_Add,   BlendSrc = STYLEALPHA_Src, BlendDest = STYLEALPHA_One,    Flags = STYLEF_RedIsAlpha | STYLEF_ColorIsFixed, SWFlags = SWSTYLEF_Translated; };

	enum SWOptFlags
	{
		SWOPT_DynLights = 1,
		SWOPT_ColoredFog = 2,
		SWOPT_FixedLight = 4
	};

	struct DrawerOpt { static const int Flags = 0; };
	struct DrawerOptF { static const int Flags = SWOPT_FixedLight; };
	struct DrawerOptC { static const int Flags = SWOPT_ColoredFog; };
	struct DrawerOptCF { static const int Flags = SWOPT_ColoredFog | SWOPT_FixedLight; };
	struct DrawerOptL { static const int Flags = SWOPT_DynLights; };
	struct DrawerOptLC { static const int Flags = SWOPT_DynLights | SWOPT_ColoredFog; };
	struct DrawerOptLF { static const int Flags = SWOPT_DynLights | SWOPT_FixedLight; };
	struct DrawerOptLCF { static const int Flags = SWOPT_DynLights | SWOPT_ColoredFog | SWOPT_FixedLight; };

	static const int fuzzcolormap[FUZZTABLE] =
	{
		 6, 11,  6, 11,  6,  6, 11,  6,  6, 11, 
		 6,  6,  6, 11,  6,  6,  6, 11, 15, 18, 
		21,  6, 11, 15,  6,  6,  6,  6, 11,  6, 
		11,  6,  6, 11, 15,  6,  6, 11, 15, 18, 
		21,  6,  6,  6,  6, 11,  6,  6, 11,  6, 
	};
}
