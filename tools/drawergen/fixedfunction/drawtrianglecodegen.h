/*
**  DrawTriangle code generation
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

#include "drawercodegen.h"

struct SSATriVertex
{
	SSAFloat x, y, z, w;
	SSAFloat varying[TriVertex::NumVarying];
};

class DrawTriangleCodegen : public DrawerCodegen
{
public:
	void Generate(TriDrawVariant variant, TriBlendMode blendmode, bool truecolor, SSAValue args, SSAValue thread_data);

private:
	void LoadArgs(SSAValue args, SSAValue thread_data);
	SSATriVertex LoadTriVertex(SSAValue v);
	void LoadUniforms(SSAValue uniforms);
	void Setup();
	SSAInt FloatTo28_4(SSAFloat v);
	void LoopBlockY();
	void LoopBlockX();
	void LoopFullBlock();
	void LoopPartialBlock();
	void SetupAffineBlock();

	SSAVec4i ProcessPixel32(SSAVec4i bg, SSAInt *varying);
	SSAInt ProcessPixel8(SSAInt bg, SSAInt *varying);

	SSAVec4i TranslateSample32(SSAInt uvoffset);
	SSAInt TranslateSample8(SSAInt uvoffset);
	SSAVec4i Sample32(SSAInt uvoffset);
	SSAInt Sample8(SSAInt uvoffset);
	SSAInt Shade8(SSAInt c);
	SSAVec4i ToBgra(SSAInt index);
	SSAInt ToPal8(SSAVec4i c);

	void SetStencilBlock(SSAInt block);
	void StencilSet(SSAInt x, SSAInt y, SSAUByte value);
	void StencilClear(SSAUByte value);
	SSAUByte StencilGet(SSAInt x, SSAInt y);
	SSAUByte StencilGetSingle();
	SSABool StencilIsSingleValue();

	SSAFloat gradx(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2);
	SSAFloat grady(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2);

	TriDrawVariant variant;
	TriBlendMode blendmode;
	bool truecolor;

	SSAStack<SSAInt> stack_C1, stack_C2, stack_C3;
	SSAStack<SSAInt> stack_y;
	SSAStack<SSAUBytePtr> stack_dest;
	SSAStack<SSAIntPtr> stack_subsectorGBuffer;
	SSAStack<SSAInt> stack_x;
	SSAStack<SSAUBytePtr> stack_buffer;
	SSAStack<SSAIntPtr> stack_subsectorbuffer;
	SSAStack<SSAInt> stack_iy, stack_ix;
	SSAStack<SSAInt> stack_CY1, stack_CY2, stack_CY3;
	SSAStack<SSAInt> stack_CX1, stack_CX2, stack_CX3;
	SSAStack<SSAFloat> stack_posy_w, stack_posy_varying[TriVertex::NumVarying];
	SSAStack<SSAFloat> stack_posx_w, stack_posx_varying[TriVertex::NumVarying];
	SSAStack<SSAFloat> stack_AffineW;
	SSAStack<SSAFloat> stack_AffineVaryingPosY[TriVertex::NumVarying];
	SSAStack<SSAInt> stack_AffineVaryingPosX[TriVertex::NumVarying];

	SSAUBytePtr dest;
	SSAInt pitch;
	SSATriVertex v1;
	SSATriVertex v2;
	SSATriVertex v3;
	SSAInt clipright;
	SSAInt clipbottom;
	SSAUBytePtr texturePixels;
	SSAInt textureWidth;
	SSAInt textureHeight;
	SSAUBytePtr translation;
	SSAInt color, srcalpha, destalpha;

	SSAInt light;
	SSAInt subsectorDepth;
	SSAShadeConstants shade_constants;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSABool is_fixed_light;

	SSAUBytePtr stencilValues;
	SSAIntPtr stencilMasks;
	SSAInt stencilPitch;
	SSAUByte stencilTestValue;
	SSAUByte stencilWriteValue;
	SSAIntPtr subsectorGBuffer;

	SSAUBytePtr Colormaps;
	SSAUBytePtr RGB32k;
	SSAUBytePtr BaseColors;

	SSAWorkerThread thread;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	SSAInt Y1, Y2, Y3;
	SSAInt X1, X2, X3;
	SSAInt DX12, DX23, DX31;
	SSAInt DY12, DY23, DY31;
	SSAInt FDX12, FDX23, FDX31;
	SSAInt FDY12, FDY23, FDY31;
	SSAInt minx, maxx, miny, maxy;
	SSAInt C1, C2, C3;
	SSAFloat gradWX, gradWY;
	SSAFloat gradVaryingX[TriVertex::NumVarying], gradVaryingY[TriVertex::NumVarying];
	SSAFloat shade;

	SSAInt x, y;
	SSAInt x0, x1, y0, y1;
	SSAInt currentlight;
	SSAUBytePtr currentcolormap;
	SSAFloat AffineW;
	SSAFloat AffineVaryingPosY[TriVertex::NumVarying];
	SSAInt AffineVaryingPosX[TriVertex::NumVarying];
	SSAInt AffineVaryingStepX[TriVertex::NumVarying];

	SSAFloat posy_w, posy_varying[TriVertex::NumVarying];
	SSAFloat posx_w, posx_varying[TriVertex::NumVarying];

	SSAUBytePtr StencilBlock;
	SSAIntPtr StencilBlockMask;
};
